#pragma once
#include <memory>
#include <string>
#include "../quick/quick.h"
#include <chrono>
#include <thread>
#include "protocol.h"


class SimpleClient : public QuickCallback {
public:
	SimpleClient(uint16_t local_port, const std::string& server_ip, uint16_t server_port, const std::string& send_file);
	~SimpleClient();
	bool Process();

	void OnConnected(uint32_t session_id) override;
	void OnDisconnected(uint32_t session_id) override;
	void OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port) override;
	void OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len) override;

	void PrintInfo();

private:
	void SendFile();
	void SendFileBegin();
	void SendFileEnd();
	std::unique_ptr<Quick> _quick;
	uint16_t _local_port = 0;
	std::string _server_ip;
	uint16_t _server_port;
	std::string _send_file;
	uint32_t _session_id;
	uint64_t _total_send_bytes = 0;
	bool _server_state = false;
	uint64_t _file_size = 0;
	uint64_t _file_pos = 0;
	uint64_t _send_speed = 0;
	uint64_t _print_info = 0;
};
