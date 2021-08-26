#pragma once
#include "quic_clock.h"
#include <chrono>
#include "quic_time.h"
namespace quic {
	class QuicClockImpl : public QuicClock {
	public:
		QuicTime ApproximateNow() const override {
			return Now();
		}
		QuicTime Now() const override {
			return CreateTimeFromMicroseconds(NowMicroSeconds());
		 }
		QuicWallTime WallNow() const override {
			// Offset of UNIX epoch (1970-01-01 00:00:00 UTC) from Windows FILETIME epoch
// (1601-01-01 00:00:00 UTC), in microseconds. This value is derived from the
// following: ((1970-1601)*365+89)*24*60*60*1000*1000, where 89 is the number
// of leap year days between 1601 and 1970: (1970-1601)/4 excluding 1700,
// 1800, and 1900.
			static constexpr int64_t kTimeTToMicrosecondsOffset =
				INT64_C(11644473600000000);
			uint64_t t = NowMicroSeconds() - kTimeTToMicrosecondsOffset;
			return QuicWallTime::FromUNIXMicroseconds(t);
		 }

	private:
		long long NowMicroSeconds() const {
			auto now = std::chrono::system_clock::now().time_since_epoch();
			return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
		}
	};

}// namespace quic
