/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/zero_checksum_acceptable_chunk_parameter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "net/dcsctp/public/types.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

// https://www.ietf.org/archive/id/draft-tuexen-tsvwg-sctp-zero-checksum-00.html#section-3

//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |   Type = 0x8001 (suggested)   |          Length = 8           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           Error Detection Method Identifier (EDMID)           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int ZeroChecksumAcceptableChunkParameter::kType;

std::optional<ZeroChecksumAcceptableChunkParameter>
ZeroChecksumAcceptableChunkParameter::Parse(
    webrtc::ArrayView<const uint8_t> data) {
  std::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return std::nullopt;
  }

  ZeroChecksumAlternateErrorDetectionMethod method(reader->Load32<4>());
  if (method == ZeroChecksumAlternateErrorDetectionMethod::None()) {
    return std::nullopt;
  }
  return ZeroChecksumAcceptableChunkParameter(method);
}

void ZeroChecksumAcceptableChunkParameter::SerializeTo(
    std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer = AllocateTLV(out);
  writer.Store32<4>(*error_detection_method_);
}

std::string ZeroChecksumAcceptableChunkParameter::ToString() const {
  webrtc::StringBuilder sb;
  sb << "Zero Checksum Acceptable (" << *error_detection_method_ << ")";
  return sb.Release();
}
}  // namespace dcsctp
