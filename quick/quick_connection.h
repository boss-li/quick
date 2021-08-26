#pragma once
#include <string>
#include <memory>
#include "udp_socket.h"
#include "../congestion_control/send_algorithm_interface.h"
#include "../congestion_control/pacing_sender.h"
#include "../congestion_control/rtt_stats.h"
#include "../congestion_control/quic_connection_stats.h"
#include "../congestion_control/quic_clock_impl.h"
#include "../congestion_control/quic_unacked_packet_map.h"
#include "../thread/process_thread.h"
#include "../thread/module.h"
#include "../thread/to_queued_task.h"
#include "../thread/repeating_task.h"
#include "ring_buffer.h"
#include "quick_packet.h"
#include "received_packet.h"
#include "quick_proto.h"
#include "quick.h"
#include <list>
#include <unordered_map>
#include "ack_thread.h"

class QuickConnection : public dd::Module
{
public:
	QuickConnection(QuickCallback* quick_cb, dd::ProcessThread *thread, UdpSocket* socket, uint32_t session_id, 
		const std::string &dst_ip, uint16_t dst_port, bool connected, AckThread *ack_thread);
	~QuickConnection();

	// 在调用者线程内执行
	bool SendPayload(const void* data, uint16_t data_len);

	void OnMessage(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void OnAck(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len, uint64_t received_time_microseconds);
	void OnHeartbeat();
	std::string& dst_ip() { return _dst_ip; }
	uint16_t  dst_port() { return _dst_port; }
	bool server_state() { return _server_state; }
	void SetServerState(bool state) { _server_state = state; }
	void SetAllowReconnect(bool allow) { _need_reconnect = allow; }

	// implementation Module
	int64_t TimeUntilNextProcess() override;
	void Process() override;

	void ExportDebugInfo();
	void Disconnect();
	uint64_t latest_recv_msg_time()const { return _latest_recv_msg_time; }

private:
	bool Send(const void* data, uint16_t data_len);
	void ConnectToServer();
	void MaybeInvokeCongestionEvent(bool rtt_updated, quic::QuicByteCount prior_in_flight, quic::QuicTime event_time);
	bool MaybeUpdateRtt(uint16_t largest_number, quic::QuicTime::Delta ack_delay_time, quic::QuicTime ack_receive_time);
	void MaybeWakeupProcess();
	void SendPayload(QuickPacket* packet);
	void OnAck(quic::QuicTime now, uint16_t base_packet_number, uint16_t packet_number_mask);
	uint16_t NewestPacketNumber(uint16_t base_packet_number, uint16_t packet_number_mask);
	void DeliverReceivedPackets(uint16_t sequence_number);
	bool RetransmissionPacket(uint16_t sequence_number);
	void PickTimeoutUnackedPackets();
	void CalculateAckRate(uint16_t packet_number);
	bool CanSend();
	void Stop();
	void TryReconnectServer();
	void TrySendHeartbeat();

	template<class Closure,
		typename std::enable_if<!std::is_convertible<
		Closure,
		std::unique_ptr<dd::QueuedTask>>::value>::type* = nullptr>
		void PostTask(Closure && closure) {
		_thread->PostTask(dd::ToQueuedTask(std::forward<Closure>(closure)));
	}

	template<class Closure,
		typename std::enable_if<!std::is_convertible<
		Closure,
		std::unique_ptr<dd::QueuedTask>>::value>::type* = nullptr>
		void PostDelayedTask(Closure&& closure, uint32_t milliseconds) {
		_thread->PostDelayedTask(dd::ToQueuedTask(std::forward<Closure>(closure)), milliseconds);
	}
private:
#define SEND_BUF_LEN 2048
#define MAX_RECV_PACKET_COUNT 0xFFFF + 1
#define INIT_RTT_MS 200
// 重连服务器间隔3秒
#define RECONNECT_INTERVAL_US 3000000
// 发送心跳间隔5秒
#define HEARTBEAT_INTERVAL_US 5000000


	uint32_t _session_id;
	std::string _dst_ip;
	uint16_t _dst_port;
	UdpSocket *_socket;
	dd::ProcessThread *_thread;
	dd::RepeatingTaskHandle _repeating_task;
	AckThread* _ack_thread;
	int _repeating_task_interval_ms = 20;
	bool _server_state = false;
	std::unique_ptr<uint8_t> _send_buf;
	std::unique_ptr<quic::SendAlgorithmInterface> _send_algorithm;
	std::unique_ptr<quic::PacingSender> _pacing_sender;
	std::unique_ptr<quic::QuicClock> _clock;
	std::unique_ptr<quic::RttStats> _rtt_stats;
	std::unique_ptr<quic::QuicConnectionStats> _connection_stats;
	std::unique_ptr<RingBuffer<QuickPacket>> _wait_send_packets;
	std::unique_ptr<quic::QuicUnackedPacketMap> _unacked_packet_map;
	// 上层应用发包的序列号，严格对应上层发包的顺序
	uint16_t _sequence_number = 0;
	// 当前发送的包号，_packet_number不一定等于_sequence_number.
	// 当一个包发生重传时， sequence_number不会变化，但packet_number会不同， packet_number在每发一个包后都会 + 1
	uint16_t _packet_number = 0;		
	quic::AckedPacketVector _packets_acked;
	quic::LostPacketVector _packets_lost;
	bool _rtt_updated = false;

	uint16_t _expect_recv_sequence_number = 0;// 期望接收的包序号
	std::unique_ptr<ReceivedPacket> _received_packets[MAX_RECV_PACKET_COUNT];
	
	QuickCallback *_quick_cb;
	uint64_t _retransmission_packet_timeout_us = INIT_RTT_MS * 5 * 1000;
	std::list<uint16_t> _wait_retransmission_packets;
	bool _wait_process_infinite = false;

	uint64_t _total_acked_bytes = 0;
	uint64_t _prior_total_acked_bytes = 0;
	uint64_t _print_acked_bytes_us = 0;
	uint64_t _recv_speed_bytes = 0;
	uint64_t _print_time_ms = 0;

	uint64_t _latest_received_ack_time = 0;
	struct SentState {
		SentState(uint64_t sent_time, uint64_t total_acked_bytes) {
			this->sent_time = sent_time;
			this->total_acked_bytes = total_acked_bytes;
		}
		uint64_t sent_time = 0;
		uint64_t total_acked_bytes = 0;
	};
	uint64_t _acked_rate = 0;
	uint64_t _prior_receive_ack_time = 0;
	uint64_t _prior_sent_time = 0;

	uint64_t _latest_received_timestamp;
	bool _need_reconnect = true;
	uint64_t _reconnect_time = 0;
	uint64_t _latest_send_msg_time = 0;
	uint64_t _latest_recv_msg_time = 0;

	// test
	struct PacketInfo {
		uint16_t sequence_number = 0;
		uint16_t data_len = 0;
		uint64_t timestamp = 0;
		uint16_t expect_sequence_number = 0;
		uint16_t packet_number = 0;
		PacketInfo() {}
		PacketInfo(uint16_t sequence_number, uint16_t data_len, uint64_t timestamp) {
			this->sequence_number = sequence_number;
			this->data_len = data_len;
			this->timestamp = timestamp;
		}

		PacketInfo(uint16_t sequence_number, uint16_t data_len, uint64_t timestamp, uint16_t expect_sequence_number, uint16_t packet_number) {
			this->sequence_number = sequence_number;
			this->data_len = data_len;
			this->timestamp = timestamp;
			this->expect_sequence_number = expect_sequence_number;
			this->packet_number = packet_number;
		}
	};
	std::list<PacketInfo> _send_packets_len;
	uint64_t _total_send_len = 0;
	std::list<PacketInfo> _recv_packets_len;
	uint64_t _total_recv_len = 0;
	std::list<uint16_t> _acked_packets_len;
	std::list<PacketInfo> _real_send_packets_len;
	std::list<PacketInfo> _real_recv_packets_len;
};


