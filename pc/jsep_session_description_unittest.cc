/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep_session_description.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "media/base/codec.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/session_description.h"
#include "pc/webrtc_sdp.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/socket_address.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::NotNull;
using ::testing::Values;
using webrtc::IceCandidate;
using webrtc::IceCandidateCollection;
using webrtc::IceCandidateType;
using webrtc::JsepSessionDescription;
using ::webrtc::MediaProtocolType;
using webrtc::SdpType;
using webrtc::SessionDescriptionInterface;

static const char kCandidateUfrag[] = "ufrag";
static const char kCandidatePwd[] = "pwd";
static const char kCandidateUfragVoice[] = "ufrag_voice";
static const char kCandidatePwdVoice[] = "pwd_voice";
static const char kCandidateUfragVideo[] = "ufrag_video";
static const char kCandidatePwdVideo[] = "pwd_video";
static const char kCandidateFoundation[] = "a0+B/1";
static const uint32_t kCandidatePriority = 2130706432U;  // pref = 1.0
static const uint32_t kCandidateGeneration = 2;

// This creates a session description with both audio and video media contents.
// In SDP this is described by two m lines, one audio and one video.
static std::unique_ptr<webrtc::SessionDescription>
CreateCricketSessionDescription() {
  auto desc = std::make_unique<webrtc::SessionDescription>();

  // AudioContentDescription
  auto audio = std::make_unique<webrtc::AudioContentDescription>();
  // VideoContentDescription
  auto video = std::make_unique<webrtc::VideoContentDescription>();

  audio->AddCodec(webrtc::CreateAudioCodec(103, "ISAC", 16000, 0));
  desc->AddContent(webrtc::CN_AUDIO, MediaProtocolType::kRtp, std::move(audio));

  video->AddCodec(webrtc::CreateVideoCodec(120, "VP8"));
  desc->AddContent(webrtc::CN_VIDEO, MediaProtocolType::kRtp, std::move(video));

  desc->AddTransportInfo(webrtc::TransportInfo(
      webrtc::CN_AUDIO,
      webrtc::TransportDescription(
          std::vector<std::string>(), kCandidateUfragVoice, kCandidatePwdVoice,
          webrtc::ICEMODE_FULL, webrtc::CONNECTIONROLE_NONE, nullptr)));
  desc->AddTransportInfo(webrtc::TransportInfo(
      webrtc::CN_VIDEO,
      webrtc::TransportDescription(
          std::vector<std::string>(), kCandidateUfragVideo, kCandidatePwdVideo,
          webrtc::ICEMODE_FULL, webrtc::CONNECTIONROLE_NONE, nullptr)));
  return desc;
}

class JsepSessionDescriptionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int port = 1234;
    webrtc::SocketAddress address("127.0.0.1", port++);
    webrtc::Candidate candidate(webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
                                address, 1, "", "", IceCandidateType::kHost, 0,
                                "1");
    candidate_ = candidate;
    const std::string session_id = absl::StrCat(webrtc::CreateRandomId64());
    const std::string session_version = absl::StrCat(webrtc::CreateRandomId());
    jsep_desc_ = std::make_unique<JsepSessionDescription>(SdpType::kOffer);
    ASSERT_TRUE(jsep_desc_->Initialize(CreateCricketSessionDescription(),
                                       session_id, session_version));
  }

  std::string Serialize(const SessionDescriptionInterface* desc) {
    std::string sdp;
    EXPECT_TRUE(desc->ToString(&sdp));
    EXPECT_FALSE(sdp.empty());
    return sdp;
  }

  std::unique_ptr<SessionDescriptionInterface> DeSerialize(
      const std::string& sdp) {
    auto jsep_desc = std::make_unique<JsepSessionDescription>(SdpType::kOffer);
    EXPECT_TRUE(webrtc::SdpDeserialize(sdp, jsep_desc.get(), nullptr));
    return std::move(jsep_desc);
  }

  webrtc::Candidate candidate_;
  std::unique_ptr<JsepSessionDescription> jsep_desc_;
};

TEST_F(JsepSessionDescriptionTest, CloneDefault) {
  auto new_desc = jsep_desc_->Clone();
  EXPECT_EQ(jsep_desc_->type(), new_desc->type());
  std::string old_desc_string;
  std::string new_desc_string;
  EXPECT_TRUE(jsep_desc_->ToString(&old_desc_string));
  EXPECT_TRUE(new_desc->ToString(&new_desc_string));
  EXPECT_EQ(old_desc_string, new_desc_string);
  EXPECT_EQ(jsep_desc_->session_id(), new_desc->session_id());
  EXPECT_EQ(jsep_desc_->session_version(), new_desc->session_version());
}

