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

#ifndef MODULES_RTSP_SINK_SRC_VIDEO_ENCODER_HPP_
#define MODULES_RTSP_SINK_SRC_VIDEO_ENCODER_HPP_

#include <fstream>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace cnstream {

class VideoEncoder {
 public:
  enum Event {
    NEW_FRAME = 0,
    EOS,
  };

  explicit VideoEncoder(size_t output_buffer_size = 0x100000);
  virtual ~VideoEncoder();

  void Start();
  void Stop();

  bool SendFrame(uint8_t *data, int64_t timestamp);
  bool SendFrame(void *y, void *uv, int64_t timestamp);
  bool GetFrame(uint8_t *data, uint32_t max_size, uint32_t *size, int64_t *timestamp);

  virtual uint32_t GetBitRate() { return 0; }

  void SetCallback(std::function<void(Event)> func) { event_callback_ = func; }

 protected:
  class VideoFrame {
   public:
    VideoFrame() {}
    virtual ~VideoFrame() {}
    virtual void Fill(uint8_t *data, int64_t timestamp) = 0;
  };

  virtual VideoFrame *NewFrame() = 0;
  virtual void EncodeFrame(VideoFrame *frame) = 0;
  // virtual void EncodeFrame(void *y, void *uv, int64_t timestamp) = 0;

  bool PushOutputBuffer(uint8_t *data, size_t size, uint32_t frame_id, int64_t timestamp);

  void Callback(Event event) {
    if (event_callback_) event_callback_(event);
  }

 private:
  class CircularBuffer {
   public:
    explicit CircularBuffer(size_t capacity = 0x100000);
    ~CircularBuffer();

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    // Return number of bytes written.
    size_t write(const unsigned char *data, size_t bytes);
    // Return number of bytes, without read pointer moving.
    size_t probe(unsigned char *data, size_t bytes);
    // Return number of bytes read. if data is nullptr, just move read pointer.
    size_t read(unsigned char *data, size_t bytes);

   private:
    size_t beg_index_, end_index_, size_, capacity_;
    unsigned char *data_;
  };

  struct EncodedFrameHeader {
    uint32_t frame_id;
    uint32_t length;
    uint32_t offset;
    int64_t timestamp;
  };

  int64_t init_timestamp_ = -1;

  bool running_ = false;
  bool is_client_running_ = false;
  std::thread *encode_thread_ = nullptr;

  std::mutex input_mutex_;
  VideoFrame *sync_input_frame_ = nullptr;

  std::mutex output_mutex_;
  CircularBuffer *output_circular_buffer_ = nullptr;
  EncodedFrameHeader *output_frame_header_ = nullptr;
  uint8_t *sync_output_frame_buffer_ = nullptr;
  uint32_t sync_output_frame_buffer_length_ = 0;
  bool sync_output_frame_new_ = false;

  uint32_t input_frames_dropped = 0;
  uint32_t output_frames_dropped = 0;

  std::function<void(Event)> event_callback_ = nullptr;
};  // class VideoEncoder

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_SRC_VIDEO_ENCODER_HPP_
