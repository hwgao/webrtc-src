/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/idata_chunk.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/types.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc8260#section-2.1

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |   Type = 64   |  Res  |I|U|B|E|       Length = Variable       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                              TSN                              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |        Stream Identifier      |           Reserved            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      Message Identifier                       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |    Payload Protocol Identifier / Fragment Sequence Number     |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  \                                                               \
//  /                           User Data                           /
//  \                                                               \
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int IDataChunk::kType;

std::optional<IDataChunk> IDataChunk::Parse(
    webrtc::ArrayView<const uint8_t> data) {
  std::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return std::nullopt;
  }
  uint8_t flags = reader->Load8<1>();
  TSN tsn(reader->Load32<4>());
  StreamID stream_identifier(reader->Load16<8>());
  MID mid(reader->Load32<12>());
  uint32_t ppid_or_fsn = reader->Load32<16>();

  Options options;
  options.is_end = Data::IsEnd((flags & (1 << kFlagsBitEnd)) != 0);
  options.is_beginning =
      Data::IsBeginning((flags & (1 << kFlagsBitBeginning)) != 0);
  options.is_unordered = IsUnordered((flags & (1 << kFlagsBitUnordered)) != 0);
  options.immediate_ack =
      ImmediateAckFlag((flags & (1 << kFlagsBitImmediateAck)) != 0);

  return IDataChunk(tsn, stream_identifier, mid,
                    PPID(options.is_beginning ? ppid_or_fsn : 0),
                    FSN(options.is_beginning ? 0 : ppid_or_fsn),
                    std::vector<uint8_t>(reader->variable_data().begin(),
                                         reader->variable_data().end()),
                    options);
}

void IDataChunk::SerializeTo(std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer = AllocateTLV(out, payload().size());

  writer.Store8<1>(
      (*options().is_end ? (1 << kFlagsBitEnd) : 0) |
      (*options().is_beginning ? (1 << kFlagsBitBeginning) : 0) |
      (*options().is_unordered ? (1 << kFlagsBitUnordered) : 0) |
      (*options().immediate_ack ? (1 << kFlagsBitImmediateAck) : 0));
  writer.Store32<4>(*tsn());
  writer.Store16<8>(*stream_id());
  writer.Store32<12>(*mid());
  writer.Store32<16>(options().is_beginning ? *ppid() : *fsn());
  writer.CopyToVariableData(payload());
}

std::string IDataChunk::ToString() const {
  webrtc::StringBuilder sb;
  sb << "I-DATA, type=" << (options().is_unordered ? "unordered" : "ordered")
     << "::"
     << (*options().is_beginning && *options().is_end ? "complete"
         : *options().is_beginning                    ? "first"
         : *options().is_end                          ? "last"
                                                      : "middle")
     << ", tsn=" << *tsn() << ", stream_id=" << *stream_id()
     << ", mid=" << *mid();

  if (*options().is_beginning) {
    sb << ", ppid=" << *ppid();
  } else {
    sb << ", fsn=" << *fsn();
  }
  sb << ", length=" << payload().size();
  return sb.Release();
}

}  // namespace dcsctp
