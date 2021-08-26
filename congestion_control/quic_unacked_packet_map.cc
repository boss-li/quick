#include "quic_unacked_packet_map.h"
#include <assert.h>
#include "../log/LogWrapper.h"

extern LogWrapper g_quic_log;
namespace quic {

	QuicUnackedPacketMap::QuicUnackedPacketMap()
	{
		memset(_unack_sequence_numbers, -1, sizeof(_unack_sequence_numbers));
	}

	void QuicUnackedPacketMap::AddSentPacket(uint16_t packet_number,
		QuicTime &sent_time, 
		QuickPacket* sent_packet)
	{
		uint16_t sequence_number = sent_packet->sequence_number();
		assert(_unack_sequence_numbers[packet_number] == -1);
		_unack_sequence_numbers[packet_number] = sequence_number;

#ifdef OUTPUT_VERBOSE_INFO
			dd::OutputDebugInfo("AddSentPacket set _unack_sequence_numbers[%u] = %u\r\n", packet_number, sequence_number);
#endif

		if (!_unack_packets[sequence_number].IsValid()) {
			_unack_packets[sequence_number].WriteData(sent_packet);
		}
		else {
			// 重传包，不需要再写内存
#ifdef OUTPUT_VERBOSE_INFO
				dd::OutputDebugInfo("cannot write packet, sequence number = %d\r\n",
					sequence_number);
#endif
			assert(_unack_packets[sequence_number].data_len() == sent_packet->data_len());
		}

		_bytes_in_flight += sent_packet->data_len();
		_packets_in_flight++;

#ifdef OUTPUT_VERBOSE_INFO
			dd::OutputDebugInfo("Add  packet sequence number = %d _bytes_in_flight = %llu, new sent len = %d, _packets_in_flight = %llu\r\n",
						sequence_number, _bytes_in_flight, sent_packet->data_len(), _packets_in_flight);
#endif

		_unack_packets[sequence_number].SetSentTime(sent_time.ToDebuggingValue());
	}

	void QuicUnackedPacketMap::OnPacketAcked(uint16_t packet_number, QuicTime received_ack_time)
	{
		_latest_received_packet_number = packet_number;
		int32_t sequence_number = _unack_sequence_numbers[packet_number];
		if (sequence_number == -1)
			return;
#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("OnPacketAcked set _unack_sequence_numbers[%u] = -1\r\n", packet_number);
#endif

		_unack_sequence_numbers[packet_number] = -1;
		_unack_packets[sequence_number].SetAcked(true);
		ReduceInflightPacket(sequence_number);

#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("packet reset, sequence number = %d\r\n", sequence_number);
#endif

		if (_least_unack_sequence_number != sequence_number)
			return;
		// 更新最小的未收到ack的包
		while (true) {
			if (_unack_packets[_least_unack_sequence_number].IsValid() == false || !_unack_packets[_least_unack_sequence_number].acked()) {
				break;
			}
			// 已经收到确认的包需要进行重置
	/*		char buf[256] = { 0 };
			sprintf(buf, "packet reset, sequence number = %d\r\n", _least_unack_sequence_number);
			OutputDebugStringA(buf);*/

			_unack_packets[_least_unack_sequence_number].Reset();
			_least_unack_sequence_number++;
		}
	}

	void QuicUnackedPacketMap::OnPacketLoss(uint16_t packet_number)
	{
		int32_t sequence_number = _unack_sequence_numbers[packet_number];
		if (sequence_number == -1)
			return;
		_unack_sequence_numbers[packet_number] = -1;
		ReduceInflightPacket(sequence_number);
	}

	QuicByteCount QuicUnackedPacketMap::bytes_acked(uint16_t packet_number) const
	{
		int32_t sequence_number = _unack_sequence_numbers[packet_number];
		if (sequence_number == -1)
			return 0;
		return _unack_packets[sequence_number].data_len();
	}

	void QuicUnackedPacketMap::RemovePacket(uint16_t packet_number)
	{
		int32_t sequence_number = _unack_sequence_numbers[packet_number];
		if (sequence_number == -1)
			return;
		_unack_sequence_numbers[packet_number] = -1;

#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("RemovePacket set _unack_sequence_numbers[%u] = -1\r\n", packet_number);
#endif
		ReduceInflightPacket(sequence_number);
	}

