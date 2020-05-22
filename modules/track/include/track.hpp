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

#ifndef MODULES_TRACK_HPP_
#define MODULES_TRACK_HPP_
/**
 *  \file track.hpp
 *
 *  This file contains a declaration of struct Tracker.
 */

#include <memory>
#include <mutex>
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

/// Pointer for frame information.
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;
using TrackPtr = std::shared_ptr<edk::EasyTrack>;

/**
 *  @brief Tracker is a module for realtime tracking.
 *   Extracts feature on MLU if the model path is provided.
 *   Otherwise, it would be done on CPU.
 */
class Tracker : public Module, public ModuleCreator<Tracker> {
 public:
  /**
   *  @brief  Generates tracker.
   *
   *  @param  Name : Module name.
   *
   *  @return None.
   */
  explicit Tracker(const std::string &name);
  /**
   *  @brief  Releases tracker.
   *
   *  @param  None.
   *
   *  @return None.
   */
  ~Tracker();
  /**
   *  @brief Called by pipeline when pipeline is started.
   *
   *  @param paramSet:
   *  @verbatim
   *    track_name: Optional. The algorithm name for track. It is "FeatureMatch" by default.
   *    model_path: Optional. The path of the offline model.
   *    func_name:  Optional. The function name defined in the offline model. It can be found in
                    the Cambricon twins description file. It is "subnet0" for the most cases.
   *  @endverbatim
   *  @return Returns true if the module has been opened successfully.
   */
  bool Open(cnstream::ModuleParamSet paramSet) override;

  /**
   * @brief  Called by pipeline when pipeline is stopped.
   *
   * @return  None.
   */
  void Close() override;

  /**
   * @brief Processes each frame.
   *
   * @param data : Pointer to the frame information.
   *
   * @return Whether the process succeed.
   * @retval 0: The process has run successfully and has no intercepted data.
   * @retval <0: The process is failed.
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Checks parameters for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &paramSet) const override;

 private:
  inline TrackerContext *GetTrackerContext(CNFrameInfoPtr data);
  std::unordered_map<std::string, TrackPtr> tracker_map_;
  std::unordered_map<std::thread::id, TrackerContext *> ctx_map_;
  std::mutex tracker_mutex_;
  std::string model_path_ = "";
  std::string func_name_ = "";
  std::string track_name_ = "";
  std::shared_ptr<edk::ModelLoader> pKCFloader_ = nullptr;
};  // class Tracker

}  // namespace cnstream

#endif  // MODULES_TRACK_HPP_
