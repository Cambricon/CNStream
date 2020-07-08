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
#include <unordered_map>

#include "cnstream_core.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "easyinfer/model_loader.h"
#include "easytrack/easy_track.h"

namespace cnstream {

CNSTREAM_REGISTER_EXCEPTION(Tracker);

struct TrackerContext;

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;
/// Pointer for infer object
using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

/**
 *  @brief Tracker is a module for realtime tracking
 *   It would be MLU feature extracting if the model_path provided,
 *   otherwise it would be done on CPU.
 */
class Tracker : public Module, public ModuleCreator<Tracker> {
 public:
  /**
   *  @brief  Generate tracker
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit Tracker(const std::string &name);
  /**
   *  @brief  Release tracker
   *
   *  @param  None
   *
   *  @return None
   */
  ~Tracker();
  /**
   *  @brief Called by pipeline when pipeline start
   *
   *  @param paramSet :
   * @verbatim
   * track_name: Class name for track, "FeatureMatch" provided
   * model_path: Offline model path
   * func_name:  Function name defined in the offline model, could be found in the cambricon_twins description file
               It is "subnet0" for the most case
   * @endverbatim
   *  @return if module open succeed
   */
  bool Open(ModuleParamSet paramSet) override;

  /**
   * @brief  Called by pipeline when pipeline stop
   *
   * @param  None
   *
   * @return  None
   */
  void Close() override;

  /**
   * @brief Do for each frame
   *
   * @param data : Pointer to the frame info
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: faile
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &paramSet) const override;

 private:
  TrackerContext *GetContext(CNFrameInfoPtr data);
  std::unordered_map<int, TrackerContext *> contexts_;
  std::shared_ptr<edk::ModelLoader> model_loader_ = nullptr;
  std::mutex mutex_;
  int device_id_ = 0;
  size_t batch_size_ = 1;
  std::string model_path_ = "";
  std::string func_name_ = "";
  std::string track_name_ = "";
  float max_cosine_distance_ = 0.2;
};  // class Tracker

}  // namespace cnstream

#endif  // MODULES_TRACK_INCLUDE_HPP_
