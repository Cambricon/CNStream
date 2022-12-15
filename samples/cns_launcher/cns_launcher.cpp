/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#include <signal.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>  // for memset
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnedk_platform.h"
#include "cnstream_logging.hpp"
#include "cnstream_version.hpp"
#include "data_source.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace_serialize_helper.hpp"
#include "util.hpp"

DEFINE_string(data_path, "", "video file list.");
DEFINE_string(data_name, "", "video file name.");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_int32(codec_id_start, 0, "vdec/venc first id, for CE3226 only");
DEFINE_int32(maximum_width, -1, "maximum width, for variable video resolutions and Jpeg decoding");
DEFINE_int32(maximum_height, -1, "maximum height, for variable video resolutions and Jpeg decoding");
DEFINE_int32(wait_time, 0, "time of one test case");
DEFINE_bool(loop, false, "display repeat");
DEFINE_bool(use_frame_handler, false, "valid when the data directory given is a path not a certain filename");
DEFINE_bool(enable_vin, false, "enable_vin");
DEFINE_bool(enable_vout, false, "enable_vout");
DEFINE_string(config_fname, "", "pipeline config filename");
DEFINE_string(trace_data_dir, "", "dump trace data to specified dir. An empty string means that no data is stored");

/*
  Correspondence between handler and data name or data name in data path:
    1, Sensor:     "/sensor/id=3/type=6/mipi_dev=1/bus_id=0/sns_clk_id=0"
                   Must start with /sensor/, and set all parameters of sensor behind
    2, ImageFrame: "/xxx/xxx"
                   Must be a directory not a certain filename, and set use_data_frame_handler to true
    3, ESJpegMem:  "/xxx/xxx"
                   Must be a directory not a certain filename, and set use_data_frame_handler to false (default false)
    4, ESMem:      "/xxx/xxx.h264" or "/xxx/xxx.h265"
                   Must be a certain filename ends with h264 or h265
    5, File:       "/xxx/xxx", "/xxx/%d.jpg" or "rtsp://xxx"
                   Could be a video or image filename, images sequence or a rtsp url
*/

std::vector<std::future<int>> gFeedMemFutures;

std::atomic<bool> gStopPerfPrint{false};
std::atomic<bool> gForceExit{false};
void SigIntHandler(int signo) {
  if (SIGINT == signo) {
    gForceExit.store(true);
  }
}

// --------------------------------------- Sensor ---------------------------------------
constexpr uint32_t kMaxSensorNum = 8;

// void SetSensorParams(CnedkSensorParams* sensor_params, int sensor_type, int sensor_id) {
//   sensor_params->sensor_type = sensor_type;
//   switch (sensor_id) {
//     case 0: {
//       sensor_params->mipi_dev = 0;
//       sensor_params->bus_id = 4;
//       sensor_params->sns_clk_id = 0;
//       break;
//     }
//     case 1: {
//       sensor_params->mipi_dev = 3;
//       sensor_params->bus_id = 6;
//       sensor_params->sns_clk_id = 1;
//       break;
//     }
//     case 2: {
//       sensor_params->mipi_dev = 4;
//       sensor_params->bus_id = 14;
//       sensor_params->sns_clk_id = 0;
//       break;
//     }
//     case 3: {
//       sensor_params->mipi_dev = 1;
//       sensor_params->bus_id = 0;
//       sensor_params->sns_clk_id = 1;
//       break;
//     }
//     default: {
//       sensor_params->mipi_dev = 0;
//       sensor_params->bus_id = 4;
//       sensor_params->sns_clk_id = 0;
//       break;
//     }
//   }
//   sensor_params->out_width = 1920;
//   sensor_params->out_height = 1080;
//   sensor_params->output_format = 0;  // not used at the moment
// }

void SetSensorParams(CnedkSensorParams* sensor_params, const SensorParam parsed_sensor_param) {
  sensor_params->sensor_type = parsed_sensor_param.type;
  sensor_params->mipi_dev = parsed_sensor_param.mipi_dev;
  sensor_params->bus_id = parsed_sensor_param.bus_id;
  sensor_params->sns_clk_id = parsed_sensor_param.sns_clk_id;
  sensor_params->out_width = 1920;
  sensor_params->out_height = 1080;
  sensor_params->output_format = 0;  // not used at the moment
}
// -------------------------------------- Sensor End ------------------------------------


