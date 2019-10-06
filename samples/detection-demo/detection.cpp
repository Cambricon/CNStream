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
#include <glog/logging.h>
#include <future>
#include <iostream>
#include <mutex>

#include "cnstream_core.hpp"
#include "data_source.hpp"
#include "encoder.hpp"
#include "inferencer.hpp"
#include "osd.hpp"
#include "track.hpp"
#include "util.hpp"

DEFINE_string(data_path, "", "video file list.");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_int32(wait_time, 0, "time of one test case");
DEFINE_bool(rtsp, false, "use rtsp");
DEFINE_bool(input_image, false, "input image");
DEFINE_bool(loop, false, "display repeat");
DEFINE_string(config_fname, "", "pipeline config filename");

class PipelineWatcher {
 public:
  explicit PipelineWatcher(cnstream::Pipeline* pipeline) : pipeline_(pipeline) {
    LOG_IF(FATAL, pipeline == nullptr) << "pipeline is null.";
  }

  void SetDuration(int ms) { duration_ = ms; }

  void Start() {
    if (thread_.joinable()) {
      running_ = false;
      thread_.join();
    }
    running_ = true;
    thread_ = std::thread(&PipelineWatcher::ThreadFunc, this);
  }

  void Stop() {
    running_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  ~PipelineWatcher() {}

 private:
  void ThreadFunc() {
    while (running_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(duration_));
      std::cout << "\n\n\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
                   "%%%%\n";
      pipeline_->PrintPerformanceInformation();
    }
  }
  bool running_ = false;
  std::thread thread_;
  int duration_ = 2000;  // ms
  cnstream::Pipeline* pipeline_ = nullptr;
};  // class Pipeline Watcher

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int chn_cnt, cnstream::Pipeline* pipeline) : chn_cnt_(chn_cnt), pipeline_(pipeline) {}

  void Update(const cnstream::StreamMsg& smsg) override {
    if (stop_) return;
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      eos_chn_.push_back(smsg.chn_idx);
      if (static_cast<int>(eos_chn_.size()) == chn_cnt_) {
        LOG(INFO) << "[Observer] received all EOS";
        stop_ = true;
        wakener_.set_value(0);
      }
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      LOG(ERROR) << "[Observer] received ERROR_MSG";
      stop_ = true;
      wakener_.set_value(1);
    }
  }

  void WaitForStop() {
    wakener_.get_future().get();
    pipeline_->Stop();
  }

 private:
  const int chn_cnt_ = 0;
  cnstream::Pipeline* pipeline_ = nullptr;
  bool stop_ = false;
  std::vector<int> eos_chn_;
  std::promise<int> wakener_;
};

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  std::cout << "\033[01;31m"
            << "CNSTREAM VERSION:" << cnstream::VersionString() << "\033[0m" << std::endl;

  /*
    flags to variables
  */
  std::list<std::string> video_urls = ::ReadFileList(FLAGS_data_path);

  /*
    build pipeline
  */
  cnstream::Pipeline pipeline("pipeline");
  // pipeline.BuildPipeline({source_config, detector_config, tracker_config});

  try {
    if (0 != pipeline.BuildPipelineByJSONFile(FLAGS_config_fname)) {
      LOG(ERROR) << "Build pipeline failed.";
      return EXIT_FAILURE;
    }
  } catch (std::string& e) {
    LOG(ERROR) << e;
    return EXIT_FAILURE;
  }

  /*
    message observer
   */
  MsgObserver msg_observer(static_cast<int>(video_urls.size()), &pipeline);
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  /*
    find data source
   */
  cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule("source"));
  if (nullptr == source) {
    LOG(ERROR) << "DataSource module not found.";
    return EXIT_FAILURE;
  }

  /*
    start pipeline
  */
  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*
    add stream sources...
  */
  int streams = static_cast<int>(video_urls.size());
  auto url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string& filename = *url_iter;
    if (FLAGS_input_image) {
      source->AddImageSource(std::to_string(i), filename, FLAGS_loop);
    } else {
      source->AddVideoSource(std::to_string(i), filename, FLAGS_src_frame_rate, FLAGS_loop);
    }
  }

  /* watcher, for rolling print */
  PipelineWatcher watcher(&pipeline);
  watcher.Start();

  /*
    close pipeline
  */
  if (FLAGS_loop) {
    /*
      loop, must stop by hand or by FLAGS_wait_time
    */
    if (FLAGS_wait_time) {
      std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
    } else {
      getchar();
    }

    for (int i = 0; i < streams; i++) {
      source->RemoveSource(std::to_string(i));
    }

    pipeline.Stop();
  } else {
    /*
      stop by hand or by FLGAS_wait_time
    */
    if (FLAGS_wait_time) {
      std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
      for (int i = 0; i < streams; i++) {
        source->RemoveSource(std::to_string(i));
      }
      pipeline.Stop();
    } else {
      msg_observer.WaitForStop();
    }
  }

  watcher.Stop();
  std::cout << "\n\n\n\n\n\n";

  pipeline.PrintPerformanceInformation();
  return EXIT_SUCCESS;
}
