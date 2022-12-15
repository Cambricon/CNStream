/*************************************************************************
 * Copyright (C) [2019-2021] by Cambricon, Inc. All rights reserved
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

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnstream_version.hpp"
#include "data_source.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "profiler/profile.hpp"
#include "profiler/trace_serialize_helper.hpp"
#include "util.hpp"

DEFINE_string(data_path, "", "video file list.");
DEFINE_string(data_name, "", "video file name.");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_int32(wait_time, 0, "time of one test case");
DEFINE_bool(loop, false, "display repeat");
DEFINE_string(config_fname, "", "pipeline config filename");
DEFINE_string(config_fname1, "", "another pipeline config filename");
DEFINE_string(trace_data_dir, "", "dump trace data to specified dir. An empty string means that no data is stored");

std::atomic<bool> gStopPerfPrint{false};
std::atomic<bool> gForceExit{false};
void SigIntHandler(int signo) {
  if (SIGINT == signo) {
    gForceExit.store(true);
  }
}

// -------------------------------------- CnsPipeline -----------------------------------
class CnsPipeline : public cnstream::Pipeline, public cnstream::StreamMsgObserver {
 public:
  explicit CnsPipeline(const std::string pipline_name) : cnstream::Pipeline(pipline_name) {
    SetStreamMsgObserver(this);
  }
  int Init(const std::string &config_filename) {
    if (!BuildPipelineByJSONFile(config_filename)) {
      LOGE(MULTI_PIPELINES) << "Build pipeline failed.";
      return -1;
    }
    // source module name, which is defined in pipeline json config
    // FIXME
    std::string source_name = "source";
    source_ = dynamic_cast<cnstream::DataSource *>(GetModule(source_name));
    if (nullptr == source_) {
      LOGE(MULTI_PIPELINES) << "DataSource module not found.";
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
        LOGI(MULTI_PIPELINES) << "[" << this->GetName() << "] received EOS message from stream: ["
                           << smsg.stream_id << "]";
        if (stream_set_.find(smsg.stream_id) != stream_set_.end()) {
          if (source_) source_->RemoveSource(smsg.stream_id);
          stream_set_.erase(smsg.stream_id);
        }
        if (stream_set_.empty()) {
          LOGI(MULTI_PIPELINES) << "[" << this->GetName() << "] received all EOS";
          stop_ = true;
        }
        break;

      case cnstream::StreamMsgType::STREAM_ERR_MSG:
        LOGW(MULTI_PIPELINES) << "[" << this->GetName() << "] received stream error from stream: " << smsg.stream_id
                   << ", remove it from pipeline.";
        if (stream_set_.find(smsg.stream_id) != stream_set_.end()) {
          if (source_) source_->RemoveSource(smsg.stream_id, true);
          stream_set_.erase(smsg.stream_id);
        }
        if (stream_set_.empty()) {
          LOGI(MULTI_PIPELINES) << "[" << this->GetName()
              << "] all streams is removed from pipeline, pipeline will stop.";
          stop_ = true;
        }
        break;

      case cnstream::StreamMsgType::ERROR_MSG:
        if (source_) source_->RemoveSources(true);
        stream_set_.clear();
        stop_ = true;
        break;

      case cnstream::StreamMsgType::FRAME_ERR_MSG:
        LOGW(MULTI_PIPELINES) << "[" << this->GetName() << "] received frame error from stream: " << smsg.stream_id
                   << ", pts: " << smsg.pts << ".";
        break;

      default:
        LOGE(MULTI_PIPELINES) << "[" << this->GetName() << "] unknown message type.";
        break;
    }
    if (stop_) {
      wakener_.notify_one();
    }
  }

  void WaitForStop() {
    while (1) {
      std::unique_lock<std::mutex> lk(mutex_);
      if (force_exit_ || gForceExit) break;
      if (stream_set_.empty()) {
        stop_ = true;
        force_exit_ = true;  // exit when all streams done
      }
      wakener_.wait_for(lk, std::chrono::milliseconds(100), [this]() {
        return stop_.load() || force_exit_.load() || gForceExit.load();
      });
      lk.unlock();
    }
    LOGI(MULTI_PIPELINES) << "WaitForStop(): before pipeline Stop";
    if (!stop_.load()) {
      std::unique_lock<std::mutex> lk(mutex_);
      if (nullptr != source_) {
        source_->RemoveSources();
      }
      wakener_.wait_for(lk, std::chrono::seconds(10), [this]() { return stop_.load(); });
    }
    this->Stop();
    source_ = nullptr;
  }

  int GetSourceDeviceId() {
    return source_->GetSourceParam().device_id;
  }

  void ForceStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    force_exit_.store(true);
  }

 private:
  void IncreaseStream(std::string stream_id) {
    if (stream_set_.find(stream_id) != stream_set_.end()) {
      LOGF(MULTI_PIPELINES) << "IncreaseStream() The stream is ongoing []" << stream_id;
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
  std::atomic<bool> force_exit_{false};
};
// ------------------------------------ CnsPipeline End ---------------------------------

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::INFO;
  FLAGS_colorlogtostderr = true;
  signal(SIGINT, SigIntHandler);

  LOGI(MULTI_PIPELINES) << "CNSTREAM VERSION:" << cnstream::VersionString();

  /*
    flags to variables
  */
  std::list<std::string> video_urls;
  if (FLAGS_data_name != "") {
    video_urls = {FLAGS_data_name};
  } else {
    video_urls = ::ReadFileList(FLAGS_data_path);
  }

  std::string source_name = "source";  // source module name, which is defined in pipeline json config

  /*
    build pipeline
  */
  CnsPipeline pipeline("MyPipeline");
  if (pipeline.Init(FLAGS_config_fname)) {
    LOGE(MULTI_PIPELINES) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  // second pipeline
  CnsPipeline pipeline_1("MyPipeline_1");
  if (pipeline_1.Init(FLAGS_config_fname1)) {
    LOGE(MULTI_PIPELINES) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  /*
    start pipeline
  */
  if (!pipeline.Start()) {
    LOGE(MULTI_PIPELINES) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  if (!pipeline_1.Start()) {
    LOGE(MULTI_PIPELINES) << "Pipeline1 start failed.";
    pipeline.Stop();
    return EXIT_FAILURE;
  }

  /*
    start print performance infomations of one pipeline
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
  std::future<void> perf_print_th_ret_1;
  int trace_data_file_cnt_1 = 0;
  if (pipeline_1.IsProfilingEnabled()) {
    perf_print_th_ret_1 = std::async(std::launch::async, [&pipeline_1, &trace_data_file_cnt_1] {
      cnstream::Time last_time = cnstream::Clock::now();
      int trace_data_dump_times = 0;
      cnstream::TraceSerializeHelper trace_dumper;
      while (!gStopPerfPrint) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ::PrintPipelinePerformance("Whole", pipeline_1.GetProfiler()->GetProfile());
        if (pipeline_1.IsTracingEnabled()) {
          cnstream::Duration duration(2000);
          ::PrintPipelinePerformance("Last two seconds",
                                     pipeline_1.GetProfiler()->GetProfileBefore(cnstream::Clock::now(), duration));
          if (!FLAGS_trace_data_dir.empty()) {
            cnstream::Time now_time = cnstream::Clock::now();
            trace_dumper.Serialize(pipeline_1.GetTracer()->GetTrace(last_time, now_time));
            last_time = now_time;
            if (++trace_data_dump_times == 10) {
              trace_dumper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data_pipeline1_" +
                                  std::to_string(trace_data_file_cnt_1++));
              trace_dumper.Reset();
              trace_data_dump_times = 0;
            }
          }
        }
      }
      if (pipeline_1.IsTracingEnabled() && !FLAGS_trace_data_dir.empty() && trace_data_dump_times) {
        trace_dumper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data_pipeline1_" +
                            std::to_string(trace_data_file_cnt_1++));
        trace_dumper.Reset();
      }
    });
  }


  /*
    add stream sources...
  */
  int streams = static_cast<int>(video_urls.size());
  auto url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string &filename = *url_iter;
    std::string stream_id = "stream_" + std::to_string(i);
    std::string stream_id_1 = "stream_" + std::to_string(i + streams);

    if (filename.find("rtsp://") != std::string::npos) {
      cnstream::RtspSourceParam param;
      param.url_name = filename;
      param.use_ffmpeg = false;
      param.reconnect = 10;
      pipeline.AddSource<cnstream::RtspSourceParam>(stream_id, param);
      pipeline_1.AddSource<cnstream::RtspSourceParam>(stream_id_1, param);
    } else {
      cnstream::FileSourceParam param;
      param.filename = filename;
      param.framerate = FLAGS_src_frame_rate;
      param.loop = FLAGS_loop;
      pipeline.AddSource<cnstream::FileSourceParam>(stream_id, param);
      pipeline_1.AddSource<cnstream::FileSourceParam>(stream_id_1, param);
    }
  }

    // stop/close pipeline
  {
    if (FLAGS_loop) {
      // stop by hand or by FLAGS_wait_time
      if (FLAGS_wait_time) {
        std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
        LOGI(MULTI_PIPELINES) << "run out time and quit...";
      } else {
        getchar();
        LOGI(MULTI_PIPELINES) << "receive a character from stdin and quit...";
      }
      gForceExit.store(true);
      auto a1 = std::async(std::launch::async, &CnsPipeline::WaitForStop, &pipeline);
      auto a2 = std::async(std::launch::async, &CnsPipeline::WaitForStop, &pipeline_1);
      a1.wait();
      a2.wait();
    } else {
      // stop automatically
      auto a1 = std::async(std::launch::async, &CnsPipeline::WaitForStop, &pipeline);
      auto a2 = std::async(std::launch::async, &CnsPipeline::WaitForStop, &pipeline_1);
      a1.wait();
      a2.wait();
    }
  }

  // google::ShutdownGoogleLogging();

  if (pipeline.IsProfilingEnabled()) {
    gStopPerfPrint = true;
    perf_print_th_ret.get();
    ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
  }
  if (pipeline_1.IsProfilingEnabled()) {
    gStopPerfPrint = true;
    perf_print_th_ret_1.get();
    ::PrintPipelinePerformance("Whole", pipeline_1.GetProfiler()->GetProfile());
  }
  if (pipeline.IsTracingEnabled() && !FLAGS_trace_data_dir.empty()) {
    LOGI(MULTI_PIPELINES) << "Wait for trace data merge ...";
    cnstream::TraceSerializeHelper helper;
    for (int file_index = 0; file_index < trace_data_file_cnt; ++file_index) {
      std::string filename = FLAGS_trace_data_dir + "/cnstream_trace_data_" + std::to_string(file_index);
      cnstream::TraceSerializeHelper t;
      cnstream::TraceSerializeHelper::DeserializeFromJSONFile(filename, &t);
      helper.Merge(t);
      remove(filename.c_str());
    }
    if (!helper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data.json")) {
      LOGE(MULTI_PIPELINES) << "Dump trace data failed.";
    }
  }
  if (pipeline_1.IsTracingEnabled() && !FLAGS_trace_data_dir.empty()) {
    LOGI(MULTI_PIPELINES) << "Wait for trace data merge ...";
    cnstream::TraceSerializeHelper helper;
    for (int file_index = 0; file_index < trace_data_file_cnt_1; ++file_index) {
      std::string filename = FLAGS_trace_data_dir + "/cnstream_trace_data_pipeline1_" + std::to_string(file_index);
      cnstream::TraceSerializeHelper t;
      cnstream::TraceSerializeHelper::DeserializeFromJSONFile(filename, &t);
      helper.Merge(t);
      remove(filename.c_str());
    }
    if (!helper.ToFile(FLAGS_trace_data_dir + "/cnstream_trace_data_pipeline1.json")) {
      LOGE(MULTI_PIPELINES) << "Dump trace data failed.";
    }
  }
  google::ShutdownGoogleLogging();
  return EXIT_SUCCESS;
}
