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

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnedk_platform.h"
#include "cnstream_frame_va.hpp"
#include "encode.hpp"

#include "test_base.hpp"

namespace cnstream {
static constexpr const char *gname = "encode";
static constexpr int g_channel_id = 0;
static constexpr int g_device_id = 0;
static constexpr const char *img_path = "../../data/images/19.jpg";

TEST(EncodeModule, OpenCloseWithDefaultParameters) {
  VEncode DefaultModule(gname);
  ModuleParamSet params;
  EXPECT_TRUE(DefaultModule.Open(params));
  DefaultModule.Close();
}

TEST(EncodeModule, OpenCloseWithDefinedParameters) {
  VEncode module(gname);
  ModuleParamSet params;

  params["frame_rate"] = "25";
  params["bit_rate"] = "100000";
  params["gop_size"] = "30";
  params["dst_width"] = "1280";
  params["dst_height"] = "720";
  params["hw_accel"] = "true";
  params["device_id"] = "0";
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
  VEncode module(gname);
  ModuleParamSet params;

  params["hw_accel"] = "wrong_type";
  EXPECT_FALSE(module.Open(params));
  params["hw_accel"] = "false";

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

  params["hw_accel"] = "true";
  params["device_id"] = "-1";
  EXPECT_FALSE(module.Open(params));
  params["device_id"] = "0";

  params["view_rows"] = "2";
  params["view_cols"] = "2";
  EXPECT_TRUE(module.Open(params));
  module.Close();

  {
    params["dst_width"] = "121";
    params["dst_height"] = "131";
    EXPECT_FALSE(module.Open(params));
    params.clear();
  }
}


TEST(EncodeModule, ProcessFailedCase) {
  VEncode module(gname);
  ModuleParamSet params;
  EXPECT_TRUE(module.Open(params));
  // data should not be nullptr
  EXPECT_EQ(-1, module.Process(nullptr));
  // eos is the first data processed by module will cause error
  // auto data_eos = CNFrameInfo::Create(std::to_string(0), true);
  // EXPECT_EQ(-1, module.Process(data_eos));
  // invalid width or height of data
  auto data = CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->collection.Add(kCNDataFrameTag, frame);
  EXPECT_EQ(-1, module.Process(data));
  EXPECT_EQ(-1, module.Process(data));
  module.Close();
  params = {{"file_name", "name_without_extension"}};
  EXPECT_TRUE(module.Open(params));
  EXPECT_EQ(-1, module.Process(data));
  module.Close();

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));

  params = {{"hw_accel", "true"}, {"device_id", "0"},
            {"file_name", folder_str + "encode.mp4"}};
  EXPECT_TRUE(module.Open(params));
  EXPECT_EQ(-1, module.Process(data));
  module.Close();
}

std::shared_ptr<CNFrameInfo> CreateFrame(int frame_id, int w, int h, std::string stream_id, bool is_eos = false) {
  // prepare data
  auto data = cnstream::CNFrameInfo::Create(stream_id, is_eos);
  data->SetStreamIndex(0);
  data->timestamp = 2000;

  std::string image_path = GetExePath() + img_path;
  cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);

  cv::resize(img, img, cv::Size(w, h));

  std::shared_ptr<CNDataFrame> frame = GenerateCNDataFrame(img, g_device_id);

  frame->frame_id = frame_id;

  data->collection.Add(kCNDataFrameTag, frame);

  return data;
}