// -------------------------------------- CnsPipeline -----------------------------------
class CnsPipeline : public cnstream::Pipeline, public cnstream::StreamMsgObserver {
 public:
  explicit CnsPipeline(const std::string pipline_name) : cnstream::Pipeline(pipline_name) {
    SetStreamMsgObserver(this);
  }
  int Init(const std::string &config_filename) {
    if (!BuildPipelineByJSONFile(config_filename)) {
      LOGE(CNS_LAUNCHER) << "Build pipeline failed.";
      return -1;
    }
    // source module name, which is defined in pipeline json config
    // FIXME
    std::string source_name = "source";
    source_ = dynamic_cast<cnstream::DataSource *>(GetModule(source_name));
    if (nullptr == source_) {
      LOGE(CNS_LAUNCHER) << "DataSource module not found.";
      return -1;
    }
    return 0;
  }

  template <typename T>
  int AddSource(const std::string &stream_id, const T &param) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (source_ && !source_->AddSource(cnstream::CreateSource(source_, stream_id, param))) {
      IncreaseStream(stream_id);
      return 0;
    }
    return -1;
  }

  std::shared_ptr<cnstream::SourceHandler> GetSourceHandler(const std::string &stream_id) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (source_) {
      return source_->GetSourceHandler(stream_id);
    }
    return nullptr;
  }

  void Update(const cnstream::StreamMsg &smsg) override {
    std::lock_guard<std::mutex> lg(mutex_);
    if (stop_) return;
    switch (smsg.type) {
      case cnstream::StreamMsgType::EOS_MSG:
        LOGI(CNS_LAUNCHER) << "[" << this->GetName() << "] received EOS message from stream: ["
                           << smsg.stream_id << "]";
        if (stream_set_.find(smsg.stream_id) != stream_set_.end()) {
          if (source_) source_->RemoveSource(smsg.stream_id);
          stream_set_.erase(smsg.stream_id);
        }
        if (stream_set_.empty()) {
          LOGI(CNS_LAUNCHER) << "[" << this->GetName() << "] received all EOS";
          stop_ = true;
        }
        break;

      case cnstream::StreamMsgType::STREAM_ERR_MSG:
        LOGW(CNS_LAUNCHER) << "[" << this->GetName() << "] received stream error from stream: " << smsg.stream_id
                   << ", remove it from pipeline.";
        if (stream_set_.find(smsg.stream_id) != stream_set_.end()) {
          if (source_) source_->RemoveSource(smsg.stream_id, true);
          stream_set_.erase(smsg.stream_id);
        }
        if (stream_set_.empty()) {
          LOGI(CNS_LAUNCHER) << "[" << this->GetName() << "] all streams is removed from pipeline, pipeline will stop.";
          stop_ = true;
        }
        break;

      case cnstream::StreamMsgType::ERROR_MSG:
        if (source_) source_->RemoveSources(true);
        stream_set_.clear();
        stop_ = true;
        break;

      case cnstream::StreamMsgType::FRAME_ERR_MSG:
        LOGW(CNS_LAUNCHER) << "[" << this->GetName() << "] received frame error from stream: " << smsg.stream_id
                   << ", pts: " << smsg.pts << ".";
        break;

      default:
        LOGE(CNS_LAUNCHER) << "[" << this->GetName() << "] unknown message type.";
        break;
    }
    if (stop_) {
      wakener_.notify_one();
    }
  }

  void WaitForStop() {
    while (!gForceExit.load()) {
      std::unique_lock<std::mutex> lk(mutex_);
      if (stream_set_.empty()) {
        stop_ = true;
        gForceExit = true;  // exit when all streams done
      }
      wakener_.wait_for(lk, std::chrono::milliseconds(100), [this]() { return stop_.load() || gForceExit.load(); });
      lk.unlock();
    }
    LOGI(CNS_LAUNCHER) << "WaitForStop(): before pipeline Stop";
    if (!stop_.load()) {
      std::unique_lock<std::mutex> lk(mutex_);
      if (nullptr != source_) {
        source_->RemoveSources(true);
      }
      wakener_.wait_for(lk, std::chrono::seconds(10), [this]() { return stop_.load(); });
    }
    this->Stop();
    CnedkPlatformUninit();
    source_ = nullptr;
  }

  int GetSourceDeviceId() {
    return source_->GetSourceParam().device_id;
  }

 private:
  void IncreaseStream(std::string stream_id) {
    if (stream_set_.find(stream_id) != stream_set_.end()) {
      LOGF(CNS_LAUNCHER) << "IncreaseStream() The stream is ongoing []" << stream_id;
    }
    stream_set_.insert(stream_id);
    if (stop_) stop_ = false;
  }

 private:
  cnstream::DataSource *source_ = nullptr;
  std::atomic<bool> stop_{false};
  std::set<std::string> stream_set_;
  std::condition_variable wakener_;
  mutable std::mutex mutex_;
};
// ------------------------------------ CnsPipeline End ---------------------------------

