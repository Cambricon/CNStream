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

#ifndef MODULES_OSD_HPP_
#define MODULES_OSD_HPP_
/**
 *  @file osd.hpp
 *
 *  This file contains a declaration of class Osd
 */

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "private/cnstream_param.hpp"

namespace cnstream {

/**
 * @brief osd parameter structure
 */
struct OsdParams {
  std::vector<std::string> labels;
  std::vector<std::string> secondary_labels;
  std::vector<std::string> attr_keys;
  std::string font_path = "";
  std::string logo = "";
  std::string osd_handler_name = "";
  float text_scale = 1;
  float text_thickness = 1;
  float box_thickness = 1;
  float label_size = 1;
  bool hw_accel = false;  // whether to use hw to accelrate OSD
};

struct OsdContext;

class CnOsd;

/**
 * @brief Draw objects on image,output is bgr24 images
 */
class Osd : public Module, public ModuleCreator<Osd> {
 public:
  /**
   *  @brief  Generate osd
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit Osd(const std::string& name);

  /**
   * @brief Release osd
   * @param None
   * @return None
   */
  ~Osd();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param param_set :
   * @verbatim
   *   label_path: label path
   * @endverbatim
   *
   * @return if module open succeed
   */
  bool Open(cnstream::ModuleParamSet param_set) override;

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
   * @retval <0: failed
   *
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  void OnEos(const std::string& stream_id) override;

  /**
   * @brief Check ParamSet for this module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Return true if this API run successfully. Otherwise, return false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

 private:
  std::shared_ptr<OsdContext> GetOsdContext(CNFrameInfoPtr data);
  std::unique_ptr<ModuleParamsHelper<OsdParams>> param_helper_ = nullptr;
  std::map<std::string, std::shared_ptr<OsdContext>> osd_ctxs_;
  RwLock ctx_lock_;
};  // class Osd

}  // namespace cnstream

#endif  // MODULES_OSD_HPP_
