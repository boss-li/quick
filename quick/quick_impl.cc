#include "quick_impl.h"
#include <chrono>
#include <random>
#include <thread>
#include "quick_proto.h"

LogWrapper g_quic_log;
// static
std::unique_ptr<Quick> Quick::Create(uint16_t port, bool as_server, QuickCallback* cb) {
	return std::unique_ptr<Quick>(new QuickImpl(port, as_server, cb));
}

QuickImpl::QuickImpl(uint16_t port, bool as_server, QuickCallback* cb)
	: _local_port(port), _as_server(as_server), _task_queue_factory(CreateTaskQueueFactory()),
	_task_queue(_task_queue_factory->CreateTaskQueue("quick", TaskQueueFactory::Priority::NORMAL)),
	_quick_cb(cb)
{
	LogWrapper::Load();
	g_quic_log.Open("quic");
	InitQuck();
	_task_queue.PostTask([this]() {
		_paced_thread = ProcessThread::Create("paced_sender");
		_paced_thread->Start();
	});

	_repeating_task = dd::RepeatingTaskHandle::DelayedStart(_task_queue.Get(), 1000, [this] {
		CheckTimeout();
		return 1000;
	});
}

QuickImpl::~QuickImpl()
{
	g_quic_log.Close();
	_paced_thread->Stop();

	_ack_thread_impl->StopThread();
	_ack_thread->join();
}


uint32_t QuickImpl::ConnectToServer(const std::string& ip, uint16_t port)
{
	bool finished = false;
	uint32_t session_id = 0;
	_task_queue.PostTask([this, &session_id, &finished, &ip, port]() {
		AutoLock lock(_quick_lock);
		session_id = CreateSessionId();
		auto c = new QuickConnection(_quick_cb, _paced_thread.get(), _udp_socket.get(), session_id, ip, port, false, _ack_thread_impl.get());
		_connections[session_id] = std::unique_ptr<QuickConnection>(c);
		finished = true;
	});
	while (!finished)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	return session_id;
}

bool QuickImpl::Disconnect(uint32_t session_id)
{
	if (!_task_queue.IsCurrent()) {
		_task_queue.PostTask([this, session_id] {
			Disconnect(session_id);
		});
		return true;
	}

	AutoLock lock(_quick_lock);
	auto iter = _connections.find(session_id);
	if (iter == _connections.end())
	{
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_ERROR, " cannot find session_id %u, send failed", session_id);
		return false;
	}
	iter->second->Disconnect();
	_connections.erase(iter);
	return true;
}

bool QuickImpl::Send(uint32_t session_id, const uint8_t* data, uint16_t data_len)
{
	if (data_len > MAX_QUICK_PACKET_SIZE)
		return false;
	AutoLock lock(_quick_lock);
	auto iter = _connections.find(session_id);
	if (iter == _connections.end())
	{
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_ERROR, " cannot find session_id %u, send failed", session_id);
		return false;
	}
	return iter->second->SendPayload(data, data_len);
}


void QuickImpl::ExportDebugInfo(uint32_t session_id)
{
	AutoLock lock(_quick_lock);
	auto iter = _connections.find(session_id);
	if (iter == _connections.end())
	{
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_ERROR, " cannot find session_id %u, send failed", session_id);
		return ;
	}
	iter->second->ExportDebugInfo();
}

void QuickImpl::OnRecv(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len, uint64_t received_time_miscroseconds)
{
	if (!_task_queue.IsCurrent()) {
		if (data_len == -1) {
			_task_queue.PostTask([this, ip, port]() {
				AutoLock lock(_quick_lock);
				for (auto & c : _connections) {
					if (c.second->dst_ip() == ip && c.second->dst_port() == port) {
						c.second->SetServerState(false);
						break;
					}
				}
			});
			return;
		}
		else {
			// unknown msg
			if (data_len < sizeof(QuickHello))
				return;
			uint8_t msg_id = data[0];
			uint8_t *buf = new uint8_t[data_len];
			memcpy(buf, data, data_len);

			// 对于 payloadk消息，直接发送到paced_sender线程，其它消息则发到quick线程
			if (msg_id == QUICK_MSG_PAYLOAD) {
				AutoLock lock(_quick_lock);
				ProcessPayload(ip, port, buf, data_len);
			}
			else {
				if (msg_id == QUICK_MSG_HELLO)
					int a = 0;
				_task_queue.PostTask([this, ip, port, buf, data_len, received_time_miscroseconds]() {
					OnRecv(ip, port, buf, data_len, received_time_miscroseconds);
				});
			}

			return;
		}
	}

	std::unique_ptr<uint8_t> packet(const_cast<uint8_t*>(data));

	uint8_t msg_id = data[0];
	_received_time_microseconds = received_time_miscroseconds;
	AutoLock lock(_quick_lock);
	switch (msg_id) {
	case QUICK_MSG_HELLO:
		ProcessHello(ip, port, data, data_len);
		break;
	case QUICK_MSG_HELLO_OK:
		ProcessHelloOk(ip, port, data, data_len);
		break;
	case QUICK_MSG_REJECT:
		ProcessReject(ip, port, data, data_len);
		break;
	case QUICK_MSG_SESSION_ID_REAPEAT:
		ProcessSessionIdRepeat(ip, port, data, data_len);
		break;
	case QUICK_MSG_BYE:
		ProcessBye(ip, port, data, data_len);
		break;
	//case QUICK_MSG_PAYLOAD:
	//	packet.release();
	//	ProcessPayload(ip, port, data, data_len);
	//	break;
	case QUICK_MSG_ACK:
		packet.release();
		ProcessAck(ip, port, data, data_len);
		break;
	case QUICK_MSG_HEARTBEAT:
		ProcessHeartbeat(ip, port, data, data_len);
	default:
	{
		// msg unknwon
	}
	}
}

