#pragma once
#include "../quick_client_cmd/protocol.h"
#include "../quick/quick.h"
#include <list>
#include "../thread/utility.h"
#include <fstream>

class SimpleServer : public QuickCallback {
public:
	SimpleServer(uint16_t port);
	~SimpleServer();
	bool Process();
	void OnConnected(uint32_t session_id) override;
	void OnDisconnected(uint32_t session_id) override;
	void OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port) override;
	void OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len) override;

private:
	void ProcessPackets();
	void ProcFileBegin(uint8_t *data, uint16_t data_len);
	void ProcFileData(uint8_t *data, uint16_t data_len);
	void ProcFileEnd(uint8_t *data, uint16_t data_len);
	void ProcVerbose(uint8_t *data, uint16_t data_len);

	struct Packet {
		uint32_t session_id;
		uint16_t data_len;
		std::unique_ptr<uint8_t> data;

		Packet() {}
		Packet(uint32_t session_id, uint16_t data_len, const uint8_t *data) {
			this->session_id = session_id;
			this->data_len = data_len;
			this->data.reset(new uint8_t[data_len]);
			memcpy(this->data.get(), data, data_len);
		}
	};

	uint16_t _local_port;
	std::unique_ptr<Quick> _quick;
	uint32_t _session_id;
	uint64_t _total_recv_bytes;

	dd::Lock _lock;
	std::list<Packet> _wait_process_packets;
	int _fd = 0;
	uint64_t _begin_recv_file_time = 0;
	uint64_t _recv_speed = 0;
	uint64_t _print_info = 0;
};