std::string GetPlatformName(int dev_id) {
  CnedkPlatformInfo platform_info;
  if (CnedkPlatformGetInfo(dev_id, &platform_info) < 0) {
    LOGE(CNS_LAUNCHER) << "GetPlatformName(): Get platform information failed";
    return "";
  }
  return platform_info.name;
}

struct ImageParameter {
  void* data;
  int width;
  int height;
  int stride[3];
  CnedkBufSurfaceColorFormat fmt;
  CnedkBufSurfaceMemType mem_type;
  int dev_id;
};

bool CreateBufSurfaceWithoutData(CnedkBufSurface* buf, CnedkBufSurfaceParams* buf_param,
                                 const ImageParameter param) {
  if (!buf || !buf_param) {
    LOGE(CNS_LAUNCHER) << "CreateBufSurfaceWithoutData(): Create failed. The buf or buf_param is nullptr";
    return false;
  }
  memset(buf, 0, sizeof(CnedkBufSurface));
  memset(buf_param, 0, sizeof(CnedkBufSurfaceParams));

  switch (param.fmt) {
    case CNEDK_BUF_COLOR_FORMAT_NV12:
    case CNEDK_BUF_COLOR_FORMAT_NV21:
      buf_param->plane_params.num_planes = 2;
      for (uint32_t i = 0; i < buf_param->plane_params.num_planes; i++) {
        buf_param->plane_params.width[i] = param.width;
        buf_param->plane_params.height[i] = (i == 0) ? param.height : param.height / 2;
        buf_param->plane_params.bytes_per_pix[i] = 1;
        buf_param->plane_params.pitch[i] = param.stride[i];
        buf_param->plane_params.psize[i] = buf_param->plane_params.pitch[i] * buf_param->plane_params.height[i];
        buf_param->data_size += buf_param->plane_params.psize[i];
      }
      buf_param->plane_params.offset[0] = 0;
      buf_param->plane_params.offset[1] = buf_param->plane_params.psize[0];
      break;
    case CNEDK_BUF_COLOR_FORMAT_RGB:
    case CNEDK_BUF_COLOR_FORMAT_BGR:
      buf_param->plane_params.num_planes = 1;
      buf_param->plane_params.width[0] = param.width;
      buf_param->plane_params.height[0] = param.height;
      buf_param->plane_params.bytes_per_pix[0] = 3;
      buf_param->plane_params.pitch[0] = param.stride[0];
      buf_param->plane_params.psize[0] = buf_param->plane_params.pitch[0] * buf_param->plane_params.height[0];
      buf_param->plane_params.offset[0] = 0;
      buf_param->data_size = buf_param->plane_params.psize[0];
      break;
    case CNEDK_BUF_COLOR_FORMAT_ARGB:
    case CNEDK_BUF_COLOR_FORMAT_BGRA:
      buf_param->plane_params.num_planes = 1;
      buf_param->plane_params.width[0] = param.width;
      buf_param->plane_params.height[0] = param.height;
      buf_param->plane_params.bytes_per_pix[0] = 4;
      buf_param->plane_params.pitch[0] = param.stride[0];
      buf_param->plane_params.psize[0] = buf_param->plane_params.pitch[0] * buf_param->plane_params.height[0];
      buf_param->plane_params.offset[0] = 0;
      buf_param->data_size = buf_param->plane_params.psize[0];
      break;
    default:
      LOGE(CNS_LAUNCHER) << "CreateBufSurfaceWithoutData(): Unsupported color format";
      return false;
  }

  buf_param->pitch = param.stride[0];
  buf_param->width = param.width;
  buf_param->height = param.height;
  buf_param->color_format = param.fmt;
  for (unsigned i = 0; i < buf_param->plane_params.num_planes; i++) {
    buf_param->plane_params.pitch[i] = param.stride[i];
  }
  buf_param->plane_params.offset[0] = 0;
  if (buf_param->plane_params.num_planes > 1) {
    buf_param->plane_params.offset[1] = param.height * param.stride[0];
  }

  buf_param->data_ptr = param.data;

  buf->batch_size = 1;
  buf->num_filled = 1;
  buf->device_id = param.dev_id;
  buf->mem_type = param.mem_type;
  buf->surface_list = buf_param;
  return true;
}

