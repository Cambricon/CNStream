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

#ifndef MODULES_OSD_INCLUDE_OSD_H_
#define MODULES_OSD_INCLUDE_OSD_H_

#include <memory>
#include <string>
#include <vector>

#include "cnstream_core.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

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
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet :
   @verbatim
   label_path: label path
   @endverbatim
   *
   * @return if module open succeed
   */
  bool Open(cnstream::ModuleParamSet paramSet) override;

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

 private:
  std::vector<std::string> labels_;
};  // class osd

}  // namespace cnstream

#endif  // MODULES_OSD_INCLUDE_OSD_H_
