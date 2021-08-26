
#include "quic_bandwidth.h"
#include <cinttypes>
#include <string>

namespace quic {

	std::string QuicBandwidth::ToDebuggingValue() const {
		if (bits_per_second_ < 80000) {
			return "";
			/*	return quiche::QuicheStringPrintf("%" PRId64 " bits/s (%" PRId64
					" bytes/s)",
					bits_per_second_, bits_per_second_ / 8);*/
		}

		double divisor;
		char unit;
		if (bits_per_second_ < 8 * 1000 * 1000) {
			divisor = 1e3;
			unit = 'k';
		}
		else if (bits_per_second_ < INT64_C(8) * 1000 * 1000 * 1000) {
			divisor = 1e6;
			unit = 'M';
		}
		else {
			divisor = 1e9;
			unit = 'G';
		}

		double bits_per_second_with_unit = bits_per_second_ / divisor;
		double bytes_per_second_with_unit = bits_per_second_with_unit / 8;
		return "";
	/*	return quiche::QuicheStringPrintf("%.2f %cbits/s (%.2f %cbytes/s)",
			bits_per_second_with_unit, unit,
			bytes_per_second_with_unit, unit);*/
	}

}  // namespace quic

