#pragma once
#include "quic_time_accumulator.h"

namespace quic {
	struct QuicConnectionStats {
		// Number of PROBE_BW cycles. Populated for BBRv1 and BBRv2.
		uint32_t bbr_num_cycles = 0;
		// Number of PROBE_BW cycles shortened for reno coexistence. BBRv2 only.
		uint32_t bbr_num_short_cycles_for_reno_coexistence = 0;
		// Number of ack aggregation epochs. For the same number of bytes acked, the
// smaller this value, the more ack aggregation is going on.
		uint64_t num_ack_aggregation_epochs = 0;
		// Number of times this connection went through the slow start phase.
		uint32_t slowstart_count = 0;
		// Time spent in slow start. Populated for BBRv1 and BBRv2.
		QuicTimeAccumulator slowstart_duration;
		// Whether BBR exited STARTUP due to excessive loss. Populated for BBRv1 and
 // BBRv2.
		bool bbr_exit_startup_due_to_loss = false;
	};
}// namespace quic
