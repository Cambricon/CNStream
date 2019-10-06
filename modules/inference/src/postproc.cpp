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

#include "postproc.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "cnbase/cntypes.h"

using std::cerr;
using std::endl;
using std::pair;
using std::to_string;
using std::vector;

namespace cnstream {

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

Postproc* Postproc::Create(const std::string& proc_name) {
  return libstream::ReflexObjectEx<Postproc>::CreateObject(proc_name);
}

void Postproc::set_threshold(const float threshold) { threshold_ = threshold; }

IMPLEMENT_REFLEX_OBJECT_EX(PostprocSsd, Postproc)

int PostprocSsd::Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<libstream::ModelLoader>& model,
                         const CNFrameInfoPtr& package) {
  if (net_outputs.size() != 1) {
    cerr << "[Warnning] Ssd neuron network only has one output,"
            " but get " +
                to_string(net_outputs.size()) + "\n";
    return -1;
  }

  auto data = net_outputs[0];
  auto len = model->output_shapes()[0].DataCount();
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

    std::shared_ptr<CNInferObject> obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(label);
    obj->score = score;
    obj->bbox.x = x;
    obj->bbox.y = y;
    obj->bbox.w = w;
    obj->bbox.h = h;
    package->objs.push_back(obj);
  }
  return 0;
}

}  // namespace cnstream
