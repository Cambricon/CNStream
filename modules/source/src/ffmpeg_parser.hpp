
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

#include <unistd.h>

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
  RingBuffer(RingBuffer &&) = delete;
  RingBuffer& operator=(RingBuffer&&) = delete;
  RingBuffer(const RingBuffer &) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
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
  StreamParser(const StreamParser &) = delete;
  StreamParser & operator=(const StreamParser &) = delete;
  StreamParserImpl *impl_ = nullptr;
};  // class StreamParser

class ParserHelper {
 public:
  ParserHelper() : status_(STATUS_NONE) {}

  int Init(std::string fmt) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (status_ == STATUS_NONE) {
      if (parser_.Open(fmt) < 0) {
        return -1;
      }
      status_ = STATUS_INIT;
    }
    return 0;
  }

  int Parse(unsigned char *bitstream, int size) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (status_ == STATUS_INIT) {
      if (parser_.Parse(bitstream, size) < 0) {
        return -1;
      }
      usleep(1000 * 30);  // FIXME
      VideoStreamInfo info;
      if (GetInfo(info) == true) {
        status_ = STATUS_DONE;
      }
    }
    return 0;
  }

  void Free() {
    std::unique_lock<std::mutex> lk(mutex_);
    if (status_ != STATUS_NONE && status_ != STATUS_END) {
      parser_.Close();
      status_ = STATUS_END;
    }
  }

  bool GetInfo(VideoStreamInfo &info) {  // NOLINT
    return parser_.GetInfo(info);
  }

 private:
  ParserHelper(const ParserHelper&) = delete;
  ParserHelper& operator=(const ParserHelper&) = delete;
  enum {
    STATUS_NONE,
    STATUS_INIT,
    STATUS_DONE,
    STATUS_END
  } status_;
  std::mutex mutex_;
  StreamParser parser_;
};  // class ParserHelper

bool GetVideoStreamInfo (const AVFormatContext *ic, int &video_index, VideoStreamInfo &info);  // NOLINT

struct NalDesc {
  unsigned char *nal = nullptr;
  int len = 0;
  int type = -1;
};

class H2645NalSplitter {
 public:
  virtual ~H2645NalSplitter();
  int SplitterInit(bool isH264) {
    isH264_ = isH264;
    return 0;
  }
  int SplitterWriteFrame(unsigned char *buf, int len);
  int SplitterWriteChunk(unsigned char *buf, int len);
  virtual void SplitterOnNal(NalDesc &desc, bool eos) = 0;  // NOLINT
 private:
  bool isH264_ = true;
  unsigned char *es_buffer_ = nullptr;
  int es_len_ = 0;
};  // class H2645NalSplitter

}  // namespace cnstream

#endif  // MODULES_SOURCE_PARSER_HPP_
