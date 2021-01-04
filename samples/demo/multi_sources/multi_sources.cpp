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
#include <gflags/gflags.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_core.hpp"
#include "cnstream_logging.hpp"
#include "data_source.hpp"
#include "displayer.hpp"
#include "util.hpp"

#include "profiler/pipeline_profiler.hpp"
#include "profiler/profile.hpp"

DEFINE_string(data_path, "", "video file list.");
DEFINE_string(data_name, "", "video file name.");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_string(config_fname, "", "pipeline config filename");
DEFINE_bool(loop, false, "display repeat");

cnstream::Displayer* gdisplayer = nullptr;

std::atomic<bool> thread_running{true};

std::atomic<bool> gstop_perf_print {false};

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int stream_cnt, cnstream::Pipeline* pipeline) : stream_cnt_(stream_cnt), pipeline_(pipeline) {
    eos_stream_.clear();
    stream_source_map_.clear();
  }
  void Update(const cnstream::StreamMsg& smsg) override {
    std::lock_guard<std::mutex> lg(mutex_);
    cnstream::DataSource* source = nullptr;
    switch (smsg.type) {
      case cnstream::StreamMsgType::EOS_MSG:
        LOGI(DEMO) << "[Observer] received EOS from stream:" << smsg.stream_id;
        eos_stream_.push_back(smsg.stream_id);
        if (static_cast<int>(eos_stream_.size()) == stream_cnt_) {
          LOGI(DEMO) << "[Observer] received all EOS";
          stop_ = true;
        }
        break;
      case cnstream::StreamMsgType::STREAM_ERR_MSG:
        LOGW(DEMO) << "[Observer] received stream error from stream: " << smsg.stream_id
                   << ", remove it from pipeline.";
        if (stream_source_map_.find(smsg.stream_id) != stream_source_map_.end()) {
          source = dynamic_cast<cnstream::DataSource*>(pipeline_->GetModule(stream_source_map_.at(smsg.stream_id)));
          if (source) source->RemoveSource(smsg.stream_id);
          stream_cnt_--;
          if (static_cast<int>(eos_stream_.size()) == stream_cnt_) {
            LOGI(DEMO) << "[Observer] all streams is removed from pipeline, pipeline will stop.";
            stop_ = true;
          }
        }
        break;
      case cnstream::StreamMsgType::ERROR_MSG:
        LOGE(DEMO) << "[Observer] received ERROR_MSG";
        stop_ = true;
        break;
      case cnstream::StreamMsgType::FRAME_ERR_MSG:
        LOGW(DEMO) << "[Observer] received frame error from stream: " << smsg.stream_id << ", pts: " << smsg.pts << ".";
      default:
        LOGE(DEMO) << "[Observer] unkonw message type.";
        break;
    }

    if (stop_) wakener_.notify_one();
  }

  void AddStreamSourceInfo(std::string stream_id, std::string source_name) {
    stream_source_map_[stream_id] = source_name;
  }
  void RemoveStreamSourceInfo(std::string stream_id) {
    auto it = stream_source_map_.find(stream_id);
    if (it != stream_source_map_.end()) {
      stream_source_map_.erase(it);
    }
  }
  void WaitForStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    stop_ = false;
    wakener_.wait(lk, [this]() { return stop_.load(); });
  }
  void IncreaseStreamCnt() { stream_cnt_++; }
  void DecreaseStreamCnt() { stream_cnt_--; }
  int GetStreamCnt() { return stream_cnt_.load(); }

 private:
  std::atomic<int> stream_cnt_;
  std::vector<std::string> eos_stream_;
  std::atomic<bool> stop_{false};
  std::condition_variable wakener_;
  mutable std::mutex mutex_;
  cnstream::Pipeline* pipeline_ = nullptr;
  std::unordered_map<std::string, std::string> stream_source_map_;
};

