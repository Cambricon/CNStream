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

#include "video_stream.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../rw_mutex.hpp"
#include "../video_encoder/video_encoder_ffmpeg.hpp"
#include "../video_encoder/video_encoder_mlu.hpp"
#include "cnrt.h"
#include "cnstream_logging.hpp"

namespace cnstream {

namespace video {

#define REARRANGE_BY_PTS_INTERVAL 1

#define VS_CNRT_CHECK(__EXPRESSION__)                                                                                \
  do {                                                                                                               \
    cnrtRet_t ret = (__EXPRESSION__);                                                                                \
    LOGF_IF(VideoStream, CNRT_RET_SUCCESS != ret) << "Call [" << #__EXPRESSION__ << "] failed, error code: " << ret; \
  } while (0)

#define CALL_CNRT_BY_CONTEXT(__EXPRESSION__, __DEV_ID__, __DDR_CHN__)          \
  do {                                                                         \
    int dev_id = (__DEV_ID__);                                                 \
    cnrtDev_t dev;                                                             \
    cnrtChannelType_t ddr_chn = static_cast<cnrtChannelType_t>((__DDR_CHN__)); \
    VS_CNRT_CHECK(cnrtGetDeviceHandle(&dev, dev_id));                          \
    VS_CNRT_CHECK(cnrtSetCurrentDevice(dev));                                  \
    if (ddr_chn >= 0) VS_CNRT_CHECK(cnrtSetCurrentChannel(ddr_chn));           \
    VS_CNRT_CHECK(__EXPRESSION__);                                             \
  } while (0)

class VideoStream {
 public:
  using Param = cnstream::VideoStream::Param;
  using PacketInfo = cnstream::VideoStream::PacketInfo;
  using Event = cnstream::VideoStream::Event;
  using EventCallback = cnstream::VideoStream::EventCallback;
  using ColorFormat = cnstream::VideoStream::ColorFormat;
  using Buffer = cnstream::VideoStream::Buffer;
  using Rect = cnstream::VideoStream::Rect;

  explicit VideoStream(const Param &param);
  ~VideoStream();

  bool Open();
  bool Close(bool wait_finish = false);
  bool Update(const cv::Mat &mat, ColorFormat color, int64_t timestamp, int position = -1);
  bool Update(const Buffer *buffer, int64_t timestamp, int position = -1);
  bool Clear(int position);

  void SetEventCallback(EventCallback func) { event_callback_ = func; }
  int RequestFrameBuffer(VideoFrame *frame) {
    if (encoder_) return encoder_->RequestFrameBuffer(frame);
    return -1;
  }
  int GetPacket(VideoPacket *packet, PacketInfo *info = nullptr) {
    if (encoder_) return encoder_->GetPacket(packet, info);
    return -1;
  }

 private:
  enum State {
    IDLE = 0,
    STARTING,
    RUNNING,
    STOPPING,
  };

  struct FrameInfo {
    cv::Mat mat;
    ColorFormat color;
    int64_t timestamp;
    int position;
  };

  struct Timestamp {
    int64_t init = 0;
    int64_t base = 0;
    int64_t last = -1;
    int64_t diff = 0;
    std::atomic<uint64_t> count{0};
  };

  class Comparison {
   public:
    bool operator()(const FrameInfo &a, const FrameInfo &b) const { return a.timestamp > b.timestamp; }
  };

  typedef std::priority_queue<FrameInfo, std::vector<FrameInfo>, Comparison> FrameInfoQueue;

  void MatToBuffer(const cv::Mat &mat, ColorFormat color, Buffer *buffer);
  bool Encode(const cv::Mat &mat, ColorFormat color, int64_t timestamp, int timeout_ms = -1);
  bool Encode(const Buffer *buffer, int64_t timestamp, int timeout_ms = -1);
  void RearrangeLoop();
  void ResampleLoop();