// ------------------------------------ Feed Data Async ---------------------------------
void FeedVideoStreamAsync(std::shared_ptr<cnstream::SourceHandler> handler, std::string filename, bool loop) {
  FILE *fp = fopen(filename.c_str(), "rb");
  if (!fp) {
    LOGE(CNS_LAUNCHER) << "FeedVideoStreamAsync(): Open file failed. file name : " << filename;
    cnstream::ESPacket pkt;
    pkt.data = nullptr;
    Write(handler, &pkt);
    return;
  }

  // Start another thread to read file's binary data into memory and feed it to pipeline.
  gFeedMemFutures.emplace_back(std::async(std::launch::async, [fp, handler, loop]() {
    cnstream::ESPacket pkt;
    pkt.has_pts = false;
    unsigned char buf[4096];
    while (!gForceExit.load()) {
      if (!feof(fp)) {
        int size = fread(buf, 1, 4096, fp);
        pkt.data = buf;
        pkt.size = size;
        if (Write(handler, &pkt) != 0) {
          LOGE(CNS_LAUNCHER) << "FeedVideoStreamAsync(): write failed";
          break;
        }
      } else if (loop) {
        fseek(fp, 0, SEEK_SET);
      } else {
        break;
      }
    }

    if (!feof(fp)) {
      pkt.flags |= static_cast<uint32_t>(cnstream::ESPacket::FLAG::FLAG_EOS);
    }
    pkt.data = nullptr;
    Write(handler, &pkt);
    fclose(fp);

    return 0;
  }));
}

void FeedJpegAsync(std::shared_ptr<cnstream::SourceHandler> handler, std::string filename, bool loop) {
  int index = filename.find_last_of("/");
  std::string dir_path = filename.substr(0, index);
  std::list<std::string> files = GetFileNameFromDir(dir_path, "*.jpg");
  if (files.empty()) {
    LOGE(CNS_LAUNCHER) << "FeedJpegAsync(): there is no jpeg files in directory: " << filename;
    cnstream::ESJpegPacket pkt;
    pkt.data = nullptr;
    pkt.size = 0;
    Write(handler, &pkt);
    return;
  }
  files.sort();

  // Start another thread to read file's binary data into memory and feed it to pipeline.
  gFeedMemFutures.emplace_back(std::async(std::launch::async, [files, handler, loop]() {
    auto iter = files.begin();
    size_t jpeg_buffer_size = GetFileSize(*iter);

    std::unique_ptr<unsigned char[]> buf(new unsigned char[jpeg_buffer_size]);

    cnstream::ESJpegPacket pkt;
    uint64_t pts = 0;
    while (!gForceExit.load() && iter != files.end()) {
      size_t file_size = GetFileSize(*iter);
      if (file_size > jpeg_buffer_size) {
        buf.reset(new unsigned char[file_size]);
        jpeg_buffer_size = file_size;
      }

      std::ifstream file_stream(*iter, std::ios::binary);
      if (!file_stream.is_open()) {
        LOGW(CNS_LAUNCHER) << "FeedJpegAsync(): failed to open " << (*iter);
      } else {
        file_stream.read(reinterpret_cast<char *>(buf.get()), file_size);
        pkt.data = buf.get();
        pkt.size = file_size;
        pkt.pts = pts++;
        if (Write(handler, &pkt) != 0) {
          LOGE(CNS_LAUNCHER) << "FeedJpegAsync(): write failed";
          break;
        }
      }

      if (++iter == files.end() && loop) {
        iter = files.begin();
      }
    }
    pkt.data = nullptr;
    pkt.size = 0;
    Write(handler, &pkt);
    return 0;
  }));
}

