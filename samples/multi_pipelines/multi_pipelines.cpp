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

#include <gflags/gflags.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnstream_version.hpp"
#include "data_source.hpp"
#include "util.hpp"

DEFINE_string(data_path, "", "video file list.");
DEFINE_string(data_name, "", "video file name.");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_int32(wait_time, 0, "time of one test case");
DEFINE_bool(loop, false, "display repeat");
DEFINE_string(config_fname, "", "pipeline config filename");
DEFINE_string(config_fname1, "", "another pipeline config filename");

std::atomic<bool> gstop_perf_print{false};

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int stream_cnt, cnstream::Pipeline *pipeline, std::string source_name)
      : stream_cnt_(stream_cnt), pipeline_(pipeline), source_name_(source_name) {}

  void Update(const cnstream::StreamMsg &smsg) override {
    std::lock_guard<std::mutex> lg(mutex_);
    if (stop_) return;
    cnstream::DataSource *source = nullptr;
    switch (smsg.type) {
      case cnstream::StreamMsgType::EOS_MSG:
        eos_stream_.push_back(smsg.stream_id);
        LOGI(APP) << "[Observer] received EOS from stream:" << smsg.stream_id;
        if (static_cast<int>(eos_stream_.size()) == stream_cnt_) {
          LOGI(APP) << "[Observer] received all EOS";
          stop_ = true;
        }
        break;

      case cnstream::StreamMsgType::STREAM_ERR_MSG:
        LOGW(APP) << "[Observer] received stream error from stream: " << smsg.stream_id << ", remove it from pipeline.";
        source = dynamic_cast<cnstream::DataSource *>(pipeline_->GetModule(source_name_));
        if (source) source->RemoveSource(smsg.stream_id);
        stream_cnt_--;
        if (stream_cnt_ == 0) {
          LOGI(APP) << "[Observer] all streams is removed from pipeline, pipeline will stop.";
          stop_ = true;
        }
        break;

      case cnstream::StreamMsgType::ERROR_MSG:
        LOGE(APP) << "[Observer] received ERROR_MSG";
        stop_ = true;
        break;

      case cnstream::StreamMsgType::FRAME_ERR_MSG:
        LOGW(APP) << "[Observer] received frame error from stream: " << smsg.stream_id << ", pts: " << smsg.pts << ".";
        break;

      default:
        LOGE(APP) << "[Observer] unknown message type.";
        break;
    }
    if (stop_) {
      wakener_.notify_one();
    }
  }

  void WaitForStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    stop_ = false;
    wakener_.wait(lk, [this]() { return stop_; });
    pipeline_->Stop();
  }

  void IncreaseStreamCnt() { stream_cnt_++; }
  void DecreaseStreamCnt() { stream_cnt_--; }
  int GetStreamCnt() { return stream_cnt_.load(); }

 private:
  std::atomic<int> stream_cnt_;
  cnstream::Pipeline *pipeline_ = nullptr;
  std::string source_name_;
  bool stop_ = false;
  std::vector<std::string> eos_stream_;
  std::condition_variable wakener_;
  mutable std::mutex mutex_;
};

int AddSourceForRtspStream(cnstream::DataSource *source, const std::string &stream_id, const std::string &filename) {
  auto handler = cnstream::RtspHandler::Create(source, stream_id, filename);
  return source->AddSource(handler);
}

int AddSourceForFile(cnstream::DataSource *source, const std::string &stream_id, const std::string &filename,
                     const int &frame_rate, const bool &loop) {
  auto handler = cnstream::FileHandler::Create(source, stream_id, filename, frame_rate, loop);
  return source->AddSource(handler);
}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  cnstream::InitCNStreamLogging(nullptr);

  LOGI(APP) << "CNSTREAM VERSION:" << cnstream::VersionString();

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
  cnstream::Pipeline pipeline("MyPipeline");

  if (!pipeline.BuildPipelineByJSONFile(FLAGS_config_fname)) {
    LOGE(APP) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  /*
    message observer
   */
  MsgObserver msg_observer(static_cast<int>(video_urls.size()), &pipeline, source_name);
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver *>(&msg_observer));

  /*
    find data source
   */
  cnstream::DataSource *source = dynamic_cast<cnstream::DataSource *>(pipeline.GetModule(source_name));
  if (nullptr == source) {
    LOGE(APP) << "DataSource module not found.";
    return EXIT_FAILURE;
  }

  // second pipeline
  cnstream::Pipeline pipeline1("MyPipeline1");

  // build second pipeline
  if (!pipeline1.BuildPipelineByJSONFile(FLAGS_config_fname1)) {
    LOGE(APP) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  /*
    message observer
   */
  MsgObserver msg_observer1(static_cast<int>(video_urls.size()), &pipeline1, source_name);
  pipeline1.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver *>(&msg_observer1));

  /*
    find data source
   */
  cnstream::DataSource *source1 = dynamic_cast<cnstream::DataSource *>(pipeline1.GetModule(source_name));
  if (nullptr == source1) {
    LOGE(APP) << "DataSource module not found.";
    return EXIT_FAILURE;
  }

  /*
    start pipeline
  */
  if (!pipeline.Start()) {
    LOGE(APP) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  if (!pipeline1.Start()) {
    LOGE(APP) << "Pipeline1 start failed.";
    return EXIT_FAILURE;
  }

  /*
    start print performance infomations of one pipeline
   */
  std::future<void> perf_print_th_ret;
  if (pipeline.IsProfilingEnabled()) {
    perf_print_th_ret = std::async(std::launch::async, [&pipeline] {
      while (!gstop_perf_print) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
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

    int ret = 0;
    int ret1 = 0;
    if (nullptr != source && nullptr != source1) {
      if (filename.find("rtsp://") != std::string::npos) {
        ret = AddSourceForRtspStream(source, stream_id, filename);
        ret1 = AddSourceForRtspStream(source1, stream_id_1, filename);
      } else {
        ret = AddSourceForFile(source, stream_id, filename, FLAGS_src_frame_rate, FLAGS_loop);
        ret1 = AddSourceForFile(source1, stream_id_1, filename, FLAGS_src_frame_rate, FLAGS_loop);
      }
    }

    if (ret != 0) {
      msg_observer.DecreaseStreamCnt();
    }

    if (ret1 != 0) {
      msg_observer1.DecreaseStreamCnt();
    }
  }

  // quit when get all EOS
  auto a1 = std::async(std::launch::async, &MsgObserver::WaitForStop, &msg_observer);
  auto a2 = std::async(std::launch::async, &MsgObserver::WaitForStop, &msg_observer1);
  a1.wait();
  a2.wait();

  cnstream::ShutdownCNStreamLogging();

  if (pipeline.IsProfilingEnabled()) {
    gstop_perf_print = true;
    perf_print_th_ret.get();
    ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
  }
  return EXIT_SUCCESS;
}
