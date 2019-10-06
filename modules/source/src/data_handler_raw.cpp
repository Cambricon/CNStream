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
#include "data_handler_raw.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <future>
#include <sstream>
#include <thread>
#include <utility>
#include "cninfer/mlu_context.h"
namespace cnstream {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

bool DataHandlerRaw::PrepareResources() {
  fd_ = open(filename_.c_str(), O_RDONLY);
  if (fd_ < 0) {
    LOG(ERROR) << "Failed to open file: " << filename_;
    return false;
  }

  if (param_.chunk_size_) {
    if (chunk_) delete[] chunk_;
    chunk_ = new uint8_t[param_.chunk_size_];
    if (nullptr == chunk_) {
      LOG(ERROR) << "Failed to alloc memory";
      return false;
    }
  } else {
    LOG(ERROR) << "By now, only raw chunk mode supported";
    return false;
  }

  if (dev_ctx_.dev_id != DevContext::INVALID) {
    libstream::MluContext mlu_ctx;
    mlu_ctx.set_dev_id(dev_ctx_.dev_id);
    mlu_ctx.set_channel_id(dev_ctx_.ddr_channel);
    mlu_ctx.ConfigureForThisThread();
  }

  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<RawMluDecoder>(*this);
  } else {
    LOG(ERROR) << "unsupported decoder_type";
    return false;
  }
  if (decoder_.get()) {
    DecoderContext ctx;
    // FIXME, parse bitstream to get decoder parametesrs...
    if (filename_.find(".h264") != std::string::npos || filename_.find(".264") != std::string::npos) {
      ctx.codec_id = DecoderContext::CN_CODEC_ID_H264;
    } else if (filename_.find(".h265") != std::string::npos) {
      ctx.codec_id = DecoderContext::CN_CODEC_ID_HEVC;
      /*} else if(filename_.find(".jpg") != std::string::npos) {
        ctx.codec_id = DecoderContext::CN_CODEC_ID_JPEG;
      */
    } else {
      LOG(ERROR) << "unsupported raw file format";
      return false;
    }
    ctx.pix_fmt = DecoderContext::CN_PIX_FMT_NV21;
    ctx.interlaced = param_.interlaced_;
    ctx.width = param_.width_;
    ctx.height = param_.height_;
    ctx.chunk_mode = (param_.chunk_size_ != 0);
    bool ret = decoder_->Create(&ctx);
    if (ret) {
      decoder_->ResetCount(this->interval_);
#ifdef CNS_MLU100
      /*MLU100 does not have chunk-mode, use stream-mode instead*/
      if (param_.chunk_size_ <= ctx.width * ctx.height * 3 / 4) {
        chunk_size_ = param_.chunk_size_;
      } else {
        chunk_size_ = ctx.width * ctx.height * 3 / 4;
      }
#endif
      return true;
    }
    return false;
  }
  return false;
}

void DataHandlerRaw::ClearResources() {
  if (decoder_.get()) {
    EnableFlowEos(true);
    decoder_->Destroy();
  }
  if (fd_ > 0) {
    close(fd_), fd_ = -1;
  }
  if (chunk_) {
    delete[] chunk_, chunk_ = nullptr;
  }
}

bool DataHandlerRaw::Extract() {
  if (chunk_size_ > 0) {
    /*chunk mode*/
    ssize_t len = read(fd_, chunk_, chunk_size_);
    if (len <= 0) {
      // EOF reached
      packet_.data = nullptr;
      packet_.size = 0;
      packet_.pts = 0;
      return false;
    }
    packet_.data = chunk_;
    packet_.size = len;
    packet_.pts = pts_++;
    packet_.flags = 0;
    return true;
  }
  /*frame mode*/
  /*FIXME*/
  return false;
}

bool DataHandlerRaw::Process() {
  bool ret = Extract();
  if (!ret) {
    LOG(INFO) << "Read EOS from file";
    demux_eos_.store(1);
    if (this->loop_) {
      LOG(INFO) << "Clear resources and restart";
      EnableFlowEos(false);
      decoder_->Process(nullptr, true);
      ClearResources();
      PrepareResources();
      demux_eos_.store(0);
      LOG(INFO) << "Loop...";
      return true;
    } else {
      EnableFlowEos(true);
      decoder_->Process(nullptr, true);
      return false;
    }
  }  // if (!ret)
  if (!decoder_->Process(&packet_, false)) {
    return false;
  }

  return true;
}

}  // namespace cnstream
