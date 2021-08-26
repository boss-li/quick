#include "send_algorithm_interface.h"
#include "bbr2_sender.h"
#include "quic_clock.h"

namespace quic {

	class RttStats;

	// Factory for send side congestion control algorithm.
	SendAlgorithmInterface* SendAlgorithmInterface::Create(
		const QuicClock* clock,
		const RttStats* rtt_stats,
		const QuicUnackedPacketMap* unacked_packets,
		CongestionControlType congestion_control_type,
		QuicConnectionStats* stats,
		QuicPacketCount initial_congestion_window) {
		QuicPacketCount max_congestion_window = 2000;

		return new Bbr2Sender(
			clock->ApproximateNow(), rtt_stats, unacked_packets,
			initial_congestion_window, max_congestion_window, stats);
	}

}  // namespace quic

