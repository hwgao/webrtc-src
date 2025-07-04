/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/quality_scaler_settings.h"

#include "api/field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(QualityScalerSettingsTest, ValuesNotSetByDefault) {
  FieldTrials field_trials("");
  const auto settings = QualityScalerSettings(field_trials);
  EXPECT_FALSE(settings.MinFrames());
  EXPECT_FALSE(settings.InitialScaleFactor());
  EXPECT_FALSE(settings.ScaleFactor());
  EXPECT_FALSE(settings.InitialBitrateIntervalMs());
  EXPECT_FALSE(settings.InitialBitrateFactor());
}

TEST(QualityScalerSettingsTest, ParseMinFrames) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/min_frames:100/");
  EXPECT_EQ(100, QualityScalerSettings(field_trials).MinFrames());
}

TEST(QualityScalerSettingsTest, ParseInitialScaleFactor) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/initial_scale_factor:1.5/");
  EXPECT_EQ(1.5, QualityScalerSettings(field_trials).InitialScaleFactor());
}

TEST(QualityScalerSettingsTest, ParseScaleFactor) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/scale_factor:1.1/");
  EXPECT_EQ(1.1, QualityScalerSettings(field_trials).ScaleFactor());
}

TEST(QualityScalerSettingsTest, ParseInitialBitrateInterval) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/initial_bitrate_interval_ms:1000/");
  EXPECT_EQ(1000,
            QualityScalerSettings(field_trials).InitialBitrateIntervalMs());
}

TEST(QualityScalerSettingsTest, ParseInitialBitrateFactor) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/initial_bitrate_factor:0.75/");
  EXPECT_EQ(0.75, QualityScalerSettings(field_trials).InitialBitrateFactor());
}

TEST(QualityScalerSettingsTest, ParseAll) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "min_frames:100,initial_scale_factor:1.5,scale_factor:0.9,"
      "initial_bitrate_interval_ms:5500,initial_bitrate_factor:0.7/");
  const auto settings = QualityScalerSettings(field_trials);
  EXPECT_EQ(100, settings.MinFrames());
  EXPECT_EQ(1.5, settings.InitialScaleFactor());
  EXPECT_EQ(0.9, settings.ScaleFactor());
  EXPECT_EQ(5500, settings.InitialBitrateIntervalMs());
  EXPECT_EQ(0.7, settings.InitialBitrateFactor());
}

TEST(QualityScalerSettingsTest, DoesNotParseIncorrectValue) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "min_frames:a,initial_scale_factor:b,scale_factor:c,"
      "initial_bitrate_interval_ms:d,initial_bitrate_factor:e/");
  const auto settings = QualityScalerSettings(field_trials);
  EXPECT_FALSE(settings.MinFrames());
  EXPECT_FALSE(settings.InitialScaleFactor());
  EXPECT_FALSE(settings.ScaleFactor());
  EXPECT_FALSE(settings.InitialBitrateIntervalMs());
  EXPECT_FALSE(settings.InitialBitrateFactor());
}

TEST(QualityScalerSettingsTest, DoesNotReturnTooSmallValue) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScalerSettings/"
      "min_frames:0,initial_scale_factor:0.0,scale_factor:0.0,"
      "initial_bitrate_interval_ms:-1,initial_bitrate_factor:0.0/");
  const auto settings = QualityScalerSettings(field_trials);
  EXPECT_FALSE(settings.MinFrames());
  EXPECT_FALSE(settings.InitialScaleFactor());
  EXPECT_FALSE(settings.ScaleFactor());
  EXPECT_FALSE(settings.InitialBitrateIntervalMs());
  EXPECT_FALSE(settings.InitialBitrateFactor());
}

}  // namespace
}  // namespace webrtc