  Param param_;
  RwMutex state_mtx_;
  std::atomic<int> state_{IDLE};
  std::atomic<bool> start_resample_{false};
  std::mutex queue_mtx_;
  std::condition_variable queue_cv_;
  FrameInfoQueue queue_;
  std::mutex canvas_mtx_;
  cv::Mat canvas_;
  ColorFormat canvas_color_ = ColorFormat::BGR;
  std::thread rearrange_thread_;
  std::thread resample_thread_;
  std::mutex ts_mtx_;
  std::unordered_map<int, Timestamp> timestamps_;
  VideoFrame frame_;
  std::atomic<bool> frame_available_{false};
  std::unique_ptr<VideoEncoder> encoder_ = nullptr;
  EventCallback event_callback_ = nullptr;
  std::promise<void> *eos_promise_ = nullptr;
  std::unique_ptr<Tiler> tiler_ = nullptr;
  std::mutex frame_avi_mtx_;
};

static const VideoStream::ColorFormat frame_to_buffer_color_map[] = {
    VideoStream::ColorFormat::YUV_I420, VideoStream::ColorFormat::YUV_NV12, VideoStream::ColorFormat::YUV_NV21,
    VideoStream::ColorFormat::BGR,      VideoStream::ColorFormat::RGB,
};

VideoStream::VideoStream(const Param &param) : param_(param) {}

VideoStream::~VideoStream() { Close(); }

bool VideoStream::Open() {
  WriteLockGuard slk(state_mtx_);
  if (state_ != IDLE) {
    LOGW(VideoEncoderFFmpeg) << "Open() state != IDLE";
    return false;
  }
  state_ = STARTING;

  if (param_.width < 2 || param_.height < 2) {
    LOGE(VideoStream) << "Open() invalid width or height";
    state_ = IDLE;
    return false;
  }
  if (param_.width % 2) {
    LOGW(VideoStream) << "Open() width is odd, change to " << (param_.width - 1);
    param_.width -= 1;
  }
  if (param_.height % 2) {
    LOGW(VideoStream) << "Open() height is odd, change to " << (param_.height - 1);
    param_.height -= 1;
  }

  if (param_.pixel_format > VideoPixelFormat::NV21) {
    LOGE(VideoStream) << "Open() encoder only support YUV input";
    state_ = IDLE;
    return false;
  }
  if (param_.mlu_encoder && param_.pixel_format == VideoPixelFormat::I420) {
    LOGE(VideoStream) << "Open() MLU encoder not support YUV I420 input";
    state_ = IDLE;
    return false;
  }
  if (param_.codec_type == VideoCodecType::MPEG4) {
    LOGE(VideoStream) << "Open() encoder only support encoding H264/H265/JPEG";
    state_ = IDLE;
    return false;
  }
  if (param_.codec_type == VideoCodecType::AUTO) param_.codec_type = VideoCodecType::H264;
  param_.frame_rate = param_.frame_rate > 0 ? param_.frame_rate : 25;
  param_.frame_rate = param_.frame_rate <= 60 ? param_.frame_rate : 25;
  param_.time_base = param_.time_base >= 1000 ? param_.time_base : 90000;

  if (param_.tile_cols > 1 || param_.tile_rows > 1) {
    ColorFormat color = frame_to_buffer_color_map[param_.pixel_format];
    tiler_.reset(new (std::nothrow) Tiler(param_.tile_cols, param_.tile_rows, color, param_.width, param_.height));
    if (!tiler_) {
      LOGE(VideoStream) << "Open() create tiler failed";
      state_ = IDLE;
      return false;
    }
  }

  VideoEncoder::Param param;
  param.width = param_.width;
  param.height = param_.height;
  param.frame_rate = param_.frame_rate;
  param.time_base = param_.time_base;
  param.bit_rate = param_.bit_rate;
  param.gop_size = param_.gop_size;
  param.pixel_format = param_.pixel_format;
  param.codec_type = param_.codec_type;
  param.input_buffer_count = 8;
  param.output_buffer_size = param_.bit_rate * param_.gop_size * 0.06;
  param.mlu_device_id = param_.mlu_encoder ? param_.device_id : -1;
  encoder_.reset(new (std::nothrow) VideoEncoder(param));
  if (!encoder_) {
    LOGE(VideoStream) << "Open() create video encoder failed";
    state_ = IDLE;
    return false;
  }
  auto event_callback = [this](VideoStream::Event event) {
    if (event == VideoStream::Event::EVENT_EOS) {
      // LOGI(VideoStream) << "EventCallback(EVENT_EOS)";
      if (eos_promise_) eos_promise_->set_value();
    }
    if (event_callback_) event_callback_(event);
  };
  encoder_->SetEventCallback(event_callback);
  if (VideoEncoder::SUCCESS != encoder_->Start()) {
    LOGE(VideoStream) << "Open() start video encoder failed";
    state_ = IDLE;
    return false;
  }

  state_ = RUNNING;
  if (tiler_) {
    param_.resample = true;
    rearrange_thread_ = std::thread(&VideoStream::RearrangeLoop, this);
    resample_thread_ = std::thread(&VideoStream::ResampleLoop, this);
  } else if (param_.resample) {
    rearrange_thread_ = std::thread(&VideoStream::RearrangeLoop, this);
    resample_thread_ = std::thread(&VideoStream::ResampleLoop, this);
  }
  return true;
}

bool VideoStream::Close(bool wait_finish) {
  UniqueWriteLock slk(state_mtx_);
  if (state_ != RUNNING) {
    // LOGW(VideoStream) << "Close() state != RUNNING";
    return false;
  }
  std::unique_lock<std::mutex> lk(queue_mtx_);
  state_ = STOPPING;
  lk.unlock();
  slk.Unlock();

  queue_cv_.notify_all();

  if (rearrange_thread_.joinable()) rearrange_thread_.join();
  if (resample_thread_.joinable()) resample_thread_.join();
  while (!queue_.empty()) queue_.pop();
  if (encoder_) {
    std::unique_lock<std::mutex> flk(frame_avi_mtx_);
    if (frame_available_) {
      if (VideoEncoder::SUCCESS != encoder_->SendFrame(&frame_, 2000)) {
        LOGE(VideoStream) << "Close() video encoder send empty frame failed";
      }
    }
    flk.unlock();

    if (wait_finish && param_.tile_cols == 1 && param_.tile_rows == 1 && !param_.resample) {
      std::promise<void> promise;
      eos_promise_ = &promise;
      VideoFrame eos;
      memset(&eos, 0, sizeof(eos));
      eos.SetEOS();
      auto ret = encoder_->SendFrame(&eos, 2000);
      if (ret != VideoEncoder::SUCCESS) {
        LOGE(VideoStream) << "Close() video encoder send eos failed";
      } else {
        if (std::future_status::ready != promise.get_future().wait_for(std::chrono::milliseconds(2000))) {
          LOGE(VideoStream) << "Close() wait video encoder eos back failed";
        }
      }
      eos_promise_ = nullptr;
    }

    encoder_->SetEventCallback(nullptr);
    encoder_->Stop();
  }
  canvas_.release();
  slk.Lock();
  state_ = IDLE;
  return true;
}

bool VideoStream::Update(const cv::Mat &mat, ColorFormat color, int64_t timestamp, int position) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoStream) << "Update(mat) not running";
    return false;
  }
  // LOGI(VideoStream) << "Update() timestamp=" << timestamp << ", position=" << position;
  std::unique_lock<std::mutex> lk(ts_mtx_);
  Timestamp &ts = timestamps_[position];
  /* rectify pts for loop mode */
  if (ts.last == INVALID_TIMESTAMP) {
    if (timestamp == INVALID_TIMESTAMP) {
      timestamp = ts.count * param_.time_base / param_.frame_rate;
      ts.init = 0;
    } else {
      ts.init = timestamp;
    }
    ts.base = 0;
  } else {
    if (ts.last > timestamp) {
      ts.base += (ts.last + ts.diff - timestamp);
    } else {
      ts.diff = timestamp - ts.last;
    }
  }
  ts.last = timestamp;
  timestamp += (ts.base - ts.init);
  lk.unlock();

  if (!param_.resample) {
    // re-generate timestamp
    timestamp = ts.count * param_.time_base / param_.frame_rate;
    if (!Encode(mat, color, timestamp)) return false;
    ts.count++;
  } else {
    std::unique_lock<std::mutex> lk(queue_mtx_);
    if (queue_.size() <= 50 * timestamps_.size()) {
      queue_.push((FrameInfo){mat, color, timestamp, position});
      lk.unlock();
      queue_cv_.notify_one();
      ts.count++;
    } else {
      LOGW(VideoStream) << "Update() mat_pts queue is full";
      return false;
    }
  }
  return true;
}

