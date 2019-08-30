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

#include <algorithm>
#include <atomic>
#include <functional>

#include "data_handler_ffmpeg.hpp"
#include "data_source.hpp"
#include "glog/logging.h"

namespace cnstream {

DataSource::DataSource(const std::string &name) : Module(name) {}

DataSource::~DataSource() { Close(); }

/* paramSet:
 *    "source_type": "video","image","rtsp"...
 *    "source_path":  local file path, rtsp/rtmp url, ...
 *    "decoder_type": "mlu"...
 */
bool DataSource::Open(ModuleParamSet paramSet) {
  param_set_ = paramSet;
  return true;
}

void DataSource::Close() {}

int DataSource::Process(std::shared_ptr<CNFrameInfo> data) {
  (void)data;
  LOG(ERROR) << "As a source module, Process() should not be invoked\n";
  return 0;
}

/*SendData() will be called by DataHandler*/
bool DataSource::SendData(std::shared_ptr<CNFrameInfo> data) {
  if (container_) {
    return container_->ProvideData(this, data);
  }
  return false;
}

int DataSource::AddVideoSource(const std::string &stream_id, const std::string &filename, int framerate, bool loop) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (source_map_.find(stream_id) != source_map_.end()) {
    LOG(ERROR) << "Duplicate stream_id\n";
    return -1;
  }
  auto source = std::make_shared<DataHandlerFFmpeg>(this, stream_id, filename, framerate, loop);
  if (source.get() != nullptr) {
    if (source->Open() != true) {
      LOG(ERROR) << "source Open failed";
      return -1;
    }
    source_map_[stream_id] = source;
  }
  return 0;
}

int DataSource::AddImageSource(const std::string &stream_id, const std::string &filename, bool loop) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (source_map_.find(stream_id) != source_map_.end()) {
    LOG(ERROR) << "Duplicate stream_id\n";
    return -1;
  }
  auto source = std::make_shared<DataHandlerFFmpeg>(this, stream_id, filename, -1, loop);
  if (source.get() != nullptr) {
    source_map_[stream_id] = source;
    source->Open();
  }
  return 0;
}

int DataSource::RemoveSource(const std::string &stream_id) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto iter = source_map_.find(stream_id);
  if (iter == source_map_.end()) {
    LOG(WARNING) << "source does not exist\n";
    return 0;
  }
  iter->second->Close();
  source_map_.erase(iter);
  return 0;
}

}  // namespace cnstream
