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
#include "data_source.hpp"
#include <algorithm>
#include <atomic>
#include <functional>
#include "data_handler_ffmpeg.hpp"
#include "data_handler_raw.hpp"
#include "glog/logging.h"

namespace cnstream {

DataSource::DataSource(const std::string &name) : Module(name) {}

DataSource::~DataSource() { Close(); }

static int GetDeviceId(ModuleParamSet paramSet) {
  if (paramSet.find("device_id") == paramSet.end()) {
    return -1;
  }
  std::stringstream ss;
  int device_id;
  ss << paramSet["device_id"];
  ss >> device_id;
  /*check device_id valid or not,FIXME*/
  //...
  return device_id;
}

bool DataSource::Open(ModuleParamSet paramSet) {
  if (paramSet.find("source_type") != paramSet.end()) {
    std::string source_type = paramSet["source_type"];
    if (source_type == "ffmpeg") {
      param_.source_type_ = SOURCE_FFMPEG;
    } else if (source_type == "raw") {
      param_.source_type_ = SOURCE_RAW;
    } else {
      LOG(ERROR) << "source_type " << paramSet["source_type"] << "not supported";
      return false;
    }
  }

  if (paramSet.find("output_type") != paramSet.end()) {
    std::string dec_type = paramSet["output_type"];
    if (dec_type == "cpu") {
      param_.output_type_ = OUTPUT_CPU;
    } else if (dec_type == "mlu") {
      param_.output_type_ = OUTPUT_MLU;
    } else {
      LOG(ERROR) << "output_type " << paramSet["output_type"] << "not supported";
      return false;
    }
    if (param_.output_type_ == OUTPUT_MLU) {
      param_.device_id_ = GetDeviceId(paramSet);
      if (param_.device_id_ < 0) {
        LOG(ERROR) << "output_type MLU : device_id must be set";
        return false;
      }
    }
  }

  if (paramSet.find("interval") != paramSet.end()) {
    std::stringstream ss;
    int interval;
    ss << paramSet["interval"];
    ss >> interval;
    if (interval <= 0) {
      LOG(ERROR) << "interval : invalid";
      return false;
    }
  }

  if (paramSet.find("decoder_type") != paramSet.end()) {
    std::string dec_type = paramSet["decoder_type"];
    if (dec_type == "cpu") {
      param_.decoder_type_ = DECODER_CPU;
    } else if (dec_type == "mlu") {
      param_.decoder_type_ = DECODER_MLU;
    } else {
      LOG(ERROR) << "decoder_type " << paramSet["decoder_type"] << "not supported";
      return false;
    }
    if (dec_type == "mlu") {
      param_.device_id_ = GetDeviceId(paramSet);
      if (param_.device_id_ < 0) {
        LOG(ERROR) << "decoder_type MLU : device_id must be set";
        return false;
      }
    }
  }

  if (param_.decoder_type_ == DECODER_MLU) {
    param_.reuse_cndec_buf = false;
    if (paramSet.find("reuse_cndec_buf") != paramSet.end()) {
      if (paramSet["reuse_cndec_buf"] == "true") {
        param_.reuse_cndec_buf = true;
      } else {
        param_.reuse_cndec_buf = false;
      }
    }
  }

  if (param_.source_type_ == SOURCE_RAW) {
    if (paramSet.find("chunk_size") == paramSet.end() || paramSet.find("width") == paramSet.end() ||
        paramSet.find("height") == paramSet.end() || paramSet.find("interlaced") == paramSet.end()) {
      return false;
    }
    {
      std::stringstream ss;
      ss << paramSet["chunk_size"];
      ss >> param_.chunk_size_;
    }
    {
      std::stringstream ss;
      ss << paramSet["width"];
      ss >> param_.width_;
    }
    {
      std::stringstream ss;
      ss << paramSet["height"];
      ss >> param_.height_;
    }
    {
      std::stringstream ss;
      size_t interlaced;
      ss << paramSet["interlaced"];
      ss >> interlaced;
      param_.interlaced_ = (interlaced == 0) ? false : true;
    }
  }
  return true;
}

void DataSource::Close() { RemoveSources(); }

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

  std::shared_ptr<DataHandler> source;
  if (param_.source_type_ == SOURCE_RAW) {
    source = std::make_shared<DataHandlerRaw>(this, stream_id, filename, framerate, loop);
  } else if (param_.source_type_ == SOURCE_FFMPEG) {
    source = std::make_shared<DataHandlerFFmpeg>(this, stream_id, filename, framerate, loop);
  } else {
    LOG(ERROR) << "source, not supported yet";
  }
  if (source.get() != nullptr) {
    if (source->Open() != true) {
      LOG(ERROR) << "source Open failed";
      return -1;
    }
    source_map_[stream_id] = source;
    return 0;
  }
  return -1;
}

int DataSource::AddImageSource(const std::string &stream_id, const std::string &filename, bool loop) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (source_map_.find(stream_id) != source_map_.end()) {
    LOG(ERROR) << "Duplicate stream_id\n";
    return -1;
  }

  if (param_.source_type_ == SOURCE_RAW) {
    LOG(ERROR) << "source raw, not implemented yet";
    return -1;
  }

  if (param_.source_type_ == SOURCE_FFMPEG) {
    auto source = std::make_shared<DataHandlerFFmpeg>(this, stream_id, filename, -1, loop);
    if (source.get() != nullptr) {
      source_map_[stream_id] = source;
      source->Open();
    }
    return 0;
  }

  LOG(ERROR) << "unsupported source type";
  return -1;
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

int DataSource::RemoveSources() {
  std::unique_lock<std::mutex> lock(mutex_);
  std::map<std::string, std::shared_ptr<DataHandler>>::iterator iter;
  for (iter = source_map_.begin(); iter != source_map_.end();) {
    iter->second->Close();
    source_map_.erase(iter++);
  }
  return 0;
}

}  // namespace cnstream
