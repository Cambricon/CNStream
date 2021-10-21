/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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
#include <sys/stat.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "easyinfer/mlu_memory_op.h"
#include "encode.hpp"

namespace cnstream {
static constexpr const char *gname = "encode";
static constexpr int g_channel_id = 0;
static constexpr int g_device_id = 0;

TEST(EncodeModule, OpenCloseWithDefaultParameters) {
  Encode DefaultModule(gname);
  ModuleParamSet params;
  EXPECT_TRUE(DefaultModule.Open(params));
  DefaultModule.Close();
}

TEST(EncodeModule, OpenCloseWithDefinedParameters) {
  Encode module(gname);
  ModuleParamSet params;

  params["frame_rate"] = "25";
  params["bit_rate"] = "100000";
  params["gop_size"] = "30";
  params["dst_width"] = "1280";
  params["dst_height"] = "720";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "mlu";
  params["device_id"] = "1";
  params["view_rows"] = "2";
  params["view_cols"] = "2";
  params["resample"] = "true";
  EXPECT_TRUE(module.Open(params));
  module.Close();

  params["invalid_param"] = "abc";
  EXPECT_FALSE(module.Open(params));
  module.Close();
}

TEST(EncodeModule, OpenFailed) {
  Encode module(gname);
  ModuleParamSet params;

  params["input_frame"] = "wrong_type";
  EXPECT_FALSE(module.Open(params));
  params["input_frame"] = "cpu";

  params["encoder_type"] = "wrong_type";
  EXPECT_FALSE(module.Open(params));
  params["encoder_type"] = "cpu";

  std::vector<std::string> digit_params_vec = {
      "device_id", "dst_width", "dst_height", "frame_rate", "bit_rate", "view_cols", "view_rows"};
  for (auto& param_name : digit_params_vec) {
    params[param_name] = "not_digit";
    EXPECT_FALSE(module.Open(params));
    params.erase(param_name);
  }

  params["resample"] = "not_bool";
  EXPECT_FALSE(module.Open(params));
  params["resample"] = "2";
  EXPECT_FALSE(module.Open(params));
  params.erase("resample");

  params["encoder_type"] = "mlu";
  params["device_id"] = "-1";
  EXPECT_FALSE(module.Open(params));
  params["device_id"] = "0";

  params["input_frame"] = "mlu";
  params["view_rows"] = "2";
  params["view_cols"] = "2";
  EXPECT_FALSE(module.Open(params));
  module.Close();
}


TEST(EncodeModule, ProcessFailedCase) {
  Encode module(gname);
  ModuleParamSet params;
  EXPECT_TRUE(module.Open(params));
  // data should not be nullptr
  EXPECT_EQ(-1, module.Process(nullptr));
  // eos is the first data processed by module will cause error
  auto data_eos = CNFrameInfo::Create(std::to_string(0), true);
  EXPECT_EQ(-1, module.Process(data_eos));
  // invalid width or height of data
  auto data = CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->collection.Add(kCNDataFrameTag, frame);
  frame->dst_device_id = g_device_id;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  frame->width = 0;
  frame->height = 0;
  EXPECT_EQ(-1, module.Process(data));
  frame->width = 1;
  frame->height = 1;
  EXPECT_EQ(-1, module.Process(data));
  module.Close();

  params = {{"file_name", "name_without_extension"}};
  EXPECT_TRUE(module.Open(params));
  frame->width = 1920;
  frame->height = 1080;
  EXPECT_EQ(-1, module.Process(data));
  module.Close();

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));

  frame->dst_device_id = 1;
  params = {{"input_frame", "mlu"}, {"encoder_type", "mlu"}, {"device_id", "0"},
            {"file_name", folder_str + "encode.mp4"}};
  EXPECT_TRUE(module.Open(params));
  EXPECT_EQ(-1, module.Process(data));
  module.Close();
}

