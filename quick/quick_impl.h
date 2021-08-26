#pragma once
#include "quick.h"
#include "../thread/task_queue.h"
#include "../thread/task_queue_factory.h"
#include "../thread/process_thread.h"
#include "../log/LogWrapper.h"
#include "udp_socket.h"
#include "packet_reader.h"
#include "quick_connection.h"
#include <unordered_map>
#include "../thread/utility.h"
#include "ack_thread.h"
#include <thread>

using namespace  dd;
class QuickImpl : public Quick
{
public:
	QuickImpl(uint16_t port, bool as_server , QuickCallback* cb);
	~QuickImpl();

	uint32_t ConnectToServer(const std::string& ip, uint16_t port) override;
	bool Disconnect(uint32_t session_id) override;
	bool Send(uint32_t session_id, const uint8_t* data, uint16_t data_len) override;
	void ExportDebugInfo(uint32_t session_id) override;

	void OnRecv(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len, uint64_t received_time_miscroseconds);
private:
	void InitQuck();
	uint32_t CreateSessionId();
	void ProcessHello(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessHelloOk(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessReject(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessSessionIdRepeat(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessBye(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessPayload(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessAck(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);
	void ProcessHeartbeat(const std::string &ip, uint16_t port, const uint8_t* data, int16_t data_len);

	void CheckTimeout();

#define CONNECTION_TIMEOUT_US 10000000

	std::unique_ptr<TaskQueueFactory> _task_queue_factory;
	TaskQueue _task_queue;
	dd::RepeatingTaskHandle _repeating_task;
	std::unique_ptr<ProcessThread> _paced_thread;
	uint16_t _local_port = 0;
	bool _as_server = false;
	std::unique_ptr<PacketReader> _packet_reader;
	std::unique_ptr<UdpSocket> _udp_socket;
	std::unordered_map < uint32_t, std::unique_ptr<QuickConnection>> _connections;
	QuickCallback *_quick_cb;
	Lock _quick_lock;
	uint64_t _received_time_microseconds = 0;

	std::unique_ptr<std::thread> _ack_thread;
	std::unique_ptr<AckThread> _ack_thread_impl;

	uint64_t _recv_speed_bytes = 0;
	uint64_t _print_time_ms = 0;
};

