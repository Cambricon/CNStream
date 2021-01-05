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

#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "common.hpp"
#include "easyinfer/mlu_memory_op.h"
#include "encode.hpp"

namespace cnstream {
static constexpr const char *gname = "encode";
static constexpr int g_channel_id = 0;
static constexpr int g_device_id = 0;

TEST(EncodeModule, OpenClose) {
  Encode module(gname);
  ModuleParamSet params;
  EXPECT_TRUE(module.Open(params));
  module.Close();

  params["frame_rate"] = "25";
  params["kbit_rate"] = "0x100000";
  params["gop_size"] = "30";
  params["dst_width"] = "0";
  params["dst_height"] = "0";
  params["use_ffmpeg"] = "true";   // true or false
  params["encoder_type"] = "mlu";  // cpu or mlu
  params["preproc_type"] = "cpu";  // cpu or mlu
  params["codec_type"] = "h264";   // jpeg h264 hevc
  params["device_id"] = "0";
  EXPECT_TRUE(module.Open(params));
  module.Close();
}

TEST(EncodeModule, OpenCloseFailedCase) {
  Encode module(gname);
  ModuleParamSet params;
  EXPECT_TRUE(module.Open(params));
  EXPECT_FALSE(module.Open(params));
  module.Close();

  params["preproc_type"] = "mlu";
  EXPECT_FALSE(module.Open(params));
  module.Close();
  params.clear();

  params["encoder_type"] = "mlu";
  params["dst_width"] = "1281";
  EXPECT_FALSE(module.Open(params));
  module.Close();

  params["dst_height"] = "721";
  EXPECT_FALSE(module.Open(params));
  module.Close();

  params["device_id"] = "-1";
  EXPECT_FALSE(module.Open(params));
  module.Close();

  // deprecated parameter
  params["dump_dir"] = "";
  EXPECT_FALSE(module.Open(params));
  params.clear();
  module.Close();

  params["dump_type"] = "";
  EXPECT_FALSE(module.Open(params));
  params.clear();
  module.Close();

  params["bit_rate"] = "";
  EXPECT_FALSE(module.Open(params));
  params.clear();
  module.Close();

  params["pre_type"] = "";
  EXPECT_FALSE(module.Open(params));
  params.clear();
  module.Close();

  params["enc_type"] = "";
  EXPECT_FALSE(module.Open(params));
  params.clear();
  module.Close();
}

TEST(EncodeModule, ProcessFailedCase) {
  std::shared_ptr<Module> module_ptr = std::make_shared<Encode>(gname);
  ModuleParamSet params;
  params["output_dir"] = "./encode_output";
  EXPECT_TRUE(module_ptr->Open(params));

  // data should not be nullptr
  EXPECT_EQ(-1, module_ptr->Process(nullptr));

  // eos is the first data processed by module will cause error
  auto data_eos = cnstream::CNFrameInfo::Create(std::to_string(0), true);
  EXPECT_EQ(-1, module_ptr->Process(data_eos));

  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  frame->dst_device_id = g_device_id;

  // unsupported frame format
  frame->fmt = CN_PIXEL_FORMAT_ARGB32;
  data->datas[CNDataFramePtrKey] = frame;
  EXPECT_EQ(-1, module_ptr->Process(data));
  frame->fmt = CN_PIXEL_FORMAT_ABGR32;
  data->datas[CNDataFramePtrKey] = frame;
  EXPECT_EQ(-1, module_ptr->Process(data));
  frame->fmt = CN_PIXEL_FORMAT_RGBA32;
  data->datas[CNDataFramePtrKey] = frame;
  EXPECT_EQ(-1, module_ptr->Process(data));
  frame->fmt = CN_PIXEL_FORMAT_BGRA32;
  data->datas[CNDataFramePtrKey] = frame;
  EXPECT_EQ(-1, module_ptr->Process(data));

  // invalid width or height of data
  frame->fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  frame->width = 0;
  frame->height = 0;
  data->datas[CNDataFramePtrKey] = frame;
  EXPECT_EQ(-1, module_ptr->Process(data));
  module_ptr->Close();

  params["encoder_type"] = "mlu";
  params["device_id"] = "0";
  module_ptr->Open(params);
  EXPECT_EQ(-1, module_ptr->Process(data));
  module_ptr->Close();
  params["use_ffmpeg"] = "true";
  module_ptr->Open(params);
  EXPECT_EQ(-1, module_ptr->Process(data));
  module_ptr->Close();

  // invalid height and width
  frame->width = 1920;
  frame->height = 1080;

  size_t width = frame->width;
  size_t height = frame->height;
  size_t nbytes = ALIGN(width, DEC_ALIGNMENT) * height * 3 / 2;
  edk::MluMemoryOp mem_op;
  void *src = mem_op.AllocMlu(nbytes);
  frame->stride[0] = ALIGN(width, DEC_ALIGNMENT);
  frame->stride[1] = ALIGN(width, DEC_ALIGNMENT);
  frame->ptr_mlu[0] = src;
  frame->ptr_mlu[1] = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(src) + ALIGN(width, DEC_ALIGNMENT) * height);
  frame->ctx.dev_type = DevContext::DevType::MLU;
  frame->ctx.ddr_channel = g_channel_id;
  frame->ctx.dev_id = g_device_id;
  frame->CopyToSyncMem();

