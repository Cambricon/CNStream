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

#ifndef MODULES_ENCODE_HPP_
#define MODULES_ENCODE_HPP_

/**
 *  \file cnencoder.hpp
 *
 *  This file contains a declaration of class Encode
 */

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_logging.hpp"

namespace cnstream {

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

struct EncodeContext;
struct EncodeParam;
/**
 * @brief Encode is a module for encoding the video or image on MLU.
 */
class Encode : public Module, public ModuleCreator<Encode> {
 public:
  /**
   * @brief Encode constructor
   *
   * @param name : module name
   */
  explicit Encode(const std::string& name);
  /**
   * @brief Encode destructor
   */
  ~Encode();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet :
   * @verbatim
   *   encoder_type: Optional. Use cpu encoding or mlu encoding. The default encoder_type is cpu.
   *                 Supported values are ``mlu`` and ``cpu``.
   *   codec_type:   Optional. The codec type. The default codec_type is h264.
   *                 Supported values are ``jpeg``, ``h264`` and ``hevc``.
   *   preproc_type: Optional. Preprocess data on cpu or mlu(mlu is not supported yet). The default preproc_type is cpu.
   *                 Supported value is ``cpu``.
   * 
   *   use_ffmpeg:   Optional.Do resize and color space convert using ffmpeg. The default use_ffmpeg is false.
   *                 Supported values are ``true`` and ``false``.
   *   dst_width:    Optional.The width of the output. The default dst_width is the src_width.
   *                 Supported values are digital numbers.
   *   dst_height:   Optional.The height of the output. The default dst_height is the src_height.
   *                 Supported values are digital numbers.
   *   frame_rate:   Optional.The frame rate. The default frame_rate is 25.
   *                 Supported values are digital numbers.
   *   kbit_rate:    Optional.The bit rate in kbps. The default bit rate is 1Mbps.
   *                 Supported values are digital numbers.
   *   gop_size:     Optional.The gop size. The default gop size is 30.
   *                 Supported values are digital numbers.
   *   output_dir:   Optional.The output directory. The default output directory is {CURRENT_DIR}/output.
   *                 Supported values are directories which could be accessed.
   *   device_id:    Required if encoder_type or preproc_type is set to ``mlu``. The device id.
   *                 Supported values are digital numbers.
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
   * @brief CNEncode each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   */
  int Process(CNFrameInfoPtr data) override;

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

 private:
  std::shared_ptr<EncodeContext> GetEncodeContext(CNFrameInfoPtr data);
  EncodeParam* param_ = nullptr;
  uint32_t dst_stride_;
  std::unordered_map<std::string, std::shared_ptr<EncodeContext>> ctxs_;
  RwLock ctx_lock_;
};  // class Encode

}  // namespace cnstream

#endif  // MODULES_ENCODE_HPP_
