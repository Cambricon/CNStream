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
 * @file easy_encode.h
 *
 * This file contains a declaration of the EasyDecode class and involved structures.
 */

#ifndef EASYCODEC_EASY_ENCODE_H_
#define EASYCODEC_EASY_ENCODE_H_

#include <functional>
#include "cxxutil/exception.h"
#include "easycodec/vformat.h"

namespace edk {

/**
 * @brief Rate control parameters
 */
struct RateControl {
  /// Using variable bit rate or constant bit rate
  bool vbr;
  /// The interval of ISLICE.
  uint32_t gop;
  /// The rate statistic time, the unit is senconds(s)
  uint32_t stat_time;
  /// The numerator of input frame rate of the venc channel
  uint32_t src_frame_rate_num;
  /// The denominator of input frame rate of the venc channel
  uint32_t src_frame_rate_den;
  /// The numerator of target frame rate of the venc channel
  uint32_t dst_frame_rate_num;
  /// The denominator of target frame rate of the venc channel
  uint32_t dst_frame_rate_den;
  /// Average bitrate in unit of kpbs, for cbr only.
  uint32_t bit_rate;
  /**
   * @brief level [0..5].scope of bitrate fluctuate.
   *
   * 1-5: 10%-50%. 0: SDK optimized, recommended;
   */
  uint32_t fluctuate_level;
  /// The max bitrate in unit of kbps, for vbr only .
  uint32_t max_bit_rate;
  /// The max qp
  uint32_t max_qp;
  /// The min qp
  uint32_t min_qp;
};

/**
 * @brief Video profile enumaration.
 */
enum class VideoProfile {
  BASELINE = 0,
  MAIN,
  HIGH,
};

/**
 * @brief Crop config parameters to control image crop attribute
 */
struct CropConfig {
  bool enable = false;
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
};

/**
 *  @brief Performance info for encode, only supported on mlu100.
 */
struct EncodePerfInfo {
  /// Transfer from codec to mlu for this frame, units: microsecond
  uint64_t transfer_us;
  /// Encode delay for this frame. units: microsecond
  uint64_t encode_us;
  /// input delay(from send data to codec), units: microsecond
  uint64_t input_transfer_us;
  /// pts for this frame
  uint64_t pts;
};

/**
 * @brief Encode packet callback function type
 * @param CnPacket[in] Packet containing encoded frame information
 */
using EncodePacketCallback = std::function<void(const CnPacket&)>;

/// Encode EOS callback function type
using EncodeEosCallback = std::function<void()>;

/**
 * @brief Encode performance callback function type
 * @param EncodePerfInfo[in] Encoder performance information for one frame.
 */
using EncodePerfCallback = std::function<void(const EncodePerfInfo&)>;

TOOLKIT_REGISTER_EXCEPTION(EasyEncode);

class EncodeHandler;

/**
 * @brief Easy encoder class, provide a fast and easy API to encode on MLU platform.
 */
class EasyEncode {
 public:
  friend class EncodeHandler;
  struct Attr {
    /// The maximum resolution that this endecoder can handle.
    Geometry maximum_geometry;

    /// The resolution of the output video.
    Geometry output_geometry;

    /// Input pixel format
    PixelFmt pixel_format;

    /**
     * @brief output codec type
     * @note support h264/jpeg on mlu100
     *       support h264/h265/jpeg on mlu200
     */
    CodecType codec_type;

    /// Qulity factor for jpeg encoder.
    uint32_t jpeg_qfactor = 50;

    /// Profile for video encoder.
    VideoProfile profile = VideoProfile::MAIN;

    /// Level for video encoder.
    uint32_t level;

    /// Video rate control parameters.
    RateControl rate_control;

    /// Crop parameters
    CropConfig crop_config;

    /// Whether convert to gray colorspace
    bool color2gray = false;

    /// Output packet memory on cpu or mlu
    bool output_on_cpu = true;

    /// Output buffer number
    uint32_t packet_buffer_num = 4;

    /// Whether to print encoder attribute
    bool silent = false;

    /// Callback for receive packet
    EncodePacketCallback packet_callback = NULL;

    /// Callback for receive eos
    EncodeEosCallback eos_callback = NULL;

    /// Callback for receive performance informations each packet, only supported on MLU100.
    EncodePerfCallback perf_callback = NULL;

    /// Indentification to specify device on which create encoder
    int dev_id = 0;
  };

  /**
   * @brief Create encoder by attr. Throw a Exception while error encountered.
   * @param attr[in] Encoder attribute description
   * @return Pointer to new encoder instance
   */
  static EasyEncode* Create(const Attr& attr);

  /**
   * @brief Get the encoder instance attribute
   * @return Encoder attribute
   */
  inline Attr GetAttr() const;

  /**
   * @brief Destroy the Easy Encode object
   */
  ~EasyEncode();

  /**
  * @brief send frame to encoder.
  * @param frame[in] CNframe
  * @param eos[in] default false
  * @return Return false if send data failed.
  */
  bool SendData(const CnFrame& frame, bool eos = false);

  /**
   * @brief Release encoder buffer.
   * @note Release encoder buffer each time received packet while packet content will not be used,
   *       otherwise encoder may be blocked.
   * @param buf_id[in] Codec buffer id.
   */
  void ReleaseBuffer(uint32_t buf_id);

  /**
   * @brief Copy output packet to dst.
   * @param dst[in] Copy destination
   * @param frame[in] Frame to copy
   * @return Return false when error occurs.
   */
  bool CopyPacket(void* dst, const CnPacket& packet);

 private:
  EasyEncode(const Attr& attr, EncodeHandler* handler);

  Attr attr_;
  EncodeHandler* handler_;

  EasyEncode(const EasyEncode&) = delete;
  const EasyEncode& operator=(const EasyEncode&) = delete;
};  // class EasyEncode

inline EasyEncode::Attr EasyEncode::GetAttr() const { return attr_; }

}  // namespace edk

#endif  // EASYCODEC_EASY_ENCODE_H_