std::shared_ptr<CNFrameInfo> CreateFrame(int frame_id, int w, int h, std::string stream_id, void** src_ptr) {
  size_t nbytes = ROUND_UP(w, 16) * h * 3 / 2;
  edk::MluMemoryOp mem_op;
  *src_ptr = mem_op.AllocMlu(nbytes);
  auto data = CNFrameInfo::Create(stream_id);
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  frame->frame_id = frame_id;
  data->timestamp = INVALID_TIMESTAMP;
  frame->width = w;
  frame->height = h;
  frame->stride[0] = ROUND_UP(w, 16);
  frame->stride[1] = ROUND_UP(w, 16);
  void *ptr_mlu[2] =
      {*src_ptr, reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(*src_ptr) + ROUND_UP(w, 16) * h)};
  frame->ctx.dev_type = DevContext::DevType::MLU;
  frame->ctx.ddr_channel = g_channel_id;
  frame->ctx.dev_id = g_device_id;
  frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
  frame->dst_device_id = g_device_id;
  frame->CopyToSyncMem(ptr_mlu, true);
  data->collection.Add(kCNDataFrameTag, frame);
  return data;
}

void TestFunc(const ModuleParamSet &params, std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec,
              int frame_num = 1, std::string stream_id = "0", bool resample = false) {
  int frame_id = 0;
  edk::MluMemoryOp mem_op;
  std::shared_ptr<Module> ptr = std::make_shared<Encode>(gname);
  EXPECT_TRUE(ptr->Open(params)) << " encoder_type: " << params.at("encoder_type")
                                 << ", file_name: " << params.at("file_name")
                                 << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height");

  for (auto &src_wh : src_wh_vec) {
    for (int i = 0; i < frame_num; i++) {
      auto start = std::chrono::steady_clock::now();
      void* src = nullptr;
      auto data = CreateFrame(frame_id, src_wh.first, src_wh.second, stream_id, &src);
      auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
      EXPECT_EQ(ptr->Process(data), 0) << " encoder type: " << params.at("encoder_type")
                                       << ", file_name: " << params.at("file_name")
                                       << ", src_w/h: " << src_wh.first << "/" << src_wh.second
                                       << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height")
                                       << ", process_idx: " << i;
      mem_op.FreeMlu(src);
      frame_id++;
      if (resample) {
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> diff = end - start;
        int fr = 30;
        if (params.find("frame_rate") != params.end()) {
          fr = std::stoi(params.at("frame_rate"));
        }
        double delay = 1000.0 / fr;
        int remain = delay - diff.count();
        if (remain > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(remain));
        }
      }
    }
  }
  ptr->OnEos(stream_id);
  ptr->Close();
}

TEST(EncodeModule, ProcessEncode) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{720, 480}, {1920, 1080}, {352, 288}, {501, 299}};
  std::vector<std::string> file_name_ext_vec = {"h264", "hevc", "h265", "mp4", "mkv", "jpeg"};
  ModuleParamSet params;
  int frame_num = 10;

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));

  // file extensions
  for (auto &file_name_ext : file_name_ext_vec) {
    // dst resolutions
    for (auto &dst_wh : dst_wh_vec) {
      params["dst_width"] = std::to_string(dst_wh.first);
      params["dst_height"] = std::to_string(dst_wh.second);
      // encoder type
      for (std::string encoder_type : {"cpu", "mlu"}) {
        params["encoder_type"] = encoder_type;
        // use image bgr data or origin mlu data
        for (std::string input_frame : {"cpu", "mlu"}) {
#ifndef HAVE_CNCV
          if (input_frame == "mlu" && encoder_type == "mlu") continue;
#endif
          params["input_frame"] = input_frame;
          params["file_name"] = folder_str + file_name_ext + "_" + encoder_type + "_encoder_" + input_frame +
              "_input_" + params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
          std::cout << "---- file name : " << params.at("file_name") << std::endl;
          TestFunc(params, src_wh_vec, frame_num);
        }
      }
    }
  }
}

