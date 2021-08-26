#include "quick_connection.h"
#include "quick_proto.h"
#include "../thread/utility.h"
#include "../log/LogWrapper.h"
#include <thread>

// 设置允许最大飞行包数
#define MAX_INFLIGHT_PACKET 16000

extern LogWrapper g_quic_log;
using namespace dd;
QuickConnection::QuickConnection(QuickCallback* quick_cb, dd::ProcessThread *thread, UdpSocket* socket, 
	uint32_t session_id, const std::string &dst_ip, uint16_t dst_port, bool connected, AckThread* ack_thread)
	: _quick_cb(quick_cb), _thread(thread), _socket(socket), _session_id(session_id), _dst_ip(dst_ip), _dst_port(dst_port), _server_state(connected)
	,_ack_thread(ack_thread)
{
	if (_server_state)
		_need_reconnect = false;
	_ack_thread->AddSession(_session_id, _dst_ip, _dst_port);
	_send_buf.reset(new uint8_t[SEND_BUF_LEN]);
	if (!_server_state)
		ConnectToServer();
	_wait_send_packets.reset(new RingBuffer<QuickPacket>(1000));
	_clock.reset(new quic::QuicClockImpl());
	_connection_stats.reset(new quic::QuicConnectionStats);
	_rtt_stats.reset(new quic::RttStats);
	_unacked_packet_map.reset(new quic::QuicUnackedPacketMap);
	_send_algorithm.reset(quic::SendAlgorithmInterface::Create(_clock.get(),
		_rtt_stats.get(),
		_unacked_packet_map.get(),
		quic::kBBRv2,
		_connection_stats.get(),
		32));

	_pacing_sender.reset(new quic::PacingSender);
	_pacing_sender->set_sender(_send_algorithm.get());

	_thread->RegisterModule(this, dd::RTC_FROM_HERE);
	_thread->WakeUp(this);

	PostTask([this] {
		_repeating_task = dd::RepeatingTaskHandle::DelayedStart(_thread->Current(), _repeating_task_interval_ms, [this] {
			if(!_server_state && _need_reconnect)
				TryReconnectServer();
			TrySendHeartbeat();
			MaybeWakeupProcess();
			PickTimeoutUnackedPackets();
			if (_clock->Now().ToDebuggingValue() - _print_acked_bytes_us >= 1000000) {
				_print_acked_bytes_us = _clock->Now().ToDebuggingValue();
				char buf[256] = { 0 };
				float speed = _total_acked_bytes - _prior_total_acked_bytes;
				speed = speed / 1024.0 / 1024.0;
#ifdef OUTPUT_DEBUG_INFO
				dd::OutputDebugInfo("%llu acked: %f Mbyte/s, acked_rate = %llu\r\n", _print_acked_bytes_us, speed, _acked_rate);
#endif
				_prior_total_acked_bytes = _total_acked_bytes;
			}
			return _repeating_task_interval_ms;
		});
	});
}

QuickConnection::~QuickConnection()
{
	Stop();
}

void QuickConnection::OnMessage(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len)
{
	if (!_thread->IsCurrent()) {
		PostTask([this, ip, port, data, data_len] {
			OnMessage(ip, port, data, data_len);
		});
		return;
	}
	std::unique_ptr<uint8_t> packet(const_cast<uint8_t*>(data));

	QuickPayload *msg = (QuickPayload*)data;
	int16_t payload_len = data_len - sizeof(QuickPayload) + 1;
	if (payload_len <= 0) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "process payload failed, payload_len = %d", payload_len);
		return;
	}

	_latest_recv_msg_time = _clock->Now().ToDebuggingValue();

	// udp包乱序的情况下，会出现 send_timestamp比_latest_received_timestamp小的情况，对于乱序的包直接进行丢弃，连ack都不要回复。
	// 发送端在发包时，send_timestamp是严格单调递增的，丢弃的包由发送包进行重传即可。
	// 一般情况下，只有网络路由发生变化的情况下，udp包才可能会出现乱序的情况，但在主机和虚拟机进行数据收发测试时，
	// 如果进行网速上限调整，也是有可能出现乱序的
	if (msg->send_timestamp < _latest_received_timestamp)
		return;
	_latest_received_timestamp = msg->send_timestamp;

