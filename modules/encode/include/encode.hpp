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

#ifndef MODULES_ENCODER_INCLUDE_ENCODER_HPP_
#define MODULES_ENCODER_INCLUDE_ENCODER_HPP_

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <map>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_frame_va.hpp"
#include "private/cnstream_param.hpp"

#include "video/video_stream/video_stream.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

struct EncoderContext;

struct EncodeParam {
  int device_id = 0;                 // mlu device id, -1 :disable mlu
  bool mlu_input_frame = false;      // The input frame. true: source data , false: ImageBGR()
  bool mlu_encoder = true;           // whether use mlu encoding, default is true
  int dst_width = 0;                 // Target width, preferred size same with input
  int dst_height = 0;                // Target height, preferred size same with input
  double frame_rate = 0;             // Target fps
  int bit_rate = 4000000;           // Target bit rate, default is 1Mbps
  int gop_size = 10;                 // Target gop, default is 10
  int tile_cols = 0;                 // Grids in horizontally of video tiling, only support cpu input
  int tile_rows = 0;                 // Grids in vertically of video tiling, only support cpu input
  bool resample = false;             // Resample frame with canvas, only support cpu input
  std::string file_name = "";        // File name to encode to
};

/**
 * @brief Encode is a module to encode video stream to file with/without container.
 */
class Encode : public Module, public ModuleCreator<Encode> {
 public:
  /**
   * @brief Encode constructor
   *
   * @param  name : module name
   */
  explicit Encode(const std::string& name);
  /**
   * @brief Encode destructor
   */
  ~Encode();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet : parameter set
   *
   * @return true if module open succeed, otherwise false.
   */
  bool Open(ModuleParamSet paramSet) override;

  /**
   * @brief  Called by pipeline when pipeline stop
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

  void OnEos(const std::string &stream_id) override;

 private:
  EncoderContext * GetContext(CNFrameInfoPtr data);
  EncoderContext * CreateContext(CNFrameInfoPtr data, const std::string &stream_id);

  std::unique_ptr<ModuleParamsHelper<EncodeParam>> param_helper_ = nullptr;
  std::mutex ctx_lock_;
  std::map<std::string, EncoderContext *> contexts_;
  std::set<std::string> tile_streams_;
};  // class Encode

}  // namespace cnstream

#endif  // MODULES_ENCODER_INCLUDE_ENCODER_HPP_
