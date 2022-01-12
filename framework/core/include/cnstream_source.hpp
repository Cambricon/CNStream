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
#include <memory>
#include <string>
#include <map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

class SourceHandler;
/*!
 * @class SourceModule
 *
 * @brief SourceModule is the base class of source modules.
 */
class SourceModule : public Module {
 public:
  /**
   * @brief Constructs a source module.
   *
   * @param[in] name The name of the source module.
   *
   * @return No return value.
   */
  explicit SourceModule(const std::string &name) : Module(name) { hasTransmit_.store(1); }
  /**
   * @brief Destructs a source module.
   *
   * @return No return value.
   */
  virtual ~SourceModule() { RemoveSources(); }
  /**
   * @brief Adds one stream to DataSource module. This function should be called after pipeline starts.
   *
   * @param[in] handler The source handler
   *
   * @retval Returns 0 for success, otherwise returns -1.
   */
  int AddSource(std::shared_ptr<SourceHandler> handler);
  /**
   * @brief Destructs a source module.
   *
   * @param[in] stream_id The stream identifier.
   *
   * @return Returns the handler of the stream.
   */
  std::shared_ptr<SourceHandler> GetSourceHandler(const std::string &stream_id);
  /**
   * @brief Removes one stream from ::DataSource module with given handler. This function should be called before
   * pipeline stops.
   *
   * @param[in] handler The handler of one stream.
   * @param[in] force The flag describing the removing behaviour.
   *
   * @retval 0: success (always success by now).
   *
   * @note If ``force`` sets to true, the stream will be removed immediately, otherwise the stream will be removed after
   * all cached frames are processed.
   */
  int RemoveSource(std::shared_ptr<SourceHandler> handler, bool force = false);
  /**
   * @brief Removes one stream from DataSource module with given the stream identification. This function should be
   * called before pipeline stops.
   *
   * @param[in] stream_id The stream identification.
   * @param[in] force The flag describing the removing behaviour.
   *
   * @retval 0: success (always success by now).
   *
   * @note If ``force`` sets to true, the stream will be removed immediately, otherwise the stream will be removed after
   * all cached frames are processed.
   */
  int RemoveSource(const std::string &stream_id, bool force = false);
  /**
   * @brief Removes all streams from DataSource module.
   *
   * @param[in] force The flag describing the removing behaviour.
   *
   * @retval 0: success (always success by now).
   *
   * @note If ``force`` sets to true, the stream will be removed immediately, otherwise the stream will be removed after
   * all cached frames are processed.
   */
  int RemoveSources(bool force = false);

#ifdef UNIT_TEST
 public:  // NOLINT
#else
 protected:  // NOLINT
#endif

  friend class SourceHandler;
  /**
   * @brief Gets the stream index with the given stream identifier.
   *
   * @param[in] stream_id The stream identifier.
   *
   * @return Returns the stream index.
   */
  uint32_t GetStreamIndex(const std::string &stream_id);
  /**
   * @brief Gives back the stream index to pipeline.
   *
   * @param[in] stream_id The stream identifier.
   *
   * @return No return value.
   */
  void ReturnStreamIndex(const std::string &stream_id);
  /**
   * @brief Transmits data to next stage(s) of the pipeline.
   *
   * @param[in] data The data to be transmitted.
   *
   * @return Returns true if data is transmitted successfully, othersize returns false.
   */
  bool SendData(std::shared_ptr<CNFrameInfo> data);

 private:
  int Process(std::shared_ptr<CNFrameInfo> data) override {
    (void)data;
    LOGE(CORE) << "As a source module, Process() should not be invoked\n";
    return 0;
  }

  std::mutex mutex_;
  std::map<std::string /*stream_id*/, std::shared_ptr<SourceHandler>> source_map_;
};

/**
 * @class SourceHandler
 *
 * @brief SourceHandler is a class that handles various sources, such as RTSP and video file.
 */
class SourceHandler : private NonCopyable {
 public:
  /**
   * @brief Constructs a source handler.
   *
   * @param[in] module The source module this handler belongs to.
   * @param[in] stream_id The name of the stream.
   *
   * @return No return value.
   */
  explicit SourceHandler(SourceModule *module, const std::string &stream_id) : module_(module), stream_id_(stream_id) {
    if (module_) {
      stream_index_ = module_->GetStreamIndex(stream_id_);
    }
  }
  /**
   * @brief Destructs a source module.
   *
   * @return No return value.
   */
  virtual ~SourceHandler() {
    if (module_) {
      module_->ReturnStreamIndex(stream_id_);
    }
  }
  /**
   * @brief Opens a decoder.
   *
   * @return Returns true if a decoder is opened successfully, otherwise returns false.
   */
  virtual bool Open() = 0;
  /**
   * @brief Closes a decoder.
   *
   * @return No return value.
   */
  virtual void Close() = 0;
  /**
   * @brief Stops a decoder. The Close() function should be called afterwards.
   *
   * @return No return value.
   */
  virtual void Stop() { }
  /**
   * @brief Gets the stream identification.
   *
   * @return Returns the name of stream.
   */
  std::string GetStreamId() const { return stream_id_; }
  /**
   * @brief Creates the context of ``CNFameInfo`` .
   *
   * @param[in] eos The flag marking the frame is end of stream.
   * @param[in] payload The payload of ``CNFameInfo``. It's useless now.
   *
   * @return Returns the context of ``CNFameInfo`` .
   */
  std::shared_ptr<CNFrameInfo> CreateFrameInfo(bool eos = false, std::shared_ptr<CNFrameInfo> payload = nullptr) {
    std::shared_ptr<CNFrameInfo> data = CNFrameInfo::Create(stream_id_, eos, payload);
    if (data) {
      data->SetStreamIndex(stream_index_);
    }
    return data;
  }
  /**
   * @brief Sends data to next module.
   *
   * @param[in] data The data need to be sent to next modules.
   *
   * @return Returns true if send data successfully, otherwise returns false.
   */
  bool SendData(std::shared_ptr<CNFrameInfo> data) {
    if (this->module_) {
      return this->module_->SendData(data);
    }
    return false;
  }

 protected:
  SourceModule *module_ = nullptr;
  mutable std::string stream_id_;
  uint32_t stream_index_ = INVALID_STREAM_IDX;
};

}  // namespace cnstream

#endif  // CNSTREAM_SOURCE_HPP_
