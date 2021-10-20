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



#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

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

#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "device/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "gtest/gtest.h"
#include "cnstream_frame_va.hpp"
#include "rtsp_sink.hpp"
#include "test_base.hpp"

namespace cnstream {
static constexpr const char *gname = "rtsp";
static constexpr int g_dev_id = 0;
static constexpr int g_width = 1280;
static constexpr int g_height = 720;

static int g_channel_id = 0;
static int g_frame_id = 0;

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

bool PullRtspStreamOpencv(int port = 9445) {
  std::string url = "rtsp://" + GetIp() + ":" + std::to_string(port) + "/live";
  cv::VideoCapture capture(url);

  if (!capture.isOpened()) {
    return false;
  }

  cv::Mat frame;

  int i = 3;
  while (i--) {
    if (!capture.read(frame)) {
      return false;
    }
    cv::waitKey(30);
  }
  return true;
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

std::shared_ptr<CNFrameInfo> GenTestData(CNPixelFormat pix_fmt, int width, int height, int frame_rate,
                                         void **frame_data_ptr) {
  size_t nbytes = width * height * sizeof(uint8_t) * 3;
  size_t boundary = 1 << 16;
  nbytes = (nbytes + boundary - 1) & ~(boundary - 1);  // align to 64kb

  // fake data
  void *planes[CN_MAX_PLANES] = {nullptr, nullptr, nullptr};
  edk::MluMemoryOp mem_op;
  *frame_data_ptr = mem_op.AllocMlu(nbytes);
  cnrtMemset(*frame_data_ptr, 0, nbytes);
  void *frame_data = *frame_data_ptr;
  planes[0] = frame_data;                                                                              // 0 plane
  planes[1] = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(frame_data) + width * height);      // 1 plane
  planes[2] = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(frame_data) + 2 * width * height);  // 2 plane

  auto data = cnstream::CNFrameInfo::Create(std::to_string(g_channel_id));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());

  switch (pix_fmt) {
    case CNPixelFormat::BGR24:
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_BGR24;
      break;
    case CNPixelFormat::NV12:
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
      break;
    case CNPixelFormat::NV21:
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      break;
    default:
      frame->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
      break;
  }

  data->SetStreamIndex(g_channel_id);
  frame->frame_id = g_frame_id++;
  data->timestamp = frame->frame_id * 90000 / frame_rate;
  frame->width = width;
  frame->height = height;
  void *ptr_mlu[3] = {planes[0], planes[1], planes[2]};
  frame->stride[0] = frame->stride[1] = frame->stride[2] = width;

  frame->ctx.dev_id = g_dev_id;
  frame->ctx.ddr_channel = g_channel_id;
  frame->ctx.dev_type = DevContext::DevType::MLU;
  frame->dst_device_id = g_dev_id;
  frame->CopyToSyncMem(ptr_mlu, true);
  data->collection.Add(kCNDataFrameTag, frame);
  return data;
}

void Process(std::shared_ptr<Module> ptr, CNPixelFormat pix_fmt, int width, int height, int port, int frame_rate,
             int line) {
  if (g_channel_id > 3) g_channel_id = 0;
  g_frame_id = 0;
  void *frame_data = nullptr;
  edk::MluMemoryOp mem_op;
  auto data = GenTestData(pix_fmt, width, height, frame_rate, &frame_data);

  int ret = ptr->Process(data);
  mem_op.FreeMlu(frame_data);
  auto fut = std::async(std::launch::async, PullRtspStreamFFmpeg, port);
  EXPECT_EQ(ret, 0) << line;

  for (int i = 0; i < 30; ++i) {
    data = GenTestData(pix_fmt, width, height, frame_rate, &frame_data);
    ret = ptr->Process(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / frame_rate));
    mem_op.FreeMlu(frame_data);
    EXPECT_EQ(ret, 0) << line;
  }
  // create eos frame for clearing stream idx
  cnstream::CNFrameInfo::Create(std::to_string(g_channel_id), true);
  g_channel_id++;
  fut.get();
}

void TestAllCase(ModuleParamSet params, int frame_rate, bool tiler, int line) {
  std::shared_ptr<Module> ptr = std::make_shared<RtspSink>(gname);
  EXPECT_TRUE(ptr->Open(params));
  int port = stoi(params["port"]);

  std::vector<CNPixelFormat> pixel_formats = {CNPixelFormat::NV21, CNPixelFormat::NV12};
  if (params.find("input_frame") == params.end() || params["input_frame"] == "cpu") {
    pixel_formats.push_back(CNPixelFormat::BGR24);
  }

  for (auto &format : pixel_formats) {
    Process(ptr, format, g_width, g_height, port, frame_rate, line);
    port = tiler ? -1 : port + 1;
  }
  ASSERT_NO_THROW(ptr->Close());
}

TEST(RTSP, RTSP) {
  ModuleParamSet params;
  int frame_rate = 25;

  params.clear();
  params["port"] = "9554";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "mlu";
  params["device_id"] = "0";
  params["resample"] = "false";
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, false, __LINE__);

  params.clear();
  params["port"] = "9554";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "mlu";
  params["device_id"] = "0";
  params["resample"] = "true";
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, false, __LINE__);

  params.clear();
  params["port"] = "9554";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "mlu";
  params["device_id"] = "0";
  params["view_rows"] = "2";
  params["view_cols"] = "3";
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, true, __LINE__);

  params.clear();
  params["port"] = "9554";
  params["dst_width"] = "1920";
  params["dst_height"] = "1080";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "cpu";
  params["device_id"] = "-1";
  params["resample"] = "false";
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, false, __LINE__);

  params.clear();
  params["port"] = "9554";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "cpu";
  params["device_id"] = "-1";
  params["resample"] = "true";
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, false, __LINE__);

  params.clear();
  params["port"] = "9554";
  params["dst_width"] = "720";
  params["dst_height"] = "480";
  params["view_rows"] = "2";
  params["view_cols"] = "3";
  params["input_frame"] = "cpu";
  params["encoder_type"] = "cpu";
  params["device_id"] = "-1";
  params["frame_rate"] = std::to_string(frame_rate);
  TestAllCase(params, frame_rate, true, __LINE__);

  params.clear();
  params["port"] = "9554";
  params["dst_width"] = "0";
  params["dst_height"] = "0";
  params["input_frame"] = "mlu";
  params["encoder_type"] = "cpu";
  params["device_id"] = "0";
  params["frame_rate"] = std::to_string(frame_rate);

  TestAllCase(params, frame_rate, false, __LINE__);
}

}  // namespace cnstream