void TestFunc(const ModuleParamSet &params, std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec,
              int frame_num = 1, std::string stream_id = "0", bool resample = false) {
  int frame_id = 0;
  std::shared_ptr<Module> ptr = std::make_shared<VEncode>(gname);
  EXPECT_TRUE(ptr->Open(params)) << " hw_accel: " << params.at("hw_accel")
                                 << ", file_name: " << params.at("file_name")
                                 << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height");

  for (auto &src_wh : src_wh_vec) {
    CnedkBufSurfaceCreateParams create_params;
    create_params.batch_size = 1;
    memset(&create_params, 0, sizeof(create_params));
    create_params.device_id = g_device_id;
    create_params.batch_size = 1;
    create_params.width = src_wh.first;
    create_params.height = src_wh.second;
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV21;
    create_params.mem_type = CNEDK_BUF_MEM_DEVICE;

    bool is_edge_platform = IsEdgePlatform(g_device_id);
    void* pool = nullptr;
    if (is_edge_platform) {
      create_params.mem_type = CNEDK_BUF_MEM_VB_CACHED;
      CnedkBufPoolCreate(&pool, &create_params, 1);
    }

    for (int i = 0; i < frame_num; i++) {
      auto start = std::chrono::steady_clock::now();
      auto data = CreateFrame(frame_id, src_wh.first, src_wh.second, stream_id);
      if (is_edge_platform) {
        CnedkBufSurface* surf;
        CnedkBufSurfaceCreateFromPool(&surf, pool);
        auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
        CnedkBufSurfaceCopy(frame->buf_surf->GetBufSurface(), surf);
        frame->buf_surf = std::make_shared<cnedk::BufSurfaceWrapper>(surf);
      }
      EXPECT_EQ(ptr->Process(data), 0) << " hw_accel: " << params.at("hw_accel")
                                       << ", file_name: " << params.at("file_name")
                                       << ", src_w/h: " << src_wh.first << "/" << src_wh.second
                                       << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height")
                                       << ", process_idx: " << i;
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
    auto data = CreateFrame(frame_id, src_wh.first, src_wh.second, stream_id, true);
    auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
    EXPECT_EQ(ptr->Process(data), 0);  // eos frame
    if (is_edge_platform) {
      if (pool) CnedkBufPoolDestroy(pool);
    }
  }
  ptr->OnEos(stream_id);
  ptr->Close();
  ptr.reset();
}

TEST(EncodeModule, ProcessEncode) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{720, 480}, {1920, 1080}, {352, 288}, {502, 298}};
  CnedkPlatformInfo platform_info;
  CnedkPlatformGetInfo(g_device_id, &platform_info);
  std::string platform_name(platform_info.name);
  std::vector<std::string> file_name_ext_vec;
  if (platform_name.rfind("MLU5", 0) == 0) {
    file_name_ext_vec = {"jpeg"};
  } else {
    file_name_ext_vec = {"h264", "hevc", "h265", "mp4", "mkv", "jpeg"};
  }
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
      for (std::string hw_accel : {"true"}) {
        params["hw_accel"] = hw_accel;

        // use image bgr data or origin mlu data
          params["file_name"] = folder_str + file_name_ext + "_hw_accel_" + hw_accel +
              "_input_" + params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
          LOGI(UNITEST) << "---- file name : " << params.at("file_name");
          TestFunc(params, src_wh_vec, frame_num);
      }
    }
  }
}

TEST(EncodeModule, ProcessEncodeResample) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{720, 480}, {1920, 1080}, {352, 288}, {502, 298}};
  CnedkPlatformInfo platform_info;
  CnedkPlatformGetInfo(g_device_id, &platform_info);
  std::string platform_name(platform_info.name);
  std::vector<std::string> file_name_ext_vec;
  if (platform_name.rfind("MLU5", 0) == 0) {
    file_name_ext_vec = {"jpeg"};
  } else {
    file_name_ext_vec = {"h264", "hevc", "h265", "mp4", "mkv", "jpeg"};
  }
  ModuleParamSet params;
  int frame_num = 10;

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));
  // change bit rate, gop size, test resample
  params["dst_width"] = "0";
  params["dst_height"] = "0";
  params["bit_rate"] = "5000000";
  params["gop_size"] = "40";
  params["resample"] = "true";
  params["frame_rate"] = "30";
  for (auto &file_name_ext : file_name_ext_vec) {
    for (std::string hw_accel : {"true"}) {
      params["hw_accel"] = hw_accel;
      params["file_name"] = folder_str + "resample_bit_rate_5M_gop50_hw_accel_" + hw_accel + "_"+
          params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
      LOGI(UNITEST) << "---- file name : " << params.at("file_name");
      TestFunc(params, src_wh_vec, frame_num, "0", true);
    }
  }
}

TEST(EncodeModule, ProcessEncodeMutiView) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{720, 480}, {1920, 1080}, {352, 288}, {502, 298}};
  CnedkPlatformInfo platform_info;
  CnedkPlatformGetInfo(g_device_id, &platform_info);
  std::string platform_name(platform_info.name);
  std::vector<std::string> file_name_ext_vec;
  if (platform_name.rfind("MLU5", 0) == 0) {
    file_name_ext_vec = {"jpeg"};
  } else {
    file_name_ext_vec = {"h264", "hevc", "h265", "mp4", "mkv", "jpeg"};
  }
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
  params["view_cols"] = "2";
  params["view_rows"] = "2";
  params["frame_rate"] = "30";
  params["dst_width"] = "0";
  params["dst_height"] = "0";
  for (auto &file_name_ext : file_name_ext_vec) {
    for (std::string hw_accel : {"true"}) {
      params["hw_accel"] = hw_accel;
      params["file_name"] = folder_str + "resample_bit_rate_5M_gop50_hw_accel_" + hw_accel + "_"+
          params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
      LOGI(UNITEST) << "---- file name : " << params.at("file_name");
      TestFunc(params, src_wh_vec, frame_num, "0", true);
    }
  }
}


