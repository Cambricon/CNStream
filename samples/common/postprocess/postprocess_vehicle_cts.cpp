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
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "cnstream_frame_va.hpp"
#include "postproc.hpp"
#include "cnstream_logging.hpp"

/*
 * @brief
 * Postprocessing for model cnstream/data/models/vehicle_cts_b4c4_bgra_mlu270.cambricon
 **/
class PostprocVehicleCts : public cnstream::ObjPostproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& finfo, const std::shared_ptr<cnstream::CNInferObject>& obj) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocVehicleCts, cnstream::ObjPostproc)
};  // classd ObjPostprocClassification

IMPLEMENT_REFLEX_OBJECT_EX(PostprocVehicleCts, cnstream::ObjPostproc)

int PostprocVehicleCts::Execute(const std::vector<float*>& net_outputs,
                                const std::shared_ptr<edk::ModelLoader>& model,
                                const cnstream::CNFrameInfoPtr& finfo,
                                const std::shared_ptr<cnstream::CNInferObject>& obj) {
  static const std::array<std::string, 3> category_names = {"COLOR", "TYPE", "TOWARDS"};
  static const std::array<std::vector<std::string>, 3> categories = {
    std::vector<std::string>({ /* colors */
      "BROWN", "DARK_GREY", "GREY", "WHITE", "PINK", "PURPLE",
      "RED", "GREEN", "BLUE", "GOLD", "CYAN", "YELLOW", "BLACK"
    }),
    std::vector<std::string>({ /* types */
      "MPV", "MEGA_BUS", "HGV", "MINI_BUS", "COMPACT_VAN", "MINI_VAN",
      "PICKUP", "SUV", "LIGHT_BUS", "CAR"
    }),
    std::vector<std::string>({ /* sides */
      "BACK", "FRONT", "SIDE", "BACK_LEFT", "BACK_RIGHT", "FRONT_LEFT",
      "FRONT_RIGHT"
    }),
  };
  bool check_model = true;
  if (model->OutputNum() != categories.size()) {
    check_model = false;
  } else {
    for (uint32_t output_idx = 0; output_idx < model->OutputNum(); ++output_idx) {
      if (static_cast<size_t>(model->OutputShape(output_idx).DataCount()) != categories[output_idx].size()) {
        check_model = false;
        break;
      }
    }
  }
  if (!check_model)
    LOGF(POSTPROC_VEHICLE_CTS) << "Model mismatched.";

  auto ArgMax = [] (float* data, size_t size) {
    return std::distance(data, std::max_element(data, data + size));
  };

  for (uint32_t output_idx = 0; output_idx < model->OutputNum(); ++output_idx) {
    float* net_output = net_outputs[output_idx];
    auto max_score_idx = ArgMax(net_output, model->OutputShape(output_idx).DataCount());
    if (net_output[max_score_idx] < 0.3) {
      obj->AddExtraAttribute(category_names[output_idx], "uncertain");
    } else {
      std::string score_str = std::to_string(net_output[max_score_idx]);
      score_str = score_str.substr(0, std::min(size_t(4), score_str.size()));
      std::string str = categories[output_idx][max_score_idx] +
                        " score[" + score_str + "]";
      obj->AddExtraAttribute(category_names[output_idx], str);
    }
  }

  return 0;
}