void FeedFrameAsync(std::shared_ptr<cnstream::SourceHandler> handler, std::string filename, bool loop, int dev_id) {
  int index = filename.find_last_of("/");
  std::string dir_path = filename.substr(0, index);
  std::list<std::string> files = GetFileNameFromDir(dir_path, "*.jpg");
  if (files.empty()) {
    LOGE(CNS_LAUNCHER) << "FeedFrameAsync(): there is no jpeg files in directory: " << filename;
    cnstream::ImageFrame pkt;
    pkt.data = nullptr;
    Write(handler, &pkt);
    return;
  }
  files.sort();

  std::string platform = GetPlatformName(dev_id);

  // Start another thread to read file's binary data into memory and feed it to pipeline.
  gFeedMemFutures.emplace_back(std::async(std::launch::async, [files, handler, loop, platform, dev_id]() {
    auto iter = files.begin();

    cnstream::ImageFrame pkt;
    uint64_t pts = 0;
    while (!gForceExit.load() && iter != files.end()) {
      cv::Mat bgr_frame = cv::imread(*iter);
      if (bgr_frame.empty()) {
        LOGW(CNS_LAUNCHER) << "FeedFrameAsync(): failed to open " << (*iter);
      } else {
        ImageParameter param;
        param.data = bgr_frame.data;
        param.width = bgr_frame.cols;
        param.height = bgr_frame.rows;
        param.stride[0] = bgr_frame.step;
        param.fmt = CNEDK_BUF_COLOR_FORMAT_BGR;
        param.mem_type = CNEDK_BUF_MEM_SYSTEM;
        param.dev_id = -1;

        CnedkBufSurface cpu_surf;
        CnedkBufSurfaceParams cpu_surf_param;
        if (!CreateBufSurfaceWithoutData(&cpu_surf, &cpu_surf_param, param)) {
          LOGE(CNS_LAUNCHER) << "FeedFrameAsync(): Create cpu BufSurface failed";
          break;
        }


        cnedk::BufSurfWrapperPtr wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(&cpu_surf, false);
        wrapper->SetPts(pts++);

        pkt.data = wrapper;

        if (Write(handler, &pkt) != 0) {
          LOGE(CNS_LAUNCHER) << "FeedFrameAsync(): write failed";
          break;
        }
      }

      if (++iter == files.end() && loop) {
        iter = files.begin();
      }
    }
    pkt.data = nullptr;
    Write(handler, &pkt);
    return 0;
  }));
}
// ---------------------------------- Feed Data Async End -------------------------------

