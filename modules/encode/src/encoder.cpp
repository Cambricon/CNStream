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

#include <string>
#include <thread>
#include <utility>

#include "cnstream_eventbus.hpp"
#include "encoder.hpp"

namespace cnstream {

Encoder::Encoder(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("Encoder is a module for encode video or images.");
  param_register_.Register("dump_dir",
                           "Where to store the encoded video."
                           " For example, '.' means storing to current directory.");
  param_register_.Register("dump_type",
                           "dump type, \"video\" or \"image\".");
}

#ifdef CNS_MLU220_SOC
static std::mutex dir_mutex;
static bool CreateDirectory(const std::string &dir_path) {
  if (dir_path.empty()) return true;
  std::unique_lock<std::mutex> lock(dir_mutex);
  std::string path;
  std::string sub;
  for (auto it = dir_path.begin(); it != dir_path.end(); ++it) {
    const char c = *it;
    sub.push_back(c);
    if (c == '/') {
      path.append(sub);
      if (access(path.c_str(), 0)) {
        int err = mkdir(path.c_str(), 0644);
        if (err) {
          LOG(ERROR) << "Failed ot create directory";
          return false;
        } else {
          LOG(INFO) << "create dir: " << path;
        }
      }
    }
    sub.clear();
  }

  path = dir_path;
  if (access(path.c_str(), 0)) {
    int err = mkdir(path.c_str(), 0644);
    if (err) {
      LOG(ERROR) << "Failed ot create directory";
      return false;
    } else {
      LOG(INFO) << "create dir: " << path;
    }
  }
  return true;
}
#endif

EncoderContext *Encoder::GetEncoderContext(CNFrameInfoPtr data) {
  std::unique_lock<std::mutex> lock(encoder_mutex_);
  if (data->channel_idx >= GetMaxStreamNumber()) {
    return nullptr;
  }
  EncoderContext *ctx = nullptr;
  auto search = encode_ctxs_.find(data->channel_idx);
  if (search != encode_ctxs_.end()) {
    // context exists
    ctx = search->second;
  } else {
    ctx = new (std::nothrow) EncoderContext;
    LOG_IF(FATAL, nullptr == ctx) << "Encoder::GetEncoderContext() new EncoderContext failed";
    ctx->size = cv::Size(data->frame.width, data->frame.height);
    std::string filename = std::to_string(data->channel_idx) + ".avi";
    std::string video_file;
    if (output_dir_.empty())
      video_file = filename;
    else
      video_file = output_dir_ + "/" + filename;
#ifdef CNS_MLU220_SOC
    if (CreateDirectory(output_dir_) != true) {
      delete ctx;
      return nullptr;
    }
    ctx->writer = std::move(cv::VideoWriter(video_file, CV_FOURCC('M', 'J', 'P', 'G'), 20, ctx->size));
#else
    ctx->writer = std::move(cv::VideoWriter(video_file, CV_FOURCC('V', 'P', '8', '0'), 20, ctx->size));
#endif
    if (!ctx->writer.isOpened()) {
      PostEvent(cnstream::EventType::EVENT_ERROR, "Create video file failed");
    }
    encode_ctxs_[data->channel_idx] = ctx;
  }
  return ctx;
}

Encoder::~Encoder() { Close(); }

bool Encoder::Open(ModuleParamSet paramSet) {
  if (paramSet.find("dump_dir") == paramSet.end()) {
    char *path;
    path = getcwd(NULL, 0);
    output_dir_ = path;
    // return false;
  } else {
    output_dir_ = paramSet["dump_dir"];
  }

  if (paramSet["dump_type"] == "image") {
    dump_as_image_ = true;
  } else {
    dump_as_image_ = false;
  }
  // 1, one channel binded to one thread, it can't be one channel binded to multi threads.
  // 2, the hash value, each channel_idx (key) mapped to, is unique. So, each bucket stores one value.
  // 3, set the buckets number of the unordered map to the maximum channel number before the threads are started,
  //    thus, it doesn't need to be rehashed after.
  // The three conditions above will guarantee, multi threads will write the different buckets of the unordered map,
  // and the unordered map will not be rehashed after, so, it will not cause thread safe issue, when multi threads write
  // the unordered map at the same time without locks.
  encode_ctxs_.rehash(GetMaxStreamNumber());
  return true;
}

void Encoder::Close() {
  std::unique_lock<std::mutex> lock(encoder_mutex_);
  if (encode_ctxs_.empty()) {
    return;
  }
  for (auto &pair : encode_ctxs_) {
    pair.second->writer.release();
    delete pair.second;
  }
  encode_ctxs_.clear();
}

int Encoder::Process(CNFrameInfoPtr data) {
  if (dump_as_image_) {
    cv::imwrite(output_dir_ + "/ch" + std::to_string(data->channel_idx)
        + "_stream" + data->frame.stream_id + "_frame"
        + std::to_string(data->frame.frame_id)
        + ".jpg", *data->frame.ImageBGR());
  } else {
    EncoderContext *ctx = GetEncoderContext(data);
    if (ctx == nullptr) {
      LOG(ERROR) << "Get Encoder Context Failed.";
      return -1;
    }
    ctx->writer.write(*data->frame.ImageBGR());
  }
  return 0;
}

bool Encoder::CheckParamSet(const ModuleParamSet &paramSet) const {
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Encoder] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("dump_type") != paramSet.end()) {
    if (paramSet.at("dump_type") != "video" && paramSet.at("dump_type") != "image") {
      LOG(ERROR) << "[Encoder] (ERROR) unsupported dump type: \""
          << paramSet.at("dump_type") << "\". Choose from \"video\" and \"image\".";
      return false;
    }
  }
  return true;
}

}  // namespace cnstream
