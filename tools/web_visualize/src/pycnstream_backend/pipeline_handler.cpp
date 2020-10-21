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
    LOG(ERROR) << "pipeline is nullptr.";
    return false;
  }

  if (0 != ppipeline_->BuildPipelineByJSONFile(config_fname)) {
    LOG(ERROR) << "Build pipeline by json file failed.";
    delete ppipeline_;
    ppipeline_ = nullptr;
    return false;
  }

  auto source = dynamic_cast<cnstream::DataSource *>(ppipeline_->GetModule("source"));
  if (nullptr == source) {
    LOG(ERROR) << "Get source module failed, source module name is 'source' by now.";
    delete ppipeline_;
    ppipeline_ = nullptr;
    return false;
  }

  auto end_module = ppipeline_->GetEndModule();
  if (end_module == nullptr) {
    LOG(ERROR) << "Get end module failed, please make sure end module is a converged node.";
    delete ppipeline_;
    ppipeline_ = nullptr;
    return false;
  }

  perf_dir_ = perf_dir;
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
  if (!ppipeline_->CreatePerfManager({}, perf_dir_)) {
    LOG(ERROR) << "create perf Manager failed.";
    return false;
  }

  if (!ppipeline_->Start()) {
    LOG(ERROR) << "pipeline start failed.";
    return false;
  }

  return true;
}

void PipelineHandler::Stop() {
  LOG(INFO) << "stop pipeline.";
  std::lock_guard<std::mutex> lock(stop_mtx_);
  if (ppipeline_) {
    RemoveStream(stream_id_);
    stream_id_ = "";
    ppipeline_->Stop();
    delete ppipeline_;
    ppipeline_ = nullptr;
  }

  LOG(INFO) << "stop pipeline succeed.";
}

bool PipelineHandler::AddStream(const std::string &stream_url, const std::string &stream_id, int fps, bool loop) {
  if (nullptr == ppipeline_ || stream_url.empty() || stream_id.empty()) {
    return false;
  }

  auto source = dynamic_cast<cnstream::DataSource *>(ppipeline_->GetModule("source"));
  if (nullptr == source) return false;

  ppipeline_->AddPerfManager(stream_id, perf_dir_);
  auto handler = cnstream::FileHandler::Create(source, stream_id, stream_url, fps, loop);
  int ret = source->AddSource(handler);
  if (ret < 0) {
    LOG(ERROR) << "add source to pipeline failed.";
    ppipeline_->RemovePerfManager(stream_id);
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
  ppipeline_->RemovePerfManager(stream_id);
  return true;
}
