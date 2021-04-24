/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#include "gtest/gtest.h"
#include "video_encoder.hpp"

class MyVideoEncoder : public cnstream::VideoEncoder {
 public:
  explicit MyVideoEncoder(size_t size) : VideoEncoder(size) {}

  bool CallPushOutputBuffer(uint8_t *data, size_t size, uint32_t frame_id, int64_t timestamp) {
    return PushOutputBuffer(data, size, frame_id, timestamp);
  }

  class frame : public VideoFrame {
    void Fill(uint8_t *data, int64_t timestamp) override {}
  };

  VideoFrame *NewFrame() override { return nullptr; }

  void EncodeFrame(VideoFrame *frame) override {}
};

TEST(RTSP, VideoEncoderGetFrame) {
  MyVideoEncoder enc(150);
  enc.Start();
  // enc.GetFrame(uint8_t *data, uint32_t max_size, uint32_t *size, int64_t *timestamp);
  constexpr size_t max_size = 100;
  uint8_t data[max_size]{1};

  EXPECT_FALSE(enc.GetFrame(nullptr, 32, nullptr, nullptr));
  uint32_t size;
  int64_t timestamp;
  EXPECT_FALSE(enc.GetFrame(nullptr, 2, &size, &timestamp));  // make is_client_running true
  EXPECT_TRUE(enc.CallPushOutputBuffer(data, 100, 0, 0));     // push fake data
  EXPECT_TRUE(enc.GetFrame(nullptr, 2, &size, &timestamp));
  EXPECT_FALSE(enc.CallPushOutputBuffer(data, 100, 0, 0));  // buffer is not enough
  EXPECT_TRUE(enc.GetFrame(data, max_size, &size, &timestamp));
  EXPECT_TRUE(enc.CallPushOutputBuffer(data, 100, 0, 0));        // two step write
  EXPECT_TRUE(enc.GetFrame(data, max_size, &size, &timestamp));  // two step read
  enc.Stop();
}

TEST(RTSP, VideoEncoderPushOutputBuffer) {
  MyVideoEncoder enc(0x10000);
  enc.Start();
  EXPECT_FALSE(enc.CallPushOutputBuffer(nullptr, 0, 1, 0));
  enc.Stop();
}
