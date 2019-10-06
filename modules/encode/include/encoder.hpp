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

#ifndef ENCODER_HPP_
#define ENCODER_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

/**
 * @brief Encoder context structer
 */
struct EncoderContext {
  cv::VideoWriter writer;
  cv::Size size;
};

/**
 * @brief Encoder is a module for encode the video or image.
 */
class Encoder : public Module, public ModuleCreator<Encoder> {
 public:
  /**
   *  @brief  Generate Encoder
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit Encoder(const std::string& name);
  /**
   *  @brief  Release Encoder
   *
   *  @param  None
   *
   *  @return None
   */
  ~Encoder();

  /**
  * @brief Called by pipeline when pipeline start.
  *
  * @param paramSet :
  @verbatim
  dump_dir: ouput_dir
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
   * @brief Encode each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   */
  int Process(CNFrameInfoPtr data) override;

 private:
  EncoderContext* GetEncoderContext(CNFrameInfoPtr data);
  std::string output_dir_;
  std::unordered_map<int, EncoderContext*> encode_ctxs_;
};  // class Encoder

}  // namespace cnstream

#endif  // ENCODER_HPP_
