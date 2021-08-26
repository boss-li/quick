#pragma once
#include "quic_types.h"
#include "quic_time.h"
#include "quic_packet_number.h"
#include "../quick/quick_packet.h"
#include <map>
#include <unordered_map>
#include <list>

namespace quic {

	template <typename U>
	inline bool IsNewer(U value, U prev_value) {
		static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
		// kBreakpoint is the half-way mark for the type U. For instance, for a
		// uint16_t it will be 0x8000, and for a uint32_t, it will be 0x8000000.
		constexpr U kBreakpoint = (std::numeric_limits<U>::max() >> 1) + 1;
		// Distinguish between elements that are exactly kBreakpoint apart.
		// If t1>t2 and |t1-t2| = kBreakpoint: IsNewer(t1,t2)=true,
		// IsNewer(t2,t1)=false
		// rather than having IsNewer(t1,t2) = IsNewer(t2,t1) = false.
		if (value - prev_value == kBreakpoint) {
			return value > prev_value;
		}
		return value != prev_value &&
			static_cast<U>(value - prev_value) < kBreakpoint;
	}

	class QuicUnackedPacketMap {
	public:
		explicit QuicUnackedPacketMap();
		void AddSentPacket(uint16_t packet_number, QuicTime &sent_time, QuickPacket* sent_packet);
		void AAA(){}
		void OnPacketAcked(uint16_t packet_number, QuicTime received_ack_time);
		void OnPacketLoss(uint16_t packet_number);
		void RemovePacket(uint16_t packet_number);
		void PickTimeoutUnackedPackets(std::list<uint16_t> &packets, uint64_t timeout_us, uint64_t now);
		void PickLostPackets(std::list<uint16_t> &packets, uint16_t latest_number);

		// Returns the sum of bytes from all packets in flight.
		QuicByteCount bytes_in_flight() const { return _bytes_in_flight; }
		QuicPacketCount packets_in_flight() const { return _packets_in_flight; }
		QuicByteCount bytes_acked(uint16_t packet_number) const;
		
		// Returns the smallest packet number of a serialized packet which has not
		// been acked by the peer.  If there are no unacked packets, returns 0.
		QuicPacketNumber GetLeastUnacked() const;
		uint16_t GetLeastUnackedPacketNumber() const {
			return _least_unack_packet_number;
		}
		uint16_t GetLeastUnackedSequenceNumber() const {
			return _least_unack_sequence_number;
		}
		bool IsUnacked(uint16_t packet_number) const;
		bool IsAcked(uint16_t packet_number) const;
		uint64_t sent_time(uint16_t packet_number) const;
		QuickPacket* packet(uint16_t sequence_number);
		int32_t sequence_number(uint16_t packet_number) const;
		bool HasPacket(uint16_t packet_number) const;

	private:
		void ReduceInflightPacket(int32_t sequence_number);
#define MAX_UNACK_PKT_COUNT 0xFFFF + 1
		QuickPacket _unack_packets[MAX_UNACK_PKT_COUNT];
		// 在高吞吐量的情况下，map的效率比较低，所有这里使用数组。 例如在AddSentPacket函数里面，如果使用map，
		// 将数据放入到map的操作将会影响发送速度，进而影响接收速度（不使用map速度可以达到100Mbyte/s，使用了map只到70Mbyte/s)。
		// 数组下标表示packet number，数组元素表示sequence number
		int32_t _unack_sequence_numbers[MAX_UNACK_PKT_COUNT];
		uint16_t _least_unack_sequence_number = 0;
		uint16_t _least_unack_packet_number = 0;
		QuicByteCount _bytes_in_flight = 0;
		QuicPacketCount _packets_in_flight = 0;
		uint16_t _latest_received_packet_number = 0;
	};
}// namespace quic
