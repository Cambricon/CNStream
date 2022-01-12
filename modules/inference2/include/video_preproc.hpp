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

#ifndef MODULES_INFERENCE2_INCLUDE_VIDEO_PREPROC_HPP_
#define MODULES_INFERENCE2_INCLUDE_VIDEO_PREPROC_HPP_

/**
 *  \file video_preproc.hpp
 *
 *  This file contains a declaration of class VideoPreproc
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnis/contrib/video_helper.h"
#include "cnis/infer_server.h"
#include "cnis/processor.h"
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "reflex_object.h"

namespace cnstream {

/**
 * @brief VideoPreproc is the base class of video preprocessing.
 */
class VideoPreproc : virtual public ReflexObjectEx<VideoPreproc> {
 public:
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  virtual ~VideoPreproc() {}
  /**
   * @brief Creates a preprocess object with the given preprocess's class name.
   *
   * @param[in] proc_name The preprocess class name.
   *
   * @return The pointer to preprocess object.
   */
  static VideoPreproc* Create(const std::string& proc_name);

  /**
   * @brief Initializes preprocessing parameters.
   *
   * @param[in] params The preprocessing parameters.
   *
   * @return Returns ture for success, otherwise returns false.
   **/
  virtual bool Init(const std::unordered_map<std::string, std::string>& params) { return true; }

  /**
   * @brief Sets model input pixel format.
   *
   * @param[in] fmt The model input pixel format.
   *
   * @return No return value.
   */
  void SetModelInputPixelFormat(infer_server::video::PixelFmt fmt) {
    model_input_pixel_format_ = fmt;
  }

  /**
   * @brief Executes preprocessing on the origin data.
   *
   * @param[out] model_input The input of neural network.
   * @param[in] input_data The raw input data. The user could get infer_server::video::VideoFrame object from it.
   * @param[in] model_info The model information, e.g., input/output number, shape and etc.
   *
   * @note The input_data holds infer_server::video::VideoFrame object. Use the statement to get video frame: 
   *      `const infer_server::video::VideoFrame& frame = input_data.GetLref<infer_server::video::VideoFrame>();`.
   *       After preprocessing, you should set the result to model_output. For example, the model only has one input,
   *       then you should copy the result to `model_input->buffers[0].MutableData()`  which is a void pointer.
   *
   * @return Returns true if successful, otherwise returns false.
   */
  virtual bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
                       const infer_server::ModelInfo* model_info) = 0;

 protected:
  infer_server::video::PixelFmt model_input_pixel_format_ = infer_server::video::PixelFmt::RGB24;
};  // class VideoPreproc

}  // namespace cnstream

#endif  // ifndef MODULES_INFERENCE2_INCLUDE_VIDEO_PREPROC_HPP_