  auto data_tmp = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame_tmp(new (std::nothrow) CNDataFrame());
  frame_tmp->fmt = CN_PIXEL_FORMAT_YUV420_NV21;
  frame_tmp->width = 0;
  frame_tmp->height = 0;
  data_tmp->datas[CNDataFramePtrKey] = frame_tmp;

  module_ptr->Open(params);
  EXPECT_EQ(1, module_ptr->Process(data));
  EXPECT_EQ(-1, module_ptr->Process(data_tmp));
  module_ptr->Close();

  mem_op.FreeMlu(src);
}

TEST(EncodeModule, CheckParamSetFailedCase) {
  std::shared_ptr<Module> ptr = std::make_shared<Encode>(gname);
  ModuleParamSet params;
  EXPECT_TRUE(ptr->CheckParamSet(params));

  params["preproc_type"] = "wrong_type";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["use_ffmpeg"] = "wrong_boolean";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["encoder_type"] = "wrong_type";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["codec_type"] = "wrong_type";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["frame_rate"] = "not_digit";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();
  // should set mlu device id
  params["encoder_type"] = "mlu";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params["device_id"] = "wrong_id";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["device_id"] = "0";
  params["preproc_type"] = "mlu";
  params["encoder_type"] = "cpu";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["frame_rate"] = "not_digit";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["kbit_rate"] = "not_digit";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["gop_size"] = "not_digit";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["dst_width"] = "1281";
  params["dst_height"] = "720";
  params["preproc_type"] = "cpu";
  params["encoder_type"] = "mlu";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params["dst_width"] = "1280";
  params["dst_height"] = "721";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params["dst_width"] = "1281";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  // unknown parameter
  params["unknown_param"] = "unknown";
  EXPECT_TRUE(ptr->CheckParamSet(params));
  params.clear();

  // deprecated parameter
  params["dump_dir"] = "";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["dump_type"] = "";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["bit_rate"] = "";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["pre_type"] = "";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();

  params["enc_type"] = "";
  EXPECT_FALSE(ptr->CheckParamSet(params));
  params.clear();
}

