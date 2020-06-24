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
#include <map>
#include <memory>
#include <string>
#include "data_handler_ffmpeg.hpp"
#include "data_handler_raw.hpp"
#include "glog/logging.h"

namespace cnstream {

DataSource::DataSource(const std::string &name) : SourceModule(name) {
  param_register_.SetModuleDesc("DataSource is a module for handling input data (videos or images)."
                                " Feed data to codec and send decoded data to the next module if there is one.");
  param_register_.Register("source_type", "Input source type. It could be ffmpeg or raw.");
  param_register_.Register("output_type", "Where the outputs will be stored. It could be cpu or mlu,"
                           "It is used when decoder_type is cpu.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");
  param_register_.Register("interval", "How many frames will be discarded between two frames"
                           " which will be sent to codec.");
  param_register_.Register("decoder_type", "Which the input data will be decoded by. It could be cpu or mlu.");
  param_register_.Register("output_width", "After decoding, the width of the output frames.");
  param_register_.Register("output_height", "After decoding, the height of the output frames.");
  param_register_.Register("reuse_cndec_buf", "This parameter decides whether the codec buffer that stores output data"
                           "will be held and reused by the framework afterwards. It should be true or false.");
  param_register_.Register("chunk_size", "How many bytes will be sent to codec once."
                           " Chunk size is used when source_type is raw.");
  param_register_.Register("width", "When source_type is raw, we need to set it to  the width of input data.");
  param_register_.Register("height", "When source_type is raw, we need to set it to the height of input data.");
  param_register_.Register("interlaced", "When source_type is raw, we need to set interlaced to fasle or true."
                           " It should be set according to the input data.");
  param_register_.Register("input_buf_number", "Codec buffer number for storing input data."
                           " Basically, we do not need to set it, as it will be allocated automatically.");
  param_register_.Register("output_buf_number", "Codec buffer number for storing output data."
                           " Basically, we do not need to set it, as it will be allocated automatically.");
}

DataSource::~DataSource() {}

static int GetDeviceId(ModuleParamSet paramSet) {
  if (paramSet.find("device_id") == paramSet.end()) {
    return -1;
  }
  std::stringstream ss;
  int device_id;
  ss << paramSet["device_id"];
  ss >> device_id;
  /*check device_id valid or not,FIXME*/
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
      LOG(ERROR) << "source_type " << paramSet["source_type"] << " not supported";
      return false;
    }
  }

  if (paramSet.find("output_type") != paramSet.end()) {
    std::string out_type = paramSet["output_type"];
    if (out_type == "cpu") {
      param_.output_type_ = OUTPUT_CPU;
    } else if (out_type == "mlu") {
      param_.output_type_ = OUTPUT_MLU;
    } else {
      LOG(ERROR) << "output_type " << paramSet["output_type"] << " not supported";
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
      LOG(ERROR) << "decoder_type " << paramSet["decoder_type"] << " not supported";
      return false;
    }
    if (dec_type == "mlu") {
      param_.device_id_ = GetDeviceId(paramSet);
      if (param_.device_id_ < 0) {
        LOG(ERROR) << "decoder_type MLU : device_id must be set";
        return false;
      }
    }
    if (dec_type == "mlu" && param_.output_type_ == OUTPUT_CPU) {
      LOG(ERROR) << "decoder_type MLU : output_type must be mlu.";
      return false;
    }
  }

  if (paramSet.find("output_width") != paramSet.end()) {
    std::string str_w = paramSet["output_width"];
    std::string str_h = paramSet["output_height"];
    size_t w = std::stoi(str_w);
    size_t h = std::stoi(str_h);
    if (w != 0) {
      param_.output_w = w;
    }
    if (h != 0) {
      param_.output_h = h;
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

  if (paramSet.find("input_buf_number") != paramSet.end()) {
    std::string ibn_str = paramSet["input_buf_number"];
    std::stringstream ss;
    ss << paramSet["input_buf_number"];
    ss >> param_.input_buf_number_;
  }

  if (paramSet.find("output_buf_number") != paramSet.end()) {
    std::string obn_str = paramSet["output_buf_number"];
    std::stringstream ss;
    ss << paramSet["output_buf_number"];
    ss >> param_.output_buf_number_;
  }

  return true;
}

void DataSource::Close() { RemoveSources(); }

std::shared_ptr<SourceHandler> DataSource::CreateSource(const std::string &stream_id, const std::string &filename,
                                                        int framerate, bool loop) {
  if (stream_id.empty() || filename.empty()) {
    LOG(ERROR) << "invalid stream_id or filename";
    return nullptr;
  }
  SourceHandler *ptr = nullptr;
  if (param_.source_type_ == SOURCE_RAW) {
    DataHandlerRaw* DataHandlerRaw_ptr = new(std::nothrow) DataHandlerRaw(this, stream_id, filename, framerate, loop);
    LOG_IF(FATAL, nullptr == DataHandlerRaw_ptr) << "DataSource::CreateSource() new DataHandlerRaw failed";
    ptr = dynamic_cast<SourceHandler *>(DataHandlerRaw_ptr);
  } else if (param_.source_type_ == SOURCE_FFMPEG) {
    DataHandlerFFmpeg* DataHandlerFFmpeg_ptr =
      new(std::nothrow) DataHandlerFFmpeg(this, stream_id, filename, framerate, loop);
    LOG_IF(FATAL, nullptr == DataHandlerFFmpeg_ptr) << "DataSource::CreateSource() new DataHandlerFFmpeg failed";
    ptr = dynamic_cast<SourceHandler *>(DataHandlerFFmpeg_ptr);
  } else {
    LOG(ERROR) << "source, not supported yet";
  }
  if (ptr != nullptr) {
    std::shared_ptr<SourceHandler> source(ptr);
    return source;
  }
  return nullptr;
}

bool DataSource::CheckParamSet(ModuleParamSet paramSet) {
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[DataSource] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("source_type") != paramSet.end()) {
    std::string source_type = paramSet["source_type"];
    if (source_type != "ffmpeg" && source_type != "raw") {
      LOG(ERROR) << "[DataSource] [source_type] " << paramSet["source_type"] << " not supported";
      return false;
    }
    if (source_type == "raw") {
      if (paramSet.find("chunk_size") == paramSet.end() || paramSet.find("width") == paramSet.end() ||
          paramSet.find("height") == paramSet.end() || paramSet.find("interlaced") == paramSet.end()) {
        LOG(ERROR) << "[DataSource] [source_type] raw : "
                   << "[chunk_size], [width], [height], [interlaced] must be set." << std::endl;
        return false;
      }
    }
  }

  if (paramSet.find("output_type") != paramSet.end()) {
    if (paramSet["output_type"] != "cpu" && paramSet["output_type"] != "mlu") {
      LOG(ERROR) << "[DataSource] [output_type] " << paramSet["output_type"] << " not supported";
      return false;
    }
    if (paramSet["output_type"] == "mlu") {
      int device_id = GetDeviceId(paramSet);
      if (device_id < 0) {
        LOG(ERROR) << "[DataSource] [output_type] MLU : device_id must be set";
        return false;
      }
    }
  }

  std::string err_msg;
  if (!checker.IsNum({"interval", "output_width", "output_height", "chunk_size", "width", "height", "input_buf_number",
                      "output_buf_number"},
                     paramSet, err_msg, true)) {
    LOG(ERROR) << "[DataSource] " << err_msg;
    return false;
  }

  if (paramSet.find("decoder_type") != paramSet.end()) {
    std::string dec_type = paramSet["decoder_type"];
    if (dec_type != "cpu" && dec_type != "mlu") {
      LOG(ERROR) << "[DataSource] [decoder_type] " << paramSet["decoder_type"] << " not supported.";
      return false;
    }

    if (dec_type == "mlu") {
      int device_id = GetDeviceId(paramSet);
      if (device_id < 0) {
        LOG(ERROR) << "[DataSource] [decoder_type] MLU : device_id must be set";
        return false;
      }

      if (paramSet.find("reuse_cndec_buf") != paramSet.end()) {
        std::string reuse = paramSet["reuse_cndec_buf"];
        if (reuse != "true" && reuse != "false") {
          LOG(ERROR) << "[DataSource] [reuse_cndec_buf] must be true or false";
          return false;
        }
      }
    }
  }

  return true;
}

}  // namespace cnstream
