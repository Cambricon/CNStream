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

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "cnstream_frame_va.hpp"
#include "postproc.hpp"
#include "cnstream_logging.hpp"

class PostprocClassification : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocClassification, cnstream::Postproc)
};  // classd PostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(PostprocClassification, cnstream::Postproc)

int PostprocClassification::Execute(const std::vector<float*>& net_outputs,
                                    const std::shared_ptr<edk::ModelLoader>& model,
                                    const cnstream::CNFrameInfoPtr& package) {
  if (net_outputs.size() != 1) {
    LOGE(DEMO) << "[Warning] classification neuron network only has one output,"
                  " but get " +
                      std::to_string(net_outputs.size());
    return -1;
  }

  auto data = net_outputs[0];
  auto len = model->OutputShape(0).DataCount();
  auto pscore = data;

  float mscore = 0;
  int label = 0;
  for (decltype(len) i = 0; i < len; ++i) {
    auto score = *(pscore + i);
    if (score > mscore) {
      mscore = score;
      label = i;
    }
  }

  auto obj = std::make_shared<cnstream::CNInferObject>();
  obj->id = std::to_string(label);
  obj->score = mscore;

  cnstream::CNInferObjsPtr objs_holder = package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
  objs_holder->objs_.push_back(obj);
  return 0;
}

class ObjPostprocClassification : public cnstream::ObjPostproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& finfo, const std::shared_ptr<cnstream::CNInferObject>& obj) override;

  DECLARE_REFLEX_OBJECT_EX(ObjPostprocClassification, cnstream::ObjPostproc)
};  // classd ObjPostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(ObjPostprocClassification, cnstream::ObjPostproc)

int ObjPostprocClassification::Execute(const std::vector<float*>& net_outputs,
                                       const std::shared_ptr<edk::ModelLoader>& model,
                                       const cnstream::CNFrameInfoPtr& finfo,
                                       const std::shared_ptr<cnstream::CNInferObject>& obj) {
  if (net_outputs.size() != 1) {
    LOGE(DEMO) << "[Warning] classification neuron network only has one output,"
                  " but get " + std::to_string(net_outputs.size());
    return -1;
  }

  auto data = net_outputs[0];
  auto len = model->OutputShape(0).DataCount();
  auto pscore = data;

  float mscore = 0;
  int label = 0;
  for (decltype(len) i = 0; i < len; ++i) {
    auto score = *(pscore + i);
    if (score > mscore) {
      mscore = score;
      label = i;
    }
  }

  cnstream::CNInferAttr attr;
  attr.id = 0;
  attr.value = label;
  attr.score = mscore;
  obj->AddAttribute("classification", attr);
  return 0;
}
