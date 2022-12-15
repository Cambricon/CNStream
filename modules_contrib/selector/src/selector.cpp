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

#include "selector.hpp"

#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"

namespace cnstream {

struct SelectorParams {
  size_t window_size;
  std::string strategies;
  std::map<std::string, std::string> strategies_param;
};

struct SelectorContext {
  std::vector<Strategy *> strategies_;
  std::queue<CNFrameInfoPtr> cached_frames_;
};

Selector::Selector(const std::string &name) : ModuleEx(name) {
  param_register_.SetModuleDesc("Selector is a module to select objects. Mark ignored objects.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<SelectorParams>(name));
  std::vector<ModuleParamDesc> register_param = {
      {"window_size", "0", "The frames will be cached.", PARAM_OPTIONAL, OFFSET(SelectorParams, window_size),
       ModuleParamParser<size_t>::Parser, "size_t"},
      {"strategies", "", "The select strategies will be used.", PARAM_REQUIRED, OFFSET(SelectorParams, strategies),
       ModuleParamParser<std::string>::Parser, "string"}};
  param_helper_->Register(register_param, &param_register_);
}

SelectorContext *Selector::GetContext(CNFrameInfoPtr data) {
  auto params = param_helper_->GetParams();
  SelectorContext *ctx = nullptr;
  std::lock_guard<std::mutex> lk(mutex_);
  std::string stream_id = data->stream_id;
  auto search = contexts_.find(stream_id);
  if (search != contexts_.end()) {
    // context exists
    ctx = search->second;
  } else {
    if (data->IsEos()) {
      return nullptr;
    }
    auto frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    if (nullptr == frame) {
      return nullptr;
    }
    ctx = new SelectorContext;
    for (auto &strategy_params : params.strategies_param) {
      auto strategy = Strategy::Create(strategy_params.first);
      if (strategy == nullptr) {
        LOGE(SELECTOR) << "[Selector] Create strategy \"" << strategy_params.first << "\" failed";
        continue;
      }
      std::string config_params =
          strategy_params.second + "; frame_w = " + std::to_string(frame->buf_surf->GetWidth()) +
          "; frame_h = " + std::to_string(frame->buf_surf->GetHeight()) +
          "; window_size = " + std::to_string(params.window_size);
      strategy->Config(config_params);
      ctx->strategies_.push_back(strategy);
    }
    contexts_[stream_id] = ctx;
  }
  return ctx;
}

Selector::~Selector() { Close(); }

bool Selector::Open(ModuleParamSet param_set) {
  auto strategies_parser = [](const ModuleParamSet &param_set, const std::string &param_name, const std::string &value,
                              void *result) -> bool {
    if (!value.empty()) {
      static_cast<std::map<std::string, std::string> *>(result)->insert({param_name, value});
    }
    return true;
  };

  std::vector<ModuleParamDesc> register_param;
  std::vector<std::string> strategies = StringSplitT(param_set["strategies"], ',');
  for (size_t i = 0; i < strategies.size(); ++i) {
    ModuleParamDesc desc = ModuleParamDesc({strategies[i], "", "The select strategies will be used.", PARAM_OPTIONAL,
                                            OFFSET(SelectorParams, strategies_param), strategies_parser,
                                            "std::map<std::string, std::string>"});
    register_param.emplace_back(desc);
  }
  if (!param_helper_->Register(register_param, &param_register_)) {
    LOGE(Selector) << "[" << GetName() << "] register parameters failed.";
    return false;
  }
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(Selector) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }
  auto params = param_helper_->GetParams();
  if (params.strategies_param.empty()) return false;

  return true;
}

void Selector::Close() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (contexts_.empty()) return;
  for (auto &pair : contexts_) {
    auto ctx = pair.second;
    if (ctx) {
      for (auto &strategy : ctx->strategies_) {
        if (strategy) delete strategy;
      }
      ctx->strategies_.clear();
      delete ctx;
    }
  }
  contexts_.clear();
}

