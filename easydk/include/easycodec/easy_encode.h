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
  /// The numerator of encode frame rate of the venc channel
  uint32_t frame_rate_num;
  /// The denominator of encode frame rate of the venc channel
  uint32_t frame_rate_den;
  /// Average bitrate in unit of kpbs, for cbr only.
  uint32_t bit_rate;
  /// The max bitrate in unit of kbps, for vbr only .
  uint32_t max_bit_rate;
  /// The max qp
  uint32_t max_qp = 51;
  /// The min qp
  uint32_t min_qp = 0;
};

/**
 * @brief Video profile enumaration.
 */
enum class VideoProfile {
  H264_BASELINE = 0,
  H264_MAIN,
  H264_HIGH,
  H264_HIGH_10,

  H265_MAIN,
  H265_MAIN_STILL,
  H265_MAIN_INTRA,
  H265_MAIN_10,
  PROFILE_MAX
};

/**
 * @brief Video codec level
 */
enum class VideoLevel {
  H264_1 = 0,
  H264_1B,
  H264_11,
  H264_12,
  H264_13,
  H264_2,
  H264_21,
  H264_22,
  H264_3,
  H264_31,
  H264_32,
  H264_4,
  H264_41,
  H264_42,
  H264_5,
  H264_51,

  H265_MAIN_1,
  H265_HIGH_1,
  H265_MAIN_2,
  H265_HIGH_2,
  H265_MAIN_21,
  H265_HIGH_21,
  H265_MAIN_3,
  H265_HIGH_3,
  H265_MAIN_31,
  H265_HIGH_31,
  H265_MAIN_4,
  H265_HIGH_4,
  H265_MAIN_41,
  H265_HIGH_41,
  H265_MAIN_5,
  H265_HIGH_5,
  H265_MAIN_51,
  H265_HIGH_51,
  H265_MAIN_52,
  H265_HIGH_52,
  H265_MAIN_6,
  H265_HIGH_6,
  H265_MAIN_61,
  H265_HIGH_61,
  H265_MAIN_62,
  H265_HIGH_62,
  LEVEL_MAX
};

/*
 * @brief cncodec GOP type, see cncodec developer guide
 */
enum class GopType {
  BIDIRECTIONAL,
  LOW_DELAY,
  PYRAMID
};

/**
 * @brief Crop config parameters to control image crop attribute
 * @attention Not support on MLU270 and MLU220
 */
struct CropConfig {
  bool enable = false;
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
};

/**
 * @brief Encode packet callback function type
 * @param CnPacket[in] Packet containing encoded frame information
 */
using EncodePacketCallback = std::function<void(const CnPacket&)>;

/// Encode EOS callback function type
using EncodeEosCallback = std::function<void()>;

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
    Geometry frame_geometry;

    /**
     * @brief Input pixel format
     * @note h264/h265 support NV21/NV12/I420/RGBA/BGRA/ARGB/ABGR
     *       jpeg support NV21/NV12
     */
    PixelFmt pixel_format;

    /**
     * @brief output codec type
     * @note support h264/h265/jpeg
     */
    CodecType codec_type = CodecType::H264;

    /// Color standard
    ColorStd color_std = ColorStd::ITU_BT_2020;

    /// Qulity factor for jpeg encoder.
    uint32_t jpeg_qfactor = 50;

    /// Profile for video encoder.
    VideoProfile profile = VideoProfile::H264_MAIN;

    /// Video encode level
    VideoLevel level = VideoLevel::H264_41;

    /// Video rate control parameters.
    RateControl rate_control;

    /// Crop parameters
    CropConfig crop_config;

    /// Input buffer number
    uint32_t input_buffer_num = 3;

    /// Output buffer number
    uint32_t output_buffer_num = 4;

    /// P frame number in gop default 0
    uint32_t p_frame_num = 0;

    /// B frame number in gop when profile is above main, default 0
    uint32_t b_frame_num = 0;

    /// MB count of intra refresh, default 0 for not enable intra refresh
    uint32_t ir_count = 0;

    /// Slice max MB count, default 0
    uint32_t max_mb_per_slice = 0;

    /// GOP type, @see GopType
    GopType gop_type = GopType::BIDIRECTIONAL;

    /// Init table for CABAC, 0,1,2 for H264 and 0,1 for HEVC, default 0
    uint32_t cabac_init_idc = 0;

    /// insert SPS/PPS before IDR,1, insert, 0 not
    uint32_t insertSpsPpsWhenIDR = 1;

    /// Whether to print encoder attribute
    bool silent = false;

    /// Callback for receive packet
    EncodePacketCallback packet_callback = NULL;

    /// Callback for receive eos
    EncodeEosCallback eos_callback = NULL;

    /// Indentification to specify device on which create encoder
    int dev_id = 0;
  };

  /**
   * @brief Create encoder by attr. Throw a Exception while error encountered.
   * @param attr[in] Encoder attribute description
   * @return Pointer to new encoder instance
   */
  static EasyEncode* Create(const Attr& attr);

  void AbortEncoder();

  /**
   * @brief Get the encoder instance attribute
   * @return Encoder attribute
   */
  Attr GetAttr() const;

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
  bool SendDataCPU(const CnFrame& frame, bool eos = false);

  /**
   * @brief Release encoder buffer.
   * @note Release encoder buffer each time received packet while packet content will not be used,
   *       otherwise encoder may be blocked.
   * @param buf_id[in] Codec buffer id.
   */
  void ReleaseBuffer(uint64_t buf_id);

 private:
  EasyEncode();

  EncodeHandler* handler_ = nullptr;

  EasyEncode(const EasyEncode&) = delete;
  const EasyEncode& operator=(const EasyEncode&) = delete;
};  // class EasyEncode

}  // namespace edk

#endif  // EASYCODEC_EASY_ENCODE_H_
