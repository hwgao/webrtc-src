/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_DECODE_TIMING_H_
#define VIDEO_FRAME_DECODE_TIMING_H_

#include <stdint.h>

#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/timing.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class FrameDecodeTiming {
 public:
  FrameDecodeTiming(Clock* clock, VCMTiming const* timing);
  ~FrameDecodeTiming() = default;
  FrameDecodeTiming(const FrameDecodeTiming&) = delete;
  FrameDecodeTiming& operator=(const FrameDecodeTiming&) = delete;

  // Any frame that has decode delay more than this in the past can be
  // fast-forwarded.
  static constexpr TimeDelta kMaxAllowedFrameDelay = TimeDelta::Millis(5);

  struct FrameSchedule {
    Timestamp latest_decode_time;
    Timestamp render_time;
  };

  std::optional<FrameSchedule> OnFrameBufferUpdated(
      uint32_t next_temporal_unit_rtp,
      uint32_t last_temporal_unit_rtp,
      TimeDelta max_wait_for_frame,
      bool too_many_frames_queued);

 private:
  Clock* const clock_;
  VCMTiming const* const timing_;
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_DECODE_TIMING_H_
