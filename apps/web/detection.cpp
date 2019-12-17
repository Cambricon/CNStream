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
#include <algorithm>
#include <condition_variable>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#include "cnstream_core.hpp"
#include "data_source.hpp"
#include "fps_stats.hpp"

DEFINE_bool(rtsp, false, "use rtsp");
DEFINE_bool(loop, false, "display repeat");
DEFINE_string(config_fname, "./dehaze_config.json", "pipeline config filename");

cnstream::FpsStats* gfps_stats = nullptr;

// check pipeline and show the fps
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
      if (gfps_stats) {
        gfps_stats->ShowStatistics();
      } else {
        std::cout << "FpsStats has not been added to pipeline, fps will not be print." << std::endl;
      }
    }
  }
  bool running_ = false;
  std::thread thread_;
  int duration_ = 2000;  // ms
  cnstream::Pipeline* pipeline_ = nullptr;
};  // class Pipeline Watcher

class Detector;

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(cnstream::Pipeline* pipeline, Detector* detector);

  void Update(const cnstream::StreamMsg& smsg) override;

  void WaitForStop();

 private:
  cnstream::Pipeline* pipeline_ = nullptr;
  Detector* detector_ = nullptr;
  bool stop_ = false;
  std::promise<int> wakener_;
};

class Detector {
 public:
  Detector() { pipeline_ = new cnstream::Pipeline("pipeline"); };
  ~Detector() {
    delete msg_observer_;
    delete pipeline_;
  };
  void BuildPipelineByJSONFile(char* config_fname) {
    const std::string config = config_fname;
    try {
      if (0 != pipeline_->BuildPipelineByJSONFile(config)) {
        LOG(ERROR) << "Build pipeline failed.";
        throw "Pipeline error";
      }
    } catch (std::string& e) {
      LOG(ERROR) << e;
      throw e;
    }
    LOG(INFO) << "build pipeline success!";
    if (!pipeline_->Start()) {
      LOG(ERROR) << "Pipeline start failed.";
      throw "Pipeline error";
    }
    LOG(INFO) << "Pipeline start success!";
    cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline_->GetModule("source"));
    if (nullptr == source) {
      LOG(ERROR) << "DataSource module not found.";
      throw "DataSource error!";
    }
    InitWatcherPipeline();
    InitFreeChn();
    msg_observer_ = new MsgObserver(pipeline_, this);
    pipeline_->SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(msg_observer_));
    LOG(INFO) << "Set MsgObserver success!";
  }
  void InitFreeChn(int count = 32) {
    for (int i = 0; i < count; ++i) {
      free_chn_.push_back(i);
    }
  }
  void InitWatcherPipeline() {
    gfps_stats = dynamic_cast<cnstream::FpsStats*>(pipeline_->GetModule("fps_stats"));
    watcher_ = new PipelineWatcher(pipeline_);
    watcher_->Start();
  }
  void PushFreeChn(int chn_idx) {
    std::unique_lock<std::mutex> lck(fc_mutex_);
    free_chn_.insert(free_chn_.begin(), chn_idx);
    fc_cv_.notify_one();
  }
  int PopFreeChn() {
    std::unique_lock<std::mutex> lck(fc_mutex_);
    int chn_idx;
    if (free_chn_.size() == 0) {
      fc_cv_.wait(lck);
    }
    chn_idx = *free_chn_.begin();
    free_chn_.erase(free_chn_.begin());
    return chn_idx;
  }
  int AddImageSource(char* filename) {
    const std::string str = filename;
    int chn_idx = this->PopFreeChn();
    LOG(INFO) << "get chn_idx " << chn_idx;
    cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline_->GetModule("source"));
    source->AddVideoSource(std::to_string(chn_idx), str, FLAGS_loop);
    LOG(INFO) << "add image source success";
    std::unique_lock<std::mutex> lck(ec_mutex_);
    ec_cv_.wait(lck);
    while (true) {
      auto iter = std::find(eos_chn_.begin(), eos_chn_.end(), chn_idx);
      if (iter != eos_chn_.end()) {
        eos_chn_.erase(iter);
        return chn_idx;
      } else {
        ec_cv_.wait(lck);
      }
    }
  }
  cnstream::Pipeline* GetPipeline() { return pipeline_; }
  MsgObserver* GetObserver() { return msg_observer_; }
  void PushEosChn(int eos_chn) {
    std::unique_lock<std::mutex> lck(ec_mutex_);
    eos_chn_.push_back(eos_chn);
    ec_cv_.notify_all();
  }

 private:
  std::mutex fc_mutex_;
  std::condition_variable fc_cv_;
  std::mutex ec_mutex_;
  std::condition_variable ec_cv_;
  std::vector<int> free_chn_;
  std::vector<int> eos_chn_;
  cnstream::Pipeline* pipeline_;
  MsgObserver* msg_observer_;
  PipelineWatcher* watcher_;
};

MsgObserver::MsgObserver(cnstream::Pipeline* pipeline, Detector* detector) : pipeline_(pipeline), detector_(detector) {}

void MsgObserver::Update(const cnstream::StreamMsg& smsg) {
  if (stop_) return;
  if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
    cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline_->GetModule("source"));
    source->RemoveSource(std::to_string(smsg.chn_idx));
    detector_->PushEosChn(smsg.chn_idx);
    LOG(INFO) << "free chn_idx" << smsg.chn_idx;
    detector_->PushFreeChn(smsg.chn_idx);
  } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
    LOG(ERROR) << "[Observer] received ERROR_MSG";
    stop_ = true;
    wakener_.set_value(1);
  }
}

void MsgObserver::WaitForStop() {
  wakener_.get_future().get();
  pipeline_->Stop();
}

extern "C" {
Detector* Detector_new() { return new Detector(); }
void Detector_buildPipelineByJSONFile(Detector* detector, char* config_fname) {
  detector->BuildPipelineByJSONFile(config_fname);
}
int Detector_addImageSource(Detector* detector, char* filename) { return detector->AddImageSource(filename); }
void Detector_waitForStop(Detector* detector) { detector->GetObserver()->WaitForStop(); }
void Detector_immediatelyStop(Detector* detector) { detector->GetPipeline()->Stop(); }
}