int Selector::Process(CNFrameInfoPtr data) {
  if (!data) return -1;

  SelectorContext *ctx = GetContext(data);
  if (!data->IsEos()) {
    if (!ctx) {
      LOGE(SELECTOR) << "Get Selector Context Failed.";
      TransmitData(data);
      return -1;
    }
  } else {
    if (ctx) {
      while (!ctx->cached_frames_.empty()) {
        auto frame = ctx->cached_frames_.front();
        ctx->cached_frames_.pop();
        if (!cnstream::IsStreamRemoved(data->stream_id)) {
          Select(nullptr, frame, ctx);
        }
        TransmitData(frame);
      }
      for (auto &strategy : ctx->strategies_) {
        if (strategy) delete strategy;
      }
      ctx->strategies_.clear();
      std::lock_guard<std::mutex> lk(mutex_);
      delete ctx;
      contexts_.erase(data->stream_id);
    }
    TransmitData(data);
    return 1;
  }

  if (IsStreamRemoved(data->stream_id)) {
    while (!ctx->cached_frames_.empty()) {
      auto frame = ctx->cached_frames_.front();
      ctx->cached_frames_.pop();
      TransmitData(frame);
    }
    TransmitData(data);
    return 1;
  }

  if (!data->collection.HasValue(cnstream::kCNInferObjsTag)) return 0;
  CNFrameInfoPtr provide_frame = nullptr;

  auto params = param_helper_->GetParams();
  if (params.window_size == 0) {
    provide_frame = data;
  } else {
    ctx->cached_frames_.push(data);
    if (ctx->cached_frames_.size() > params.window_size) {
      provide_frame = ctx->cached_frames_.front();
      ctx->cached_frames_.pop();
    }
  }

  Select(data, provide_frame, ctx);

  if (provide_frame) TransmitData(provide_frame);
  return 1;
}

void Selector::Select(CNFrameInfoPtr current, CNFrameInfoPtr provide, SelectorContext *ctx) {
  auto params = param_helper_->GetParams();
  if (current != nullptr) {
    CNInferObjsPtr current_objs = nullptr;
    if (current->collection.HasValue(kCNInferObjsTag)) {
      current_objs = current->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    }
    if (!current_objs) return;
    CNDataFramePtr current_frame = current->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    std::unique_lock<std::mutex> lk(current_objs->mutex_);
    for (auto &obj : current_objs->objs_) {
      bool best_obj = false;
      for (auto &strategy : ctx->strategies_) {
        // LOGE(SELECTOR) << "[Selector] Process object id: " << obj->track_id <<
        //    " in frame: " << current->frame.frame_id;
        bool ret = strategy->Process(obj, current_frame->frame_id);
        if (!best_obj && ret) best_obj = true;
      }
      if (params.window_size == 0 && !best_obj) {
        obj->AddExtraAttribute("SkipObject", "true");
      }
    }
  }

  if (params.window_size > 0 && provide != nullptr) {
    CNInferObjsPtr provide_objs = nullptr;
    if (provide->collection.HasValue(kCNInferObjsTag)) {
      provide_objs = provide->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
    }
    if (!provide_objs) return;
    CNDataFramePtr provide_frame = provide->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
    std::unique_lock<std::mutex> lk(provide_objs->mutex_);
    for (auto &obj : provide_objs->objs_) {
      bool best_obj = false;
      for (auto &strategy : ctx->strategies_) {
        best_obj = strategy->Check(obj, provide_frame->frame_id);
        if (best_obj) break;
      }
      if (!best_obj) {
        obj->AddExtraAttribute("SkipObject", "true");
        // LOGE(SELECTOR) << "[Selector] drop object id: " << obj->track_id <<
        //    " in frame: " << provide->frame.frame_id;
      } else {
        // LOGE(SELECTOR) << "[Selector] retain object id: " << obj->track_id <<
        //    " in frame: " << provide->frame.frame_id;
      }
    }
  }

  for (auto &strategy : ctx->strategies_) {
    strategy->UpdateFrame();
  }
}

}  // namespace cnstream