TEST_F(JsepSessionDescriptionTest, CloneRollback) {
  auto jsep_desc = std::make_unique<JsepSessionDescription>(SdpType::kRollback);
  auto new_desc = jsep_desc->Clone();
  EXPECT_EQ(jsep_desc->type(), new_desc->type());
}

TEST_F(JsepSessionDescriptionTest, CloneWithCandidates) {
  webrtc::Candidate candidate_v4(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("192.168.1.5", 1234), kCandidatePriority, "", "",
      IceCandidateType::kSrflx, kCandidateGeneration, kCandidateFoundation);
  webrtc::Candidate candidate_v6(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("::1", 1234), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);

  IceCandidate jice_v4("audio", 0, candidate_v4);
  IceCandidate jice_v6("audio", 0, candidate_v6);
  IceCandidate jice_v4_video("video", 0, candidate_v4);
  IceCandidate jice_v6_video("video", 0, candidate_v6);
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v4));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v6));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v4_video));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v6_video));
  auto new_desc = jsep_desc_->Clone();
  EXPECT_EQ(jsep_desc_->type(), new_desc->type());
  std::string old_desc_string;
  std::string new_desc_string;
  EXPECT_TRUE(jsep_desc_->ToString(&old_desc_string));
  EXPECT_TRUE(new_desc->ToString(&new_desc_string));
  EXPECT_EQ(old_desc_string, new_desc_string);
}

// Test that number_of_mediasections() returns the number of media contents in
// a session description.
TEST_F(JsepSessionDescriptionTest, CheckSessionDescription) {
  EXPECT_EQ(2u, jsep_desc_->number_of_mediasections());
}

// Test that we can add a candidate to a session description without MID.
TEST_F(JsepSessionDescriptionTest, AddCandidateWithoutMid) {
  IceCandidate jsep_candidate("", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(0);
  ASSERT_TRUE(ice_candidates != nullptr);
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidate* ice_candidate = ice_candidates->at(0);
  ASSERT_TRUE(ice_candidate != nullptr);
  candidate_.set_username(kCandidateUfragVoice);
  candidate_.set_password(kCandidatePwdVoice);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));
  EXPECT_EQ(0, ice_candidate->sdp_mline_index());
  EXPECT_EQ("audio", ice_candidate->sdp_mid());
  EXPECT_EQ(0u, jsep_desc_->candidates(1)->count());
}

// Test that we can add and remove candidates to a session description with
// MID. Removing candidates requires MID.
TEST_F(JsepSessionDescriptionTest, AddAndRemoveIceCandidatesWithMid) {
  // mid and m-line index don't match, in this case mid is preferred.
  std::string mid = "video";
  IceCandidate jsep_candidate(mid, 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(0u, jsep_desc_->candidates(0)->count());
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(1);
  ASSERT_THAT(ice_candidates, NotNull());
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidate* ice_candidate = ice_candidates->at(0);
  ASSERT_THAT(ice_candidate, NotNull());
  candidate_.set_username(kCandidateUfragVideo);
  candidate_.set_password(kCandidatePwdVideo);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));
  // The mline index should have been updated according to mid.
  EXPECT_EQ(1, ice_candidate->sdp_mline_index());

  EXPECT_EQ(1u, jsep_desc_->RemoveCandidate(ice_candidate));
  EXPECT_EQ(0u, jsep_desc_->candidates(0)->count());
  EXPECT_EQ(0u, jsep_desc_->candidates(1)->count());
}

// TODO(bugs.webrtc.org/8395): Remove this test and leave
// AddAndRemoveIceCandidatesWithMid.
TEST_F(JsepSessionDescriptionTest, AddAndRemoveCandidatesWithMid) {
  // mid and m-line index don't match, in this case mid is preferred.
  std::string mid = "video";
  IceCandidate jsep_candidate(mid, 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(0u, jsep_desc_->candidates(0)->count());
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(1);
  ASSERT_TRUE(ice_candidates != nullptr);
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidate* ice_candidate = ice_candidates->at(0);
  ASSERT_TRUE(ice_candidate != nullptr);
  candidate_.set_username(kCandidateUfragVideo);
  candidate_.set_password(kCandidatePwdVideo);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));
  // The mline index should have been updated according to mid.
  EXPECT_EQ(1, ice_candidate->sdp_mline_index());

  std::vector<webrtc::Candidate> candidates(1, candidate_);
  candidates[0].set_transport_name(mid);
  EXPECT_EQ(1u, jsep_desc_->RemoveCandidates(candidates));
  EXPECT_EQ(0u, jsep_desc_->candidates(0)->count());
  EXPECT_EQ(0u, jsep_desc_->candidates(1)->count());
}