bool VideoStream::Update(const Buffer *buffer, int64_t timestamp, int position) {
  ReadLockGuard slk(state_mtx_);
  if (state_ != RUNNING) {
    LOGW(VideoStream) << "Update(buffer) not running";
    return false;
  }
  Timestamp &ts = timestamps_[position];
  timestamp = ts.count * param_.time_base / param_.frame_rate;
  ts.count++;
  return Encode(buffer, timestamp);
}

bool VideoStream::Clear(int position) {
  cv::Mat black;
  ColorFormat color = frame_to_buffer_color_map[param_.pixel_format];
  if (color <= ColorFormat::YUV_NV21) {
    black = cv::Mat(param_.height * 3 / 2, param_.width, CV_8UC1);
    memset(black.data, 0, param_.width * param_.height);
    memset(black.data + param_.width * param_.height, 0x80, param_.width * param_.height / 2);
  } else if (color <= ColorFormat::RGB) {
    black = cv::Mat(param_.height, param_.width, CV_8UC3);
    memset(black.data, 0, param_.width * param_.height * 3);
  } else {
    black = cv::Mat(param_.height, param_.width, CV_8UC4);
    memset(black.data, 0, param_.width * param_.height * 4);
  }
  return Update(black, color, INVALID_TIMESTAMP, position);
}

