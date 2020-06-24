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

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <sstream>
#include <thread>
#include <utility>
#include <memory>

#include "data_handler.hpp"
#include "data_source.hpp"
#include "easyinfer/mlu_context.h"
#include "ffmpeg_decoder.hpp"
#include "glog/logging.h"
#include "threadsafe_queue.hpp"
#include "data_handler_mem.hpp"
#include "perf_manager.hpp"

namespace cnstream {

struct IOBuffer {
  explicit IOBuffer(unsigned char *buf, int size) {
    if (buf && size) {
      buf_ = (unsigned char *)av_malloc(sizeof(unsigned char) * size);
      if (buf_) {
        memcpy(buf_, buf, size);
        size_ = size;
      } else {
        size_ = 0;
      }
    } else {
      buf_ = nullptr;
      size_ = 0;
    }
  }
  ~IOBuffer() {
    if (buf_) {
      av_freep(&buf_);
    }
    size_ = 0;
  }
  unsigned char *buf_ = nullptr;
  int size_;
};

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
//
// FFMPEG use AVCodecParameters instead of AVCodecContext
// since from version 3.1(libavformat/version:57.40.100)
//
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

struct local_ffmpeg_init {
  local_ffmpeg_init() {
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
  }
};
static local_ffmpeg_init init_ffmpeg;
int DataHandlerMem::Write(unsigned char *buf, int size) {
  int offset = 0;
  while (running_.load()) {
    int w_size = (size - offset > io_buffer_size_) ? io_buffer_size_ : (size - offset);
    if (this->queue_.Size() >= 32) {
      usleep(1000);
      continue;
    }
    this->queue_.Push(std::make_shared<IOBuffer>(buf + offset, w_size));
    offset += w_size;
    if (offset >= size) {
      break;
    }
  }
  return size;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
  using IOBufferPtr = std::shared_ptr<IOBuffer>;
  ThreadSafeQueue<IOBufferPtr> *queue_
    = reinterpret_cast<ThreadSafeQueue<IOBufferPtr>*>(opaque);
  if (queue_) {
    IOBufferPtr iobuffer;
    queue_->WaitAndPop(iobuffer);

    if (iobuffer->size_) {
      if (buf_size < iobuffer->size_) {
        std::cout << "read_packet --- expected "
        << buf_size << ",written " << iobuffer->size_ << ", should not happen" << std::endl;
        return AVERROR_EOF;
      }
      memcpy(buf, iobuffer->buf_, iobuffer->size_);
      return iobuffer->size_;
    }
  }
  return AVERROR_EOF;
}

bool DataHandlerMem::PrepareResources(bool demux_only) {
  // format context
  p_format_ctx_ = avformat_alloc_context();
  if (!p_format_ctx_) {
    return false;
  }
  io_buffer_ = (unsigned char*)av_malloc(io_buffer_size_ + FF_INPUT_BUFFER_PADDING_SIZE);
  avio_ = avio_alloc_context(io_buffer_, io_buffer_size_, 0, &this->queue_, &read_packet, NULL, NULL);
  if (!avio_) {
    avformat_close_input(&p_format_ctx_);
    return false;
  }
  p_format_ctx_->pb = avio_;

  // open input
  int ret_code = avformat_open_input(&p_format_ctx_, "mem", NULL, NULL);
  if (0 != ret_code) {
    LOG(ERROR) << "Couldn't open input stream.";
    return false;
  }

  // find video stream information
  ret_code = avformat_find_stream_info(p_format_ctx_, NULL);
  if (ret_code < 0) {
    LOG(ERROR) << "Couldn't find stream information.";
    return false;
  }

  video_index_ = -1;
  AVStream* vstream = nullptr;
  for (uint32_t loop_i = 0; loop_i < p_format_ctx_->nb_streams; loop_i++) {
    vstream = p_format_ctx_->streams[loop_i];
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    if (vstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
#else
    if (vstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
#endif
      video_index_ = loop_i;
      break;
    }
  }
  if (video_index_ == -1) {
    LOG(ERROR) << "Didn't find a video stream.";
    return false;
  }
  av_init_packet(&packet_);
  packet_.data = NULL;
  packet_.size = 0;

  if (param_.decoder_type_ == DecoderType::DECODER_MLU) {
    decoder_ = std::make_shared<FFmpegMluDecoder>(this);
  } else if (param_.decoder_type_ == DecoderType::DECODER_CPU) {
    decoder_ = std::make_shared<FFmpegCpuDecoder>(this);
  } else {
    LOG(ERROR) << "unsupported decoder_type";
    return false;
  }
  if (decoder_.get()) {
    bool ret = decoder_->Create(vstream);
    if (ret) {
      return true;
    }
    return false;
  }
  return false;
}

void DataHandlerMem::ClearResources(bool demux_only) {
  if (decoder_.get()) {
    decoder_->Destroy();
  }
  if (p_format_ctx_) {
    avformat_close_input(&p_format_ctx_);
    if (avio_) {
      av_freep(&avio_);
    }
    p_format_ctx_ = nullptr;
  }
  video_index_ = -1;
  first_frame_ = true;
}

bool DataHandlerMem::Extract() {
  while (true) {
    if (av_read_frame(p_format_ctx_, &packet_) < 0) {
      return false;
    }

    if (packet_.stream_index != video_index_) {
      av_packet_unref(&packet_);
      continue;
    }

    if (first_frame_) {
      if (packet_.flags & AV_PKT_FLAG_KEY) {
        first_frame_ = false;
      } else {
        av_packet_unref(&packet_);
        continue;
      }
    }

    AVStream* vstream = p_format_ctx_->streams[video_index_];
    // find pts information
    if (AV_NOPTS_VALUE == packet_.pts && find_pts_) {
      find_pts_ = false;
      LOG(WARNING) << "Didn't find pts informations, "
                   << "use ordered numbers instead. ";
    } else if (AV_NOPTS_VALUE != packet_.pts) {
      find_pts_ = true;
      packet_.pts = av_rescale_q(packet_.pts, vstream->time_base, {1, 90000});
    }
    if (find_pts_ == false) {
      packet_.pts = pts_++;
    }
    return true;
  }
}

bool DataHandlerMem::Process() {
  bool ret = Extract();

  if (perf_manager_ != nullptr) {
    std::string thread_name = "cn-" + module_->GetName() + stream_id_;
    perf_manager_->Record(false, PerfManager::GetDefaultType(), module_->GetName(), packet_.pts);
    perf_manager_->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(packet_.pts),
                          module_->GetName() + "_th", "'" + thread_name + "'");
  }

  if (!ret) {
    LOG(INFO) << "Read EOS from file";
    demux_eos_.store(1);
    decoder_->Process(nullptr, true);
    return false;
  }  // if (!ret)

  if (!decoder_->Process(&packet_, false)) {
    av_packet_unref(&packet_);
    return false;
  }

  av_packet_unref(&packet_);
  return true;
}

}  // namespace cnstream