TEST_F(JsepSessionDescriptionTest, AddCandidateAlreadyHasUfrag) {
  candidate_.set_username(kCandidateUfrag);
  candidate_.set_password(kCandidatePwd);
  IceCandidate jsep_candidate("audio", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(0);
  ASSERT_TRUE(ice_candidates != nullptr);
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidate* ice_candidate = ice_candidates->at(0);
  ASSERT_TRUE(ice_candidate != nullptr);
  candidate_.set_username(kCandidateUfrag);
  candidate_.set_password(kCandidatePwd);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));

  EXPECT_EQ(0u, jsep_desc_->candidates(1)->count());
}

// Test that we can not add a candidate if there is no corresponding media
// content in the session description.
TEST_F(JsepSessionDescriptionTest, AddBadCandidate) {
  IceCandidate bad_candidate1("", 55, candidate_);
  EXPECT_FALSE(jsep_desc_->AddCandidate(&bad_candidate1));

  IceCandidate bad_candidate2("some weird mid", 0, candidate_);
  EXPECT_FALSE(jsep_desc_->AddCandidate(&bad_candidate2));
}

// Tests that repeatedly adding the same candidate, with or without credentials,
// does not increase the number of candidates in the description.
TEST_F(JsepSessionDescriptionTest, AddCandidateDuplicates) {
  IceCandidate jsep_candidate("", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(1u, jsep_desc_->candidates(0)->count());

  // Add the same candidate again.  It should be ignored.
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(1u, jsep_desc_->candidates(0)->count());

  // Create a new candidate, identical except that the ufrag and pwd are now
  // populated.
  candidate_.set_username(kCandidateUfragVoice);
  candidate_.set_password(kCandidatePwdVoice);
  IceCandidate jsep_candidate_with_credentials("", 0, candidate_);

  // This should also be identified as redundant and ignored.
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate_with_credentials));
  EXPECT_EQ(1u, jsep_desc_->candidates(0)->count());
}

// Test that the connection address is set to a hostname address after adding a
// hostname candidate.
TEST_F(JsepSessionDescriptionTest, AddHostnameCandidate) {
  webrtc::Candidate c;
  c.set_component(webrtc::ICE_CANDIDATE_COMPONENT_RTP);
  c.set_protocol(webrtc::UDP_PROTOCOL_NAME);
  c.set_address(webrtc::SocketAddress("example.local", 1234));
  c.set_type(IceCandidateType::kHost);
  const size_t audio_index = 0;
  IceCandidate hostname_candidate("audio", audio_index, c);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&hostname_candidate));

  ASSERT_NE(nullptr, jsep_desc_->description());
  ASSERT_EQ(2u, jsep_desc_->description()->contents().size());
  const auto& content = jsep_desc_->description()->contents()[audio_index];
  EXPECT_EQ("0.0.0.0:9",
            content.media_description()->connection_address().ToString());
}

// Test that we can serialize a JsepSessionDescription and deserialize it again.
TEST_F(JsepSessionDescriptionTest, SerializeDeserialize) {
  std::string sdp = Serialize(jsep_desc_.get());

  auto parsed_jsep_desc = DeSerialize(sdp);
  EXPECT_EQ(2u, parsed_jsep_desc->number_of_mediasections());

  std::string parsed_sdp = Serialize(parsed_jsep_desc.get());
  EXPECT_EQ(sdp, parsed_sdp);
}

// Test that we can serialize a JsepSessionDescription when a hostname candidate
// is the default destination and deserialize it again. The connection address
// in the deserialized description should be the dummy address 0.0.0.0:9.
TEST_F(JsepSessionDescriptionTest, SerializeDeserializeWithHostnameCandidate) {
  webrtc::Candidate c;
  c.set_component(webrtc::ICE_CANDIDATE_COMPONENT_RTP);
  c.set_protocol(webrtc::UDP_PROTOCOL_NAME);
  c.set_address(webrtc::SocketAddress("example.local", 1234));
  c.set_type(IceCandidateType::kHost);
  const size_t audio_index = 0;
  const size_t video_index = 1;
  IceCandidate hostname_candidate_audio("audio", audio_index, c);
  IceCandidate hostname_candidate_video("video", video_index, c);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&hostname_candidate_audio));
  EXPECT_TRUE(jsep_desc_->AddCandidate(&hostname_candidate_video));

  std::string sdp = Serialize(jsep_desc_.get());

  auto parsed_jsep_desc = DeSerialize(sdp);
  EXPECT_EQ(2u, parsed_jsep_desc->number_of_mediasections());

  ASSERT_NE(nullptr, parsed_jsep_desc->description());
  ASSERT_EQ(2u, parsed_jsep_desc->description()->contents().size());
  const auto& audio_content =
      parsed_jsep_desc->description()->contents()[audio_index];
  const auto& video_content =
      parsed_jsep_desc->description()->contents()[video_index];
  EXPECT_EQ("0.0.0.0:9",
            audio_content.media_description()->connection_address().ToString());
  EXPECT_EQ("0.0.0.0:9",
            video_content.media_description()->connection_address().ToString());
}