void VideoStream::MatToBuffer(const cv::Mat &mat, ColorFormat color, Scaler::Buffer *buffer) {
  if (!buffer) return;
  buffer->width = mat.cols;
  buffer->height = mat.rows;
  buffer->color = color;
  buffer->mlu_device_id = -1;
  if (color <= ColorFormat::YUV_NV21) {
    buffer->height = mat.rows * 2 / 3;
    buffer->data[0] = mat.data;
    buffer->stride[0] = mat.step;
    buffer->data[1] = mat.data + mat.step * buffer->height;
    buffer->stride[1] = mat.step;
    if (color == ColorFormat::YUV_I420) {
      buffer->data[2] = mat.data + mat.step * buffer->height * 5 / 4;
      buffer->stride[1] = buffer->stride[2] = mat.step / 2;
    }
  } else {
    buffer->data[0] = mat.data;
    buffer->stride[0] = mat.step;
  }
}

bool VideoStream::Encode(const cv::Mat &mat, ColorFormat color, int64_t timestamp, int timeout_ms) {
  Buffer buffer;
  memset(&buffer, 0, sizeof(Buffer));
  MatToBuffer(mat, color, &buffer);
  return Encode(&buffer, timestamp, timeout_ms);
}

bool VideoStream::Encode(const Buffer *buffer, int64_t timestamp, int timeout_ms) {
  if (!buffer) return false;
  std::unique_lock<std::mutex> flk(frame_avi_mtx_);
  if (!frame_available_) {
    memset(&frame_, 0, sizeof(VideoFrame));
    if (VideoEncoder::SUCCESS != encoder_->RequestFrameBuffer(&frame_, timeout_ms)) {
      LOGE(VideoStream) << "Update() video encoder request frame buffer failed";
      return false;
    }
    frame_available_ = true;
  }
  flk.unlock();

  std::unique_ptr<uint8_t[]> data = nullptr;
  if (buffer->mlu_device_id < 0) {
    Buffer buf, *enc_buf;
    memset(&buf, 0, sizeof(Buffer));
    buf.width = frame_.width;
    buf.height = frame_.height;
    buf.color = frame_to_buffer_color_map[frame_.pixel_format];
    buf.mlu_device_id = -1;
    if (!param_.mlu_encoder) {
      // CPU Frame && CPU Encoder
      buf.data[0] = frame_.data[0];
      buf.stride[0] = frame_.stride[0];
      buf.data[1] = frame_.data[1];
      buf.stride[1] = frame_.stride[1];
      if (buf.color == ColorFormat::YUV_I420) {
        buf.data[2] = frame_.data[2];
        buf.stride[2] = frame_.stride[2];
      }
      if (!Scaler::Process(buffer, &buf, nullptr, nullptr, Scaler::Carrier::LIBYUV)) {
        LOGE(VideoStream) << "Encode() scaler process 1 failed";
        return false;
      }
      enc_buf = &buf;
    } else {
      // CPU Frame && MLU Encoder
      if (buffer->width == frame_.width && buffer->height == frame_.height && buffer->color == buf.color &&
          buffer->stride[0] == frame_.stride[0] && buffer->stride[1] == frame_.stride[1] &&
          (buffer->color != ColorFormat::YUV_I420 ||
           (buffer->color == ColorFormat::YUV_I420 && buffer->stride[2] == frame_.stride[2]))) {
        enc_buf = const_cast<Buffer *>(buffer);
      } else {
        data.reset(new uint8_t[(frame_.stride[0] + frame_.stride[1] / 2 + frame_.stride[2] / 2) * frame_.height]);
        buf.data[0] = data.get();
        buf.stride[0] = frame_.stride[0];
        buf.data[1] = buf.data[0] + frame_.stride[0] * frame_.height;
        buf.stride[1] = frame_.stride[1];
        if (buf.color == ColorFormat::YUV_I420) {
          buf.data[2] = buf.data[0] + (frame_.stride[0] + frame_.stride[1] / 2) * frame_.height;
          buf.stride[2] = frame_.stride[2];
        }
        if (!Scaler::Process(buffer, &buf, nullptr, nullptr, Scaler::Carrier::LIBYUV)) {
          LOGE(VideoStream) << "Encode() scaler process 2 failed";
          return false;
        }
        enc_buf = &buf;
      }

      int copy_size;
      copy_size = frame_.stride[0] * frame_.height;
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(frame_.data[0]), enc_buf->data[0], copy_size,
                                      CNRT_MEM_TRANS_DIR_HOST2DEV),
                           param_.device_id, -1);
      copy_size = frame_.stride[1] * frame_.height / 2;
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(frame_.data[1]), enc_buf->data[1], copy_size,
                                      CNRT_MEM_TRANS_DIR_HOST2DEV),
                           param_.device_id, -1);
      if (enc_buf->color == ColorFormat::YUV_I420) {
        copy_size = frame_.stride[2] * frame_.height / 2;
        CALL_CNRT_BY_CONTEXT(cnrtMemcpy(reinterpret_cast<void *>(frame_.data[2]), enc_buf->data[2], copy_size,
                                        CNRT_MEM_TRANS_DIR_HOST2DEV),
                             param_.device_id, -1);
      }
    }
  } else {
    Buffer buf;
    memset(&buf, 0, sizeof(Buffer));
    buf.width = frame_.width;
    buf.height = frame_.height;
    buf.color = frame_to_buffer_color_map[frame_.pixel_format];
    buf.mlu_device_id = -1;
    if (!param_.mlu_encoder) {
      // MLU Frame && CPU Encoder
      if (buffer->width == frame_.width && buffer->height == frame_.height && buffer->color == buf.color &&
          buffer->stride[0] == frame_.stride[0] && buffer->stride[1] == frame_.stride[1] &&
          (buffer->color != ColorFormat::YUV_I420 ||
           (buffer->color == ColorFormat::YUV_I420 && buffer->stride[2] == frame_.stride[2]))) {
        buf.data[0] = frame_.data[0];
        buf.stride[0] = frame_.stride[0];
        buf.data[1] = frame_.data[1];
        buf.stride[1] = frame_.stride[1];
        if (buf.color == ColorFormat::YUV_I420) {
          buf.data[2] = frame_.data[2];
          buf.stride[2] = frame_.stride[2];
        }
      } else {
        data.reset(new uint8_t[(buffer->stride[0] + buffer->stride[1] / 2 + buffer->stride[2] / 2) * buffer->height]);
        buf.width = buffer->width;
        buf.height = buffer->height;
        buf.data[0] = data.get();
        buf.stride[0] = buffer->stride[0];
        buf.data[1] = buf.data[0] + buffer->stride[0] * buffer->height;
        buf.stride[1] = buffer->stride[1];
        if (buffer->color == ColorFormat::YUV_I420) {
          buf.data[2] = buf.data[0] + (buffer->stride[0] + buffer->stride[1] / 2) * buffer->height;
          buf.stride[2] = buffer->stride[2];
        }
      }

      int copy_size;
      copy_size = buffer->stride[0] * buffer->height;
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(buf.data[0], buffer->data[0], copy_size, CNRT_MEM_TRANS_DIR_DEV2HOST),
                           buffer->mlu_device_id, -1);
      copy_size = buffer->stride[1] * buffer->height / 2;
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(buf.data[1], buffer->data[1], copy_size, CNRT_MEM_TRANS_DIR_DEV2HOST),
                           buffer->mlu_device_id, -1);
      if (buffer->color == ColorFormat::YUV_I420) {
        copy_size = buffer->stride[2] * buffer->height / 2;
        CALL_CNRT_BY_CONTEXT(cnrtMemcpy(buf.data[2], buffer->data[2], copy_size, CNRT_MEM_TRANS_DIR_DEV2HOST),
                             buffer->mlu_device_id, -1);
      }

      if (data) {
        Buffer dst_buf;
        memset(&dst_buf, 0, sizeof(Buffer));
        dst_buf.color = buf.color;
        buf.color = buffer->color;
        dst_buf.mlu_device_id = -1;
        dst_buf.width = frame_.width;
        dst_buf.height = frame_.height;
        dst_buf.data[0] = frame_.data[0];
        dst_buf.stride[0] = frame_.stride[0];
        dst_buf.data[1] = frame_.data[1];
        dst_buf.stride[1] = frame_.stride[1];
        if (dst_buf.color == ColorFormat::YUV_I420) {
          dst_buf.data[2] = frame_.data[2];
          dst_buf.stride[2] = frame_.stride[2];
        }
        if (!Scaler::Process(&buf, &dst_buf, nullptr, nullptr, Scaler::Carrier::LIBYUV)) {
          LOGE(VideoStream) << "Encode() scaler process 3 failed";
          return false;
        }
      }
    } else {
      // MLU Frame && MLU Encoder
      if (buffer->mlu_device_id != param_.device_id) {
        LOGE(VideoStream) << "Encode() buffer device id(" << buffer->mlu_device_id << ") mismatch with param device id("
                          << param_.device_id << ")";
        return false;
      }

      buf.width = frame_.width;
      buf.height = frame_.height;
      buf.data[0] = frame_.data[0];
      buf.stride[0] = frame_.stride[0];
      buf.data[1] = frame_.data[1];
      buf.stride[1] = frame_.stride[1];
      buf.mlu_device_id = frame_.GetMluDeviceId();
      if (!Scaler::Process(buffer, &buf)) {
        LOGE(VideoStream) << "Encode() scaler process 4 failed";
        return false;
      }
    }
  }

  frame_.pts = timestamp;
  frame_.dts = INVALID_TIMESTAMP;
  flk.lock();
  auto ret = encoder_->SendFrame(&frame_, timeout_ms);
  frame_available_ = false;
  flk.unlock();
  if (ret != VideoEncoder::SUCCESS) {
    LOGE(VideoStream) << "Update() video encoder send frame failed";
    return false;
  }
  return true;
}

