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
#include "display_stream.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

class Displayer : public Module, public ModuleCreator<Displayer> {
 public:
  explicit Displayer(const std::string& name);
  ~Displayer();

  /*
   * @brief Called by pipeline when pipeline start.
   * @paramSet
   *   window-width: display window width
   *   window-height: display window height
   *   cols: display image columns
   *   rows: display image rows
   *   display-rate: display refresh rate
   */
  bool Open(ModuleParamSet paramSet) override;

  /*
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;

  /*
   * @brief display each frame
   */
  int Process(CNFrameInfoPtr data) override;

 private:
  DisplayStream stream_;
};  // class Displayer

}  // namespace cnstream

#endif  // DISPLAYER_HPP_
