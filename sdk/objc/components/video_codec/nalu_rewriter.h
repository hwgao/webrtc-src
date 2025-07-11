/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOX_NALU_REWRITER_H_
#define SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOX_NALU_REWRITER_H_

#include <CoreMedia/CoreMedia.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "rtc_base/buffer.h"

using webrtc::H264::NaluIndex;

namespace webrtc {

// Converts a sample buffer emitted from the VideoToolbox encoder into a buffer
// suitable for RTP. The sample buffer is in avcc format whereas the rtp buffer
// needs to be in Annex B format. Data is written directly to `annexb_buffer`.
bool H264CMSampleBufferToAnnexBBuffer(CMSampleBufferRef avcc_sample_buffer,
                                      bool is_keyframe,
                                      webrtc::Buffer* annexb_buffer);

// Converts a buffer received from RTP into a sample buffer suitable for the
// VideoToolbox decoder. The RTP buffer is in annex b format whereas the sample
// buffer is in avcc format.
// If `is_keyframe` is true then `video_format` is ignored since the format will
// be read from the buffer. Otherwise `video_format` must be provided.
// Caller is responsible for releasing the created sample buffer.
bool H264AnnexBBufferToCMSampleBuffer(ArrayView<const uint8_t> annexb_buffer,
                                      CMVideoFormatDescriptionRef video_format,
                                      CMSampleBufferRef* out_sample_buffer,
                                      CMMemoryPoolRef memory_pool);

// Returns a video format description created from the sps/pps information in
// the Annex B buffer. If there is no such information, nullptr is returned.
// The caller is responsible for releasing the description.
CMVideoFormatDescriptionRef CreateVideoFormatDescription(
    ArrayView<const uint8_t> annexb_buffer);

// Helper class for reading NALUs from an RTP Annex B buffer.
class AnnexBBufferReader final {
 public:
  explicit AnnexBBufferReader(ArrayView<const uint8_t> annexb_buffer);
  ~AnnexBBufferReader();
  AnnexBBufferReader(const AnnexBBufferReader& other) = delete;
  void operator=(const AnnexBBufferReader& other) = delete;

  // Returns a pointer to the beginning of the next NALU slice without the
  // header bytes and its length. Returns false if no more slices remain.
  bool ReadNalu(ArrayView<const uint8_t>& out_nalu);

  // Returns the number of unread NALU bytes, including the size of the header.
  // If the buffer has no remaining NALUs this will return zero.
  size_t BytesRemaining() const;

  // Reset the reader to start reading from the first NALU
  void SeekToStart();

  // Seek to the next position that holds a NALU of the desired type,
  // or the end if no such NALU is found.
  // Return true if a NALU of the desired type is found, false if we
  // reached the end instead
  bool SeekToNextNaluOfType(H264::NaluType type);

 private:
  // Returns the the next offset that contains NALU data.
  size_t FindNextNaluHeader(const uint8_t* start,
                            size_t length,
                            size_t offset) const;

  const ArrayView<const uint8_t> buffer_;
  std::vector<NaluIndex> offsets_;
  std::vector<NaluIndex>::iterator offset_;
};

// Helper class for writing NALUs using avcc format into a buffer.
class AvccBufferWriter final {
 public:
  explicit AvccBufferWriter(ArrayView<uint8_t> avcc_buffer);
  ~AvccBufferWriter() {}
  AvccBufferWriter(const AvccBufferWriter& other) = delete;
  void operator=(const AvccBufferWriter& other) = delete;

  // Writes the data slice into the buffer. Returns false if there isn't
  // enough space left.
  bool WriteNalu(ArrayView<const uint8_t> data);

  // Returns the unused bytes in the buffer.
  size_t BytesRemaining() const;

 private:
  ArrayView<uint8_t> buffer_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOX_NALU_REWRITER_H_
