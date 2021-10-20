/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include <memory>
#include <string>
#include <vector>
#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "postproc.hpp"

static inline
size_t ArgMax(float* begin, int len) {
  return std::distance(begin, std::max_element(begin, begin + len));
}

class PostprocLprnet : public cnstream::ObjPostproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& finfo, const std::shared_ptr<cnstream::CNInferObject>& obj) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocLprnet, cnstream::ObjPostproc)
};  // classd PostprocLprnet

IMPLEMENT_REFLEX_OBJECT_EX(PostprocLprnet, cnstream::ObjPostproc)

int PostprocLprnet::Execute(const std::vector<float*>& net_outputs,
                            const std::shared_ptr<edk::ModelLoader>& model,
                            const cnstream::CNFrameInfoPtr& finfo,
                            const std::shared_ptr<cnstream::CNInferObject>& obj) {
  static const std::string kChars[] = {
    "京", "沪", "津", "渝", "冀", "晋", "蒙", "辽", "吉", "黑",
    "苏", "浙", "皖", "闽", "赣", "鲁", "豫", "鄂", "湘", "粤",
    "桂", "琼", "川", "贵", "云", "藏", "陕", "甘", "青", "宁",
    "新",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "J", "K",
    "L", "M", "N", "P", "Q", "R", "S", "T", "U", "V",
    "W", "X", "Y", "Z"
  };
  static constexpr int kNChar = 65;
  static constexpr int kPlateLen = 7;
  const int seq_len = model->OutputShape(0).C();
  float* data = net_outputs[0];
  LOGF_IF(POSTPROC_LPRNET, seq_len <= kNChar) << "Can not deal with this lprnet model!";
  const int nlabel = model->OutputShape(0).H();
  std::string plate_number = "";
  int pre_ch_idx = kNChar;
  float score = 0.0f;
  int len = 0;
  for (int label_idx = 0; label_idx < nlabel; ++label_idx) {
    int ch_idx = ArgMax(data + label_idx * seq_len, seq_len);
    if (ch_idx >= kNChar) continue;
    if (pre_ch_idx != ch_idx) {
      plate_number += kChars[ch_idx];
      score += data[label_idx * seq_len + ch_idx];
      len++;
    }
    pre_ch_idx = ch_idx;
  }
  if (len != kPlateLen) return 0;
  score /= len;
  if (score < threshold_) return 0;
  if (obj->collection.HasValue("plate_container")) {
    // plate_container set in PostprocMSSDPlateDetection
    // see CNStream/samples/common/postprocess/postprocess_mobilenet_ssd_plate_detection.cpp
    obj->collection.Get<decltype(obj)>("plate_container")->AddExtraAttribute("plate_number", plate_number);
    obj->collection.Get<decltype(obj)>("plate_container")->AddExtraAttribute("plate_ocr_score", std::to_string(score));
  }
  return 0;
}
