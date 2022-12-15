/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
// #define LOCAL_DEBUG_DUMP_IMAGE
#ifdef LOCAL_DEBUG_DUMP_IMAGE
#include <atomic>
#include <string>
#include "opencv2/imgproc/types_c.h"
#include "opencv2/opencv.hpp"
#endif

#include <limits>
#include <vector>

#include "cnstream_postproc.hpp"

static const std::vector<std::string> chinese_plate_codes{
    "京", "沪", "津", "渝", "冀", "晋", "蒙", "辽", "吉", "黑", "苏", "浙", "皖", "闽", "赣", "鲁", "豫",
    "鄂", "湘", "粤", "桂", "琼", "川", "贵", "云", "藏", "陕", "甘", "青", "宁", "新", "0",  "1",  "2",
    "3",  "4",  "5",  "6",  "7",  "8",  "9",  "A",  "B",  "C",  "D",  "E",  "F",  "G",  "H",  "J",  "K",
    "L",  "M",  "N",  "P",  "Q",  "R",  "S",  "T",  "U",  "V",  "W",  "X",  "Y",  "Z",  "港", "学", "使",
    "警", "澳", "挂", "军", "北", "南", "广", "沈", "兰", "成", "济", "海", "民", "航", "空"};

static const std::vector<std::string> chinese_plate_codes_2{
    "京", "沪", "津", "渝", "冀", "晋", "蒙", "辽", "吉", "黑",
    "苏", "浙", "皖", "闽", "赣", "鲁", "豫", "鄂", "湘", "粤",
    "桂", "琼", "川", "贵", "云", "藏", "陕", "甘", "青", "宁",
    "新",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "J", "K",
    "L", "M", "N", "P", "Q", "R", "S", "T", "U", "V",
    "W", "X", "Y", "Z", "I", "O", "-"
};

class PostprocLprnet : public cnstream::Postproc {
 public:
  int Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<cnstream::CNFrameInfoPtr>& packages,
              const std::vector<cnstream::CNInferObjectPtr>& objects,
              const cnstream::LabelStrings& labels);

  DECLARE_REFLEX_OBJECT_EX(PostprocLprnet, cnstream::Postproc);
};  // class PostprocLprnet

IMPLEMENT_REFLEX_OBJECT_EX(PostprocLprnet, cnstream::Postproc);

static inline size_t ArgMax(const float* preds, int ci, int nc, int classes) {
  float max = std::numeric_limits<float>::lowest();
  int cls = -1;
  for (int i = 0; i < classes; ++i) {
    int offset = i * nc + ci;
    if (preds[offset] > max) {
      cls = i;
      max = preds[offset];
    }
  }
  return cls;
}

int PostprocLprnet::Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                            const std::vector<cnstream::CNFrameInfoPtr>& packages,
                            const std::vector<cnstream::CNInferObjectPtr>& objects,
                            const cnstream::LabelStrings& labels) {
  LOGF_IF(PostprocLprnet, model_info.InputNum() != 1);
  LOGF_IF(PostprocLprnet, model_info.OutputNum() != 1);
  LOGF_IF(PostprocLprnet, net_outputs.size() != 1);

  cnedk::BufSurfWrapperPtr output = net_outputs[0].first;  // data
  if (!output->GetHostData(0)) {
    LOGE(PostprocLprnet) << " copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output->GetHostData(0, batch_idx));
    cnstream::CNFrameInfoPtr package = packages[batch_idx];
    cnstream::CNInferObjectPtr object = objects[batch_idx];

    const auto output_sp = net_outputs[0].second;
    std::vector<int> plate_indexes;
    std::vector<int> best_indexes;

    int sequence_length = 0;
    int label_size = 0;
    sequence_length = output_sp[2];  // 18
    label_size = output_sp[1];
    best_indexes.reserve(sequence_length);
    // find max score indexes
    for (int i = 0; i < sequence_length; ++i) {
      best_indexes[i] = static_cast<unsigned>(ArgMax(data, i, sequence_length, label_size));
    }

    if (label_size == 84) {
      // filter code indexes
      for (int i = 0; i < sequence_length; ++i) {
        if (best_indexes[i] < static_cast<int>(chinese_plate_codes.size()) &&
            (i == 0 || best_indexes[i] != best_indexes[i - 1] || best_indexes[i] != label_size - 1)) {
          plate_indexes.push_back(best_indexes[i]);
        }
      }
      // remove duplicated chinese characters no matter if different character
      if (plate_indexes.size() >= 2 && plate_indexes[0] <= 31 && plate_indexes[1] <= 31) {
        if (plate_indexes[0] >= plate_indexes[1]) {
          plate_indexes.erase(plate_indexes.begin() + 1);
        } else {
          plate_indexes.erase(plate_indexes.begin());
        }
      }
    } else if (label_size == 68) {
      int previous_character = label_size - 1;
      // remove duplicated characters and character `-`
      for (int i = 0; i < sequence_length; ++i) {
        if (label_size - 1 == best_indexes[i]) {
          previous_character = best_indexes[i];
          continue;
        }
        if (previous_character != best_indexes[i]) {
          plate_indexes.push_back(best_indexes[i]);
          previous_character = best_indexes[i];
        }
      }
    } else {
      LOGE("PostprocLprnet") << "output shape [1] = " << output_sp[1] << " is invalid";
    }

    if (plate_indexes.size() != 7) continue;
    // convert to string
    std::string plate_str;
    for (auto& i : plate_indexes) {
      if (output_sp[1] == 84) {
        plate_str += chinese_plate_codes[i];
      } else {
        plate_str += chinese_plate_codes_2[i];
      }
    }
    object->AddExtraAttribute("PlateNumber", plate_str);
  }
  return 0;
}
