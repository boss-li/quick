#include "quick_packet.h"
#include <assert.h>
#include <string.h>

QuickPacket::QuickPacket()
{
	_data.reset(new uint8_t[MAX_PKT_LEN]);
}

QuickPacket::QuickPacket(const QuickPacket* other)
{
	_data.reset(new uint8_t[MAX_PKT_LEN]);
	_data_len = other->_data_len;
	memcpy(_data.get(), other->_data.get(), _data_len);
	_pkt_sent_time = other->_pkt_sent_time;
	_sequence_number = other->_sequence_number;
}

void QuickPacket::WriteData(uint16_t sequence_number, uint8_t* data, uint16_t data_len, uint64_t sent_time)
{
	_sequence_number = sequence_number;
	assert(data_len <= MAX_PKT_LEN);
	_data_len = data_len;
	memcpy(_data.get(), data, data_len);
	_pkt_sent_time = sent_time;
}

void QuickPacket::WriteData(const QuickPacket* other)
{
	_data_len = other->_data_len;
	memcpy(_data.get(), other->_data.get(), _data_len);
	_pkt_sent_time = other->_pkt_sent_time;
	_sequence_number = other->_sequence_number;
}

void QuickPacket::Reset()
{
	_data_len = 0;
	_pkt_sent_time = 0;
	_acked = false;
}

bool QuickPacket::IsValid() const
{
	return _data_len != 0;
}
