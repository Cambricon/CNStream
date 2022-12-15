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

#ifndef MODULES_VOUT_HPP_
#define MODULES_VOUT_HPP_
/*!
 *  @file vout.hpp
 *
 *  This file contains a declaration of the VoutParam, and the Vout class.
 */
#include <memory>
#include <string>
#include <utility>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_pipeline.hpp"
#include "private/cnstream_param.hpp"

namespace cnstream {

struct VoutParam {
  int mode = 0;
  int framerate;
  std::string stream_id = "";
};

/*!
 * @class Vout
 *
 * @brief Vout is a class to handle picutres to be rendered.
 *
 */
class Vout : public ModuleEx, public ModuleCreator<Vout> {
 public:
  /*!
   * @brief Constructs a Vout object.
   *
   * @param[in] name The name of this module.
   *
   * @return No return value.
   */
  explicit Vout(const std::string &name);

  /*!
   * @brief Destructs a Vout object.
   *
   * @return No return value.
   */
  ~Vout();

  /*!
   * @brief Initializes the configuration of the Vout module.
   *
   * This function will be called by the pipeline when the pipeline starts.
   *
   * @param[in] param_set The module's parameter set to configure a Vout module.
   *
   * @return Returns true if the parammeter set is supported and valid, othersize returns false.
   */
  bool Open(ModuleParamSet param_set) override;

  /*!
   * @brief Frees the resources that the object may have acquired.
   *
   * This function will be called by the pipeline when the pipeline stops.
   *
   * @return No return value.
   */
  void Close() override;

  /**
   * @brief Process data
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /*!
   * @brief Checks the parameter set for the DataSource module.
   *
   * @param[in] param_set Parameters for this module.
   *
   * @return Returns true if all parameters are valid. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &param_set) const override;

  /*!
   * @brief Gets the parameters of the Vout module.
   *
   * @return Returns the parameters of this module.
   *
   * @note This function should be called after ``Open`` function.
   */
  VoutParam GetVoutParam() const { return param_; }

 private:
  VoutParam param_;
  std::unique_ptr<ModuleParamsHelper<VoutParam>> param_helper_ = nullptr;
};  // class Vout

}  // namespace cnstream

#endif  // MODULES_VOUT_HPP_
