/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"

#include <cstring>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr char kIrrelatedSimulcastStreamFrameData[] = "Dummy!";

}  // namespace

scoped_refptr<VideoFrameBuffer> CreateDummyFrameBuffer() {
  // Use i420 buffer here as default one and supported by all codecs.
  scoped_refptr<I420Buffer> buffer = I420Buffer::Create(2, 2);
  memcpy(buffer->MutableDataY(), kIrrelatedSimulcastStreamFrameData, 2);
  memcpy(buffer->MutableDataY() + buffer->StrideY(),
         kIrrelatedSimulcastStreamFrameData + 2, 2);
  memcpy(buffer->MutableDataU(), kIrrelatedSimulcastStreamFrameData + 4, 1);
  memcpy(buffer->MutableDataV(), kIrrelatedSimulcastStreamFrameData + 5, 1);
  return buffer;
}

bool IsDummyFrame(const VideoFrame& video_frame) {
  if (video_frame.width() != 2 || video_frame.height() != 2) {
    return false;
  }
  scoped_refptr<I420BufferInterface> buffer =
      video_frame.video_frame_buffer()->ToI420();
  if (memcmp(buffer->DataY(), kIrrelatedSimulcastStreamFrameData, 2) != 0) {
    return false;
  }
  if (memcmp(buffer->DataY() + buffer->StrideY(),
             kIrrelatedSimulcastStreamFrameData + 2, 2) != 0) {
    return false;
  }
  if (memcmp(buffer->DataU(), kIrrelatedSimulcastStreamFrameData + 4, 1) != 0) {
    return false;
  }
  if (memcmp(buffer->DataV(), kIrrelatedSimulcastStreamFrameData + 5, 1) != 0) {
    return false;
  }
  return true;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
