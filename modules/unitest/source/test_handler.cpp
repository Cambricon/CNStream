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

#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnedk_buf_surface_util.hpp"

#include "cnstream_source.hpp"
#include "data_handler_file.hpp"
#include "data_source.hpp"
#include "test_base.hpp"

static constexpr int g_device_id = 0;

namespace cnstream {

static constexpr const char *gname = "source";
static constexpr const char *gmp4_path = "../../modules/unitest/data/img.mp4";
static constexpr const char *img_path = "../../data/images/19.jpg";

// device = 0 represents mlu, device = 1 represents cpu

class SourceHandlerTest : public SourceHandler {
 public:
  explicit SourceHandlerTest(DataSource *module, const std::string &stream_id) : SourceHandler(module, stream_id) {}
  ~SourceHandlerTest() {}

  bool Open() override { return true; }
  void Close() override {}
};

TEST(SourceHandler, Construct) {
  DataSource src(gname);
  auto handler = std::make_shared<SourceHandlerTest>(&src, std::to_string(0));
  EXPECT_TRUE(handler != nullptr);
}

TEST(SourceHandler, GetStreamId) {
  DataSource src(gname);
  auto handler = std::make_shared<SourceHandlerTest>(&src, std::to_string(123));
  ASSERT_TRUE(handler != nullptr);
  EXPECT_TRUE(handler->GetStreamId() == std::to_string(123));
  auto handler_2 = std::make_shared<SourceHandlerTest>(&src, std::to_string(2));
  ASSERT_TRUE(handler_2 != nullptr);
  EXPECT_TRUE(handler_2->GetStreamId() == std::to_string(2));
  auto handler_3 = std::make_shared<SourceHandlerTest>(&src, std::to_string(100));
  ASSERT_TRUE(handler_3 != nullptr);
  EXPECT_TRUE(handler_3->GetStreamId() == std::to_string(100));
}


static std::shared_ptr<SourceHandler> CreateFileHandle(DataSource* src,
                                                       std::string filename,
                                                       std::string stream_id = "0",
                                                       int framerate = 30,
                                                       bool loop = false) {
  Resolution maximum_resolution;
  maximum_resolution.width = 1920;
  maximum_resolution.height = 1080;

  FileSourceParam param;
  param.filename = filename;
  param.framerate = framerate;
  param.loop = loop;
  param.max_res = maximum_resolution;

  auto handle = CreateSource(src, stream_id, param);
  return handle;
}

TEST(DataHandlerFile, OpenClose) {
  DataSource src(gname);
  std::string mp4_path = GetExePath() + gmp4_path;

  // DataSource *psrc = nullptr;
  // auto handler_wrong = FileHandler::Create(psrc, std::to_string(0), mp4_path, 30, false);
  // ASSERT_TRUE(handler_wrong == nullptr);

  ModuleParamSet param;
  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";
  param["interval"] = "1";
  EXPECT_FALSE(src.Open(param));
  auto handler = CreateFileHandle(&src, mp4_path, "0", 30, false);
  EXPECT_TRUE(handler->Open());
  EXPECT_EQ(handler->GetStreamId(), "0");
  handler->Close();

  param["output_type"] = "cpu";
  param["decoder_type"] = "cpu";
  param["device_id"] = "-1";
  src.Open(param);
  EXPECT_TRUE(handler->Open());
  EXPECT_EQ(handler->GetStreamId(), "0");
  handler->Close();
}

TEST(DataHandlerFile, PrepareResources) {
  ModuleParamSet param;
  param["device_id"] = "0";
  param["interval"] = "1";
  param["bufpool_size"] = "1";
  param["device_id"] = "0";
  DataSource src(gname);
  src.Open(param);
  std::string h264_path = GetExePath() + "../../modules/unitest/data/img.h264";
  std::string flv_path = GetExePath() + "../../modules/unitest/data/img.flv";
  std::string mkv_path = GetExePath() + "../../modules/unitest/data/img.mkv";
  std::string mp4_path = GetExePath() + "../../modules/unitest/data/img.mp4";
  std::string h265_path = GetExePath() + "../../modules/unitest/data/265.mp4";
  std::string car_path = GetExePath() + "../../modules/unitest/data/cars_short.mp4";

  // H264
  auto handler = CreateFileHandle(&src, h264_path, "0", 30, false);
  EXPECT_TRUE(handler->Open());
  handler->Close();

  // flv
  handler = CreateFileHandle(&src, flv_path, "0", 30, false);
  EXPECT_TRUE(handler->Open());
  handler->Close();

  // mkv
  handler = CreateFileHandle(&src, mkv_path, "0", 30, false);
  EXPECT_TRUE(handler->Open());
  handler->Close();

  // mp4
  handler = CreateFileHandle(&src, mp4_path, "0", 30, false);
  EXPECT_TRUE(handler->Open());
  handler->Close();

  // H265
  handler = CreateFileHandle(&src, h265_path, "0", 30, false);
  EXPECT_TRUE(handler->Open());
  handler->Close();

  param["output_type"] = "mlu";
  param["decoder_type"] = "mlu";
  param["device_id"] = "0";

  // mlu decoder
  EXPECT_FALSE(src.Open(param));
  EXPECT_TRUE(handler->Open());
  handler->Close();
}

class SourceObserver : public IModuleObserver {
 public:
  int GetCnt() {
    return count;
  }
  void Wait() {
    while (!get_eos) {
      std::chrono::seconds sec(1);
      std::this_thread::sleep_for(sec);
    }
  }
  void Reset() {
    get_eos.store(false);
    count.store(0);
  }

