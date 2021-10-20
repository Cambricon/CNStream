/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "data_source.hpp"
#include "test_base.hpp"

static constexpr const char *gmp4_path = "../../modules/unitest/source/data/img_300x300.mp4";

class MsgObserverForTest : cnstream::StreamMsgObserver {
 public:
  void Update(const cnstream::StreamMsg& smsg) override {
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      wakener_.set_value();
    }
  }

  void WaitForEos() {
    wakener_.get_future().get();
  }

 private:
  std::promise<void> wakener_;
};

class ImageReceiver : public cnstream::Module, public cnstream::ModuleCreator<ImageReceiver> {
 public:
  explicit ImageReceiver(const std::string& mname) : cnstream::Module(mname) {}

  bool Open(cnstream::ModuleParamSet param_set) override { return true; }

  void Close() override {}

  int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
    cnstream::CNDataFramePtr frame = data->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
    frames.push_back(frame);
    return 0;
  }

  const std::vector<cnstream::CNDataFramePtr>& GetFrames() const { return frames; }

  void Clear() { frames.clear(); }

 private:
  std::vector<cnstream::CNDataFramePtr> frames;
};

bool CompareFrames(const std::vector<cnstream::CNDataFramePtr> &src_frames,
                   const std::vector<cnstream::CNDataFramePtr> &aligned_frames) {
  EXPECT_EQ(src_frames.size(), aligned_frames.size());
  size_t frame_num = src_frames.size();
  for (size_t fi = 0; fi < frame_num; ++fi) {
    auto src_frame = src_frames[fi];
    auto aligned_frame = aligned_frames[fi];
    EXPECT_FALSE(aligned_frame->stride[0] % 128);
    EXPECT_FALSE(aligned_frame->stride[1] % 128);

    auto src_mat = src_frame->ImageBGR();
    auto dst_mat = aligned_frame->ImageBGR();
    EXPECT_EQ(0, memcmp(src_mat.data, dst_mat.data, src_mat.total() * src_mat.elemSize()));
  }
  return true;
}

std::vector<cnstream::CNDataFramePtr> GetFrames(const cnstream::ModuleParamSet &source_params) {
  cnstream::Pipeline pipeline("pipeline");

  cnstream::CNModuleConfig receiver_config;
  receiver_config.name = "receiver";
  receiver_config.className = "ImageReceiver";
  receiver_config.maxInputQueueSize = 5;
  receiver_config.parallelism = 1;

  cnstream::CNModuleConfig source_config;
  source_config.name = "source";
  source_config.className = "cnstream::DataSource";
  source_config.next = {"receiver"};
  source_config.parameters = source_params;
  source_config.maxInputQueueSize = 0;
  source_config.parallelism = 0;

  EXPECT_TRUE(pipeline.BuildPipeline({source_config, receiver_config}));

  cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule("source"));
  ImageReceiver* receiver = dynamic_cast<ImageReceiver*>(pipeline.GetModule("receiver"));

  EXPECT_NE(nullptr, source);
  EXPECT_NE(nullptr, receiver);

  MsgObserverForTest observer;
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&observer));

  EXPECT_TRUE(pipeline.Start());

  std::string filename = GetExePath() + gmp4_path;
  auto handler =
      cnstream::FileHandler::Create(source, "0", filename, 30, false);
  EXPECT_NE(nullptr, handler);
  EXPECT_EQ(0, source->AddSource(handler));

  observer.WaitForEos();

  pipeline.Stop();

  return receiver->GetFrames();
}

static bool TestFunc(const std::string &decoder_type, const std::string &output_type) {
  cnstream::ModuleParamSet source_params = {
    std::make_pair("decoder_type", decoder_type),
    std::make_pair("output_type", decoder_type),
    std::make_pair("device_id", "0")
  };

  auto origin_frames = GetFrames(source_params);

  source_params["apply_stride_align_for_scaler"] = "true";

  source_params["output_type"] = output_type;
  auto aligned_frames = GetFrames(source_params);

  // compare frames
  return CompareFrames(origin_frames, aligned_frames);
}

TEST(Source_StrideAlign, mlu_decoder_output_cpu) {
  // EXPECT_TRUE(TestFunc("mlu", "cpu"));
}

TEST(Source_StrideAlign, mlu_decoder_output_mlu) {
  EXPECT_TRUE(TestFunc("mlu", "mlu"));
}

TEST(Source_StrideAlign, cpu_decoder_output_cpu) {
  EXPECT_TRUE(TestFunc("cpu", "cpu"));
}

TEST(Source_StrideAlign, cpu_decoder_output_mlu) {
  EXPECT_TRUE(TestFunc("cpu", "mlu"));
}
