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

#ifndef DISPLAYER_HPP_
#define DISPLAYER_HPP_

#include <opencv2/opencv.hpp>

#include <memory>
#include <string>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

class DisplayStream;

/**
 * @brief Displayer is a module for displaying the vedio
 */
class Displayer : public Module, public ModuleCreator<Displayer> {
 public:
  /**
   *  @brief  Generate Displayer
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit Displayer(const std::string& name);

  /**
   *  @brief  Release Displayer
   *
   *  @param  None
   *
   *  @return None
   */
  ~Displayer();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet :
   @verbatim
      window-width: display window width
      window-height: display window height
      cols: display image columns
      rows: display image rows
      refresh-rate: display refresh rate
   @endverbatim
   *
   * @return if module open succeed
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
   * @brief display each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   */
  int Process(CNFrameInfoPtr data) override;

 private:
  DisplayStream* stream_;
};  // class Displayer

}  // namespace cnstream

#endif  // DISPLAYER_HPP_
