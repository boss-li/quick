#pragma once
#include "udp_socket.h"
#include "../thread/utility.h"
#include "received_packet.h"
#include "quick_proto.h"
#include <list>
#include <atomic>
#include <unordered_map>

// 为了能够以相对平滑的时间间隔发出ack消息，在单独的线程内进行ack消息发送
class AckThread {
public:
	explicit AckThread(UdpSocket* socket);
	void AddSession(uint32_t session_id, std::string &dst_ip, uint16_t dst_port);
	void RemoveSession(uint32_t session_id);

	void Process();
	void StopThread();
	void Append(uint32_t session_id, uint16_t packet_number, uint64_t received_time);

private:
#define MAX_ACK_PACKET_COUNT 16
	struct WaitAckSession {
		WaitAckSession() {}
		WaitAckSession(uint32_t session_id, const std::string& dst_ip, uint16_t dst_port) {
			this->session_id = session_id;
			this->dst_ip = dst_ip;
			this->dst_port = dst_port;
		}
		uint32_t session_id = 0;
		std::string dst_ip;
		uint16_t dst_port = 0;
		std::list<WaitAckPacket> wait_ack_packets;
		int32_t previous_ack_base_packet_number = -1;
		int32_t previous_ack_packet_number_mask = -1;
	};

	bool Send(const std::string &ip, uint16_t port, const void* data, uint16_t data_len);
	void TrySendAck(WaitAckSession* session);
	void SendAck(WaitAckSession* session, QuickAck& ack);
	void WriteMask(uint16_t base_pkt_number, uint16_t pkt_number, uint16_t &mask);

	std::atomic<bool> _running;
	UdpSocket *_socket;
	dd::Lock _lock;
	std::unordered_map<uint32_t, WaitAckSession> _wait_ack_sessions;
	const uint16_t _max_ack_interval_microseconds = 4000;	// 设置最大ack间隔为2ms
	
};
