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

#ifndef MODULES_Discard_frame_HPP_
#define MODULES_Discard_frame_HPP_
/**
 *  This file contains a declaration of struct DiscardFrame
 */

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {
/**
 * @brief discard frame every n frames
 */

class DiscardFrame : public Module, public ModuleCreator<DiscardFrame> {
 public:
  /**
   *  @brief  Generate DiscardFrame
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit DiscardFrame(const std::string& name);

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet:
   * @verbatim
   *       discard_interval:discard_interval
   * @endverbatim
   *
   * @return if module open succeed
   */
  bool Open(ModuleParamSet paramSet) override;

  /**
   * @brief Called by pipeline when pipeline end.
   */
  void Close() override;

  /**
   * @brief Do for each frame
   *
   * @param data : Pointer to the frame info
   *
   * @return whether process will discard frame
   * @retval 0: donot discard frame
   * @retval 1: discard frame
   *
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

  virtual ~DiscardFrame();

 private:
  int frame_Mod = 0;
};  // class DiscardFrame

}  // namespace cnstream
#endif