#if 0
TEST(EncodeModule, ProcessCpuEncodeMultiViews) {
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_1 = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_2 = {{352, 288}, {960, 540}, {704, 576}};
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_3 = {{3840, 2160}, {1920, 1080}, {1280, 720}};
  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec_4 = {{1024, 768}, {2560, 1440}, {1920, 1080}};
  std::vector<std::vector<std::pair<uint32_t, uint32_t>>> src_wh_vec =
       {src_wh_vec_1, src_wh_vec_2, src_wh_vec_3, src_wh_vec_4};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{960, 540}, {1920, 1080}, {1280, 720}, {502, 298}};
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
      for (auto &hw_accel : {"true", "false"}) {
        params["hw_accel"] = hw_accel;
        params["file_name"] = folder_str + "multi_hw_accel_" + hw_accel +
            "_encoder_" + params.at("dst_width") + "x" + params.at("dst_height") + "." + file_name_ext;
        LOGI(UNITEST) << "---- file name : " << params.at("file_name");
        TestFuncMultiView(params, src_wh_vec, frame_num);
      }
    }
  }
}
#endif

std::string GetIp() {
  void *tmpAddrPtr = NULL;
  struct ifaddrs *ifAddrStruct = NULL;
  getifaddrs(&ifAddrStruct);

  std::string valid_ip;
  while (ifAddrStruct != NULL) {
    if (ifAddrStruct->ifa_addr->sa_family == AF_INET) {
      tmpAddrPtr = &(reinterpret_cast<struct sockaddr_in *>(ifAddrStruct->ifa_addr))->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
      // LOGI(RTSP_UNITTEST) << "Check out ip4: " << ifAddrStruct->ifa_name << ":" << addressBuffer;
      std::string str = addressBuffer;
      if (str.substr(0, 1) != "0" && str.substr(0, 3) != "127" && str.substr(0, 3) != "172") {
        valid_ip = str;
        break;
      }
    }
    valid_ip = "get invalid ip ...";
    ifAddrStruct = ifAddrStruct->ifa_next;
  }
  LOGI(RTSP_UNITTEST) << "valid_ip: " << valid_ip;
  return valid_ip;
}

