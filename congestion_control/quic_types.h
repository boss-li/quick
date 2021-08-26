#pragma once
#include <array>
#include <cstddef>
#include <ostream>
#include <vector>
#include "quic_packet_number.h"
#include "quic_time.h"

namespace quic {

	using QuicPacketLength = uint16_t;
	using QuicControlFrameId = uint32_t;
	using QuicMessageId = uint32_t;
	using QuicDatagramFlowId = uint64_t;
	
	using QuicByteCount = uint64_t;
	using QuicPacketCount = uint64_t;

	enum CongestionControlType {
		kCubicBytes,
		kRenoBytes,
		kBBR,
		kPCC,
		kGoogCC,
		kBBRv2,
	};

	// Information about a newly acknowledged packet.
	struct AckedPacket {
		constexpr AckedPacket(QuicPacketNumber packet_number,
			QuicPacketLength bytes_acked,
			QuicTime receive_timestamp)
			: packet_number(packet_number),
			bytes_acked(bytes_acked),
			receive_timestamp(receive_timestamp) {}

		friend std::ostream& operator<<(
			std::ostream& os,
			const AckedPacket& acked_packet);

		QuicPacketNumber packet_number;
		// Number of bytes sent in the packet that was acknowledged.
		QuicPacketLength bytes_acked;
		// The time |packet_number| was received by the peer, according to the
		// optional timestamp the peer included in the ACK frame which acknowledged
		// |packet_number|. Zero if no timestamp was available for this packet.
		QuicTime receive_timestamp;
	};

	// A vector of acked packets.
	using AckedPacketVector = std::vector<AckedPacket>;

	// Information about a newly lost packet.
	struct LostPacket {
		LostPacket(QuicPacketNumber packet_number, QuicPacketLength bytes_lost)
			: packet_number(packet_number), bytes_lost(bytes_lost) {}

		friend std::ostream& operator<<(
			std::ostream& os,
			const LostPacket& lost_packet);

		QuicPacketNumber packet_number;
		// Number of bytes sent in the packet that was lost.
		QuicPacketLength bytes_lost;
	};

	// A vector of lost packets.
	using LostPacketVector = std::vector<LostPacket>;

	enum HasRetransmittableData : uint8_t {
		NO_RETRANSMITTABLE_DATA,
		HAS_RETRANSMITTABLE_DATA,
	};

	struct  NextReleaseTimeResult {
		// The ideal release time of the packet being sent.
		QuicTime release_time;
		// Whether it is allowed to send the packet before release_time.
		bool allow_burst;
	};
}// namespace quic
