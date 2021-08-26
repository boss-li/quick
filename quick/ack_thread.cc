#include "ack_thread.h"
#include <chrono>
#include <thread>
#include <assert.h>

AckThread::AckThread(UdpSocket* socket)
	: _socket(socket)
{
	_running = true;
}

void AckThread::AddSession(uint32_t session_id, std::string &dst_ip, uint16_t dst_port)
{
	WaitAckSession w(session_id, dst_ip, dst_port);
	dd::AutoLock lock(_lock);
	_wait_ack_sessions[session_id] = w;
}

void AckThread::RemoveSession(uint32_t session_id)
{
	dd::AutoLock lock(_lock);
	auto iter = _wait_ack_sessions.find(session_id);
	if (iter != _wait_ack_sessions.end())
		_wait_ack_sessions.erase(iter);
}

#ifdef _WIN32
void SleepMicroSecondds() {
	HANDLE hTimer = nullptr;
	LARGE_INTEGER liDurTime;
	liDurTime.QuadPart = -2000;
	hTimer = CreateWaitableTimerA(nullptr, TRUE, "WaitableTimer");
	if (hTimer == nullptr)
		return;
	if (!SetWaitableTimer(hTimer, &liDurTime, 0, nullptr, nullptr, 0))
		return;
	if (WaitForSingleObject(hTimer, INFINITE) != WAIT_OBJECT_0)
		return;
	return;
}
#else
void SleepMicroSecondds() {
	usleep(2000);
}
#endif

void AckThread::Process()
{
#ifdef _WIN32
	::timeBeginPeriod(1);
#endif
	while (_running) {
		SleepMicroSecondds();
	//	std::this_thread::sleep_for(std::chrono::milliseconds(1));
		{
			dd::AutoLock lock(_lock);
			for (auto iter = _wait_ack_sessions.begin(); iter != _wait_ack_sessions.end(); iter++){
				auto aa = iter->first;
				TrySendAck(&iter->second);
			}
		}
	}
#ifdef _WIN32
	::timeEndPeriod(1);
#endif
}

void AckThread::StopThread()
{
	_running = false;
}

void AckThread::Append(uint32_t session_id, uint16_t packet_number, uint64_t received_time)
{
	dd::AutoLock lock(_lock);
	auto iter = _wait_ack_sessions.find(session_id);
	if (iter != _wait_ack_sessions.end()) {
		iter->second.wait_ack_packets.push_back(WaitAckPacket(packet_number, received_time));
	}
}

void AckThread::SendAck(WaitAckSession* session, QuickAck& ack)
{
	if (session->previous_ack_base_packet_number == -1) {
		session->previous_ack_base_packet_number = ack.base_pkt_number;
		session->previous_ack_packet_number_mask = ack.pkt_number_mask;
	}
	ack.previous_ack_base_pkt_number = session->previous_ack_base_packet_number;
	ack.previous_ack_pkt_number_mask = session->previous_ack_packet_number_mask;
	Send(session->dst_ip, session->dst_port, &ack, sizeof(ack));
	session->previous_ack_base_packet_number = ack.base_pkt_number;
	session->previous_ack_packet_number_mask = ack.pkt_number_mask;
}

void AckThread::WriteMask(uint16_t base_pkt_number, uint16_t pkt_number, uint16_t &mask)
{
	assert(pkt_number > base_pkt_number);
	int offset = pkt_number - base_pkt_number;
	mask = (1 << (offset - 1)) | mask;
}

bool AckThread::Send(const std::string &ip, uint16_t port, const void* data, uint16_t data_len)
{
	return _socket->Send(ip, port, (const uint8_t*)data, data_len);
}

void AckThread::TrySendAck(WaitAckSession* session)
{
	if (session->wait_ack_packets.size() == 0)
		return;
	QuickAck ack;
	uint32_t base_pkt_number = -1;
	uint64_t now;
	uint16_t packet_number = 0;
	uint64_t received_time = 0;
	uint64_t max_packet_number_received_time;

	ack.session_id = session->session_id;
	base_pkt_number = -1;
	max_packet_number_received_time = 0;
	now = dd::GetCurrentTickMicroSecond();
	packet_number = 0;
	received_time = 0;

	for (auto iter = session->wait_ack_packets.begin(); iter != session->wait_ack_packets.end();) {
		if (now - iter->received_time < _max_ack_interval_microseconds)
			break;
		packet_number = iter->packet_number;
		received_time = iter->received_time;
		iter = session->wait_ack_packets.erase(iter);

		if (base_pkt_number == -1) {
			base_pkt_number = packet_number;
			ack.base_pkt_number = base_pkt_number;
			max_packet_number_received_time = received_time;

#ifdef OUTPUT_VERBOSE_INFO
			dd::OutputDebugInfo("send ack, packet number = %d, now = %llu, recv-ack delta = %d\r\n", packet_number, now, now - received_time);
#endif
			continue;
		}
		if (packet_number - base_pkt_number > MAX_ACK_PACKET_COUNT) {
			ack.max_pkt_number_delay_us = now - max_packet_number_received_time;
			SendAck(session, ack);

			base_pkt_number = packet_number;
			ack.base_pkt_number = base_pkt_number;
			ack.pkt_number_mask = 0;
			max_packet_number_received_time = received_time;
		}
		else {
			max_packet_number_received_time = received_time;
			WriteMask(base_pkt_number, packet_number, ack.pkt_number_mask);
#ifdef OUTPUT_VERBOSE_INFO
			dd::OutputDebugInfo("send ack, packet number = %d, now = %llu, recv-ack delta = %d\r\n", packet_number, now, now - received_time);
#endif
		}
	}
	if (base_pkt_number != -1) {
		ack.max_pkt_number_delay_us = now - max_packet_number_received_time;
		SendAck(session, ack);
	}
}
