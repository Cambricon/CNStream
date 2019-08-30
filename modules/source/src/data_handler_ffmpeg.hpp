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

#ifndef MODULES_SOURCE_HANDLER_FFMPEG_HPP_
#define MODULES_SOURCE_HANDLER_FFMPEG_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <string>
#include <thread>
#include "cndecode/cndecode.h"
#include "cnstream_timer.hpp"
#include "cnvformat/cnvformat.h"
#include "data_handler.hpp"
#include "data_source.hpp"

namespace cnstream {

class DataHandlerFFmpeg : public DataHandler {
 public:
  explicit DataHandlerFFmpeg(DataSource* module, const std::string& stream_id, const std::string& filename,
                             int framerate, bool loop)
      : DataHandler(module), stream_id_(stream_id), filename_(filename), frame_rate_(framerate), loop_(loop) {}
  ~DataHandlerFFmpeg() {
    Close();
    PrintPerformanceInfomation();
  }

  bool Open() override;
  void Close() override;

 public:
  bool CheckTimeOut(uint64_t ul_current_time);

 private:
  std::string stream_id_;
  std::string filename_;
  int frame_rate_;
  bool loop_;
  std::atomic<int> running_{0};
  std::thread thread_;
  void ExtractingLoop();

 private:
  // ffmpeg demuxer
  AVFormatContext* p_format_ctx_ = nullptr;
  AVBitStreamFilterContext* bitstream_filter_ctx_ = nullptr;
  AVDictionary* options_ = NULL;
  AVPacket packet_;
  int video_index_ = -1;
  bool first_frame_ = true;
  uint64_t last_receive_frame_time_ = 0;
  uint8_t max_receive_time_out_ = 3;
  bool find_pts_ = true;  // set it to true by default!

 private:
  // decoder handler
  DevContext dev_ctx_;
  uint32_t chn_idx_;
  uint64_t frame_id_ = 0;
  libstream::CnDecode* instance_ = nullptr;
  std::atomic<int> eos_got_{0};
  std::atomic<int> send_flow_eos_{0};

  bool SendPacket(const libstream::CnPacket& packet, bool eos);
  bool PrepareResources();
  void ClearResources();
  bool Extract(libstream::CnPacket* pdata);
  void ReleaseData(libstream::CnPacket* pdata);

  CNTimer fps_calculators[4];
  void PrintPerformanceInfomation() const {
    printf("stream_id: %s:\n", stream_id_.c_str());
    fps_calculators[0].PrintFps("transfer memory: ");
    fps_calculators[1].PrintFps("decode delay: ");
    fps_calculators[2].PrintFps("send data to codec: ");
    fps_calculators[3].PrintFps("output :");
  }
  void PerfCallback(const libstream::CnDecodePerfInfo& info) {
    fps_calculators[0].Dot(1.0f * info.transfer_us / 1000, 1);
    fps_calculators[1].Dot(1.0f * info.decode_us / 1000, 1);
    fps_calculators[2].Dot(1.0f * info.total_us / 1000, 1);
    fps_calculators[3].Dot(1);
  }
  void FrameCallback(const libstream::CnFrame& frame);
  void EOSCallback();
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_FFMPEG_HPP_
