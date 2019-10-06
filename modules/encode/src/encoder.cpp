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
  ctx->writer.write(*data->frame.ImageBGR());
  return 0;
}

}  // namespace cnstream