void TestFunc(const ModuleParamSet &params, std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec, bool src_bgr,
              int stream_id = 0) {
  int frame_id = 0;
  std::shared_ptr<Module> ptr = std::make_shared<Encode>(gname);
  EXPECT_TRUE(ptr->Open(params)) << " encoder type: " << params.at("encoder_type")
                                 << ", preproc type: " << params.at("preproc_type")
                                 << ", codec type: " << params.at("codec_type")
                                 << ", dst wh: " << params.at("dst_width") << " " << params.at("dst_height");

  for (auto &src_wh : src_wh_vec) {
    size_t nbytes = ALIGN(src_wh.first, DEC_ALIGNMENT) * src_wh.second * 3 / 2;
    edk::MluMemoryOp mem_op;
    void *src = mem_op.AllocMlu(nbytes);
    auto data = cnstream::CNFrameInfo::Create(std::to_string(stream_id));
    std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
    data->SetStreamIndex(stream_id);
    frame->frame_id = frame_id++;
    data->timestamp = frame->frame_id;
    frame->width = src_wh.first;
    frame->height = src_wh.second;
    frame->stride[0] = ALIGN(src_wh.first, DEC_ALIGNMENT);
    frame->stride[1] = ALIGN(src_wh.first, DEC_ALIGNMENT);
    frame->ptr_mlu[0] = src;
    frame->ptr_mlu[1] =
        reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(src) + ALIGN(src_wh.first, DEC_ALIGNMENT) * src_wh.second);
    frame->ctx.dev_type = DevContext::DevType::MLU;
    frame->ctx.ddr_channel = g_channel_id;
    frame->ctx.dev_id = g_device_id;
    frame->fmt = CN_PIXEL_FORMAT_YUV420_NV21;
    frame->dst_device_id = g_device_id;
    frame->CopyToSyncMem();
    if (src_bgr) {
      frame->ImageBGR();
    }
    data->datas[CNDataFramePtrKey] = frame;

    EXPECT_EQ(ptr->Process(data), 1) << " encoder type: " << params.at("encoder_type")
                                     << ", preproc type: " << params.at("preproc_type")
                                     << ", codec type: " << params.at("codec_type") << ", image bgr: " << src_bgr
                                     << ", src wh: " << src_wh.first << " " << src_wh.second
                                     << ", dst wh: " << params.at("dst_width") << " " << params.at("dst_height");

    mem_op.FreeMlu(src);
  }

  ptr->Close();
}

TEST(EncodeModule, ProcessCpuEncode) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh;
  src_wh.push_back({720, 480});
  src_wh.push_back({1200, 720});
  src_wh.push_back({360, 240});
  std::vector<std::pair<std::string, std::string>> dst_wh_vec;
  dst_wh_vec.push_back({"720", "480"});
  dst_wh_vec.push_back({"1920", "1080"});
  dst_wh_vec.push_back({"360", "240"});
  std::vector<std::string> codec_type_vec = {"h264", "hevc", "jpeg"};
  std::vector<std::string> use_ffmpeg_vec = {"true", "false"};
  ModuleParamSet params;
  params["output_dir"] = "./encode_output";
  params["encoder_type"] = "cpu";
  params["preproc_type"] = "cpu";
  params["device_id"] = "-1";

  for (auto &use_ffmpeg : use_ffmpeg_vec) {
    params["use_ffmpeg"] = use_ffmpeg;
    for (auto &codec_type : codec_type_vec) {
      params["codec_type"] = codec_type;
      for (auto &dst_wh : dst_wh_vec) {
        params["dst_width"] = dst_wh.first;
        params["dst_height"] = dst_wh.second;
        TestFunc(params, src_wh, false);
      }
    }
  }
}

TEST(EncodeModule, ProcessMluEncode) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh;
  src_wh.push_back({720, 480});
  src_wh.push_back({1200, 720});
  src_wh.push_back({360, 240});
  std::vector<std::pair<std::string, std::string>> dst_wh_vec;
  dst_wh_vec.push_back({"720", "480"});
  dst_wh_vec.push_back({"1920", "1080"});
  dst_wh_vec.push_back({"360", "240"});
  std::vector<std::string> codec_type_vec = {"h264", "hevc", "jpeg"};
  std::vector<std::string> use_ffmpeg_vec = {"true", "false"};

  ModuleParamSet params;
  params["output_dir"] = "./encode_output";
  params["frame_rate"] = "25";
  params["kbit_rate"] = "0x100000";
  params["gop_size"] = "30";
  params["encoder_type"] = "mlu";
  params["preproc_type"] = "cpu";
  params["device_id"] = std::to_string(g_device_id);

  int stream_id = 0;
  for (auto &use_ffmpeg : use_ffmpeg_vec) {
    params["use_ffmpeg"] = use_ffmpeg;
    for (auto &codec_type : codec_type_vec) {
      params["codec_type"] = codec_type;
      for (auto &dst_wh : dst_wh_vec) {
        params["dst_width"] = dst_wh.first;
        params["dst_height"] = dst_wh.second;
        TestFunc(params, src_wh, false, stream_id);
        TestFunc(params, src_wh, true, stream_id + 1);
        stream_id += 2;
      }
    }
  }
}
}  // namespace cnstream
