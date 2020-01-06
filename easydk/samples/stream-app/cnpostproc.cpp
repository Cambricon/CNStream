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

#include <algorithm>  // sort
#include <cstring>    // memset
#include <string>
#include <utility>
#include <vector>

#include "cnpostproc.h"
#include "cxxutil/logger.h"

using std::pair;
using std::vector;
using std::to_string;

namespace edk {

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

void CnPostproc::set_batch_index(const int batch_index) { batch_index_ = batch_index; }

void CnPostproc::set_threshold(const float threshold) { threshold_ = threshold; }

vector<DetectObject> CnPostproc::Execute(const vector<pair<float*, uint64_t>>& net_outputs) {
  return Postproc(net_outputs);
}

bool CnPostproc::CheckInvalidObject(const DetectObject& obj) {
  if (obj.bbox.width <= 0 || obj.bbox.height <= 0) {
    return false;
  }
  return true;
}

vector<DetectObject> ClassificationPostproc::Postproc(const vector<pair<float*, uint64_t>>& net_outputs) {
  if (net_outputs.size() != 1) {
    LOG(WARNING,
        "Classification neuron network only has one output,"
        " but get " +
            to_string(net_outputs.size()));
  }

  float* data = net_outputs[0].first;
  uint64_t len = net_outputs[0].second;

  vector<DetectObject> objs;
  for (decltype(len) i = 0; i < len; ++i) {
    DetectObject obj;
    memset(&obj, 0, sizeof(DetectObject));
    obj.label = i;
    obj.score = data[i];
    objs.push_back(obj);
  }

  std::sort(objs.begin(), objs.end(), [](const DetectObject& a, const DetectObject& b) { return a.score > b.score; });

  return objs;
}

vector<DetectObject> SsdPostproc::Postproc(const vector<pair<float*, uint64_t>>& net_outputs) {
  if (net_outputs.size() != 1) {
    LOG(WARNING,
        "Ssd neuron network only has one output,"
        " but get " +
            to_string(net_outputs.size()));
  }
  vector<DetectObject> objs;

#ifdef CNSTK_MLU100
  float* data = net_outputs[0].first;
  uint64_t len = net_outputs[0].second;
  uint64_t box_num = len / 6;

  if (len % 6 != 0) {
    LOG(WARNING,
        "The output of the ssd is a multiple of 6, but "
        " the number is " +
            to_string(len));
    return objs;
  }

  float* pxmin = data + len * this->batch_index_;
  float* pymin = pxmin + box_num;
  float* pxmax = pymin + box_num;
  float* pymax = pxmax + box_num;
  float* pscore = pymax + box_num;
  float* plabel = pscore + box_num;

  for (decltype(box_num) bi = 0; bi < box_num; ++bi) {
    DetectObject obj;
    obj.label = *(plabel + bi);
    if (0 == obj.label) continue;
    obj.label--;
    obj.score = *(pscore + bi);
    if (threshold_ > 0 && obj.score < threshold_) continue;
    obj.bbox.x = CLIP(*(pxmin + bi));
    obj.bbox.y = CLIP(*(pymin + bi));
    obj.bbox.width = CLIP(*(pxmax + bi)) - CLIP(*(pxmin + bi));
    obj.bbox.height = CLIP(*(pymax + bi)) - CLIP(*(pymin + bi));

    if (obj.bbox.width <= 0) continue;
    if (obj.bbox.height <= 0) continue;
    objs.push_back(obj);
  }

#elif CNSTK_MLU270
  if (this->batch_index_ >= 64) {
    LOG(ERROR, "batch index: " + std::to_string(this->batch_index_) + " is over 64");
    return objs;
  }

  // auto batch_index = this->batch_index_;
  float* data = net_outputs[0].first;
  // auto len = net_outputs[0].second;
  float box_num = data[0];  // get box num by batch index
  data += 64;               // skip box num of all batch

  for (decltype(box_num) bi = 0; bi < box_num; ++bi) {
    DetectObject obj;
    // if (data[0] != batch_index) continue;
    if (data[1] == 0) continue;
    obj.label = data[1] - 1;
    obj.score = data[2];
    if (threshold_ > 0 && obj.score < threshold_) continue;
    obj.bbox.x = CLIP(data[3]);
    obj.bbox.y = CLIP(data[4]);
    obj.bbox.width = CLIP(data[5]) - obj.bbox.x;
    obj.bbox.height = CLIP(data[6]) - obj.bbox.y;
    objs.push_back(obj);
    data += 7;
  }
#endif

  return objs;
}

vector<DetectObject> FasterrcnnPostproc::Postproc(const vector<pair<float*, uint64_t>>& net_outputs) {
  LOG(WARNING, "FasterRCNN unsupported.");
  return vector<DetectObject>(0);
}

vector<DetectObject> Yolov3Postproc::Postproc(const vector<pair<float*, uint64_t>>& net_outputs) {
  vector<DetectObject> objs;
  float* data = net_outputs[0].first;
  uint64_t len = net_outputs[0].second;
  uint64_t box_num = len / 7;

  data += len * this->batch_index_;
  float* plabel = data;
  // auto pbscore = data + 1 * box_num;
  float* pbscorexcscore = data + 2 * box_num;
  float* pxmin = data + 3 * box_num;
  float* pxmax = data + 4 * box_num;
  float* pymin = data + 5 * box_num;
  float* pymax = data + 6 * box_num;

  for (decltype(box_num) bi = 0; bi < box_num; ++bi) {
    DetectObject obj;
    obj.label = static_cast<int>(*(plabel + bi));
    obj.score = *(pbscorexcscore + bi);
    if (threshold_ > 0 && obj.score < threshold_) continue;
    obj.bbox.x = *(pxmin + bi);
    obj.bbox.y = *(pymin + bi);
    obj.bbox.width = *(pxmax + bi) - *(pxmin + bi);
    obj.bbox.height = *(pymax + bi) - *(pymin + bi);
    obj.bbox.x = (obj.bbox.x - padl_ratio_) / (1 - padl_ratio_ - padr_ratio_);
    obj.bbox.y = (obj.bbox.y - padt_ratio_) / (1 - padb_ratio_ - padt_ratio_);
    obj.bbox.width /= (1 - padl_ratio_ - padr_ratio_);
    obj.bbox.height /= (1 - padb_ratio_ - padt_ratio_);
    obj.track_id = -1;
    if (obj.label == 0) continue;
    if (obj.bbox.width <= 0) continue;
    if (obj.bbox.x < 0) continue;
    if (obj.bbox.y < 0) continue;
    if (obj.bbox.height <= 0) continue;
    objs.push_back(obj);
  }
  return objs;
}

}  // namespace edk
