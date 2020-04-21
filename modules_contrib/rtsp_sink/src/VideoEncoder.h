#ifndef __VIDEO_ENCODER_H__
#define __VIDEO_ENCODER_H__
extern "C" {
#include <string.h>
}

#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

class VideoEncoder {
 public:
  enum PictureFormat {
    YUV420P = 0,
    RGB24,
    BGR24,
    NV21,
    NV12,
  };
  enum CodecType {
    H264 = 0,
    HEVC,
    MPEG4,
  };
  enum Event {
    NEW_FRAME = 0,
    EOS,
  };

  explicit VideoEncoder(uint32_t input_queue_size = 0, size_t output_buffer_size = 0x100000);
  virtual ~VideoEncoder();

  void Start();
  void Stop();

  bool SendFrame(uint8_t *data, int64_t timestamp);
  bool GetFrame(uint8_t *data, uint32_t max_size, uint32_t *size, int64_t *timestamp);

  virtual uint32_t GetBitrate() { return 0; }

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

  void Loop();

  int64_t init_timestamp_ = -1;

  bool running_ = false;
  std::thread *encode_thread_ = nullptr;

  std::mutex input_mutex_;
  std::queue<VideoFrame *> input_data_q_;
  std::queue<VideoFrame *> input_free_q_;
  uint32_t input_queue_size_ = 0;
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
};      // VideoEncoder
#endif  //  __VIDEO_ENCODER_H__
