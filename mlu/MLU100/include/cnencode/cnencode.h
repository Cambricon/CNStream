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

#ifndef LIBSTREAM_INCLUDE_CNENCODE_H_
#define LIBSTREAM_INCLUDE_CNENCODE_H_

#include <functional>
#include "cnbase/cntypes.h"
#include "cnbase/streamlibs_error.h"
#include "cnvformat/cnvformat.h"

namespace libstream {

struct CnRateControl {
  bool vbr;
  /* the interval of ISLICE. */
  uint32_t gop;
  /* the rate statistic time, the unit is senconds(s) */
  uint32_t stat_time;
  /* the numerator of input frame rate of the venc channel */
  uint32_t src_frame_rate_num;
  /* the denominator of input frame rate of the venc channel */
  uint32_t src_frame_rate_den;
  /* the numerator of target frame rate of the venc channel */
  uint32_t dst_frame_rate_num;
  /* the denominator of target frame rate of the venc channel */
  uint32_t dst_frame_rate_den;
  /* for cbr only */
  /* average bitrate in unit of kpbs*/
  uint32_t bit_rate;
  /* level [0..5].scope of bitrate fluctuate.
     1-5: 10%-50%. 0: SDK optimized, recommended; */
  uint32_t fluctuate_level;
  /* for vbr only */
  /* the max bitrate in unit of kbps*/
  uint32_t max_bit_rate;
  /* the max qp */
  uint32_t max_qp;
  /* the min qp */
  uint32_t min_qp;
};

enum CnVideoProfile {
  BASELINE = 0,
  MAIN,
  HIGH,
};

struct CnCropConfig {
  bool enable = false;
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
};

/*************************************
 * performance info for encode
 * supported on mlu100.
 *************************************/
typedef struct {
  /***********************************************
   * transfer from codec to mlu for this frame
   * units: microsecond
   ***********************************************/
  uint64_t transfer_us;
  /***********************************************
   * encode delay for this frame.
   * units: microsecond
   ***********************************************/
  uint64_t encode_us;
  /***********************************************
   * input delay(from send data to codec)
   * units: microsecond
   ***********************************************/
  uint64_t input_transfer_us;
  /***********************************************
   * pts for this frame
   ***********************************************/
  uint64_t pts;
} CnEncodePerfInfo;

using CnEncodePacketCallback = std::function<void(const CnPacket&)>;
using CnEncodeEosCallback = std::function<void()>;
using CnEncodePerfCallback = std::function<void(const CnEncodePerfInfo&)>;

STREAMLIBS_REGISTER_EXCEPTION(CnEncode);

class CnEncodeHandler;

class CnEncode {
 public:
  friend class CnEncodeHandler;
  struct Attr {
    /*****************************
     * the maximum resolution that this endecoder can handle.
     *****************************/
    ::CnGeometry maximum_geometry;
    /*****************************
     * the resolution of the output video.
     *****************************/
    ::CnGeometry output_geometry;
    /*****************************
     * input pixel format
     *****************************/
    CnPixelFormat pixel_format;
    /*****************************
     * output codec type
     * support h264/jpeg on mlu100
     * support h264/h265/jpeg on mlu200
     *****************************/
    CnCodecType codec_type;
    /*****************************
     * qulity factor for jpeg encoder
     *****************************/
    uint32_t jpeg_qfactor = 50;
    /*****************************
     * profile and level for video encoder
     *****************************/
    CnVideoProfile profile = CnVideoProfile::MAIN;
    uint32_t level;
    /*****************************
     * video rate control parameters
     *****************************/
    CnRateControl rate_control;
    /*****************************
     * crop parameters
     *****************************/
    CnCropConfig crop_config;
    bool color2gray = false;
    /*****************************
     * packet memory on cpu or mlu
     *****************************/
    bool output_on_cpu = true;
    /*****************************
     * output buffer number
     *****************************/
    uint32_t packet_buffer_num = 4;
    /*****************************
     * whether to print useful messages.
     *****************************/
    bool silent = false;
    /*****************************
     * callback for receive packet
     *****************************/
    CnEncodePacketCallback packet_callback = NULL;
    /*****************************
     * callback for receive eos
     *****************************/
    CnEncodeEosCallback eos_callback = NULL;
    /*****************************
     * callback for receive performance informations each packet
     *****************************/
    CnEncodePerfCallback perf_callback = NULL;
    /*****************************
     * create encoder on which device
     *****************************/
    int dev_id = 0;
  };

  static CnEncode* Create(const Attr& attr);

  Attr attr() const;

  ~CnEncode();

  /**************************************
   * send frame to encoder.
   **************************************/
  bool SendData(const CnFrame& frame, bool eos = false);

  /**************************************
   * release codec buffer each time received packet
   * when Attr::output_on_cpu is true.
   **************************************/
  void ReleaseBuffer(uint32_t buf_id);

  /**************************************
   * copy out packet to dst.
   **************************************/
  bool CopyPacket(void* dst, const CnPacket& packet);

 private:
  CnEncode() {}

  Attr attr_;

  CnEncodeHandler* handler_;

  // DISABLE COPY AND ASSIGN
  CnEncode(const CnEncode&) = delete;
  const CnEncode& operator=(const CnEncode&) = delete;
};  // class CnEncode

inline CnEncode::Attr CnEncode::attr() const { return attr_; }

}  // namespace libstream

#endif  // LIBSTREAM_INCLUDE_CNENCODE_H_