#ifdef OUTPUT_VERBOSE_INFO
	dd::OutputDebugInfo("recv packet, packet number = %u, sequence number = %u, len = %d, now = %llu\r\n", msg->packet_number, msg->sequence_number,
			payload_len, _clock->Now().ToDebuggingValue());
#endif

	uint64_t timestamp = 0;
	memcpy(&timestamp, msg->payload, sizeof(&time));
	PacketInfo info(msg->sequence_number, payload_len, timestamp, _expect_recv_sequence_number, msg->packet_number);
	//_real_recv_packets_len[timestamp] = info;
#ifdef OUTPUT_DEBUG_INFO
		_real_recv_packets_len.push_back(info);
#endif

	_ack_thread->Append(_session_id, msg->packet_number, _clock->Now().ToDebuggingValue());

	// 只有比当前期望接收的序号更新的包才放入到缓冲。
	// 重传的情况下，sequence_number可能会比 _expect_recv_sequence_number更旧
	if (quic::IsNewer(msg->sequence_number, _expect_recv_sequence_number) || msg->sequence_number == _expect_recv_sequence_number) {
		if(_received_packets[msg->sequence_number].get())
			assert(_received_packets[msg->sequence_number].get()->data_len == payload_len);
		_received_packets[msg->sequence_number].reset(new ReceivedPacket(msg->sequence_number, msg->payload, payload_len));

	}

	DeliverReceivedPackets(msg->sequence_number);
}

void QuickConnection::OnAck(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len, uint64_t received_time_microseconds)
{
	if (!_thread->IsCurrent()) {



		PostTask([this, ip, port, data, data_len, received_time_microseconds] {
			OnAck(ip, port, data, data_len, received_time_microseconds);
		});
		return;
	}
	std::unique_ptr<uint8_t> packet(const_cast<uint8_t*>(data));

	_latest_recv_msg_time = received_time_microseconds;

	QuickAck *ack = (QuickAck*)data;
	_packets_acked.clear();
	_packets_lost.clear();
	quic::QuicTime now(received_time_microseconds);

#ifdef OUTPUT_VERBOSE_INFO
	dd::OutputDebugInfo("received ack delta = %u\r\n", received_time_microseconds - _prior_receive_ack_time);
	_prior_receive_ack_time = received_time_microseconds;
#endif

	uint16_t newest_packet_number = NewestPacketNumber(ack->base_pkt_number, ack->pkt_number_mask);
	auto delta = quic::QuicTime::Delta::FromMicroseconds(ack->max_pkt_number_delay_us);
	_rtt_updated = MaybeUpdateRtt(newest_packet_number, delta, now);

	//CalculateAckRate(newest_packet_number);

	// 将所有包号小于此ack消息中最小包号的包都认为丢失了
	//_unacked_packet_map->PickLostUnackedPackets(_wait_retransmission_packets, ack->previous_ack_base_pkt_number);

	auto prior_bytes_in_fligh = _unacked_packet_map->bytes_in_flight();
	//if (ack->previous_ack_base_pkt_number != ack->base_pkt_number)
	//	OnAck(now, ack->previous_ack_base_pkt_number, ack->previous_ack_pkt_number_mask);
	OnAck(now, ack->base_pkt_number, ack->pkt_number_mask);

	// test lww
	// 这里计算 _packets_lost还是有问题的，_packet_lost应该等于新丢失的包，可以通过 	_unacked_packet_map->PickLostPackets 来获取
	// 两个相邻ack包之间缺失的包就认为是丢失的包, 例如：
	// 1、2、5、6，就认为包3、4是丢失的
	for (int i = 1; i < _packets_acked.size(); i++) {
		auto distance = _packets_acked[i].packet_number - _packets_acked[i - 1].packet_number;
		if (distance > 1) {
			for (int k = 1; k < distance; k++) {
				uint16_t packet_number = _packets_acked[i - 1].packet_number.ToUint64() + k;
				quic::QuicPacketNumber packet_lost(packet_number);
				uint16_t bytes_lost = _unacked_packet_map->bytes_acked(packet_number);
				if(bytes_lost == 0)
					continue;;
				int32_t sequence_number = _unacked_packet_map->sequence_number(packet_number);
				//_wait_retransmission_packets[sequence_number] = sequence_number;
				_packets_lost.push_back(quic::LostPacket(packet_lost, bytes_lost));
			//	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "OnAck, packet number = %d, sequence number = %d need to retransmission",
				//	packet_number, sequence_number);
				//_unacked_packet_map->RemovePacket(packet_number);
			}
		}
	}


	if (_packets_acked.size()) {
		_latest_received_ack_time = received_time_microseconds;
		//uint16_t latest_packet_number = _packets_acked[_packets_acked.size() - 1].packet_number.ToUint64();

		//只有收到有效的确认消息才进行丢包检测
		_unacked_packet_map->PickLostPackets(_wait_retransmission_packets, newest_packet_number + 1);
	}

	MaybeInvokeCongestionEvent(_rtt_updated, prior_bytes_in_fligh, now);
	//_thread->WakeUp(this);
}

