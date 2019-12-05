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

#ifndef MODULES_RTSP_INCLUDE_SINK_HPP_
#define MODULES_RTSP_INCLUDE_SINK_HPP_
/**
 *  \file rtsp_sink.hpp
 *
 *  This file contains a declaration of class RtspSink
 */

#include <memory>
#include <string>
#include <unordered_map>
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

#include "rtsp_sink_stream.hpp"

using namespace cnstream;

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;
/**
 * @brief RtspSink context structer
 */
struct RtspSinkContext {
  RTSPSinkJoinStream* stream_;
};

/**
 * @brief RtspSink is a module to deliver stream by RTSP protocol
 */
class RtspSink : public Module, public ModuleCreator<RtspSink> {
 public:
  /**
   *  @brief  Generate RtspSink
   *
   *  @param  Name : module name
   *
   *  @return None
   */
  explicit RtspSink(const std::string& name);
  /**
   *  @brief  Release RtspSink
   *
   *  @param  None
   *
   *  @return None
   */
  ~RtspSink();

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
  RtspSinkContext* GetRtspSinkContext(CNFrameInfoPtr data);
  int http_port_;
  int udp_port_;
  int rows_;
  int cols_;
  bool is_mosaic_style_ = false;
  float frame_rate_ = 0;
  std::string enc_type;
  RTSPSinkJoinStream::PictureFormat format_;
  std::unordered_map<int, RtspSinkContext*> ctxs_;
};  // class RtspSink

#endif  // MODULES_RTSP_INCLUDE_SINK_HPP_