// Tests that we can serialize and deserialize a JsepSesssionDescription
// with candidates.
TEST_F(JsepSessionDescriptionTest, SerializeDeserializeWithCandidates) {
  std::string sdp = Serialize(jsep_desc_.get());

  // Add a candidate and check that the serialized result is different.
  IceCandidate jsep_candidate("audio", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  std::string sdp_with_candidate = Serialize(jsep_desc_.get());
  EXPECT_NE(sdp, sdp_with_candidate);

  auto parsed_jsep_desc = DeSerialize(sdp_with_candidate);
  std::string parsed_sdp_with_candidate = Serialize(parsed_jsep_desc.get());

  EXPECT_EQ(sdp_with_candidate, parsed_sdp_with_candidate);
}

// TODO(zhihuang): Modify these tests. These are used to verify that after
// adding the candidates, the connection_address field is set correctly. Modify
// those so that the "connection address" is tested directly.
// Tests serialization of SDP with only IPv6 candidates and verifies that IPv6
// is used as default address in c line according to preference.
TEST_F(JsepSessionDescriptionTest, SerializeSessionDescriptionWithIPv6Only) {
  // Stun has a high preference than local host.
  webrtc::Candidate candidate1(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("::1", 1234), kCandidatePriority, "", "",
      IceCandidateType::kSrflx, kCandidateGeneration, kCandidateFoundation);
  webrtc::Candidate candidate2(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("::2", 1235), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);

  IceCandidate jice1("audio", 0, candidate1);
  IceCandidate jice2("audio", 0, candidate2);
  IceCandidate jice3("video", 0, candidate1);
  IceCandidate jice4("video", 0, candidate2);
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice1));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice2));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice3));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice4));
  std::string message = Serialize(jsep_desc_.get());

  // Should have a c line like this one.
  EXPECT_NE(message.find("c=IN IP6 ::1"), std::string::npos);
  // Shouldn't have a IP4 c line.
  EXPECT_EQ(message.find("c=IN IP4"), std::string::npos);
}

// Tests serialization of SDP with both IPv4 and IPv6 candidates and
// verifies that IPv4 is used as default address in c line even if the
// preference of IPv4 is lower.
TEST_F(JsepSessionDescriptionTest,
       SerializeSessionDescriptionWithBothIPFamilies) {
  webrtc::Candidate candidate_v4(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("192.168.1.5", 1234), kCandidatePriority, "", "",
      IceCandidateType::kSrflx, kCandidateGeneration, kCandidateFoundation);
  webrtc::Candidate candidate_v6(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("::1", 1234), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);

  IceCandidate jice_v4("audio", 0, candidate_v4);
  IceCandidate jice_v6("audio", 0, candidate_v6);
  IceCandidate jice_v4_video("video", 0, candidate_v4);
  IceCandidate jice_v6_video("video", 0, candidate_v6);
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v4));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v6));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v4_video));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice_v6_video));
  std::string message = Serialize(jsep_desc_.get());

  // Should have a c line like this one.
  EXPECT_NE(message.find("c=IN IP4 192.168.1.5"), std::string::npos);
  // Shouldn't have a IP6 c line.
  EXPECT_EQ(message.find("c=IN IP6"), std::string::npos);
}

