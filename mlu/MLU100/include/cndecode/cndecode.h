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
#ifndef LIBSTREAM_INCLUDE_CNDECODE_H_
#define LIBSTREAM_INCLUDE_CNDECODE_H_
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include "cnbase/cntypes.h"
#include "cnbase/streamlibs_error.h"
#include "cnvformat/cnvformat.h"

namespace libstream {

enum CnVideoMode {
  FRAME_MODE,  // packet with one frame
  STREAM_MODE  // stream packet
};

/*************************************
 * performance info for decode
 * supported on mlu100.
 *************************************/
typedef struct {
  /***********************************************
   * transfer from codec to mlu for this frame
   * units: microsecond
   ***********************************************/
  uint64_t transfer_us;
  /***********************************************
   * decode delay for this frame.
   * units: microsecond
   ***********************************************/
  uint64_t decode_us;
  /***********************************************
   * total delay(from send data to frame callback)
   * units: microsecond
   ***********************************************/
  uint64_t total_us;
  /***********************************************
   * pts for this frame
   ***********************************************/
  uint64_t pts;
}CnDecodePerfInfo;

using CnDecodeFrameCallback = std::function<void(const CnFrame&)>;
using CnDecodeEOSCallback = std::function<void()>;
using CnDecodePerfCallback = std::function<void(const CnDecodePerfInfo&)>;

class CnDecodeHandler;
STREAMLIBS_REGISTER_EXCEPTION(CnDecode);
class CnDecode {
 public:
  struct Attr {
    /*************************
     * the rate of drop frame
     * useful for mlu100
     *************************/
    double drop_rate = 0;
    /*****************************
     * the maximum resolution that this decoder can handle.
     *****************************/
    ::CnGeometry maximum_geometry;
    /*****************************
     * the resolution of the output frames.
     *****************************/
    ::CnGeometry output_geometry;
    /*****************************
     * substream geometry, only support on mllu100.
     * if substream_geometry.w and h is 0, then substream not open.
     *****************************/
    ::CnGeometry substream_geometry;
    CnCodecType codec_type;
    /*****************************
     * video mode, support on mlu100
     *****************************/
    CnVideoMode video_mode;
    /*****************************
     * the pixel format of output frames.
     * use SupportedPixelFormat
     * to judge whether the format is supported.
     *****************************/
    CnPixelFormat pixel_format;
    /*****************************
     * the output buffer count.
     *****************************/
    uint32_t frame_buffer_num = 3;
    /*****************************
     * interlaced data or progressive data
     * not supported on mlu100.
     *****************************/
    bool interlaced = false;
    /*****************************
     * frame callback
     *****************************/
    CnDecodeFrameCallback frame_callback = NULL;
    /*****************************
     * substream callback.
     * supported on mlu100
     *****************************/
    CnDecodeFrameCallback substream_callback = NULL;
    /*****************************
     * decode perfomance infomations callback for each frame
     *****************************/
    CnDecodePerfCallback perf_callback = NULL;
    /*****************************
     * eos callback
     *****************************/
    CnDecodeEOSCallback eos_callback = NULL;
    /*****************************
     * whether to print useful messages.
     *****************************/
    bool silent = false;
    /*****************************
     * create decoder on which device
     *****************************/
    int dev_id = 0;
  };  // struct Attr

  enum Status {
    /*********************
     * running, SendData and Callback are active.
     *********************/
    RUNNING,
    /*********************
     * pause, SendData and Callback are blocked.
     *********************/
    PAUSE,
    /*********************
     * stopped, decoder was destroied.
     *********************/
    STOP,
    /*********************
     * received eos.
     *********************/
    EOS
  };  // Enum Status

  /***********************************
   * create decoder by attr.
   * when error occurs, an StreamlibsError is thrown.
   ***********************************/
  static CnDecode* Create(const Attr& attr) noexcept(false);

  Attr attr() const;
  Status status() const;

  /***********************************
   * start decode.
   * status turn to RUNNING.
   * you'd better call it just once before SendData.
   ***********************************/
  // bool Start();

  /***********************************
   * status turn to PAUSE when status is RUNNING.
   ***********************************/
  bool Pause();

  /***********************************
   * status turn to RUNNING when status is PAUSE.
   * resume SendData and Callback from blocked.
   ***********************************/
  bool Resume();

  /***********************************
   * status turn to STOP and decoder to be destroied.
   * if status is STOP now, return false.
   ***********************************/
  // bool Stop();

  /***********************************
   * send data to decoder.
   * block when STATUS is pause.
   * An StreamLibsError is thrown when send data failed.
   * return false when STATUS is not UNINITIALIZED or STOP.
   ***********************************/
  bool SendData(const CnPacket& packet, bool eos = false) noexcept(false);

  /***********************************
   * release decoder's buffer.
   * after use buffer get in [frame_callback], remember to release it
   * use [CnFrame::buf_id]. Or decode maybe blocked.
   ***********************************/
  void ReleaseBuffer(uint32_t buf_id);

  /***********************************
   * copy frame from device to host.
   * when error occurs, return false.
   ***********************************/
  bool CopyFrame(void* dst, const CnFrame& frame);

  /***********************************
   * substream open or not
   ***********************************/
  bool SubstreamOpened() const;

  ~CnDecode();

  friend class CnDecodeHandler;

 private:
  CnDecode();
  CnDecode(const CnDecode&) = delete;
  CnDecode& operator=(const CnDecode&) = delete;
  Attr attr_;
  /******************************************
   * status is RUNNING after object be constructed.
   ******************************************/
  Status status_ = RUNNING;
  std::mutex status_mtx_;
  std::condition_variable status_cond_;
  CnDecodeHandler* handler_ = nullptr;

  /******************************************
   * eos workarround
   ******************************************/
  std::mutex eos_mtx_;
  std::condition_variable eos_cond_;
  bool send_eos_ = false;
  bool got_eos_ = false;

  uint32_t packets_count_ = 0;
  uint32_t frames_count_ = 0;
};  // class CnDecode

inline CnDecode::Attr CnDecode::attr() const {
  return attr_;
}

inline CnDecode::Status CnDecode::status() const {
  return status_;
}

inline bool CnDecode::SubstreamOpened() const {
  return attr_.substream_geometry.w * attr_.substream_geometry.h > 0;
}

}  // namespace libstream

#endif  // LIBSTREAM_INCLUDE_CNDECODE_H_

