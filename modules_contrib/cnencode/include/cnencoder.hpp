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
#include <string>
#include <unordered_map>
#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

#include "cnencoder_stream.hpp"
#include "cnrt.h"

namespace cnstream {

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

/**
 * @brief CNEncoder context structer
 */
struct CNEncoderContext {
  CNEncoderStream* stream_;
};

/**
 * @brief CNEncoder is a module for encode the video or image on MLU.
 */
class CNEncoder : public Module, public ModuleCreator<CNEncoder> {
 public:
  /**
   *  @brief  Generate CNEncoder
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit CNEncoder(const std::string& name);
  /**
   *  @brief  Release Encoder
   *
   *  @param  None
   *
   *  @return None
   */
  ~CNEncoder();

  /**
  * @brief Called by pipeline when pipeline start.
  *
  * @param paramSet :
  @verbatim
     frame_rate_: frame rate
     bit_rate_: bit rate
     gop_size_: gop size
     cn_type_: cnencoder type
     cn_fomate_: cnencoder formate
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
   * @brief CNEncode each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   */
  int Process(CNFrameInfoPtr data) override;

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet);

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
  CNEncoderStream::CodecType cn_type_;
  CNEncoderStream::PictureFormat cn_format_;
  std::unordered_map<int, CNEncoderContext*> ctxs_;
};  // class CNEncoder

}  // namespace cnstream
#endif
