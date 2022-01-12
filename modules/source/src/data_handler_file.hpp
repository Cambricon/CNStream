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

#ifndef MODULES_SOURCE_HANDLER_FILE_HPP_
#define MODULES_SOURCE_HANDLER_FILE_HPP_

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "cnstream_logging.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "util/video_parser.hpp"
#include "util/video_decoder.hpp"


namespace cnstream {

class FileHandlerImpl : public IParserResult, public IDecodeResult, public SourceRender {
 public:
  explicit FileHandlerImpl(DataSource *module, const std::string &filename, int framerate, bool loop,
                           const MaximumVideoResolution& maximum_resolution, FileHandler *handler)  // NOLINT
      :SourceRender(handler), module_(module), filename_(filename),
       framerate_(framerate), loop_(loop), maximum_resolution_(maximum_resolution), handler_(*handler),
       stream_id_(handler_.GetStreamId()), parser_(stream_id_) {}
  ~FileHandlerImpl() {}
  bool Open();
  void Close();
  void Stop();

 private:
  DataSource *module_ = nullptr;
  std::string filename_;
  int framerate_;
  bool loop_ = false;
  MaximumVideoResolution maximum_resolution_;
  FileHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;

#ifdef UNIT_TEST
 public:  // NOLINT
#else
 private:  // NOLINT
#endif
  bool PrepareResources(bool demux_only = false);
  void ClearResources(bool demux_only = false);
  bool Process();
  void Loop();

  // IParserResult methods
  void OnParserInfo(VideoInfo *info) override;
  void OnParserFrame(VideoEsFrame *frame) override;

  // IDecodeResult methods
  void OnDecodeError(DecodeErrorCode error_code) override;
  void OnDecodeFrame(DecodeFrame *frame) override;
  void OnDecodeEos() override;

  /**/
  std::atomic<int> running_{0};
  std::thread thread_;
  bool eos_sent_ = false;

 private:
  FFParser parser_;
  std::shared_ptr<Decoder> decoder_ = nullptr;
  bool dec_create_failed_ = false;
  bool decode_failed_ = false;
  bool eos_reached_ = false;

#ifdef UNIT_TEST
 public:  // NOLINT
  void SetDecodeParam(const DataSourceParam &param) { param_ = param; }
#endif
};  // class FileHandlerImpl

/***********************************************************************
 * @brief FrController is used to control the frequency of sending data.
 ***********************************************************************/
class FrController {
 public:
  FrController() {}
  explicit FrController(uint32_t frame_rate) : frame_rate_(frame_rate) {}
  void Start() { start_ = std::chrono::steady_clock::now(); }
  void Control() {
    if (0 == frame_rate_) return;
    double delay = 1000.0 / frame_rate_;
    end_ = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> diff = end_ - start_;
    auto gap = delay - diff.count() - time_gap_;
    if (gap > 0) {
      std::chrono::duration<double, std::milli> dura(gap);
      std::this_thread::sleep_for(dura);
      time_gap_ = 0;
    } else {
      time_gap_ = -gap;
    }
    Start();
  }
  inline uint32_t GetFrameRate() const { return frame_rate_; }
  inline void SetFrameRate(uint32_t frame_rate) { frame_rate_ = frame_rate; }

 private:
  uint32_t frame_rate_ = 0;
  double time_gap_ = 0;
  std::chrono::time_point<std::chrono::steady_clock> start_, end_;
};  // class FrController

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_FILE_HPP_
