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

#ifndef MODULES_DISPLAYER_HPP_
#define MODULES_DISPLAYER_HPP_
/**
 *  \file displayer.hpp
 *
 *  This file contains a declaration of class Displayer
 */

#include <memory>
#include <string>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

class SDLVideoPlayer;

/**
 * @brief Displayer is a module for displaying the video.
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
   * @verbatim
   *   window-width: display window width
   *   window-height: display window height
   *   cols: display image columns
   *   rows: display image rows
   *   refresh-rate: display refresh rate
   * @endverbatim
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

  /**
   * @brief GUI event loop
   */
  void GUILoop(const std::function<void()>& quit_callback);

  /**
   *@brief return whether show
   */
  inline bool Show() { return show_; }

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

 private:
  SDLVideoPlayer* player_;
  bool show_ = false;
};  // class Displayer

}  // namespace cnstream

#endif  // MODULES_DISPLAYER_HPP_
