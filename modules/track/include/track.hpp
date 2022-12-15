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

#ifndef MODULES_TRACK_INCLUDE_HPP_
#define MODULES_TRACK_INCLUDE_HPP_
/**
 *  \file track.hpp
 *
 *  This file contains a declaration of struct Tracker
 */

#include <map>
#include <memory>
#include <string>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

#include "private/cnstream_param.hpp"

namespace infer_server {
class ModelInfo;
}

namespace cnstream {

using InferVideoPixelFmt = infer_server::NetworkInputFormat;

typedef struct TrackParams {
  uint32_t device_id = 0;
  InferVideoPixelFmt input_format = infer_server::NetworkInputFormat::RGB;
  uint32_t priority = 0;
  uint32_t engine_num = 1;
  uint32_t batch_timeout = 1000;  ///< only support in dynamic batch strategy
  bool show_stats = false;
  float max_cosine_distance = 0.2;
  std::string model_path = "";
  std::string track_name = "";
} TrackParams;


struct TrackerContext;

/**
 * @class Tracker
 *
 * @brief Tracker is a module for realtime tracking.
 *   It would be MLU feature extracting if the model_path is provided, otherwise it would be done on CPU.
 */
class Tracker : public ModuleEx, public ModuleCreator<Tracker> {
 public:
  /**
   *  @brief  Generates a tracker.
   *
   *  @param[in]  Name Module name.
   *
   *  @return None.
   */
  explicit Tracker(const std::string &name);
  /**
   *  @brief Releases a tracker.
   *
   *  @param None.
   *
   *  @return None.
   */
  ~Tracker();
  /**
   *  @brief Configures a module.
   *
   *  @param[in] param_set  This module's parameters to configure Tracker. Please use `cnstream_inspect` tool to get each
   parameter's detail information.

   *  @return Returns true if opened successfully, otherwise returns false.
   */
  bool Open(ModuleParamSet param_set) override;

  /**
   * @brief  Closes a module.
   *
   * @param  None
   *
   * @return  None
   */
  void Close() override;

  /**
   * @brief Processes each frame data.
   *
   * @param[in] data  Pointer to the frame infomation.
   *
   * @retval 0 successful with no data intercepted.
   * @retval <0 failure
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for this module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Return true if this API run successfully. Otherwise, return false.
   */
  bool CheckParamSet(const ModuleParamSet& param_set) const override;

 private:
  std::unique_ptr<ModuleParamsHelper<TrackParams>> param_helper_ = nullptr;
  bool InitFeatureExtractor(const CNFrameInfoPtr &data);
  TrackerContext *GetContext(const CNFrameInfoPtr &data);
  std::map<int, TrackerContext *> contexts_;
  std::shared_ptr<infer_server::ModelInfo> model_ = nullptr;
  std::mutex mutex_;
  std::function<void(const CNFrameInfoPtr, bool)> match_func_;
  bool need_feature_ = true;
};  // class Tracker
extern int tracker_priority_;

}  // namespace cnstream

#endif  // MODULES_TRACK_INCLUDE_HPP_
