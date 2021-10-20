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
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "cnstream_frame_va.hpp"
#include "postproc.hpp"

class PostprocMSSDPlateDetection : public cnstream::ObjPostproc {
 public:
  /**
   * @brief Execute postproc on neural ssd network outputs
   *
   * @param net_outputs: neural network outputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store processed result
   * @param obj: the object to be processed
   *
   * @return return 0 if succeed
   */
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package,
              const std::shared_ptr<cnstream::CNInferObject>& obj) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocMSSDPlateDetection, cnstream::ObjPostproc)
};  // class PostprocMSSDPlateDetection

IMPLEMENT_REFLEX_OBJECT_EX(PostprocMSSDPlateDetection, cnstream::ObjPostproc)

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

int PostprocMSSDPlateDetection::Execute(const std::vector<float*>& net_outputs,
                                        const std::shared_ptr<edk::ModelLoader>& model,
                                        const cnstream::CNFrameInfoPtr& package,
                                        const std::shared_ptr<cnstream::CNInferObject>& obj) {
  auto data = net_outputs[0];
  auto box_num = data[0];
  data += 64;

  // find the plate with the highest score
  float max_score = -1.0f;
  cnstream::CNInferBoundingBox selected_bbox;
  for (decltype(box_num) bi = 0; bi < box_num; ++bi) {
    float cur_score = data[2];
    if (cur_score > max_score) {
      max_score = cur_score;
      selected_bbox.x = data[3];
      selected_bbox.y = data[4];
      selected_bbox.w = data[5] - selected_bbox.x;
      selected_bbox.h = data[6] - selected_bbox.y;
    }
    data += 7;
  }
  if (max_score < threshold_) return 0;  // no plate found
  // coordinates to the original image
  const auto& vehicle_bbox = obj->bbox;
  selected_bbox.x = selected_bbox.x * vehicle_bbox.w + vehicle_bbox.x;
  selected_bbox.y = selected_bbox.y * vehicle_bbox.h + vehicle_bbox.y;
  selected_bbox.w = selected_bbox.w * vehicle_bbox.w;
  selected_bbox.h = selected_bbox.h * vehicle_bbox.h;
  selected_bbox.x = CLIP(selected_bbox.x);
  selected_bbox.y = CLIP(selected_bbox.y);
  selected_bbox.w = std::min(1 - selected_bbox.x, selected_bbox.w);
  selected_bbox.h = std::min(1 - selected_bbox.y, selected_bbox.h);
  if (selected_bbox.w <= 0.0f || selected_bbox.h <= 0.0f) return 0;
  std::shared_ptr<cnstream::CNInferObject> plate_object = std::make_shared<cnstream::CNInferObject>();
  plate_object->id = "80";  // the label index in CNStream/data/models/label_map_coco_add_license_plate.txt
  plate_object->score = max_score;
  plate_object->bbox = selected_bbox;
  // plate flag is used by PlateFilter
  // see CNStream/samples/common/obj_filter/plate_filter.cpp
  plate_object->collection.Add("plate_flag", true);
  // in order to facilitate the addition of the recognized license plate to the vehicle attributes.
  // see CNStream/samples/common/postprocess/postprocess_lprnet.cpp
  plate_object->collection.Add("plate_container", obj);

  cnstream::CNInferObjsPtr objs_holder = package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  cnstream::CNObjsVec& objs = objs_holder->objs_;
  std::lock_guard<std::mutex> lk(objs_holder->mutex_);
  objs.push_back(plate_object);
  return 0;
}