int AddSourceForFile(cnstream::DataSource* source, const std::string& stream_id, const std::string& filename,
                     const int& frame_rate, const bool& loop) {
  auto handler = cnstream::FileHandler::Create(source, stream_id, filename, frame_rate, loop);
  return source->AddSource(handler);
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  cnstream::InitCNStreamLogging(nullptr);

  LOGI(DEMO) << "CNSTREAM VERSION:" << cnstream::VersionString();

  /*
    flags to variables
  */
  std::list<std::string> video_urls;
  if (FLAGS_data_name != "") {
    video_urls = {FLAGS_data_name};
  } else {
    video_urls = ::ReadFileList(FLAGS_data_path);
  }

  std::string first_source_name = "source0";  // source module name, which is defined in pipeline json config
  std::string second_source_name = "source1";

  /*
    build pipeline
  */
  cnstream::Pipeline pipeline("MyPipeline");
  // pipeline.BuildPipeline({source_config, detector_config, tracker_config});
  if (0 != pipeline.BuildPipelineByJSONFile(FLAGS_config_fname)) {
    LOGE(DEMO) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  /*
    message observer
   */

  int streams = static_cast<int>(video_urls.size());
  MsgObserver msg_observer(streams * 2, &pipeline);
  /*
    different sources cannot share the same stream_id.
  */
  for (int i = 0; i < streams; i++) {
    std::string source0_stream_id = "stream_" + std::to_string(i);
    std::string source1_stream_id = "stream_" + std::to_string(i + streams);
    msg_observer.AddStreamSourceInfo(std::move(source0_stream_id), first_source_name);
    msg_observer.AddStreamSourceInfo(std::move(source1_stream_id), second_source_name);
  }
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));
  /*
    find data source
   */
  cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule(first_source_name));
  cnstream::DataSource* second_source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule(second_source_name));
  if (nullptr == source || nullptr == second_source) {
    LOGE(DEMO) << "DataSource module not found.";
    return EXIT_FAILURE;
  }

  /*
    start pipeline
  */
  if (!pipeline.Start()) {
    LOGE(DEMO) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*
    start print performance infomations
   */
  std::future<void> perf_print_th_ret;
  if (pipeline.IsProfilingEnabled()) {
    perf_print_th_ret = std::async(std::launch::async, [&pipeline] {
      while (!gstop_perf_print) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
        if (pipeline.IsTracingEnabled()) {
          cnstream::Duration duration(2000);
          ::PrintPipelinePerformance("Last two seconds",
                                     pipeline.GetProfiler()->GetProfileBefore(cnstream::Clock::now(), duration));
        }
      }
    });
  }

  /*
    add stream sources...
  */
  auto url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string& filename = *url_iter;
    std::string stream_id = "stream_" + std::to_string(i);

    int ret = 0;
    if (nullptr != source) {
      ret = AddSourceForFile(source, stream_id, filename, FLAGS_src_frame_rate, FLAGS_loop);
    }

    if (ret != 0) {
      msg_observer.DecreaseStreamCnt();
      msg_observer.RemoveStreamSourceInfo(stream_id);
    }
  }

  url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string& filename = *url_iter;
    std::string stream_id = "stream_" + std::to_string(i + streams);

    int ret = 0;
    if (nullptr != second_source) {
      ret = AddSourceForFile(second_source, stream_id, filename, FLAGS_src_frame_rate, FLAGS_loop);
    }

    if (ret != 0) {
      msg_observer.DecreaseStreamCnt();
      msg_observer.RemoveStreamSourceInfo(stream_id);
    }
  }
  auto quit_callback = [&pipeline, streams, &source, &second_source]() {
    // stop feed-data threads before remove-sources...
    thread_running.store(false);
    for (int i = 0; i < streams; i++) {
      source->RemoveSource("stream_" + std::to_string(i));
      second_source->RemoveSource("stream_" + std::to_string(i + streams));
    }
    pipeline.Stop();
  };

  gdisplayer = dynamic_cast<cnstream::Displayer*>(pipeline.GetModule("displayer"));

  if (gdisplayer && gdisplayer->Show()) {
    gdisplayer->GUILoop(quit_callback);
  } else {
    /*
     * close pipeline
     */
    auto a1 = std::async(std::launch::async, &MsgObserver::WaitForStop, &msg_observer);
    a1.wait();
    thread_running.store(false);
    pipeline.Stop();
  }

  cnstream::ShutdownCNStreamLogging();
  if (pipeline.IsProfilingEnabled()) {
    gstop_perf_print = true;
    perf_print_th_ret.get();
    ::PrintPipelinePerformance("Whole", pipeline.GetProfiler()->GetProfile());
  }
  return EXIT_SUCCESS;
}