void QuickConnection::OnAck(quic::QuicTime now, uint16_t base_packet_number, uint16_t packet_number_mask)
{
	quic::QuicPacketNumber packet_number(base_packet_number);
	uint16_t acked_len = _unacked_packet_map->bytes_acked(base_packet_number);
	int32_t sequence_number = _unacked_packet_map->sequence_number(base_packet_number);
		static char buf[256] = { 0 };
	if (acked_len && _unacked_packet_map->IsAcked(base_packet_number) == false) {
		_packets_acked.push_back(quic::AckedPacket(packet_number, acked_len, now));
		_unacked_packet_map->OnPacketAcked(base_packet_number, now);
		//_total_acked_bytes += acked_len;


		uint16_t payload_len = acked_len - sizeof(QuickPayload) + 1;
		_total_acked_bytes += payload_len;
#ifdef OUTPUT_DEBUG_INFO
			_acked_packets_len.push_back(payload_len);
			dd::OutputDebugInfo("recv ack for packet, packet number = %d, sequence number = %d\r\n",
				base_packet_number, sequence_number);
#endif
	}
	else {
	}


	for (int i = 0; i < MAX_ACK_PACKET_COUNT; i++) {
		if (packet_number_mask & (1 << i)) {
			uint16_t pn = base_packet_number + i + 1;
			quic::QuicPacketNumber packet_number(pn);
			uint16_t acked_len = _unacked_packet_map->bytes_acked(pn);
			if (acked_len && !_unacked_packet_map->IsAcked(pn)) {
				_packets_acked.push_back(quic::AckedPacket(packet_number, acked_len, now));
				sequence_number = _unacked_packet_map->sequence_number(pn);
				_unacked_packet_map->OnPacketAcked(pn, now);

				uint16_t payload_len = acked_len - sizeof(QuickPayload) + 1;
				_total_acked_bytes += payload_len;
#ifdef OUTPUT_VERBOSE_INFO
					_acked_packets_len.push_back(payload_len);
					dd::OutputDebugInfo("recv ack for packet, packet number = %d, sequence number = %d\r\n", pn, sequence_number);
#endif
			}
			else {
			}
		}
	}
}

void QuickConnection::OnHeartbeat()
{
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "session %u recv heartbeat", _session_id);
	_latest_recv_msg_time = _clock->Now().ToDebuggingValue();
}

uint16_t QuickConnection::NewestPacketNumber(uint16_t base_packet_number, uint16_t packet_number_mask)
{
	uint16_t number = base_packet_number;
	for (int i = 0; i < MAX_ACK_PACKET_COUNT; i++) {
		if (packet_number_mask & (1 << i)) {
			number = base_packet_number + i + 1;
		}
	}
	return number;
}