 private:
  void Notify(std::shared_ptr<CNFrameInfo> data) override {
    if (!data->IsEos()) {
      count++;
    } else {
      get_eos = true;
    }
  }
  std::atomic<int> count{0};
  std::atomic<bool> get_eos{false};
};



TEST(DataHandlerFile, ProcessMlu) {
  SourceObserver observer;

  DataSource src(gname);
  src.SetObserver(&observer);
  std::string h264_path = GetExePath() + "../../modules/unitest/data/img.h264";
  // std::string flv_path = GetExePath() + "../../modules/unitest/data/img.flv";
  std::string mkv_path = GetExePath() + "../../modules/unitest/data/img.mkv";
  std::string mp4_path = GetExePath() + "../../modules/unitest/data/img.mp4";
  std::string hevc_path = GetExePath() + "../../modules/unitest/data/img.hevc";
  std::string car_path = GetExePath() + "../../modules/unitest/data/cars_short.mp4";
  std::string wrong_path = "/fake/data/image.h264";
  std::string empty_path = "";

  {  // H264
    auto handler = CreateFileHandle(&src, h264_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 5);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // mkv test
    auto handler = CreateFileHandle(&src, mkv_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 5);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // mp4 test
    auto handler = CreateFileHandle(&src, mp4_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 5);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // hevc test
    auto handler = CreateFileHandle(&src, hevc_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 5);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // car test
    auto handler = CreateFileHandle(&src, car_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 11);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // wrong url test
    auto handler = CreateFileHandle(&src, wrong_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    // observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 0);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // wrong url test
    auto handler = CreateFileHandle(&src, wrong_path, "0", 30, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    // observer.Wait();
    src.Close();
    EXPECT_EQ(observer.GetCnt(), 0);
    observer.Reset();
    src.RemoveSource(handler);
  }
  {  // set loop to true
    auto handler = CreateFileHandle(&src, car_path, "0", 30, true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    src.RemoveSource(handler);
    observer.Wait();
    src.Close();
    observer.Reset();
  }
  {  // empty stream_id
    auto handler = CreateFileHandle(&src, car_path, "", 30, true);
    EXPECT_EQ(handler, nullptr);
  }
  {  // empty handle
    EXPECT_NE(src.AddSource(nullptr), 0);
  }
}

static std::shared_ptr<SourceHandler> CreateRtspHandle(DataSource* src,
                                                       std::string rtsp_url,
                                                       std::string stream_id = "0",
                                                       int framerate = 30,
                                                       bool use_ffmpeg = false,
                                                       bool enable_output_resolution = false) {
  Resolution maximum_resolution;
  maximum_resolution.width = 1920;
  maximum_resolution.height = 1080;

  RtspSourceParam param;
  param.url_name = rtsp_url;
  param.use_ffmpeg = use_ffmpeg;
  param.reconnect = 10;
  param.max_res = maximum_resolution;
  if (enable_output_resolution) {
    param.out_res.width = 1920;
    param.out_res.height = 1080;
  }

  auto handle = CreateSource(src, stream_id, param);
  return handle;
}

TEST(DataHandlerRtsp, ProcessMlu) {
  SourceObserver observer;
  std::string rtsp_url = "rtsp://admin:hello123@10.100.202.30:554/cam/realmonitor?channel=1&subtype=0";
  std::string wrong_url = "rtsp://fakeurl";

  DataSource src(gname);
  src.SetObserver(&observer);
  {  // live555 rtsp
    auto handler = CreateRtspHandle(&src, rtsp_url, "0", 30, false, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    src.RemoveSource(handler);
    observer.Wait();
    handler->Stop();
    handler->Close();
    src.Close();
    observer.Reset();
  }
  {  // ffmpeg rtsp
    auto handler = CreateRtspHandle(&src, rtsp_url, "0", 30, true, false);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    src.RemoveSource(handler);
    handler->Stop();
    handler->Close();
    observer.Wait();
    src.Close();
    observer.Reset();
  }
  {  // set output resolution
    auto handler = CreateRtspHandle(&src, rtsp_url, "0", 30, true, true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    src.RemoveSource(handler);
    observer.Wait();
    src.Close();
    observer.Reset();
  }
  {  // wrong live555 rtsp
    auto handler = CreateRtspHandle(&src, wrong_url, "0", 30, false, false);
    EXPECT_EQ(src.AddSource(handler), 0);

    // std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    // src.RemoveSource(handler);
    // observer.Wait();
    // src.Close();
    observer.Reset();
  }
  {  // wrong ffmpeg rtsp
    auto handler = CreateRtspHandle(&src, wrong_url, "0", 30, true, false);
    EXPECT_NE(src.AddSource(handler), 0);
    // std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    // src.RemoveSource(handler);
    // observer.Wait();
    src.Close();
    observer.Reset();
  }
  {  // empty stream_id
    auto handler = CreateRtspHandle(&src, rtsp_url, "", 30, true, false);
    EXPECT_EQ(handler, nullptr);
  }
  {  // empty handle
    EXPECT_NE(src.AddSource(nullptr), 0);
  }
}

static std::shared_ptr<SourceHandler> CreateESMemHandle(DataSource* src,
                                                        ESMemSourceParam::DataType type,
                                                        std::string stream_id = "0",
                                                        bool enable_output_resolution = false) {
  Resolution maximum_resolution;
  maximum_resolution.width = 1920;
  maximum_resolution.height = 1080;

  ESMemSourceParam param;
  param.max_res = maximum_resolution;
  param.data_type = type;
  if (enable_output_resolution) {
    param.out_res.width = 1920;
    param.out_res.height = 1080;
  }
  auto handle = CreateSource(src, stream_id, param);
  return handle;
}

TEST(DataHandlerEsMem, ProcessMlu) {
  std::string h264_path = GetExePath() + "../../modules/unitest/data/img.h264";
  std::string hevc_path = GetExePath() + "../../modules/unitest/data/img.hevc";

  ModuleParamSet param;
  param["device_id"] = "0";
  param["interval"] = "1";
  param["bufpool_size"] = "1";
  param["device_id"] = "0";

  SourceObserver observer;
  DataSource src(gname);

  src.SetObserver(&observer);
  {  // h264
    src.Open(param);
    auto handler = CreateESMemHandle(&src, ESMemSourceParam::DataType::H264, "0");
    EXPECT_EQ(src.AddSource(handler), 0);
    ESPacket package;
    std::ifstream in_stream(h264_path, std::ifstream::binary);
    in_stream.seekg(0, in_stream.end);
    int length = in_stream.tellg();
    in_stream.seekg(0, in_stream.beg);
    uint8_t* temp_data = new uint8_t[length];
    in_stream.read(reinterpret_cast<char*>(temp_data), length);
    in_stream.close();

    package.data = temp_data;
    package.size = length;
    package.pts = 0;

    EXPECT_EQ(Write(handler, &package), 0);

    memset(&package, 0, sizeof(package));
    package.data = nullptr;
    package.flags = uint32_t(ESPacket::FLAG::FLAG_EOS);
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
    delete[] temp_data;
  }
  {  // h264 enable output resoultion
    param["interval"] = "3";
    src.Open(param);
    auto handler = CreateESMemHandle(&src, ESMemSourceParam::DataType::H264, "0", true);
    EXPECT_EQ(src.AddSource(handler), 0);
    ESPacket package;
    std::ifstream in_stream(h264_path, std::ifstream::binary);
    in_stream.seekg(0, in_stream.end);
    int length = in_stream.tellg();
    in_stream.seekg(0, in_stream.beg);
    uint8_t* temp_data = new uint8_t[length];
    in_stream.read(reinterpret_cast<char*>(temp_data), length);
    in_stream.close();

    package.data = temp_data;
    package.size = length;
    package.pts = 0;

    EXPECT_EQ(Write(handler, &package), 0);

    memset(&package, 0, sizeof(package));
    package.data = nullptr;
    package.flags = uint32_t(ESPacket::FLAG::FLAG_EOS);
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
    delete[] temp_data;
  }
  {  // h265
    auto handler = CreateESMemHandle(&src, ESMemSourceParam::DataType::H265, "0");
    EXPECT_EQ(src.AddSource(handler), 0);
    ESPacket package;
    std::ifstream in_stream(hevc_path, std::ifstream::binary);
    in_stream.seekg(0, in_stream.end);
    int length = in_stream.tellg();
    in_stream.seekg(0, in_stream.beg);

    uint8_t* temp_data = new uint8_t[length];
    in_stream.read(reinterpret_cast<char*>(temp_data), length);
    in_stream.close();

    package.data = temp_data;
    package.size = length;
    package.pts = 0;

    EXPECT_EQ(Write(handler, &package), 0);

    memset(&package, 0, sizeof(package));
    package.data = nullptr;
    package.flags = uint32_t(ESPacket::FLAG::FLAG_EOS);
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
    delete[] temp_data;
  }
  {  // empty package
    auto handler = CreateESMemHandle(&src, ESMemSourceParam::DataType::H264, "0");
    EXPECT_EQ(src.AddSource(handler), 0);
    int size = 0x1000;
    uint8_t* temp_data = new uint8_t[size];
    memset(temp_data, 0, size * sizeof(uint8_t));

    ESPacket package;
    package.data = temp_data;
    package.size = size;
    package.pts = 0;

    EXPECT_EQ(Write(handler, &package), 0);

    memset(&package, 0, sizeof(package));
    package.data = nullptr;
    package.flags = uint32_t(ESPacket::FLAG::FLAG_EOS);
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
    delete[] temp_data;
  }
  {  // wrong package
    auto handler = CreateESMemHandle(&src, ESMemSourceParam::DataType::H265, "0");
    EXPECT_EQ(src.AddSource(handler), 0);
    ESPacket package;
    std::ifstream in_stream(h264_path, std::ifstream::binary);
    in_stream.seekg(0, in_stream.end);
    int length = in_stream.tellg();
    in_stream.seekg(0, in_stream.beg);

    uint8_t* temp_data = new uint8_t[length];
    in_stream.read(reinterpret_cast<char*>(temp_data), length);
    in_stream.close();

    package.data = temp_data;
    package.size = length;
    package.pts = 0;

    EXPECT_NE(Write(handler, &package), 0);

    memset(&package, 0, sizeof(package));
    package.data = nullptr;
    package.flags = uint32_t(ESPacket::FLAG::FLAG_EOS);
    EXPECT_NE(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
    delete[] temp_data;
  }
  {  // empty stream_id
    auto handler = CreateESMemHandle(&src, ESMemSourceParam::DataType::H265, "");
    EXPECT_EQ(handler, nullptr);
  }
  {  // empty handle
    EXPECT_NE(src.AddSource(nullptr), 0);
  }
}

static std::shared_ptr<SourceHandler> CreateESJpegMemHandle(DataSource* src,
                                                            std::string stream_id = "0",
                                                            bool enable_output_resolution = false) {
  Resolution maximum_resolution;
  maximum_resolution.width = 1920;
  maximum_resolution.height = 1080;

  ESJpegMemSourceParam param;
  memset(&param, 0, sizeof(param));
  param.max_res = maximum_resolution;
  if (enable_output_resolution) {
    param.out_res = maximum_resolution;
  }

  auto handle = CreateSource(src, stream_id, param);
  return handle;
}


TEST(DataHandlerEsJpegMem, ProcessMlu) {
  ModuleParamSet param;
  param["device_id"] = "0";
  param["interval"] = "1";
  param["bufpool_size"] = "1";
  param["device_id"] = "0";

  SourceObserver observer;
  DataSource src(gname);
  src.Open(param);
  src.SetObserver(&observer);
  int image_count = 10;

  {
    auto handler = CreateESJpegMemHandle(&src, "0", false);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::string image_path = GetExePath() + img_path;
    for (int i = 0; i < image_count; ++i) {
      ESJpegPacket package;

      std::ifstream in_stream(image_path, std::ifstream::binary);
      in_stream.seekg(0, in_stream.end);
      int length = in_stream.tellg();
      in_stream.seekg(0, in_stream.beg);

      package.data = new uint8_t[length];
      in_stream.read(reinterpret_cast<char*>(package.data), length);
      in_stream.close();

      package.size = length;
      package.pts = 0;
      EXPECT_EQ(Write(handler, &package), 0);
      delete[] package.data;
    }
    ESJpegPacket package;
    package.data = nullptr;
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {  // enable output resolution
    auto handler = CreateESJpegMemHandle(&src, "0", true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::string image_path = GetExePath() + img_path;
    for (int i = 0; i < image_count; ++i) {
      ESJpegPacket package;

      std::ifstream in_stream(image_path, std::ifstream::binary);
      in_stream.seekg(0, in_stream.end);
      int length = in_stream.tellg();
      in_stream.seekg(0, in_stream.beg);

      package.data = new uint8_t[length];
      in_stream.read(reinterpret_cast<char*>(package.data), length);
      in_stream.close();

      package.size = length;
      package.pts = 0;
      EXPECT_EQ(Write(handler, &package), 0);
      delete[] package.data;
    }
    ESJpegPacket package;
    package.data = nullptr;
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);

    src.Close();
    observer.Reset();
  }
  {  // interval
    param["interval"] = "3";
    src.Open(param);
    auto handler = CreateESJpegMemHandle(&src, "0", true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::string image_path = GetExePath() + img_path;
    for (int i = 0; i < image_count; ++i) {
      ESJpegPacket package;

      std::ifstream in_stream(image_path, std::ifstream::binary);
      in_stream.seekg(0, in_stream.end);
      int length = in_stream.tellg();
      in_stream.seekg(0, in_stream.beg);

      package.data = new uint8_t[length];
      in_stream.read(reinterpret_cast<char*>(package.data), length);
      in_stream.close();

      package.size = length;
      package.pts = 0;
      EXPECT_EQ(Write(handler, &package), 0);
      delete[] package.data;
    }
    ESJpegPacket package;
    package.data = nullptr;
    EXPECT_EQ(Write(handler, &package), 0);

    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {  // empty stream_id
    auto handler = CreateESJpegMemHandle(&src, "", true);
    EXPECT_EQ(handler, nullptr);
  }
  {  // empty handle
    EXPECT_NE(src.AddSource(nullptr), 0);
  }
}

static cnedk::BufSurfWrapperPtr GenerateBufsurface(std::string img_path, int device_id,
                          CnedkBufSurfaceColorFormat corlor_format = CNEDK_BUF_COLOR_FORMAT_NV21,
                          CnedkBufSurfaceMemType mem_type = CNEDK_BUF_MEM_DEVICE) {
  std::string image_path = GetExePath() + img_path;
  cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);

  CnedkBufSurfaceCreateParams create_params;
  create_params.batch_size = 1;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = device_id;
  create_params.batch_size = 1;
  create_params.width = img.cols;
  create_params.height = img.rows;
  create_params.color_format = corlor_format;
  create_params.mem_type = mem_type;

  CnedkBufSurface* surf;
  CnedkBufSurfaceCreate(&surf, &create_params);

  if (mem_type == CNEDK_BUF_MEM_DEVICE) {
    CnedkBufSurface* cpu_surf;
    create_params.mem_type = CNEDK_BUF_MEM_SYSTEM;
    CnedkBufSurfaceCreate(&cpu_surf, &create_params);
    CvtBgrToYuv420sp(img, 0, cpu_surf);
    CnedkBufSurfaceCopy(cpu_surf, surf);
    CnedkBufSurfaceDestroy(cpu_surf);
  } else if (mem_type == CNEDK_BUF_MEM_SYSTEM) {
    CvtBgrToYuv420sp(img, 0, surf);
  } else {
    return nullptr;
  }

  return std::make_shared<cnedk::BufSurfaceWrapper>(surf, false);
}

static std::shared_ptr<SourceHandler> CreateImageFrameHandle(DataSource* src,
                                                             std::string stream_id = "0",
                                                             bool enable_output_resolution = false) {
  Resolution resolution;
  resolution.width = 1920;
  resolution.height = 1080;


  ImageFrameSourceParam param;
  memset(&param, 0, sizeof(param));
  if (enable_output_resolution) {
    param.out_res = resolution;
  }

  auto handle = CreateSource(src, stream_id, param);
  return handle;
}

TEST(DataHandlerImageFrame, ProcessMlu) {
  bool is_edge_platform = IsEdgePlatform(g_device_id);
  if (is_edge_platform) return;

  SourceObserver observer;
  DataSource src(gname);
  src.SetObserver(&observer);
  int image_count = 10;
  {
    auto handler = CreateImageFrameHandle(&src, "0", false);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::async(std::launch::async, [image_count, handler]() {
      for (int i = 0; i < image_count; ++i) {
        cnedk::BufSurfWrapperPtr surf = GenerateBufsurface(std::string(img_path), 0);
        ImageFrame frame;
        frame.data = surf;
        EXPECT_EQ(Write(handler, &frame), 0);
      }
      ImageFrame frame;
      frame.data = nullptr;
      EXPECT_EQ(Write(handler, &frame), 0);   // send eos package
    });
    observer.Wait();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {  // send eos only
    auto handler = CreateImageFrameHandle(&src, "0", false);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::async(std::launch::async, [image_count, handler]() {
      ImageFrame frame;
      frame.data = nullptr;
      EXPECT_EQ(Write(handler, &frame), 0);   // send eos package
    });
    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {   // enable output resoultion
    auto handler = CreateImageFrameHandle(&src, "0", true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::async(std::launch::async, [image_count, handler]() {
      for (int i = 0; i < image_count; ++i) {
        cnedk::BufSurfWrapperPtr surf = GenerateBufsurface(std::string(img_path), 0);
        ImageFrame frame;
        frame.data = surf;
        EXPECT_EQ(Write(handler, &frame), 0);
      }
      ImageFrame frame;
      frame.data = nullptr;
      EXPECT_EQ(Write(handler, &frame), 0);   // send eos package
    });
    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {   // cpu data
    auto handler = CreateImageFrameHandle(&src, "0", true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::async(std::launch::async, [image_count, handler]() {
      for (int i = 0; i < image_count; ++i) {
        cnedk::BufSurfWrapperPtr surf = GenerateBufsurface(std::string(img_path), 0,
                                                           CNEDK_BUF_COLOR_FORMAT_NV21,
                                                           CNEDK_BUF_MEM_SYSTEM);
        ImageFrame frame;
        frame.data = surf;
        EXPECT_EQ(Write(handler, &frame), 0);
      }
      ImageFrame frame;
      frame.data = nullptr;
      EXPECT_EQ(Write(handler, &frame), 0);   // send eos package
    });
    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {   // wrong corlor format
    auto handler = CreateImageFrameHandle(&src, "0", true);
    EXPECT_EQ(src.AddSource(handler), 0);
    std::async(std::launch::async, [image_count, handler]() {
      for (int i = 0; i < image_count; ++i) {
        cnedk::BufSurfWrapperPtr surf = GenerateBufsurface(std::string(img_path), 0,
                                                           CNEDK_BUF_COLOR_FORMAT_BGR,
                                                           CNEDK_BUF_MEM_SYSTEM);
        ImageFrame frame;
        frame.data = surf;
        EXPECT_EQ(Write(handler, &frame), 0);
      }
      ImageFrame frame;
      frame.data = nullptr;
      EXPECT_EQ(Write(handler, &frame), 0);   // send eos package
    });
    observer.Wait();
    handler->Stop();
    handler->Close();
    src.RemoveSource(handler);
    src.Close();
    observer.Reset();
  }
  {  // empty stream_id
    auto handler = CreateImageFrameHandle(&src, "", true);
    EXPECT_EQ(handler, nullptr);
  }
  {  // empty handle
    EXPECT_NE(src.AddSource(nullptr), 0);
  }
}

// TEST(DataHandlerFile, ProcessCpu) {
//   DataSource src(gname);
//   std::string mp4_path = GetExePath() + "../../modules/unitest/data/cars_short.mp4";
//   // H264
//   auto handler = FileHandler::Create(&src, std::to_string(0), mp4_path, 30, false);
//   auto file_handler = std::dynamic_pointer_cast<FileHandler>(handler);
//   ModuleParamSet param;
//   param["output_type"] = "cpu";
//   param["decoder_type"] = "cpu";

//   EXPECT_TRUE(src.Open(param));
//   EXPECT_TRUE(file_handler->Open());
//   file_handler->Close();

//   EXPECT_TRUE(file_handler->impl_->PrepareResources());
//   // cars.mp4 has 11 frames
//   for (uint32_t i = 0; i < 11; i++) {
//     EXPECT_TRUE(file_handler->impl_->Process());
//   }
//   // loop is set to false, send eos and return false
//   EXPECT_FALSE(file_handler->impl_->Process());

//   file_handler->impl_->ClearResources();
// }

}  // namespace cnstream
