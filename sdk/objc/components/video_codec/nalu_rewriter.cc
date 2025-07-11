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

#include "sdk/objc/components/video_codec/nalu_rewriter.h"

#include <CoreFoundation/CoreFoundation.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "api/array_view.h"
#include "common_video/h264/h264_common.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

using H264::kAud;
using H264::kSps;
using H264::NaluIndex;
using H264::NaluType;
using H264::ParseNaluType;

const char kAnnexBHeaderBytes[4] = {0, 0, 0, 1};
const size_t kAvccHeaderByteSize = sizeof(uint32_t);

bool H264CMSampleBufferToAnnexBBuffer(CMSampleBufferRef avcc_sample_buffer,
                                      bool is_keyframe,
                                      webrtc::Buffer* annexb_buffer) {
  RTC_DCHECK(avcc_sample_buffer);

  // Get format description from the sample buffer.
  CMVideoFormatDescriptionRef description =
      CMSampleBufferGetFormatDescription(avcc_sample_buffer);
  if (description == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to get sample buffer's description.";
    return false;
  }

  // Get parameter set information.
  int nalu_header_size = 0;
  size_t param_set_count = 0;
  OSStatus status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
      description, 0, nullptr, nullptr, &param_set_count, &nalu_header_size);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to get parameter set.";
    return false;
  }
  RTC_CHECK_EQ(nalu_header_size, kAvccHeaderByteSize);
  RTC_DCHECK_EQ(param_set_count, 2);

  // Truncate any previous data in the buffer without changing its capacity.
  annexb_buffer->SetSize(0);

  // Place all parameter sets at the front of buffer.
  if (is_keyframe) {
    size_t param_set_size = 0;
    const uint8_t* param_set = nullptr;
    for (size_t i = 0; i < param_set_count; ++i) {
      status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          description, i, &param_set, &param_set_size, nullptr, nullptr);
      if (status != noErr) {
        RTC_LOG(LS_ERROR) << "Failed to get parameter set.";
        return false;
      }
      // Update buffer.
      annexb_buffer->AppendData(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
      annexb_buffer->AppendData(reinterpret_cast<const char*>(param_set),
                                param_set_size);
    }
  }

  // Get block buffer from the sample buffer.
  CMBlockBufferRef block_buffer =
      CMSampleBufferGetDataBuffer(avcc_sample_buffer);
  if (block_buffer == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to get sample buffer's block buffer.";
    return false;
  }
  CMBlockBufferRef contiguous_buffer = nullptr;
  // Make sure block buffer is contiguous.
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(
        nullptr, block_buffer, nullptr, nullptr, 0, 0, 0, &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: "
                        << status;
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    // Retain to make cleanup easier.
    CFRetain(contiguous_buffer);
    block_buffer = nullptr;
  }

  // Now copy the actual data.
  char* data_ptr = nullptr;
  size_t block_buffer_size = CMBlockBufferGetDataLength(contiguous_buffer);
  status = CMBlockBufferGetDataPointer(contiguous_buffer, 0, nullptr, nullptr,
                                       &data_ptr);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to get block buffer data.";
    CFRelease(contiguous_buffer);
    return false;
  }
  size_t bytes_remaining = block_buffer_size;
  while (bytes_remaining > 0) {
    // The size type here must match `nalu_header_size`, we expect 4 bytes.
    // Read the length of the next packet of data. Must convert from big endian
    // to host endian.
    RTC_DCHECK_GE(bytes_remaining, (size_t)nalu_header_size);
    uint32_t* uint32_data_ptr = reinterpret_cast<uint32_t*>(data_ptr);
    uint32_t packet_size = CFSwapInt32BigToHost(*uint32_data_ptr);
    // Update buffer.
    annexb_buffer->AppendData(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
    annexb_buffer->AppendData(data_ptr + nalu_header_size, packet_size);

    size_t bytes_written = packet_size + sizeof(kAnnexBHeaderBytes);
    bytes_remaining -= bytes_written;
    data_ptr += bytes_written;
  }
  RTC_DCHECK_EQ(bytes_remaining, (size_t)0);

  CFRelease(contiguous_buffer);
  return true;
}