void QuickConnection::DeliverReceivedPackets(uint16_t sequence_number)
{
	// 当前接收到的包并非期望的包就不进行处理
	if (sequence_number != _expect_recv_sequence_number)
		return;
	char buf[256] = { 0 };
	uint64_t timestamp = 0;
	while (true) {
		if(_received_packets[_expect_recv_sequence_number] == nullptr)
			break;
		auto packet = _received_packets[_expect_recv_sequence_number].get();
		_quick_cb->OnMessage(_session_id, packet->data.get(), packet->data_len);

		memcpy(&timestamp, packet->data.get(), sizeof(timestamp));
#ifdef OUTPUT_DEBUG_INFO
			_recv_packets_len.push_back(PacketInfo(_expect_recv_sequence_number, packet->data_len, timestamp));
#endif
		_total_recv_len += packet->data_len;

#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("deliver packet, sequence number = %d, data_len = %d\r\n", _expect_recv_sequence_number, packet->data_len);
#endif

		auto now = dd::GetCurrentTick();
		_recv_speed_bytes += packet->data_len;
			if (now - _print_time_ms >= 1000) {
				_print_time_ms = now;

				float speed = (float)_recv_speed_bytes / 1024.0 / 1024.0;
#ifdef OUTPUT_DEBUG_INFO
				dd::OutputDebugInfo("deliver received packet speed: %f Mbyte/s\r\n", speed);
#endif
				g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "deliver received packet speed: %f Mbyte/s", speed);
				_recv_speed_bytes = 0;
			}

		_received_packets[_expect_recv_sequence_number++].reset();
	}
}

bool QuickConnection::RetransmissionPacket(uint16_t sequence_number)
{
	char buf[256];
	QuickPacket *packet = _unacked_packet_map->packet(sequence_number);
	if (packet == nullptr) {
		return false;
	}
#ifdef OUTPUT_DEBUG_INFO
	dd::OutputDebugInfo("retransmission packet , sequence number = %d\r\n", sequence_number);
#endif
	//g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "retransmission packet, sequence_number = %d", sequence_number);
	SendPayload(packet);
	return true;
}

void QuickConnection::PickTimeoutUnackedPackets()
{
	_unacked_packet_map->PickTimeoutUnackedPackets(_wait_retransmission_packets, _retransmission_packet_timeout_us, _clock->Now().ToDebuggingValue());
}

void QuickConnection::CalculateAckRate(uint16_t packet_number)
{
	//uint64_t now = _clock->Now().ToDebuggingValue();
	//SentState * sent_state = _sent_states[packet_number].get();
	//if (sent_state == nullptr)
	//	return;
	//_acked_rate = (_total_acked_bytes - sent_state->total_acked_bytes) * 8 * 1000 * 1000 / (now - sent_state->sent_time);
	//_acked_rate = _acked_rate / 8 / 1024 /1024;
}

bool QuickConnection::CanSend()
{
	uint16_t least_unack_packet_number = _unacked_packet_map->GetLeastUnackedPacketNumber();
	// 防止packet number 夸度过大
	if (_packet_number - least_unack_packet_number >= 32766)
		return false;
	// 防止覆盖还没有ack的包
	if (_unacked_packet_map->sequence_number(_packet_number) != -1)
		return false;
	return true;
}

void QuickConnection::Stop()
{
	_ack_thread->RemoveSession(_session_id);
	_thread->DeRegisterModule(this);
	if (_repeating_task.Running())
		_repeating_task.Stop();
}

void QuickConnection::TryReconnectServer()
{
	auto now = _clock->Now().ToDebuggingValue();
	if (now - _reconnect_time < RECONNECT_INTERVAL_US)
		return;
	_reconnect_time = now;
	ConnectToServer();
}

void QuickConnection::TrySendHeartbeat()
{
	auto now = _clock->Now().ToDebuggingValue();
	if (now - _latest_send_msg_time < HEARTBEAT_INTERVAL_US)
		return;
	_latest_send_msg_time = now;
	QuickHeartbeat msg;
	msg.session_id = _session_id;
	Send(&msg, sizeof(msg));
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "session %u send heartbeat", _session_id);
}

