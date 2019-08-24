/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef ENCODER_HPP_
#define ENCODER_HPP_

#include <opencv2/opencv.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

struct EncoderContext {
  cv::VideoWriter writer;
  cv::Size size;
};

class Encoder : public cnstream::Module {
 public:
  explicit Encoder(const std::string& name);
  ~Encoder();

  /*
   * @brief Called by pipeline when pipeline start.
   * @paramSet
   *   dump_dir: ouput_dir path
   */
  bool Open(cnstream::ModuleParamSet paramSet) override;

  /*
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;

  /*
   * @brief do encode for each frame
   */
  int Process(CNFrameInfoPtr data) override;

 private:
  EncoderContext* GetEncoderContext(CNFrameInfoPtr data);
  std::string output_dir_;
  std::unordered_map<int, EncoderContext*> encode_ctxs_;
};  // class Encoder

#endif  // ENCODER_HPP_
