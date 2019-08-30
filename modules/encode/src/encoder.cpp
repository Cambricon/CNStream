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

#include "encoder.hpp"
#include "cnstream_eventbus.hpp"

namespace cnstream {

Encoder::Encoder(const std::string &name) : Module(name) {}

EncoderContext *Encoder::GetEncoderContext(CNFrameInfoPtr data) {
  EncoderContext *ctx = nullptr;
  auto search = encode_ctxs_.find(data->channel_idx);
  if (search != encode_ctxs_.end()) {
    // context exists
    ctx = search->second;
  } else {
    ctx = new EncoderContext;
    ctx->size = cv::Size(data->frame.width, data->frame.height);
    std::string video_file = output_dir_ + "/" + std::to_string(data->channel_idx) + ".avi";
    ctx->writer = std::move(cv::VideoWriter(video_file, CV_FOURCC('D', 'I', 'V', 'X'), 20, ctx->size));
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
    return false;
  }
  output_dir_ = paramSet["dump_dir"];
  return true;
}

void Encoder::Close() {
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
  EncoderContext *ctx = GetEncoderContext(data);

  const int width = data->frame.width;
  const int height = data->frame.height;

  uint8_t *img_data = new uint8_t[data->frame.GetBytes()];
  uint8_t *t = img_data;

  for (int i = 0; i < data->frame.GetPlanes(); ++i) {
    memcpy(t, data->frame.data[i]->GetCpuData(), data->frame.GetPlaneBytes(i));
    t += data->frame.GetPlaneBytes(i);
  }

  cv::Mat img;

  switch (data->frame.fmt) {
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      img = cv::Mat(height, width, CV_8UC3, img_data);
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      img = cv::Mat(height, width, CV_8UC3, img_data);
      cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
      img = bgr;
    } break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
      img = bgr;
    } break;
    default:
      LOG(WARNING) << "[Encoder] Unsupport pixel format.";
      delete[] img_data;
      return -1;
  }

  ctx->writer.write(img);

  delete[] img_data;

  return 0;
}

}  // namespace cnstream