bool QuickConnection::SendPayload(const void* data, uint16_t data_len)
{
	// 这里调用 GetLeastUnackedSequenceNumber应该是要加锁的，但加不加锁得到的sequnce_number对判断不会有不得影响，
	// 为了提高效率，就没必要加锁了
	uint16_t least_unack_sequence_number = _unacked_packet_map->GetLeastUnackedSequenceNumber();
	if (quic::IsNewer(least_unack_sequence_number, _sequence_number) && least_unack_sequence_number != _sequence_number) {
		return false;
	}
	// 如果_sequence_number已经发送且在等待ack，那么就不允许再把相同sequence number的包进入到缓冲队列。
	// 否则会出现相同的sequence number但对应不同的包。原因在于uint16_t在高速发送的情况下，很短时间内就会发生环回
	if (_unacked_packet_map->packet(_sequence_number) != nullptr)
		return false;

	auto p = _wait_send_packets->GetWriteablePacket();
	if (p == nullptr)
		return false;
	if (data_len > 1350)
		return false;
	QuickPayload *payload = (QuickPayload*)_send_buf.get();
	uint16_t pkt_len = sizeof(QuickPayload) - 1 + data_len;
	payload->msg_id = QUICK_MSG_PAYLOAD;
	payload->session_id = _session_id;
	payload->sequence_number = _sequence_number;
	memcpy(payload->payload, data, data_len);
	p->WriteData(_sequence_number++, (uint8_t*)payload, pkt_len, _clock->Now().ToDebuggingValue());
	_wait_send_packets->WritePacket();
	// 这里不需要调用Wakup，否则会拖慢发送速度
//	_thread->WakeUp(this);

	
	return true;
}

void QuickConnection::SendPayload(QuickPacket* packet)
{
	quic::QuicTime now = _clock->Now();
	QuickPayload* p = (QuickPayload*)packet->data();
	p->packet_number = _packet_number;
	p->send_timestamp = now.ToDebuggingValue();
	Send(packet->data(), packet->data_len());

	int payload_len = packet->data_len() - sizeof(QuickPayload) + 1;

	uint64_t timestamp = 0;
	memcpy(&timestamp, p->payload, sizeof(&timestamp));
	PacketInfo info(p->sequence_number, payload_len, timestamp);
#ifdef OUTPUT_VERBOSE_INFO
		_real_send_packets_len.push_back(info);
#endif

#ifdef OUTPUT_VERBOSE_INFO
	dd::OutputDebugInfo("SendPayload , packet number = %d, sequence number = %d, data_len = %d\r\n", p->packet_number, p->sequence_number, payload_len);
#endif
	//_prior_sent_time = now.ToDebuggingValue();

	quic::QuicPacketNumber packet_number(_packet_number);
	_pacing_sender->OnPacketSent(now,
		_unacked_packet_map->bytes_in_flight(),
		packet_number,
		packet->data_len(),
		quic::HAS_RETRANSMITTABLE_DATA);

	_unacked_packet_map->AddSentPacket(_packet_number, now, packet);
	_packet_number++;
}

int64_t QuickConnection::TimeUntilNextProcess()
{
	auto t = _pacing_sender->TimeUntilSend(_clock->Now(), _unacked_packet_map->bytes_in_flight());
	if (_wait_send_packets->GetPacketCount() == 0 && _wait_retransmission_packets.size() == 0)
		t = quic::QuicTime::Delta::FromMilliseconds(1);
	if (t.IsInfinite())
	{
		_wait_process_infinite = true;
		t = quic::QuicTime::Delta::FromMilliseconds(1);
	}
	else
		_wait_process_infinite = false;
	if (t.ToSeconds() > 5 && t.IsInfinite() == false)
		int a = 0;
	// test
	//return 0;
	return t.ToMilliseconds();
}

void QuickConnection::Process()
{
	if (!CanSend())
		return;
	// 先发送需要重传的包
	for (auto iter = _wait_retransmission_packets.begin(); iter != _wait_retransmission_packets.end();) {
		uint16_t sequence_number = *iter;
		iter = _wait_retransmission_packets.erase(iter);
		bool ret = RetransmissionPacket(sequence_number);
		if (ret)
			return;
	}

	if (_unacked_packet_map->packets_in_flight() > MAX_INFLIGHT_PACKET) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "total inflight packets %u, stop send new packet", _unacked_packet_map->packets_in_flight());
		return;
	}
	auto p = _wait_send_packets->GetReadablepacket();
	if (p == nullptr)
		return;
	if (_unacked_packet_map->HasPacket(_packet_number)) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "unacked buffer has packet number %d, stop send...", _packet_number);
		return;
	}

	// test
	//uint64_t now = _clock->Now().ToDebuggingValue();
	//QuickPayload* packet = (QuickPayload*)p->data();
	//memcpy(packet->payload, &now, sizeof(now));

	SendPayload(p);

	uint16_t payload_len = p->data_len() - sizeof(QuickPayload) + 1;
	_total_send_len += payload_len;
