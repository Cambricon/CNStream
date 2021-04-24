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

#ifndef MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_HPP_
#define MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_HPP_

#include <memory>
#include <string>
#include <unordered_map>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

struct RtspSinkContext;

/// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

/**
 * @brief The enum of color format
 */
enum ColorFormat {
  YUV420 = 0,  /// Planar Y4-U1-V1
  RGB24,       /// Packed R8G8B8
  BGR24,       /// Packed B8G8R8
  NV21,        /// Semi-Planar Y4-V1U1
  NV12,        /// Semi-Planar Y4-U1V1
};

/**
 * @brief The enum of video codec type
 */
enum VideoCodecType {
  H264 = 0,   /// H264
  HEVC,       /// HEVC
  MPEG4,      /// MPEG4
};

/**
 * @brief The enum of encoder type
 */
enum EncoderType {
  FFMPEG = 0,  /// encoder with ffmpeg
  MLU,         /// encoder with mlu
};

/**
 * @brief The struct of rtsp parameters
 */
struct RtspParam {
  int frame_rate = 25;               // Target fps
  int udp_port = 9554;               // UDP port
  int http_port = 8080;              // RTSP-over-HTTP channel port
  int src_width = 1920;              // Source width;
  int src_height = 1080;             // Source height;
  int dst_width = 1920;              // Target width,prefered size same with input
  int dst_height = 1080;             // Target height,prefered size same with input
  int gop = 20;                      // Target gop,default is 10
  int kbps = 2 * 1024;               // Target Kbps,default is 2*1024(2M)
  ColorFormat color_format = NV21;   // Color format
  VideoCodecType codec_type = H264;  // Video codec type
  EncoderType enc_type = FFMPEG;     // Encoder type

  int device_id;             // Device id
  int view_rows;             // Row of the display grid. Only used in mosaic mode.
  int view_cols;             // Column of the display grid. Only used in mosaic mode.
  bool resample = true;      // Resample before encode. False only used in single mode.
  std::string view_mode;     // Display mode
  std::string color_mode;    // Color mode
  std::string preproc_type;  // Preproc type
  std::string encoder_type;  // Encoder type
};

/**
 * @brief RtspSink is a module to deliver stream by RTSP protocol
 */
class RtspSink : public ModuleEx, public ModuleCreator<RtspSink> {
 public:
  /**
   * @brief RtspSink constructor
   *
   * @param  name : module name
   */
  explicit RtspSink(const std::string& name);
  /**
   * @brief RtspSink destructor
   */
  ~RtspSink();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet : parameter set
   *
   * @return ture if module open succeed, otherwise false.
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

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet& paramSet) const override;

 private:
  std::shared_ptr<RtspSinkContext> GetRtspSinkContext(CNFrameInfoPtr data);
  RtspParam GetRtspParam(CNFrameInfoPtr data);
  std::shared_ptr<RtspSinkContext> CreateRtspSinkContext(CNFrameInfoPtr data);
  void OnStreamEos(CNFrameInfoPtr data);
  void SetParam(const ModuleParamSet& paramSet, std::string name, int* variable, int default_value);
  void SetParam(const ModuleParamSet& paramSet, std::string name, std::string* variable, std::string default_value);
  void SetParam(const ModuleParamSet& paramSet, std::string name, bool* variable, bool default_value);

  RtspParam params_;

  bool is_mosaic_style_ = false;

  RwLock rtsp_lock_;
  std::unordered_map<int, std::shared_ptr<RtspSinkContext>> contexts_;
};  // class RtspSink

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_HPP_
