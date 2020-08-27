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

#ifndef MODULES_CNENCODER_HPP_
#define MODULES_CNENCODER_HPP_

/**
 *  \file cnencoder.hpp
 *
 *  This file contains a declaration of class CNEncoder
 */

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

struct CNEncoderContext;
/**
 * @brief CNEncoder is a module for encoding the video or image on MLU.
 */
class CNEncoder : public Module, public ModuleCreator<CNEncoder> {
 public:
  /**
   * @brief The enum of picture format
   */
  enum PictureFormat {
    YUV420P = 0,  /// Planar Y4-U1-V1
    RGB24,        /// Packed R8G8B8
    BGR24,        /// Packed B8G8R8
    NV21,         /// Semi-Planar Y4-V1U1
    NV12,         /// Semi-Planar Y4-U1V1
  };
  /**
   * @brief The enum of codec type
   */
  enum CodecType {
    H264 = 0,   /// H264
    HEVC,       /// HEVC
    MPEG4,      /// MPEG4
    JPEG        /// JPEG
  };
  /**
   * @brief CNEncoder constructor
   *
   * @param name : module name
   */
  explicit CNEncoder(const std::string& name);
  /**
   * @brief CNEncoder destructor
   */
  ~CNEncoder();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet :
   * @verbatim
   *   frame_rate_: frame rate
   *   bit_rate_: bit rate
   *   gop_size_: gop size
   *   cn_type_: cnencoder type
   *   cn_fomate_: cnencoder formate
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
  /**
   * @brief Records the start time and the end time of the module
   *
   * @param data A pointer to the information of the frame.
   * @param is_finished If it is false, records start time, otherwise records end time.
   *
   * @return void
   */
  void RecordTime(std::shared_ptr<CNFrameInfo> data, bool is_finished) override;

 private:
  CNEncoderContext* GetCNEncoderContext(CNFrameInfoPtr data);
  std::string pre_type_;
  std::string enc_type_;
  uint32_t device_id_ = 0;
  uint32_t bit_rate_ = 0;
  uint32_t gop_size_ = 0;
  uint32_t frame_rate_ = 0;
  uint32_t dst_width_ = 0;
  uint32_t dst_height_ = 0;
  CodecType cn_type_;
  PictureFormat cn_format_;
  std::unordered_map<std::string, CNEncoderContext*> ctxs_;
  std::mutex mutex_;
};  // class CNEncoder

}  // namespace cnstream
#endif