	void QuicUnackedPacketMap::PickTimeoutUnackedPackets(std::list<uint16_t> &packets, uint64_t timeout_us, uint64_t now)
	{
		int32_t sequence_number = 0;
		uint16_t packet_len = 0;
		// 判断为丢失的包需要从_unack_sequence_numbers里面删除，免得下次循环又看到这个包
		while(true){
			sequence_number = _unack_sequence_numbers[_least_unack_packet_number];
			if(sequence_number == -1)
				break;
			QuickPacket* packet = &_unack_packets[sequence_number];
			if (!packet->acked() && (now - packet->sent_time() >= timeout_us)){
				packets.push_back(sequence_number);
				ReduceInflightPacket(sequence_number);
				_unack_sequence_numbers[_least_unack_packet_number++] = -1;

#ifdef OUTPUT_VERBOSE_INFO
				dd::OutputDebugInfo("PickTimeoutUnackedPackets set _unack_sequence_numbers[%u] = -1\r\n", _least_unack_packet_number);
#endif
			}
			else {
				// 因为_unack_sequence_numbers里面的元素都是时间有序的，所以如果遇到一个包没有超时，那么后面的包也不会超时
				break;
			}
		}
	}

	void QuicUnackedPacketMap::PickLostPackets(std::list<uint16_t> &packets, uint16_t latest_number)
	{
		if (IsNewer(_least_unack_packet_number, latest_number))
			return;
		int32_t sequence_number = -1;
		while (IsNewer(latest_number, _least_unack_packet_number)) {
			sequence_number = _unack_sequence_numbers[_least_unack_packet_number];
			if (sequence_number != -1) {
				packets.push_back(sequence_number);
#ifdef OUTPUT_VERBOSE_INFO
				dd::OutputDebugInfo("PickLostPackets packet lost, packet number = %d, sequence number = %d\r\n",
					_least_unack_packet_number, sequence_number);
#endif

				ReduceInflightPacket(sequence_number);
				_unack_sequence_numbers[_least_unack_packet_number] = -1;
			}

			_least_unack_packet_number++;

#ifdef OUTPUT_VERBOSE_INFO
			dd::OutputDebugInfo("PickLostPackets _unack_sequence_numbers[%d] = -1\r\n", _least_unack_packet_number);
#endif
		}
		_least_unack_packet_number = latest_number;

#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("PickLostPackets set _least_unack_packet_number = %d\r\n", _least_unack_packet_number);
#endif
	}

	QuicPacketNumber QuicUnackedPacketMap::GetLeastUnacked() const {
		return QuicPacketNumber(_least_unack_packet_number);
	}

	bool QuicUnackedPacketMap::IsUnacked(uint16_t packet_number) const
	{
		// lww
		return false;
	}

	bool QuicUnackedPacketMap::IsAcked(uint16_t packet_number) const
	{
		int32_t sequence_number = _unack_sequence_numbers[packet_number];
		if (sequence_number == -1)
			return true;
		if (_unack_packets[sequence_number].IsValid())
			return _unack_packets[sequence_number].acked();
		return true;
	}

	uint64_t QuicUnackedPacketMap::sent_time(uint16_t packet_number) const
	{
		return 0;
	}

	QuickPacket* QuicUnackedPacketMap::packet(uint16_t sequence_number)
	{
		if (_unack_packets[sequence_number].IsValid())
			return &_unack_packets[sequence_number];
		else
			return nullptr;
	}

	int32_t QuicUnackedPacketMap::sequence_number(uint16_t packet_number) const
	{
		return _unack_sequence_numbers[packet_number];
	}

	bool QuicUnackedPacketMap::HasPacket(uint16_t packet_number) const
	{
		if(_unack_sequence_numbers[packet_number] != -1)
			return true;
		return false;
	}

	void QuicUnackedPacketMap::ReduceInflightPacket(int32_t sequence_number)
	{
		if (sequence_number == -1)
			return;
		uint16_t data_len = _unack_packets[sequence_number].data_len();
		if (_bytes_in_flight >= data_len)
			_bytes_in_flight -= data_len;
		else
			_bytes_in_flight = 0;
		if (_packets_in_flight >= 1)
			_packets_in_flight--;
		else
			_packets_in_flight = 0;

#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("Reduce, packet sequence number = %d   _bytes_in_flight = %llu,reduce len = %d, _packets_in_flight = %llu\r\n",
				sequence_number, _bytes_in_flight, data_len, _packets_in_flight);
#endif
	}

}// namespace quic