bool H264AnnexBBufferToCMSampleBuffer(ArrayView<const uint8_t> annexb_buffer,
                                      CMVideoFormatDescriptionRef video_format,
                                      CMSampleBufferRef* out_sample_buffer,
                                      CMMemoryPoolRef memory_pool) {
  RTC_DCHECK(out_sample_buffer);
  RTC_DCHECK(video_format);
  *out_sample_buffer = nullptr;

  AnnexBBufferReader reader(annexb_buffer);
  if (reader.SeekToNextNaluOfType(kSps)) {
    // Buffer contains an SPS NALU - skip it and the following PPS
    ArrayView<const uint8_t> data;
    if (!reader.ReadNalu(data)) {
      RTC_LOG(LS_ERROR) << "Failed to read SPS";
      return false;
    }
    if (!reader.ReadNalu(data)) {
      RTC_LOG(LS_ERROR) << "Failed to read PPS";
      return false;
    }
  } else {
    // No SPS NALU - start reading from the first NALU in the buffer
    reader.SeekToStart();
  }

  // Allocate memory as a block buffer.
  CMBlockBufferRef block_buffer = nullptr;
  CFAllocatorRef block_allocator = CMMemoryPoolGetAllocator(memory_pool);
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault, nullptr, reader.BytesRemaining(), block_allocator,
      nullptr, 0, reader.BytesRemaining(), kCMBlockBufferAssureMemoryNowFlag,
      &block_buffer);
  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to create block buffer.";
    return false;
  }

  // Make sure block buffer is contiguous.
  CMBlockBufferRef contiguous_buffer = nullptr;
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(kCFAllocatorDefault, block_buffer,
                                           block_allocator, nullptr, 0, 0, 0,
                                           &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: "
                        << status;
      CFRelease(block_buffer);
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    block_buffer = nullptr;
  }

  // Get a raw pointer into allocated memory.
  size_t block_buffer_size = 0;
  char* data_ptr = nullptr;
  status = CMBlockBufferGetDataPointer(contiguous_buffer, 0, nullptr,
                                       &block_buffer_size, &data_ptr);
  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to get block buffer data pointer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  RTC_DCHECK(block_buffer_size == reader.BytesRemaining());

  // Write Avcc NALUs into block buffer memory.
  AvccBufferWriter writer(
      MakeArrayView(reinterpret_cast<uint8_t*>(data_ptr), block_buffer_size));
  while (reader.BytesRemaining() > 0) {
    ArrayView<const uint8_t> nalu_data;
    if (reader.ReadNalu(nalu_data)) {
      writer.WriteNalu(nalu_data);
    }
  }

  // Create sample buffer.
  status = CMSampleBufferCreate(kCFAllocatorDefault, contiguous_buffer, true,
                                nullptr, nullptr, video_format, 1, 0, nullptr,
                                0, nullptr, out_sample_buffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create sample buffer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  CFRelease(contiguous_buffer);
  return true;
}

CMVideoFormatDescriptionRef CreateVideoFormatDescription(
    ArrayView<const uint8_t> annexb_buffer) {
  const uint8_t* param_set_ptrs[2] = {};
  size_t param_set_sizes[2] = {};
  AnnexBBufferReader reader(annexb_buffer);
  // Skip everyting before the SPS, then read the SPS and PPS
  if (!reader.SeekToNextNaluOfType(kSps)) {
    return nullptr;
  }
  ArrayView<const uint8_t> param_set;
  if (reader.ReadNalu(param_set)) {
    param_set_ptrs[0] = param_set.data();
    param_set_sizes[0] = param_set.size();
  } else {
    RTC_LOG(LS_ERROR) << "Failed to read SPS";
    return nullptr;
  }
  if (reader.ReadNalu(param_set)) {
    param_set_ptrs[1] = param_set.data();
    param_set_sizes[1] = param_set.size();
  } else {
    RTC_LOG(LS_ERROR) << "Failed to read PPS";
    return nullptr;
  }

  // Parse the SPS and PPS into a CMVideoFormatDescription.
  CMVideoFormatDescriptionRef description = nullptr;
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault, 2, param_set_ptrs, param_set_sizes, 4, &description);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create video format description.";
    return nullptr;
  }
  return description;
}

AnnexBBufferReader::AnnexBBufferReader(ArrayView<const uint8_t> annexb_buffer)
    : buffer_(annexb_buffer) {
  offsets_ = H264::FindNaluIndices(annexb_buffer);
  offset_ = offsets_.begin();
}

AnnexBBufferReader::~AnnexBBufferReader() = default;

bool AnnexBBufferReader::ReadNalu(ArrayView<const uint8_t>& out_nalu) {
  if (offset_ == offsets_.end()) {
    return false;
  }
  out_nalu =
      buffer_.subview(offset_->payload_start_offset, offset_->payload_size);
  ++offset_;
  return true;
}

size_t AnnexBBufferReader::BytesRemaining() const {
  if (offset_ == offsets_.end()) {
    return 0;
  }
  return buffer_.size() - offset_->start_offset;
}

void AnnexBBufferReader::SeekToStart() {
  offset_ = offsets_.begin();
}

bool AnnexBBufferReader::SeekToNextNaluOfType(NaluType type) {
  for (; offset_ != offsets_.end(); ++offset_) {
    if (offset_->payload_size < 1)
      continue;
    if (ParseNaluType(buffer_[offset_->payload_start_offset]) == type)
      return true;
  }
  return false;
}

AvccBufferWriter::AvccBufferWriter(ArrayView<uint8_t> avcc_buffer)
    : buffer_(avcc_buffer) {}

bool AvccBufferWriter::WriteNalu(ArrayView<const uint8_t> data) {
  // Check if we can write this length of data.
  if (data.size() + kAvccHeaderByteSize > BytesRemaining()) {
    return false;
  }
  // Write length header, which needs to be big endian.
  uint32_t big_endian_length = CFSwapInt32HostToBig(data.size());
  memcpy(buffer_.data(), &big_endian_length, sizeof(big_endian_length));
  buffer_ = buffer_.subview(sizeof(big_endian_length));
  // Write data.
  memcpy(buffer_.data(), data.data(), data.size());
  buffer_ = buffer_.subview(data.size());
  return true;
}

size_t AvccBufferWriter::BytesRemaining() const {
  return buffer_.size();
}

}  // namespace webrtc
