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

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "postproc.hpp"

using std::cerr;
using std::endl;
using std::pair;
using std::to_string;
using std::vector;

/**
 * @brief Post process for ssd
 */
class PostprocSsd : public cnstream::Postproc {
 public:
  /**
   * @brief Execute postproc on neural ssd network outputs
   *
   * @param net_outputs: neural network outputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store processed result
   *
   * @return return 0 if succeed
   */
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocSsd, cnstream::Postproc)
};  // class PostprocSsd

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

IMPLEMENT_REFLEX_OBJECT_EX(PostprocSsd, cnstream::Postproc)

int PostprocSsd::Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                         const cnstream::CNFrameInfoPtr& package) {
#ifdef CNS_MLU100
  if (net_outputs.size() != 1) {
    cerr << "[Warnning] Ssd neuron network only has one output,"
            " but get " +
                to_string(net_outputs.size()) + "\n";
    return -1;
  }

  auto data = net_outputs[0];
  auto len = model->OutputShapes()[0].DataCount();
  auto box_num = len / 6;

  if (len % 6 != 0) {
    cerr << "[Warnning] The output of the ssd is a multiple of 6, but "
            " the number is " +
                to_string(len) + "\n";
    return -1;
  }

  auto pxmin = data;
  auto pymin = pxmin + box_num;
  auto pxmax = pymin + box_num;
  auto pymax = pxmax + box_num;
  auto pscore = pymax + box_num;
  auto plabel = pscore + box_num;

  int label;
  float score, x, y, w, h;
  for (decltype(box_num) bi = 0; bi < box_num; ++bi) {
    label = *(plabel + bi);
    if (0 == label) continue;
    label--;
    score = *(pscore + bi);
    if (threshold_ > 0 && score < threshold_) continue;
    x = CLIP(*(pxmin + bi));
    y = CLIP(*(pymin + bi));
    w = CLIP(*(pxmax + bi)) - CLIP(*(pxmin + bi));
    h = CLIP(*(pymax + bi)) - CLIP(*(pymin + bi));

    if (w <= 0) continue;
    if (h <= 0) continue;

    std::shared_ptr<cnstream::CNInferObject> obj = std::make_shared<cnstream::CNInferObject>();
    obj->id = std::to_string(label);
    obj->score = score;
    obj->bbox.x = x;
    obj->bbox.y = y;
    obj->bbox.w = w;
    obj->bbox.h = h;
    package->objs.push_back(obj);
  }

#elif CNS_MLU270
  auto data = net_outputs[0];
  // auto len = net_outputs[0].second;
  auto box_num = data[0];
  data += 64;

  for (decltype(box_num) bi = 0; bi < box_num; ++bi) {
    // if (data[0] != batch_index) continue;
    if (data[1] == 0) continue;
    if (threshold_ > 0 && data[2] < threshold_) continue;
    std::shared_ptr<cnstream::CNInferObject> object = std::make_shared<cnstream::CNInferObject>();
    object->id = std::to_string(data[1] - 1);
    object->score = data[2];
    object->bbox.x = data[3];
    object->bbox.y = data[4];
    object->bbox.w = data[5] - object->bbox.x;
    object->bbox.h = data[6] - object->bbox.y;

    package->objs.push_back(object);
    data += 7;
  }
#endif
  return 0;
}

