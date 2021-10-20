/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_CONTRIB_FAKESINK_HPP_
#define MODULES_CONTRIB_FAKESINK_HPP_

#include <memory>
#include <string>

#include "cnstream_module.hpp"

namespace cnstream {

/**
 * @brief FakeSink is a module for synchronization.
 */
class FakeSink : public Module, public ModuleCreator<FakeSink> {
 public:
  /**
   *  @brief  Generate FakeSink
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit FakeSink(const std::string& name);
  /**
   *  @brief  Release FakeSink
   *
   *  @param  None
   *
   *  @return None
   */
  ~FakeSink() {}

  /**
   *  @brief Called by pipeline when pipeline start.
   *
   *  @param  paramSet : void
   *
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
   * @brief Process data
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;
};  // class FakeSink

}  // namespace cnstream

#endif  // MODULES_CONTRIB_FAKESINK_HPP_
