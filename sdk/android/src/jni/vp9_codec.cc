/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "api/environment/environment.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "sdk/android/generated_libvpx_vp9_jni/LibvpxVp9Decoder_jni.h"
#include "sdk/android/generated_libvpx_vp9_jni/LibvpxVp9Encoder_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

jlong JNI_LibvpxVp9Encoder_Create(JNIEnv* jni, jlong j_webrtc_env_ref) {
  return NativeToJavaPointer(
      CreateVp9Encoder(*reinterpret_cast<const Environment*>(j_webrtc_env_ref))
          .release());
}

static jboolean JNI_LibvpxVp9Encoder_IsSupported(JNIEnv* jni) {
  return !SupportedVP9Codecs().empty();
}

static jlong JNI_LibvpxVp9Decoder_CreateDecoder(JNIEnv* jni) {
  return jlongFromPointer(VP9Decoder::Create().release());
}

static jboolean JNI_LibvpxVp9Decoder_IsSupported(JNIEnv* jni) {
  return !SupportedVP9Codecs().empty();
}

}  // namespace jni
}  // namespace webrtc
