#pragma once
#include "quic_time.h"

namespace quic {
	class QuicClock {
	public:
		QuicClock();
		virtual ~QuicClock();

		QuicClock(const QuicClock&) = delete;
		QuicClock& operator=(const QuicClock&) = delete;

		QuicTime::Delta ComputeCalibrationOffset() const;

		void SetCalibrationOffset(QuicTime::Delta offset);
		virtual QuicTime ApproximateNow() const = 0;
		virtual QuicTime Now() const = 0;
		virtual QuicWallTime WallNow() const = 0;
		virtual QuicTime ConvertWallTimeToQuicTime(const QuicWallTime& walltime)const;

	protected:
		QuicTime CreateTimeFromMicroseconds(uint64_t time_us)const {
			return QuicTime(time_us);
		}

	private:
		bool is_calibrated_;
		QuicTime::Delta calibration_offset_;
	};
}// namespace quic