void VideoStream::RearrangeLoop() {
  auto start = std::chrono::steady_clock::now();
  int64_t ts = 0, r_ts = 0;
  bool first = true;
  bool resample_started = false;

  while (state_ == RUNNING) {
    std::unique_lock<std::mutex> lk(queue_mtx_);
    size_t buffer_count = 10 * std::max(static_cast<size_t>(1), timestamps_.size());
    queue_cv_.wait(lk, [&]() {
      return (state_ != RUNNING || (first && queue_.size() >= buffer_count) || (!first && !queue_.empty()));
    });
    if (state_ != RUNNING) break;

    if (first && queue_.size() < buffer_count) continue;
    auto frame = queue_.top();
    queue_.pop();
    double r = 1e6 / static_cast<double>(param_.time_base);
    ts = frame.timestamp * r;
    if (first) {
      r_ts = ts;
      start = std::chrono::steady_clock::now();
      first = false;
      LOGI(VideoStream) << "RearrangeLoop() start rearrange, queue size=" << queue_.size();
    }
    lk.unlock();

    auto end = std::chrono::steady_clock::now();
    auto dura = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    int64_t rt = ts - r_ts - dura.count();
    if (rt > 0) std::this_thread::sleep_for(std::chrono::microseconds(rt));
#if REARRANGE_BY_PTS_INTERVAL
    r_ts = ts;
    start = std::chrono::steady_clock::now();
#endif

    if (!tiler_) {
      std::lock_guard<std::mutex> lk(canvas_mtx_);
      canvas_.release();
      canvas_ = frame.mat;
      canvas_color_ = frame.color;
      if (!resample_started) {
        start_resample_ = true;
        resample_started = true;
      }
    } else {
      Buffer buffer;
      memset(&buffer, 0, sizeof(Buffer));
      MatToBuffer(frame.mat, frame.color, &buffer);
      if (!tiler_->Blit(&buffer, frame.position)) {
        LOGE(VideoStream) << "RearrangeLoop() tiler blit in pos: " << frame.position << " failed";
      }
    }
  }
}

