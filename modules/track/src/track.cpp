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

#include "track.hpp"

namespace cnstream {

Tracker::Tracker(const std::string &name) : Module(name) {}

Tracker::~Tracker() { Close(); }

TrackerContext *Tracker::GetTrackerContext(CNFrameInfoPtr data) {
  TrackerContext *ctx = nullptr;
  auto it = tracker_ctxs_.find(data->channel_idx);
  if (it != tracker_ctxs_.end()) {
    ctx = it->second;
  } else {
    ctx = new TrackerContext;
    tracker_ctxs_[data->channel_idx] = ctx;
  }
  return ctx;
}

bool Tracker::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("model_path") != paramSet.end() && paramSet.find("func_name") != paramSet.end()) {
    std::string model_path = paramSet.find("model_path")->second;
    std::string func_name = paramSet.find("func_name")->second;
    ploader_ = std::make_shared<libstream::ModelLoader>(model_path, func_name);
  }
  return true;
}

void Tracker::Close() {
  for (auto &pair : tracker_ctxs_) {
    if (pair.second->processer_) {
      delete pair.second->processer_;
      pair.second->processer_ = nullptr;
    }
    delete pair.second;
  }
  tracker_ctxs_.clear();
}

int Tracker::Process(std::shared_ptr<CNFrameInfo> data) {
  TrackerContext *ctx = GetTrackerContext(data);
  if (!ctx->processer_) {
    ctx->processer_ = libstream::CnTrack::Create("DeepSortTrack");
    ctx->processer_->SetModel(ploader_);
  }

  std::vector<CnDetectObject> in, out;
  for (size_t i = 0; i < data->objs.size(); i++) {
    CnDetectObject obj;
    obj.label = std::stoi(data->objs[i]->id);
    obj.score = data->objs[i]->score;
    obj.x = data->objs[i]->bbox.x;
    obj.y = data->objs[i]->bbox.y;
    obj.w = data->objs[i]->bbox.w;
    obj.h = data->objs[i]->bbox.h;
    in.push_back(obj);
  }
#ifdef HAVE_OPENCV
  ctx->processer_->UpdateCpuFrame(*data->frame.ImageBGR(), in, &out);
#else
#error OpenCV required
#endif
  data->objs.clear();
  for (size_t i = 0; i < out.size(); i++) {
    std::shared_ptr<CNInferObject> obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(out[i].label);
    obj->track_id = std::to_string(out[i].track_id);
    obj->score = out[i].score;
    obj->bbox.x = out[i].x;
    obj->bbox.y = out[i].y;
    obj->bbox.w = out[i].w;
    obj->bbox.h = out[i].h;
    data->objs.push_back(obj);
  }
  return 0;
}

}  // namespace cnstream
