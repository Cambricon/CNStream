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

#ifndef CNSTREAM_SOURCE_HPP_
#define CNSTREAM_SOURCE_HPP_

/**
 * @file cnstream_source.hpp
 *
 * This file contains a declaration of the Source Module class.
 */

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_module.hpp"

namespace cnstream {

class SourceHandler;
class SourceModule : public Module {
 public:
  explicit SourceModule(const std::string &name) : Module(name) { }
  /**
   * @brief Add one stream to DataSource module, should be called after pipeline starts.
   * @param
   *   stream_id[in]: unique stream identifier.
   *   filename[in]: source path, local-file-path/rtsp-url/jpg-sequences, etc.
   *   framerate[in]: source data input frequency
   *   loop[in]: whether to reload source when EOF is reached or not
   * @return
   *    0: success,
   *   -1: error occurs
   */
  int AddVideoSource(const std::string &stream_id, const std::string &filename, int framerate, bool loop = false);

  /**
   * @brief Remove one stream from DataSource module,should be called before pipeline stops.
   * @param
   *   stream_id[in]: unique stream identifier.
   * @return
   *    0: success (always success by now)
   */
  int RemoveSource(const std::string &stream_id);

  int Process(std::shared_ptr<CNFrameInfo> data) override;

 protected:
  friend class SourceHandler;
  uint32_t GetStreamIndex(const std::string &stream_id);
  void ReturnStreamIndex(const std::string &stream_id);
  /**
   * @brief Transmit data to next stage(s) of the pipeline
   * @param
   *   data[in]: data to be transmitted.
   * @return
   *   true if data is transmitted successfully,othersize false
   */
  bool SendData(std::shared_ptr<CNFrameInfo> data);

  virtual std::shared_ptr<SourceHandler> CreateSource(const std::string &stream_id, const std::string &filename,
                                                      int framerate, bool loop = false) = 0;

  /**
   * @hidebrief Remove all streams from DataSource module
   * @param
   * @return
   *    0: success (always success by now)
   */
  int RemoveSources();

 private:
  std::mutex mutex_;
  std::map<std::string /*stream_id*/, std::shared_ptr<SourceHandler>> source_map_;
};

class SourceHandler {
 public:
  explicit SourceHandler(SourceModule *module, const std::string &stream_id, int frame_rate, bool loop)
      : module_(module), stream_id_(stream_id), frame_rate_(frame_rate), loop_(loop) {
    stream_index_ = module_->GetStreamIndex(stream_id_);
  }
  virtual ~SourceHandler() { module_->ReturnStreamIndex(stream_id_); }

  virtual bool Open() = 0;
  virtual void Close() = 0;

 public:
  std::string GetStreamId() const { return stream_id_; }
  uint32_t GetStreamIndex() const { return stream_index_; }
  bool SendData(std::shared_ptr<CNFrameInfo> data) {
    if (this->module_) {
      return this->module_->SendData(data);
    }
    return false;
  }

 protected:
#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  SourceModule *module_ = nullptr;
  std::string stream_id_;
  int frame_rate_ = 0;
  bool loop_ = false;
  uint32_t stream_index_;
  std::shared_ptr<PerfManager> perf_manager_;
};

}  // namespace cnstream

#endif  // CNSTREAM_SOURCE_HPP_