void VideoStream::ResampleLoop() {
  int64_t delay_us = 0;
  auto start = std::chrono::steady_clock::now();
  int64_t timestamp = INVALID_TIMESTAMP, pts;
  int64_t index = 0;

  while (state_ == RUNNING) {
    if (!tiler_ && !start_resample_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    auto end = std::chrono::steady_clock::now();
    auto dura = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    int64_t rt = delay_us - dura.count();
    if (rt > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(rt));
      start = std::chrono::steady_clock::now();
    } else {
      start = end;
      if (delay_us > 0 && rt < 0) {
        LOGW(VideoStream) << "ResampleLoop() last operation takes time over pts interval";
      }
    }
    if (timestamp == INVALID_TIMESTAMP) {
      timestamp = index * 1e6 / param_.frame_rate;
      pts = index * param_.time_base / param_.frame_rate;
    }
    // LOGI(VideoStream) << "ResampleLoop() encode pts: " << pts;
    if (!tiler_) {
      std::lock_guard<std::mutex> lk(canvas_mtx_);
      Encode(canvas_, canvas_color_, pts);
    } else {
      Encode(tiler_->GetCanvas(), pts);
      tiler_->ReleaseCanvas();
    }
    int64_t ts = timestamp;
    timestamp = ++index * 1e6 / param_.frame_rate;
    pts = index * param_.time_base / param_.frame_rate;
    delay_us = timestamp - ts;
  }
}

}  // namespace video

