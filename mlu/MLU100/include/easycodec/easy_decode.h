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

/**
 * @file easy_decode.h
 *
 * This file contains a declaration of the EasyEncode class and involved structures.
 */

#ifndef EASYCODEC_EASY_DECODE_H_
#define EASYCODEC_EASY_DECODE_H_

#include <chrono>
#include <functional>

#include "cxxutil/exception.h"
#include "easycodec/vformat.h"

namespace edk {

enum class VideoMode {
  FRAME_MODE,  ///< packet with one frame
  STREAM_MODE  ///< stream packet
};

/**
 * @brief Performance info for decode.
 * @attention Only supported on mlu100.
 */
struct DecodePerfInfo {
  /// Transfer from codec to mlu for this frame. units: microsecond.
  uint64_t transfer_us;
  /// Decode delay for this frame. units: microsecond
  uint64_t decode_us;
  /// Total delay (from send data to frame callback). units: microsecond
  uint64_t total_us;
  /// pts for this frame
  uint64_t pts;
};

/**
 * @brief Decode packet callback function type
 * @param CnFrame[in] Frame containing decoded frame information
 */
using DecodeFrameCallback = std::function<void(const CnFrame&)>;

/// Decode EOS callback function type
using DecodeEOSCallback = std::function<void()>;

/**
 * @brief Decode performance callback function type
 * @param DecodePerfInfo[in] Decoder performance information for one frame.
 */
using DecodePerfCallback = std::function<void(const DecodePerfInfo&)>;

class DecodeHandler;

TOOLKIT_REGISTER_EXCEPTION(EasyDecode);

/**
 * @brief Easy decode class, provide a fast and easy API to decode on MLU platform
 */
class EasyDecode {
 public:
  struct Attr {
    /// The ratio of drop frame, only support on MLU100
    double drop_rate = 0;

    /// The maximum resolution that this decoder can handle.
    Geometry maximum_geometry;

    /// The resolution of the output frames.
    Geometry output_geometry;

    /// Substream geometry, only support on MLU100. Substream will be disabled, if w or h is 0.
    Geometry substream_geometry;

    /// Video codec type
    CodecType codec_type;

    /// Video mode, support on mlu100
    VideoMode video_mode;

    /// The pixel format of output frames.
    PixelFmt pixel_format;

    /// The input buffer count, only supported on mlu270
    uint32_t input_buffer_num = 2;

    /// The output buffer count.
    uint32_t frame_buffer_num = 3;

    /// Interlaced data or progressive data, not supported on mlu100.
    bool interlaced = false;

    /// Frame callback
    DecodeFrameCallback frame_callback = NULL;

    /// Substream callback. supported on mlu100
    DecodeFrameCallback substream_callback = NULL;

    /// Decode perfomance infomations callback for each frame, only supported on MLU100
    DecodePerfCallback perf_callback = NULL;

    /// EOS callback
    DecodeEOSCallback eos_callback = NULL;

    /// whether to print useful messages.
    bool silent = false;

    /// create decoder on which device
    int dev_id = 0;
  };  // struct Attr

  /**
   * @brief Decoder status enumeration
   */
  enum class Status {
    RUNNING,  ///< running, SendData and Callback are active.
    PAUSED,   ///< pause, SendData and Callback are blocked.
    STOP,     ///< stopped, decoder was destroied.
    EOS       ///< received eos.
  };          // Enum Status

  /**
   * @brief Create decoder by attr. Throw a Exception while error encountered.
   * @param attr[in] Decoder attribute description
   * @attention status is RUNNING after object be constructed.
   * @return Pointer to new decoder instance
   */
  static EasyDecode* Create(const Attr& attr) noexcept(false);

  /**
   * @brief Get the decoder instance attribute
   * @return Decoder attribute
   */
  Attr GetAttr() const;

  /**
   * @brief Get current state of decoder
   * @return Decoder status
   */
  Status GetStatus() const;

  /**
   * @brief Turn status from RUNNING to PAUSED.
   * @note Do nothing and return false if status is not RUNNING, otherwise, turn to PAUSED and return true.
   * @return Return true if pause succeeded.
   */
  bool Pause();

  /**
   * @brief Turn status from PAUSED to RUNNING, unblock SendData and Callback.
   * @note Do nothing and return false if status is not PAUSED, otherwise, turn to RUNNING and return true.
   * @return Return true if resume succeeded.
   */
  bool Resume();

  /**
   * @brief Send data to decoder, block when STATUS is pause.
   *        An Exception is thrown when send data failed.
   * @return return false when STATUS is not UNINITIALIZED or STOP.
   */
  bool SendData(const CnPacket& packet, bool eos = false) noexcept(false);

  /**
   * @brief Release decoder's buffer.
   * @note Release decoder buffer While buffer content will not be used, or decoder may be blocked.
   * @param buf_id[in] Codec buffer id.
   */
  void ReleaseBuffer(uint32_t buf_id);

  /**
   * @brief copy frame from device to host.
   * @param dst[in] copy destination
   * @param frame[in] Frame you want to copy
   * @return when error occurs, return false.
   */
  bool CopyFrame(void* dst, const CnFrame& frame);

  /**
   * @brief Query whether substream is enabled.
   * @return Return true if substream is enabled.
   */
  bool SubstreamEnabled() const;

  /**
   * @brief Destroy the Easy Decode object
   */
  ~EasyDecode();

  friend class DecodeHandler;

 private:
  explicit EasyDecode(const Attr& attr);
  EasyDecode(const EasyDecode&) = delete;
  EasyDecode& operator=(const EasyDecode&) = delete;
  Attr attr_;

  DecodeHandler* handler_ = nullptr;
};  // class EasyDecode

inline EasyDecode::Attr EasyDecode::GetAttr() const { return attr_; }

inline bool EasyDecode::SubstreamEnabled() const { return attr_.substream_geometry.w * attr_.substream_geometry.h > 0; }

}  // namespace edk

#endif  // EASYCODEC_EASY_DECODE_H_