// Tests serialization of SDP with both UDP and TCP candidates and
// verifies that UDP is used as default address in c line even if the
// preference of UDP is lower.
TEST_F(JsepSessionDescriptionTest,
       SerializeSessionDescriptionWithBothProtocols) {
  // Stun has a high preference than local host.
  webrtc::Candidate candidate1(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "tcp",
      webrtc::SocketAddress("::1", 1234), kCandidatePriority, "", "",
      IceCandidateType::kSrflx, kCandidateGeneration, kCandidateFoundation);
  webrtc::Candidate candidate2(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("fe80::1234:5678:abcd:ef12", 1235),
      kCandidatePriority, "", "", IceCandidateType::kHost, kCandidateGeneration,
      kCandidateFoundation);

  IceCandidate jice1("audio", 0, candidate1);
  IceCandidate jice2("audio", 0, candidate2);
  IceCandidate jice3("video", 0, candidate1);
  IceCandidate jice4("video", 0, candidate2);
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice1));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice2));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice3));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice4));
  std::string message = Serialize(jsep_desc_.get());

  // Should have a c line like this one.
  EXPECT_NE(message.find("c=IN IP6 fe80::1234:5678:abcd:ef12"),
            std::string::npos);
  // Shouldn't have a IP4 c line.
  EXPECT_EQ(message.find("c=IN IP4"), std::string::npos);
}

// Tests serialization of SDP with only TCP candidates and verifies that
// null IPv4 is used as default address in c line.
TEST_F(JsepSessionDescriptionTest, SerializeSessionDescriptionWithTCPOnly) {
  // Stun has a high preference than local host.
  webrtc::Candidate candidate1(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "tcp",
      webrtc::SocketAddress("::1", 1234), kCandidatePriority, "", "",
      IceCandidateType::kSrflx, kCandidateGeneration, kCandidateFoundation);
  webrtc::Candidate candidate2(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "tcp",
      webrtc::SocketAddress("::2", 1235), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);

  IceCandidate jice1("audio", 0, candidate1);
  IceCandidate jice2("audio", 0, candidate2);
  IceCandidate jice3("video", 0, candidate1);
  IceCandidate jice4("video", 0, candidate2);
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice1));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice2));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice3));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice4));

  std::string message = Serialize(jsep_desc_.get());
  EXPECT_EQ(message.find("c=IN IP6 ::3"), std::string::npos);
  // Should have a c line like this one when no any default exists.
  EXPECT_NE(message.find("c=IN IP4 0.0.0.0"), std::string::npos);
}

// Tests that the connection address will be correctly set when the Candidate is
// removed.
TEST_F(JsepSessionDescriptionTest, RemoveCandidateAndSetConnectionAddress) {
  webrtc::Candidate candidate1(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("::1", 1234), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);
  candidate1.set_transport_name("audio");

  webrtc::Candidate candidate2(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "tcp",
      webrtc::SocketAddress("::2", 1235), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);
  candidate2.set_transport_name("audio");

  webrtc::Candidate candidate3(
      webrtc::ICE_CANDIDATE_COMPONENT_RTP, "udp",
      webrtc::SocketAddress("192.168.1.1", 1236), kCandidatePriority, "", "",
      IceCandidateType::kHost, kCandidateGeneration, kCandidateFoundation);
  candidate3.set_transport_name("audio");

  IceCandidate jice1("audio", 0, candidate1);
  IceCandidate jice2("audio", 0, candidate2);
  IceCandidate jice3("audio", 0, candidate3);

  size_t audio_index = 0;
  auto media_desc =
      jsep_desc_->description()->contents()[audio_index].media_description();

  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice1));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice2));
  ASSERT_TRUE(jsep_desc_->AddCandidate(&jice3));

  std::vector<webrtc::Candidate> candidates;
  EXPECT_EQ("192.168.1.1:1236", media_desc->connection_address().ToString());

  candidates.push_back(candidate3);
  ASSERT_TRUE(jsep_desc_->RemoveCandidates(candidates));
  EXPECT_EQ("[::1]:1234", media_desc->connection_address().ToString());

  candidates.clear();
  candidates.push_back(candidate2);
  ASSERT_TRUE(jsep_desc_->RemoveCandidates(candidates));
  EXPECT_EQ("[::1]:1234", media_desc->connection_address().ToString());

  candidates.clear();
  candidates.push_back(candidate1);
  ASSERT_TRUE(jsep_desc_->RemoveCandidates(candidates));
  EXPECT_EQ("0.0.0.0:9", media_desc->connection_address().ToString());
}

class EnumerateAllSdpTypesTest : public ::testing::Test,
                                 public ::testing::WithParamInterface<SdpType> {
};

TEST_P(EnumerateAllSdpTypesTest, TestIdentity) {
  SdpType type = GetParam();

  const char* str = webrtc::SdpTypeToString(type);
  EXPECT_EQ(type, webrtc::SdpTypeFromString(str));
}

INSTANTIATE_TEST_SUITE_P(JsepSessionDescriptionTest,
                         EnumerateAllSdpTypesTest,
                         Values(SdpType::kOffer,
                                SdpType::kPrAnswer,
                                SdpType::kAnswer));
