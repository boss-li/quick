#pragma once
#include <memory>
#include <string>

class QuickPacket {
public:
	explicit QuickPacket();
	QuickPacket(const QuickPacket* other);
	void WriteData(uint16_t _sequence_number, uint8_t* data, uint16_t data_len, uint64_t sent_time);
	void WriteData(const QuickPacket* other);
	void Reset();
	bool IsValid() const;
	uint16_t data_len()const { return _data_len; }
	uint8_t* data() { return _data.get(); }
	uint16_t sequence_number() const { return _sequence_number; }
	void SetPacketNumber(uint16_t packet_number) { _packet_number = packet_number; }
	uint16_t packet_number() const { return _packet_number; }
	void SetSentTime(uint64_t sent_time) { _pkt_sent_time = sent_time; }
	uint64_t sent_time() { return _pkt_sent_time; }
	void SetAcked(bool acked) { _acked = acked; }
	bool acked() const { return _acked; }
private:
#define  MAX_PKT_LEN 1400 
	uint16_t _sequence_number = 0;
	uint16_t _packet_number = 0;
	std::unique_ptr<uint8_t> _data;
	uint16_t _data_len = 0;
	uint64_t _pkt_sent_time;		// 包发发送时间
	bool _acked = false;
};