// ------------------------------------------ main --------------------------------------
int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::INFO;
  FLAGS_colorlogtostderr = true;
  signal(SIGINT, SigIntHandler);

  LOGI(CNS_LAUNCHER) << "CNSTREAM VERSION: " << cnstream::VersionString();

  std::list<std::string> video_urls;
  if (FLAGS_data_name != "") {
    video_urls = {FLAGS_data_name};
  } else {
    video_urls = ::ReadFileList(FLAGS_data_path);
  }

  // std::vector<int> sensor_id_vec
  // if (!GetSensorId(video_urls, &sensor_id_vec)) {
  //   LOGE(CNS_LAUNCHER) << "Parse sensor od failed";
  //   return -1;
  // }
  // int sensor_num = sensor_id_vec.size() > kMaxSensorNum ? kMaxSensorNum : sensor_id_vec.size();

  std::vector<SensorParam> parsed_sensor_param;
  if (!GetSensorParam(video_urls, &parsed_sensor_param)) {
    LOGE(CNS_LAUNCHER) << "Parse sensor param failed";
    return -1;
  }
  uint32_t sensor_num = parsed_sensor_param.size() > kMaxSensorNum ? kMaxSensorNum : parsed_sensor_param.size();

  // initialize cnedk
  //
  CnedkSensorParams sensor_params[kMaxSensorNum];
  memset(sensor_params, 0, sizeof(CnedkSensorParams) * kMaxSensorNum);
  CnedkVoutParams vout_params;
  memset(&vout_params, 0, sizeof(CnedkVoutParams));

  CnedkPlatformConfig config;
  memset(&config, 0, sizeof(config));
  if (FLAGS_codec_id_start) {
    config.codec_id_start = FLAGS_codec_id_start;
  }
  if (FLAGS_enable_vout) {
    config.vout_params = &vout_params;
    vout_params.max_input_width = 1920;
    vout_params.max_input_height = 1080;
    vout_params.input_format = 0;  // not used at the moment
  }
  if (FLAGS_enable_vin) {
    config.sensor_num = sensor_num;
    config.sensor_params = sensor_params;
    for (uint32_t i = 0; i < sensor_num; i++) {
      // SetSensorParams(&sensor_params[i], 6, sensor_id_vec[i]);
      SetSensorParams(&sensor_params[i], parsed_sensor_param[i]);
    }
  }
  if (CnedkPlatformInit(&config) < 0) {
    LOGE(CNS_LAUNCHER) << "Init platform failed";
    return -1;
  }

  // build pipeline
  CnsPipeline pipeline("CnsPipeline");
  if (pipeline.Init(FLAGS_config_fname)) {
    LOGE(CNS_LAUNCHER) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }
  // start pipeline
  if (!pipeline.Start()) {
    LOGE(CNS_LAUNCHER) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  std::string platform = GetPlatformName(pipeline.GetSourceDeviceId());

  /*
    start print performance informations
   */
  std::future<void> perf_print_th_ret;
  int trace_data_file_cnt = 0;
  if (pipeline.IsProfilingEnabled()) {
    perf_print_th_ret = std::async(std::launch::async, [&pipeline, &trace_data_file_cnt] {
      cnstream::Time last_time = cnstream::Clock::now();
      int trace_data_dump_times = 0;
      cnstream::TraceSerializeHelper trace_dumper;
      while (!gStopPerfPrint) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
        if (pipeline.IsTracingEnabled()) {
          cnstream::Duration duration(2000);
          ::PrintPipelinePerformance("Last two seconds",
                                     pipeline.GetProfiler()->GetProfileBefore(cnstream::Clock::now(), duration));
          if (!FLAGS_trace_data_dir.empty()) {
            cnstream::Time now_time = cnstream::Clock::now();
            trace_dumper.Serialize(pipeline.GetTracer()->GetTrace(last_time, now_time));
            last_time = now_time;
            if (++trace_data_dump_times == 10) {
              trace_dumper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data_" +
                                  std::to_string(trace_data_file_cnt++));
              trace_dumper.Reset();
              trace_data_dump_times = 0;
            }
          }
        }
      }
      if (pipeline.IsTracingEnabled() && !FLAGS_trace_data_dir.empty() && trace_data_dump_times) {
        trace_dumper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data_" + std::to_string(trace_data_file_cnt++));
        trace_dumper.Reset();
      }
    });
  }

  int max_width = FLAGS_maximum_width;
  int max_height = FLAGS_maximum_height;
  if (platform == "CE3226") {
    max_width = max_width <= 0 ? 1920 : max_width;
    max_height = max_height <= 0 ? 1080 : max_height;
  } else {
    max_width = max_width <= 0 ? 0 : max_width;
    max_height = max_height <= 0 ? 0 : max_height;
  }
  LOGI(CNS_LAUNCHER) << "max_width: " << max_width << ", max_height: " << max_height;

  cnstream::Resolution maximum_resolution;
  maximum_resolution.width = max_width;
  maximum_resolution.height = max_height;
  // For CE3226
  cnstream::Resolution out_resolution;
  out_resolution.width = 1920;
  out_resolution.height = 1080;

  // add stream sources...
  int streams = static_cast<int>(video_urls.size());
  auto url_iter = video_urls.begin();
  uint32_t sensor_idx = 0;
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string &filename = *url_iter;
    std::string stream_id = "stream_" + std::to_string(i);

    if (filename.find("rtsp://") != std::string::npos) {
      cnstream::RtspSourceParam param;
      param.url_name = filename;
      param.use_ffmpeg = false;
      param.reconnect = 10;
      param.max_res = maximum_resolution;
      if (platform == "CE3226") {
        param.out_res = out_resolution;
      }
      pipeline.AddSource<cnstream::RtspSourceParam>(stream_id, param);
    } else if (filename.find("/sensor/") != std::string::npos) {  // FIXME
      if (!FLAGS_enable_vin) continue;
      if (sensor_idx > kMaxSensorNum) {
        LOGW(CNS_LAUNCHER) << "input sensor number is greater than maximum: " << kMaxSensorNum;
        continue;
      }
      cnstream::SensorSourceParam param;
      param.sensor_id = sensor_idx;
      param.out_res = out_resolution;
      pipeline.AddSource<cnstream::SensorSourceParam>(stream_id, param);
      sensor_idx++;
    } else if (FLAGS_use_frame_handler && CheckDir(filename)) {
      cnstream::ImageFrameSourceParam param;
      if (platform == "CE3226") {
        param.out_res = out_resolution;
      }
      pipeline.AddSource<cnstream::ImageFrameSourceParam>(stream_id, param);
      FeedFrameAsync(pipeline.GetSourceHandler(stream_id), filename, FLAGS_loop, pipeline.GetSourceDeviceId());
    } else if (CheckDir(filename)) {
      cnstream::ESJpegMemSourceParam param;
      param.max_res = maximum_resolution;
      if (platform == "CE3226") {
        param.out_res = out_resolution;
      }
      pipeline.AddSource<cnstream::ESJpegMemSourceParam>(stream_id, param);
      FeedJpegAsync(pipeline.GetSourceHandler(stream_id), filename, FLAGS_loop);
    } else if (filename.find(".h264") != std::string::npos || filename.find(".h265") != std::string::npos) {
      cnstream::ESMemSourceParam param;
      param.max_res = maximum_resolution;
      if (platform == "CE3226") {
        param.out_res = out_resolution;
      }
      if (filename.find(".h264") != std::string::npos) {
        param.data_type = cnstream::ESMemSourceParam::DataType::H264;
      } else {
        param.data_type = cnstream::ESMemSourceParam::DataType::H265;
      }
      pipeline.AddSource<cnstream::ESMemSourceParam>(stream_id, param);
      FeedVideoStreamAsync(pipeline.GetSourceHandler(stream_id), filename, FLAGS_loop);
    } else {
      cnstream::FileSourceParam param;
      param.filename = filename;
      param.framerate = FLAGS_src_frame_rate;
      param.loop = FLAGS_loop;
      param.max_res = maximum_resolution;
      if (platform == "CE3226") {
        param.out_res = out_resolution;
      }
      pipeline.AddSource<cnstream::FileSourceParam>(stream_id, param);
    }
  }

  // stop/close pipeline
  {
    if (FLAGS_loop) {
      // stop by hand or by FLAGS_wait_time
      if (FLAGS_wait_time) {
        std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
        LOGI(CNS_LAUNCHER) << "run out time and quit...";
      } else {
        getchar();
        LOGI(CNS_LAUNCHER) << "receive a character from stdin and quit...";
      }
      gForceExit.store(true);
      for (unsigned int i = 0; i < gFeedMemFutures.size(); i++) {
        gFeedMemFutures[i].wait();
      }
      pipeline.WaitForStop();
    } else {
      // stop automatically
      pipeline.WaitForStop();
      gForceExit.store(true);
      for (unsigned int i = 0; i < gFeedMemFutures.size(); i++) {
        gFeedMemFutures[i].wait();
      }
    }
  }
  gFeedMemFutures.clear();

  if (pipeline.IsProfilingEnabled()) {
    gStopPerfPrint = true;
    perf_print_th_ret.get();
    ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
  }

  if (pipeline.IsTracingEnabled() && !FLAGS_trace_data_dir.empty()) {
    LOGI(CNS_LAUNCHER) << "Wait for trace data merge ...";
    cnstream::TraceSerializeHelper helper;
    for (int file_index = 0; file_index < trace_data_file_cnt; ++file_index) {
      std::string filename = FLAGS_trace_data_dir + "/cnstream_trace_data_" + std::to_string(file_index);
      cnstream::TraceSerializeHelper t;
      cnstream::TraceSerializeHelper::DeserializeFromJSONFile(filename, &t);
      helper.Merge(t);
      remove(filename.c_str());
    }
    if (!helper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data.json")) {
      LOGE(CNS_LAUNCHER) << "Dump trace data failed.";
    }
  }
  google::ShutdownGoogleLogging();
  return EXIT_SUCCESS;
}
