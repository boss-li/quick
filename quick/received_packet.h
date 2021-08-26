#pragma once
#include <memory>
#include <string>

struct WaitAckPacket {
	WaitAckPacket(){}
	 WaitAckPacket(uint16_t packet_number, uint64_t received_time) {
		this->packet_number = packet_number;
		this->received_time = received_time;
	}
	uint16_t packet_number = 0;
	uint64_t received_time = 0;
};

struct ReceivedPacket {
	uint16_t sequence_number = 0;
	uint16_t data_len = 0;
	std::unique_ptr<uint8_t> data;

	explicit ReceivedPacket(uint16_t sequence_number, const uint8_t* data, uint16_t data_len) {
		this->data_len = data_len;
		this->data.reset(new uint8_t[data_len]);
		memcpy(this->data.get(), data, data_len);
		this->sequence_number = sequence_number;
	}
};