#ifdef OUTPUT_DEBUG_INFO
		_send_packets_len.push_back(PacketInfo(p->sequence_number(), payload_len, now));
#endif
	_wait_send_packets->RemovePacket();
}


void QuickConnection::ExportDebugInfo()
{
	dd::OutputDebugInfo("send packet info: \r\n");
	for (auto & a : _send_packets_len) {
		dd::OutputDebugInfo("%d %d, %llu\r\n", a.sequence_number, a.data_len, a.timestamp);
	}

	dd::OutputDebugInfo("real send packet info:\r\n");
	for (auto & a : _real_send_packets_len) {
		dd::OutputDebugInfo("%d %d, %llu\r\n", a.sequence_number, a.data_len, a.timestamp);
	}

	dd::OutputDebugInfo("received packet info:\r\n");
	for (auto & a : _recv_packets_len) {
		dd::OutputDebugInfo("%d %d, %llu\r\n", a.sequence_number, a.data_len, a.timestamp);
	}

	dd::OutputDebugInfo("real recv packet info:\r\n");
	for (auto & a : _real_recv_packets_len) {
		dd::OutputDebugInfo("%d %d, %llu, %d, %d\r\n", a.sequence_number, a.data_len, a.timestamp, a.expect_sequence_number, a.packet_number);
	}
}

void QuickConnection::Disconnect()
{
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "session %u send bye to server", _session_id);
	QuickBye msg;
	msg.session_id = _session_id;
	Send(&msg, sizeof(msg));

	SetServerState(false);
	bool finished = false;
	PostTask([this, &finished] {
		_ack_thread->RemoveSession(_session_id);
		if (_repeating_task.Running())
			_repeating_task.Stop();
		_need_reconnect = false;
		finished = true;
	});
	while (!finished)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

bool QuickConnection::Send(const void* data, uint16_t data_len)
{
	_latest_send_msg_time = _clock->Now().ToDebuggingValue();
	return _socket->Send(_dst_ip, _dst_port, (const uint8_t*)data, data_len);
}

void QuickConnection::ConnectToServer()
{
	if (_clock == nullptr)
		return;
	_reconnect_time = _clock->Now().ToDebuggingValue();
	QuickHello msg;
	msg.session_id = _session_id;
	Send(&msg, sizeof(msg));
}

void QuickConnection::MaybeInvokeCongestionEvent(bool rtt_updated, quic::QuicByteCount prior_in_flight, quic::QuicTime event_time)
{
	if (!rtt_updated && _packets_acked.empty() && _packets_lost.empty())
		return;
	_pacing_sender->OnCongestionEvent(rtt_updated,
		prior_in_flight,
		event_time,
		_packets_acked,
		_packets_lost);
	_packets_lost.clear();
	_packets_acked.clear();
}

bool QuickConnection::MaybeUpdateRtt(uint16_t largest_acked, quic::QuicTime::Delta ack_delay_time, quic::QuicTime ack_receive_time)
{
	if (!_unacked_packet_map->IsUnacked(largest_acked))
		return false;

	auto  sent_time = _unacked_packet_map->sent_time(largest_acked);
	quic::QuicTime::Delta send_delta = ack_receive_time - quic::QuicTime(sent_time);
	const bool min_rtt_available = !_rtt_stats->min_rtt().IsZero();
	_rtt_stats->UpdateRtt(send_delta, ack_delay_time, ack_receive_time);
	return true;
}

void QuickConnection::MaybeWakeupProcess()
{
	if (_wait_process_infinite && _unacked_packet_map->bytes_in_flight() < _send_algorithm->GetCongestionWindow()) {
		_wait_process_infinite = false;
		// 唤醒因为飞行数据超过拥塞窗口而进入无限等待的状态
		_thread->WakeUp(this);
	}
}
