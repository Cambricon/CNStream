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

#include <memory>
#include <string>
#include <map>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "easytrack/easy_track.h"

namespace infer_server { class ModelInfo; }

namespace cnstream {

struct TrackerContext;

/**
 * @class Tracker
 *
 * @brief Tracker is a module for realtime tracking.
 *   It would be MLU feature extracting if the model_path is provided, otherwise it would be done on CPU.
 */
class Tracker : public Module, public ModuleCreator<Tracker> {
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
   *  @param[in] paramSet  This module's parameters to configure Tracker. Please use `cnstream_inspect` tool to get each
   parameter's detail information.

   *  @return Returns true if opened successfully, otherwise returns false.
   */
  bool Open(ModuleParamSet paramSet) override;

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
   * @brief Checks the parameters for a module.
   *
   * @param[in] paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &paramSet) const override;

 private:
  bool InitFeatureExtractor(const CNFrameInfoPtr &data);
  TrackerContext *GetContext(const CNFrameInfoPtr &data);
  std::map<int, TrackerContext *> contexts_;
  std::shared_ptr<infer_server::ModelInfo> model_ = nullptr;
  std::mutex mutex_;
  std::function<void(const CNFrameInfoPtr, bool)> match_func_;
  int device_id_ = 0;
  std::string model_pattern1_ = "";
  std::string model_pattern2_ = "";
  std::string track_name_ = "";
  float max_cosine_distance_ = 0.2;
  int engine_num_ = 1;
  bool need_feature_ = true;
};  // class Tracker

}  // namespace cnstream

#endif  // MODULES_TRACK_INCLUDE_HPP_
