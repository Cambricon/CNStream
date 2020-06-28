
/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_SOURCE_PARSER_HPP_
#define MODULES_SOURCE_PARSER_HPP_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
};

#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

/**
 * one writer and one reader
 */
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity);
  RingBuffer(RingBuffer &&) = delete;
  RingBuffer& operator=(RingBuffer&& other) = delete;
  RingBuffer(const RingBuffer &) = delete;
  RingBuffer& operator=(const RingBuffer& other) = delete;
  ~RingBuffer() { delete[] data_;}
  size_t Size() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return size_;
  }
  size_t Capacity() const {
    return capacity_;
  }
  size_t Write(const void *data, size_t bytes);
  size_t Read(void *data, size_t bytes);
 private:
  size_t front_, rear_, size_, capacity_;
  uint8_t *data_;
  mutable std::mutex mutex_;
  mutable std::condition_variable cond_w_;
  mutable std::condition_variable cond_r_;
};  // class RingBuffer

namespace cnstream {

struct VideoStreamInfo {
  AVCodecID codec_id;
  int codec_width;
  int codec_height;
  int progressive;
  AVColorSpace color_space;
  int bitrate;  // kbps
  AVRational time_base;
  AVRational framerate;
  std::vector<unsigned char> extra_data;
};

class StreamParserImpl;
class StreamParser {
 public:
  StreamParser();
  ~StreamParser();
  int Open(std::string fmt = "");
  void Close();
  int Parse(unsigned char *bitstream, int size);
  bool GetInfo(VideoStreamInfo &info);  // NOLINT
 private:
  StreamParserImpl *impl_ = nullptr;
};

class StreamParserImpl {
 public:
  StreamParserImpl() {}
  ~StreamParserImpl() {}
  int Open(std::string fmt) {
    queue_ = new (std::nothrow) RingBuffer(256 * 1024);
    if (!queue_) return -1;
    fmt_ = fmt;
    thread_ = std::thread(&StreamParserImpl::FindInfo, this);
    return 0;
  }

  void Close() {
    if (thread_.joinable()) {
      thread_.join();
    }
    if (queue_) {
      delete queue_, queue_ = nullptr;
    }
  }

  int Parse(unsigned char *bitstream, int size);
  bool GetInfo(VideoStreamInfo &info);  // NOLINT

 private:
  void FindInfo();
  static constexpr int io_buffer_size_ = 32768;
  std::string fmt_;
  RingBuffer *queue_ = nullptr;
  std::promise<VideoStreamInfo> promise_;
  std::atomic<int> info_got_{0};
  std::atomic<int> info_ready_{0};
  VideoStreamInfo info_;
  std::thread thread_;
};  // class StreamParserImpl

bool GetVideoStreamInfo (const AVFormatContext *ic, int &video_index, VideoStreamInfo &info);  // NOLINT

}  // namespace cnstream

#endif  // MODULES_SOURCE_PARSER_HPP_