void QuickImpl::InitQuck()
{
	if (!_task_queue.IsCurrent()) {
		_task_queue.PostTask([this] {
			InitQuck();
		});
		return;
	}

	_udp_socket.reset(new UdpSocket(_local_port));
	_packet_reader.reset(new PacketReader(this, _task_queue_factory.get(), _udp_socket.get()));

	_ack_thread_impl.reset(new AckThread(_udp_socket.get()));
	_ack_thread.reset(new std::thread(&AckThread::Process, _ack_thread_impl.get()));
}

uint32_t QuickImpl::CreateSessionId()
{
	auto seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::mt19937 mt(seed);
	uint32_t id;
	while (true) {
		id = mt();
		if(_connections.find(id) == _connections.end())
			break;
	}
	return id;
}

void QuickImpl::ProcessHello(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	if (!_as_server) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "当前并非服务器模式，不接受客户端连接");
		return;
	}

	QuickHello *hello = (QuickHello *)data;
	bool need_notify = true;
	auto iter = _connections.find(hello->session_id);
	if (iter != _connections.end()) {
		if (iter->second->dst_ip() == ip && iter->second->dst_port()) {
			// 重复收到同一个客户端的hello消息，是因为HELLO_OK消息丢失了，那就再次回复ok
			need_notify = false;
		}
		else {
			QuickSessionIdRepeat msg;
			msg.session_id = hello->session_id;
			_udp_socket->Send(ip, port, (uint8_t*)&msg, sizeof(msg));
			g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "session id已存在，拒绝客户端连接");
		}
		return;
	}

	QuickHelloOk msg;
	msg.session_id = hello->session_id;
	_udp_socket->Send(ip, port, (uint8_t*)&msg, sizeof(msg));
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "同意连接, ip = %s, port = %d, session id = %u", ip.c_str(), port, hello->session_id);

	auto c = new QuickConnection(_quick_cb, _paced_thread.get(), _udp_socket.get(), hello->session_id, ip, port, true, _ack_thread_impl.get());
	_connections[hello->session_id] = std::unique_ptr<QuickConnection>(c);
	if (need_notify)
		_quick_cb->OnNewClientConnected(hello->session_id, ip, port);
}

void QuickImpl::ProcessHelloOk(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	QuickHelloOk* msg = (QuickHelloOk*)data;
	auto iter = _connections.find(msg->session_id);
	if (iter == _connections.end()) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "找不到对应的session id %u", msg->session_id);
		return;
	}
	iter->second->SetServerState(true);
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "收到 hello ok消息， 连接已建立");
	_quick_cb->OnConnected(msg->session_id);
}

void QuickImpl::ProcessReject(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "收到服务器拒绝连接消息");
}

void QuickImpl::ProcessSessionIdRepeat(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "收到 session id 重复消息，需要更换session id重新连接");
}

void QuickImpl::ProcessBye(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	QuickBye* bye = (QuickBye*)data;
	auto iter = _connections.find(bye->session_id);
	if (iter == _connections.end()) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "找不到对应的session id %u, 无法处理此 byte 消息", bye->session_id);
		return;
	}

	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "连接已断开，session id = %u, ip = %s, port = %d", bye->session_id, ip.c_str(), port);
	iter->second->SetServerState(false);
	iter->second->SetAllowReconnect(false);
	if (_as_server)
		_connections.erase(iter);
	_quick_cb->OnDisconnected(bye->session_id);
}

void QuickImpl::ProcessPayload(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	QuickPayload *msg = (QuickPayload*)data;
	auto iter = _connections.find(msg->session_id);
	if (iter == _connections.end()) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "找不到对应的session id %u, 无法处理此 payload", msg->session_id);
		return;
	}
	iter->second->OnMessage(ip, port, data, data_len);
}

void QuickImpl::ProcessAck(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	QuickAck *ack = (QuickAck*)data;
	auto iter = _connections.find(ack->session_id);
	if (iter == _connections.end()) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "找不到对应的session id %u, 无法处理此 ack", ack->session_id);
		return;
	}
	iter->second->OnAck(ip, port, data, data_len, _received_time_microseconds);
}

void QuickImpl::ProcessHeartbeat(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	QuickHeartbeat *ack = (QuickHeartbeat*)data;
	auto iter = _connections.find(ack->session_id);
	if (iter == _connections.end()) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "找不到对应的session id %u, 无法处理此 ack", ack->session_id);
		return;
	}
	iter->second->OnHeartbeat();
}

void QuickImpl::CheckTimeout()
{
	// test
	return;

	auto now = dd::GetCurrentTickMicroSecond();
	AutoLock lock(_quick_lock);
	if (!_as_server) {
		for (auto iter = _connections.begin(); iter != _connections.end(); iter++) {
			if(!iter->second->server_state() || now - iter->second->latest_recv_msg_time() < CONNECTION_TIMEOUT_US)
				continue;
			iter->second->SetServerState(false);
			_quick_cb->OnDisconnected(iter->first);
			g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "session %u recv msg timeout, disconnect!", iter->first);
		}
	}
	else {
		for (auto iter = _connections.begin(); iter != _connections.end();) {
			if(!iter->second->server_state() || now - iter->second->latest_recv_msg_time() < CONNECTION_TIMEOUT_US)
				continue;
			iter->second->SetServerState(false);
			_quick_cb->OnDisconnected(iter->first);
			g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "session %u recv msg timeout, disconnect!", iter->first);
			iter->second->Disconnect();
			iter = _connections.erase(iter);
		}
	}
}