TEST(EncodeModule, ProcessEncodeResample) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{720, 480}, {1920, 1080}, {352, 288}, {501, 299}};
  std::vector<std::string> file_name_ext_vec = {"h264", "hevc", "h265", "mp4", "mkv", "jpeg"};
  ModuleParamSet params;
  int frame_num = 10;

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));
  // change bit rate, gop size, test resample
  params["dst_width"] = std::to_string(dst_wh_vec[1].first);
  params["dst_height"] = std::to_string(dst_wh_vec[1].second);
  params["bit_rate"] = "5000000";
  params["gop_size"] = "40";
  params["resample"] = "true";
  params["frame_rate"] = "30";
  for (auto &file_name_ext : file_name_ext_vec) {
    for (std::string encoder_type : {"cpu", "mlu"}) {
      params["encoder_type"] = encoder_type;
      params["input_frame"] = "cpu";
      params["file_name"] = folder_str + "resample_bit_rate_5M_gop50_" + encoder_type + "_encoder_cpu_input" +
          params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
      std::cout << "---- file name : " << params.at("file_name") << std::endl;
      TestFunc(params, src_wh_vec, frame_num, "0", true);
    }
  }
}

void TestFuncMultiView(const ModuleParamSet &params,
                       std::vector<std::vector<std::pair<uint32_t, uint32_t>>> src_wh_vec,
                       int frame_num = 1) {
  std::shared_ptr<Module> ptr = std::make_shared<Encode>(gname);
  EXPECT_TRUE(ptr->Open(params)) << " encoder_type: " << params.at("encoder_type")
                                 << ", file_name: " << params.at("file_name")
                                 << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height");

  uint32_t stream_num = src_wh_vec.size();
  ASSERT_NE(stream_num, 0);
  uint32_t data_num = src_wh_vec[0].size();
  for (auto &src_wh : src_wh_vec) {
    ASSERT_TRUE(data_num == src_wh.size());
  }
  int frame_id = 0;
  edk::MluMemoryOp mem_op;
  for (uint32_t data_idx = 0; data_idx < data_num; data_idx++) {
    for (int i = 0; i < frame_num; i++) {
      for (uint32_t stream_id = 0 ; stream_id < stream_num; stream_id++) {
        void* src = nullptr;
        int width = src_wh_vec[stream_id][data_idx].first;
        int height = src_wh_vec[stream_id][data_idx].second;
        auto data = CreateFrame(frame_id, width, height, std::to_string(stream_id), &src);
        auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
        EXPECT_EQ(ptr->Process(data), 0) << " encoder type: " << params.at("encoder_type")
                                       << ", file_name: " << params.at("file_name")
                                       << ", src_w/h: " << width << "/" << height
                                       << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height")
                                       << ", process_idx: " << i;
        mem_op.FreeMlu(src);
        frame_id++;
      }
    }
  }
  for (uint32_t stream_id = 0 ; stream_id < stream_num; stream_id++) {
    ptr->OnEos(std::to_string(stream_id));
  }
  ptr->Close();
}

TEST(EncodeModule, ProcessCpuEncodeMultiViews) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_1 = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_2 = {{352, 288}, {960, 540}, {704, 576}};
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_3 = {{3840, 2160}, {1920, 1080}, {1280, 720}};
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_4 = {{1024, 768}, {2560, 1440}, {1920, 1080}};
  std::vector<std::vector<std::pair<uint32_t, uint32_t>>> src_wh_vec =
       {src_wh_vec_1, src_wh_vec_2, src_wh_vec_3, src_wh_vec_4};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{960, 540}, {1920, 1080}, {1280, 720}, {501, 299}};
  std::vector<std::string> file_name_ext_vec = {"h264", "hevc", "mp4", "jpeg"};
  ModuleParamSet params;
  int frame_num = 10;

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));

  params["view_cols"] = "2";
  params["view_rows"] = "3";

  for (auto &file_name_ext : file_name_ext_vec) {
    for (auto &dst_wh : dst_wh_vec) {
      params["dst_width"] = std::to_string(dst_wh.first);
      params["dst_height"] = std::to_string(dst_wh.second);
      for (auto &encoder_type : {"cpu", "mlu"}) {
        params["encoder_type"] = encoder_type;
        params["file_name"] = folder_str + "multi_" + encoder_type +
            "_encoder_" + params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
        std::cout << "---- file name : " << params.at("file_name") << std::endl;
        TestFuncMultiView(params, src_wh_vec, frame_num);
      }
    }
  }
}

}  // namespace cnstream
