// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_CONGESTION_CONTROL_BBR2_PROBE_RTT_H_
#define QUICHE_QUIC_CORE_CONGESTION_CONTROL_BBR2_PROBE_RTT_H_

#include "bbr2_misc.h"
#include "quic_time.h"
#include "quic_types.h"

namespace quic {

class Bbr2Sender;
class  Bbr2ProbeRttMode final : public Bbr2ModeBase {
 public:
  using Bbr2ModeBase::Bbr2ModeBase;

  void Enter(QuicTime now,
             const Bbr2CongestionEvent* congestion_event) override;
  void Leave(QuicTime /*now*/,
             const Bbr2CongestionEvent* /*congestion_event*/) override {}

  Bbr2Mode OnCongestionEvent(
      QuicByteCount prior_in_flight,
      QuicTime event_time,
      const AckedPacketVector& acked_packets,
      const LostPacketVector& lost_packets,
      const Bbr2CongestionEvent& congestion_event) override;

  Limits<QuicByteCount> GetCwndLimits() const override;

  bool IsProbingForBandwidth() const override { return false; }

  Bbr2Mode OnExitQuiescence(QuicTime now,
                            QuicTime quiescence_start_time) override;

  struct  DebugState {
    QuicByteCount inflight_target;
    QuicTime exit_time = QuicTime::Zero();
  };

  DebugState ExportDebugState() const;

 private:
  const Bbr2Params& Params() const;

  QuicByteCount InflightTarget() const;

  QuicTime exit_time_ = QuicTime::Zero();
};

 std::ostream& operator<<(
    std::ostream& os,
    const Bbr2ProbeRttMode::DebugState& state);

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_CONGESTION_CONTROL_BBR2_PROBE_RTT_H_