bool PullRtspStreamFFmpeg(int port = 9445) {
  if (port == -1) return true;
  AVFormatContext *format_ctx = avformat_alloc_context();
  std::string url = "rtsp://" + GetIp() + ":" + std::to_string(port) + "/live";
  LOGI(RTSP_UNITTEST) << "Pull rtsp stream, url: " << url;
  int ret = -1;
  AVDictionary *opts = nullptr;
  av_dict_set(&opts, "stimeout", "10000", 0);
  ret = avformat_open_input(&format_ctx, url.c_str(), nullptr, &opts);

  if (ret != 0) {
    fprintf(stderr, "fail to open url: %s, return value: %d\n", url.c_str(), ret);
    return -1;
  }

  ret = avformat_find_stream_info(format_ctx, nullptr);
  if (ret < 0) {
    fprintf(stderr, "fail to get stream information: %d\n", ret);
    return -1;
  }

  int video_stream_index = -1;
  fprintf(stdout, "Number of elements in AVFormatContext.streams: %d\n", format_ctx->nb_streams);
  for (uint32_t i = 0; i < format_ctx->nb_streams; ++i) {
    const AVStream *vstream = format_ctx->streams[i];
    CNS_IGNORE_DEPRECATED_PUSH
#if LIBAVFORMAT_VERSION_INT < FFMPEG_VERSION_3_1
    fprintf(stdout, "type of the encoded data: %d\n", vstream->codecpar->codec_id);
    if (vstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index = i;
      fprintf(stdout, "dimensions of the video frame in pixels: width: %d, height: %d, pixel format: %d\n",
              vstream->codecpar->width, vstream->codecpar->height, vstream->codecpar->format);
#else
    fprintf(stdout, "type of the encoded data: %d\n", vstream->codec->codec_id);
    if (vstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index = i;
      fprintf(stdout, "dimensions of the video frame in pixels: width: %d, height: %d\n", vstream->codec->width,
              vstream->codec->height);
#endif
      CNS_IGNORE_DEPRECATED_POP
    }
  }

  if (video_stream_index == -1) {
    fprintf(stderr, "no video stream\n");
    return -1;
  }

  int cnt = 0;
  AVPacket pkt;
  while (1) {
    if (++cnt > 5) break;

    ret = av_read_frame(format_ctx, &pkt);
    if (ret < 0) {
      fprintf(stderr, "error or end of file: %d\n", ret);
      continue;
    }

    if (pkt.stream_index == video_stream_index) {
      fprintf(stdout, "video stream, packet size: %d\n", pkt.size);
    }
    av_packet_unref(&pkt);
  }
  avformat_close_input(&format_ctx);
  avformat_free_context(format_ctx);

  return true;
}

void TestRTSPFunc(const ModuleParamSet &params, std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec,
              int frame_num = 1, std::string stream_id = "0") {
  int frame_id = 0;
  std::shared_ptr<Module> ptr = std::make_shared<VEncode>(gname);
  EXPECT_TRUE(ptr->Open(params)) << " hw_accel: " << params.at("hw_accel")
                                 << ", file_name: " << params.at("file_name")
                                 << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height");

  if (src_wh_vec.empty()) return;

  int port = stoi(params.at("rtsp_port"));

  auto data = CreateFrame(frame_id, src_wh_vec[0].first, src_wh_vec[0].second, stream_id);
  auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
  EXPECT_EQ(ptr->Process(data), 0);
  auto fut = std::async(std::launch::async, PullRtspStreamFFmpeg, port);

  for (auto &src_wh : src_wh_vec) {
    CnedkBufSurfaceCreateParams create_params;
    create_params.batch_size = 1;
    memset(&create_params, 0, sizeof(create_params));
    create_params.device_id = g_device_id;
    create_params.batch_size = 1;
    create_params.width = src_wh.first;
    create_params.height = src_wh.second;
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV21;
    create_params.mem_type = CNEDK_BUF_MEM_DEVICE;

    bool is_edge_platform = IsEdgePlatform(g_device_id);
    void* pool = nullptr;
    if (is_edge_platform) {
      create_params.mem_type = CNEDK_BUF_MEM_VB_CACHED;
      CnedkBufPoolCreate(&pool, &create_params, 1);
    }

    for (int i = 0; i < frame_num; i++) {
      auto data = CreateFrame(frame_id, src_wh.first, src_wh.second, stream_id);
      auto frame = data->collection.Get<std::shared_ptr<CNDataFrame>>(kCNDataFrameTag);
      if (is_edge_platform) {
        CnedkBufSurface* surf;
        CnedkBufSurfaceCreateFromPool(&surf, pool);
        CnedkBufSurfaceCopy(frame->buf_surf->GetBufSurface(), surf);
        frame->buf_surf = std::make_shared<cnedk::BufSurfaceWrapper>(surf);
      }
      EXPECT_EQ(ptr->Process(data), 0) << " hw_accel: " << params.at("hw_accel")
                                       << ", file_name: " << params.at("file_name")
                                       << ", src_w/h: " << src_wh.first << "/" << src_wh.second
                                       << ", dst_w/h: " << params.at("dst_width") << "/" << params.at("dst_height")
                                       << ", process_idx: " << i;
      frame_id++;
    }

    if (is_edge_platform) {
      if (pool) CnedkBufPoolDestroy(pool);
    }
  }
  fut.get();
  ptr->OnEos(stream_id);
  ptr->Close();
}

TEST(EncodeModule, ProcessRtsp) {
  CnedkPlatformInfo platform_info;
  CnedkPlatformGetInfo(g_device_id, &platform_info);
  std::string platform_name(platform_info.name);
  std::vector<std::string> file_name_ext_vec;
  if (platform_name.rfind("MLU5", 0) == 0) {
    return;
  }

  std::vector<std::pair<uint32_t, uint32_t>> src_wh_vec = {{720, 480}, {1200, 720}, {360, 240}};
  std::vector<std::pair<uint32_t, uint32_t>> dst_wh_vec = {{720, 480}, {1920, 1080}, {352, 288}, {502, 298}};
  ModuleParamSet params;
  int frame_num = 100;

  std::string folder_str = "./encode_output/";
  int status = mkdir(folder_str.c_str(), 0777);
  ASSERT_FALSE((status < 0) && (errno != EEXIST));
  // change bit rate, gop size, test resample
  params["dst_width"] = std::to_string(dst_wh_vec[1].first);
  params["dst_height"] = std::to_string(dst_wh_vec[1].second);
  params["bit_rate"] = "5000000";
  params["gop_size"] = "40";
  params["frame_rate"] = "30";
  params["rtsp_port"] = "9510";

  for (std::string hw_accel : {"true", "false"}) {
    params["hw_accel"] = hw_accel;
    TestRTSPFunc(params, src_wh_vec, frame_num, "0");
  }
}

}  // namespace cnstream
