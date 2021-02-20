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
#include <utility>
#include <vector>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "infer_server.h"
#include "processor.h"
#include "reflex_object.h"
#include "video_helper.h"

namespace cnstream {

/**
 * @brief Base class of video preprocessing
 */
class VideoPreproc : virtual public ReflexObjectEx<VideoPreproc> {
 public:
  /**
   * @brief do nothing
   */
  virtual ~VideoPreproc() {}
  /**
   * @brief create relative preprocess object
   *
   * @param proc_name: preprocess class name
   *
   * @return relative preprocess object pointer
   */
  static VideoPreproc* Create(const std::string& proc_name);

  /**
   * @brief set model input pixel format
   *
   * @param fmt the model input pixel format
   *
   * @return void
   */
  void SetModelInputPixelFormat(infer_server::video::PixelFmt fmt) {
    model_input_pixel_format_ = fmt;
  }

  /**
   * @brief Execute preprocessing
   *
   * @param model_input: the input of neural network. The preproc result should be set to it.
   * @param input_data: the raw input data. The user could get infer_server::video::VideoFrame object from it.
   * @param model_info: model information, e.g., input/output number, shape and etc.
   *
   * @note The input_data holds infer_server::video::VideoFrame object. Use the statement to get video frame,
   *       `const infer_server::video::VideoFrame& frame = input_data.GetLref<infer_server::video::VideoFrame>();`.
   *       After preprocessing, you should set the result to model_output. For example, the model only has one input,
   *       then you should copy the result to `model_input->buffers[0].MutableData()`  which is a void pointer.
   * 
   * @return return true if succeed
   */
  virtual bool Execute(infer_server::ModelIO* model_input, const infer_server::InferData& input_data,
                       const infer_server::ModelInfo& model_info) = 0;

 protected:
  infer_server::video::PixelFmt model_input_pixel_format_ = infer_server::video::PixelFmt::ARGB;
};  // class VideoPreproc

}  // namespace cnstream

#endif  // ifndef MODULES_INFERENCE2_INCLUDE_VIDEO_PREPROC_HPP_
