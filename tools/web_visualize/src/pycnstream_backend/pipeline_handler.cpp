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

#include "pipeline_handler.hpp"

#include <chrono>
#include <memory>
#include <string>

#include "cnstream_frame_va.hpp"
#include "data_source.hpp"

bool PipelineHandler::CreatePipeline(const std::string &config_fname, const std::string perf_dir) {
  if (config_fname.empty()) return false;

  ppipeline_ = new (std::nothrow) cnstream::Pipeline("cns-pipeline");
  if (nullptr == ppipeline_) {
    LOGE(WEBVISUAL) << "pipeline is nullptr.";
    return false;
  }

  if (0 != ppipeline_->BuildPipelineByJSONFile(config_fname)) {
    LOGE(WEBVISUAL) << "Build pipeline by json file failed.";
    delete ppipeline_;
    ppipeline_ = nullptr;
    return false;
  }

  auto source = dynamic_cast<cnstream::DataSource *>(ppipeline_->GetModule("source"));
  if (nullptr == source) {
    LOGE(WEBVISUAL) << "Get source module failed, source module name is 'source' by now.";
    delete ppipeline_;
    ppipeline_ = nullptr;
    return false;
  }

  auto end_module = ppipeline_->GetEndModule();
  if (end_module == nullptr) {
    LOGE(WEBVISUAL) << "Get end module failed, please make sure end module is a converged node.";
    delete ppipeline_;
    ppipeline_ = nullptr;
    return false;
  }

  return true;
}

PipelineHandler::~PipelineHandler() { Stop(); }

void PipelineHandler::SetMsgObserver(cnstream::StreamMsgObserver *msg_observer) {
  if (ppipeline_ && msg_observer) {
    ppipeline_->SetStreamMsgObserver(msg_observer);
  }
}

void PipelineHandler::SetDataObserver(cnstream::IModuleObserver *data_observer) {
  if (ppipeline_ && data_observer) {
    auto end_module = ppipeline_->GetEndModule();
    if (end_module) end_module->SetObserver(data_observer);
  }
}

bool PipelineHandler::Start() {
  if (!ppipeline_) return false;
  if (!ppipeline_->Start()) {
    LOGE(WEBVISUAL) << "pipeline start failed.";
    return false;
  }

  if (ppipeline_->IsProfilingEnabled()) {
    perf_print_th_ret = std::async(std::launch::async, [&] {
      while (!gstop_perf_print) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        PrintPipelinePerformance("Whole", ppipeline_->GetProfiler()->GetProfile());
        if (ppipeline_->IsTracingEnabled()) {
          cnstream::Duration duration(2000);
          PrintPipelinePerformance("Last two seconds",
                                   ppipeline_->GetProfiler()->GetProfileBefore(cnstream::Clock::now(), duration));
        }
      }
    });
  }
  return true;
}

void PipelineHandler::Stop() {
  LOGI(WEBVISUAL) << "stop pipeline.";
  std::lock_guard<std::mutex> lock(stop_mtx_);
  if (ppipeline_) {
    RemoveStream(stream_id_);
    stream_id_ = "";
    ppipeline_->Stop();
    if (ppipeline_->IsProfilingEnabled()) {
      gstop_perf_print = true;
      perf_print_th_ret.get();
      PrintPipelinePerformance("Whole", ppipeline_->GetProfiler()->GetProfile());
    }
    delete ppipeline_;
    ppipeline_ = nullptr;
  }


  LOGI(WEBVISUAL) << "stop pipeline succeed.";
}

bool PipelineHandler::AddStream(const std::string &stream_url, const std::string &stream_id, int fps, bool loop) {
  if (nullptr == ppipeline_ || stream_url.empty() || stream_id.empty()) {
    return false;
  }

  auto source = dynamic_cast<cnstream::DataSource *>(ppipeline_->GetModule("source"));
  if (nullptr == source) return false;

  auto handler = cnstream::FileHandler::Create(source, stream_id, stream_url, fps, loop);
  int ret = source->AddSource(handler);
  if (ret < 0) {
    LOGE(WEBVISUAL) << "add source to pipeline failed.";
    return false;
  }

  return true;
}

bool PipelineHandler::RemoveStream(const std::string &stream_id) {
  if (nullptr == ppipeline_ || stream_id.empty()) {
    return false;
  }

  auto source = dynamic_cast<cnstream::DataSource *>(ppipeline_->GetModule("source"));
  if (nullptr == source) return false;

  source->RemoveSource(stream_id);
  return true;
}
