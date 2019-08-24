/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef MODULES_DECODE_INCLUDE_DECODER_HPP_
#define MODULES_DECODE_INCLUDE_DECODER_HPP_

#include <memory>
#include <utility>

#include <cndecode/cndecode.h>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

struct DecoderAttribute {
  uint32_t max_video_w = 1920;                                      // maximum decodable video resolution
  uint32_t max_video_h = 1080;                                      // maximum decodable video resolution
  libstream::CnCodecType codec_type = libstream::H264;              /* Use IsSupported(libstream::CnCodecType)
              to determine whether this codec type supports. */
  libstream::CnPixelFormat pixel_format = libstream::YUV420SP_NV21; /* Use IsSupported(libstream::CnPixelFormat)
 to determine whether this pixel format supports. */
  uint32_t output_frame_w; /* output frame size, it takes effect when IsSupported(SPECIFY_THE_OUTPUT_FRAME_SIZE)
 return true. */
  uint32_t output_frame_h; /* output frame size, it takes effect when IsSupported(SPECIFY_THE_OUTPUT_FRAME_SIZE)
 return true. */
  float drop_rate = 0;     /* output rate = (1.0f - rate_scale) * input rate, it takes effect when
     IsSupported(SPECIFY_DROP_RATE) return true. */
  uint32_t frame_buffer_num = 3;
  int dev_id = 0;              // device index, specify create decode instance on which MLU device(ie. 0).
  bool output_on_cpu = false;  // specify frame output on cpu or mlu.
  libstream::CnVideoMode video_mode = libstream::FRAME_MODE;
};

enum DecoderExtraAbility { SPECIFY_THE_OUTPUT_FRAME_SIZE = 0, SPECIFY_DROP_RATE };

struct DecodeHandler;

class Decoder : public Module {
 public:
  static bool IsSupported(libstream::CnCodecType type);
  static bool IsSupported(libstream::CnPixelFormat format);
  static bool IsSupported(DecoderExtraAbility ability);

  /*
    @brief
    @param
      receiver[in]: Specify which Pipeline frame will be post to.
  */
  explicit Decoder(const std::string& name);
  ~Decoder();

  bool Open(ModuleParamSet paramSet) override;
  void Close() override;
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /*
    @brief open decode channel.
    @return
      return channel index for success, channel index will be act in output(CNDataFrame::channel_idx) and
    Decoder::SendData. channel index is ordered numbers starting with 0. return -1 for open failed and error message
    will be print.
    @attention:
      after use, close it by CloseDecodeChannel.
   */
  int OpenDecodeChannel(const DecoderAttribute& attr);

  bool CloseDecodeChannel(uint32_t channel_idx, bool print_perf_info = false);

  bool SendPacket(uint32_t channel_idx, const libstream::CnPacket& packet, bool eos = false);

  /*
    @brief print decode performance infomations, do it after close all decode channel.
   */
  void PrintPerformanceInfomation() const;

 private:
  std::map<uint32_t, DecodeHandler*> handlers_;
  std::vector<uint32_t> closed_channels_;
  int max_channel_idx_ = -1;
};  // class Decoder

}  // namespace cnstream

#endif  // MODULES_DECODE_INCLUDE_DECODER_HPP_