VideoStream::VideoStream(const Param &param) { stream_ = new (std::nothrow) cnstream::video::VideoStream(param); }

VideoStream::~VideoStream() {
  if (stream_) {
    delete stream_;
    stream_ = nullptr;
  }
}

VideoStream::VideoStream(VideoStream &&stream) {
  stream_ = stream.stream_;
  stream.stream_ = nullptr;
}

VideoStream &VideoStream::operator=(VideoStream &&stream) {
  if (stream_) {
    delete stream_;
  }
  stream_ = stream.stream_;
  stream.stream_ = nullptr;
  return *this;
}

bool VideoStream::Open() {
  if (stream_) return stream_->Open();
  return false;
}

bool VideoStream::Close(bool wait_finish) {
  if (stream_) return stream_->Close(wait_finish);
  return false;
}

bool VideoStream::Update(const cv::Mat &mat, ColorFormat color, int64_t timestamp, int position) {
  if (stream_) return stream_->Update(mat, color, timestamp, position);
  return false;
}

bool VideoStream::Update(const Buffer *buffer, int64_t timestamp, int position) {
  if (stream_) return stream_->Update(buffer, timestamp, position);
  return false;
}

bool VideoStream::Clear(int position) {
  if (stream_) return stream_->Clear(position);
  return false;
}

void VideoStream::SetEventCallback(EventCallback func) {
  if (stream_) stream_->SetEventCallback(func);
}

int VideoStream::RequestFrameBuffer(VideoFrame *frame) {
  if (stream_) return stream_->RequestFrameBuffer(frame);
  return -1;
}

int VideoStream::GetPacket(VideoPacket *packet, PacketInfo *info) {
  if (stream_) return stream_->GetPacket(packet, info);
  return -1;
}

}  // namespace cnstream
