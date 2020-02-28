#include "VideoEncoder.h"

#define DEFAULT_SYNC_OUTPUT_BUFFER_SIZE 0x100000

VideoEncoder::CircularBuffer::CircularBuffer(size_t capacity /* =0x100000 */)
    : beg_index_(0), end_index_(0), size_(0), capacity_(capacity) {
  data_ = new unsigned char[capacity];
}

VideoEncoder::CircularBuffer::~CircularBuffer() { delete[] data_; }

size_t VideoEncoder::CircularBuffer::write(const unsigned char *data, size_t bytes) {
  if (bytes == 0 || data == nullptr) return 0;

  size_t capacity = capacity_;
  size_t bytes_to_write = std::min(bytes, capacity - size_);

  // Write in a single step
  if (bytes_to_write <= capacity - end_index_) {
    memcpy(data_ + end_index_, data, bytes_to_write);
    end_index_ += bytes_to_write;
    if (end_index_ >= capacity) end_index_ = 0;
  } else {  // Write in two steps
    size_t size_1 = capacity - end_index_;
    memcpy(data_ + end_index_, data, size_1);
    size_t size_2 = bytes_to_write - size_1;
    memcpy(data_, data + size_1, size_2);
    end_index_ = size_2;
  }

  size_ += bytes_to_write;
  return bytes_to_write;
}

size_t VideoEncoder::CircularBuffer::probe(unsigned char *data, size_t bytes) {
  if (bytes == 0 || data == nullptr) return 0;

  size_t capacity = capacity_;
  size_t bytes_to_read = std::min(bytes, size_);

  // Read in a single step
  if (bytes_to_read <= capacity - beg_index_) {
    memcpy(data, data_ + beg_index_, bytes_to_read);
  } else {  // Read in two steps
    size_t size_1 = capacity - beg_index_;
    memcpy(data, data_ + beg_index_, size_1);
    size_t size_2 = bytes_to_read - size_1;
    memcpy(data + size_1, data_, size_2);
  }

  return bytes_to_read;
}

size_t VideoEncoder::CircularBuffer::read(unsigned char *data, size_t bytes) {
  if (bytes == 0) return 0;

  size_t capacity = capacity_;
  size_t bytes_to_read = std::min(bytes, size_);

  // Read in a single step
  if (bytes_to_read <= capacity - beg_index_) {
    if (data != nullptr) memcpy(data, data_ + beg_index_, bytes_to_read);
    beg_index_ += bytes_to_read;
    if (beg_index_ >= capacity) beg_index_ = 0;
  } else {  // Read in two steps
    size_t size_1 = capacity - beg_index_;
    if (data != nullptr) memcpy(data, data_ + beg_index_, size_1);
    size_t size_2 = bytes_to_read - size_1;
    if (data != nullptr) memcpy(data + size_1, data_, size_2);
    beg_index_ = size_2;
  }

  size_ -= bytes_to_read;
  return bytes_to_read;
}

VideoEncoder::VideoEncoder(uint32_t input_queue_size /* = 0 */, size_t output_buffer_size /* = 0x100000 */) {
  input_queue_size_ = input_queue_size;
  if (output_buffer_size > 0) output_circular_buffer_ = new CircularBuffer(output_buffer_size);
  output_frame_header_ = new EncodedFrameHeader;
}

VideoEncoder::~VideoEncoder() {
  if (sync_input_frame_) delete sync_input_frame_;

  // input_mutex_.lock();
  VideoFrame *frame;
  while (input_data_q_.size()) {
    frame = input_data_q_.front();
    input_data_q_.pop();
    delete frame;
  }
  while (input_free_q_.size()) {
    frame = input_free_q_.front();
    input_free_q_.pop();
    delete frame;
  }
  // input_mutex_.unlock();

  if (output_circular_buffer_) delete output_circular_buffer_;
  if (output_frame_header_) delete output_frame_header_;
  if (sync_output_frame_buffer_) delete[] sync_output_frame_buffer_;
}

void VideoEncoder::Loop() {
  VideoFrame *frame = nullptr;

  while (running_) {
    if (input_queue_size_ == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    input_mutex_.lock();
    if (!input_data_q_.empty()) {
      frame = input_data_q_.front();
      input_mutex_.unlock();
    } else {
      input_mutex_.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    if (frame != nullptr) EncodeFrame(frame);

    input_mutex_.lock();
    input_data_q_.pop();
    input_free_q_.push(frame);
    input_mutex_.unlock();
  }
}

void VideoEncoder::Start() {
  running_ = true;
  if (input_queue_size_ > 0 && encode_thread_ == nullptr) {
    encode_thread_ = new std::thread(std::bind(&VideoEncoder::Loop, this));
  }
}

void VideoEncoder::Stop() {
  if (!running_) return;
  running_ = false;
  if (encode_thread_ && encode_thread_->joinable()) {
    encode_thread_->join();
    delete encode_thread_;
    encode_thread_ = nullptr;
  }
}

bool VideoEncoder::SendFrame(uint8_t *data, int64_t timestamp) {
  if (!running_) return false;
  VideoFrame *frame = nullptr;
  if (init_timestamp_ == -1) {
    init_timestamp_ = timestamp;
    timestamp = 0;
  } else {
    timestamp -= init_timestamp_;
  }

  input_mutex_.lock();
  if (input_queue_size_ == 0) {
    if (sync_input_frame_ == nullptr) {
      sync_input_frame_ = NewFrame();
    }
    sync_input_frame_->Fill(data, timestamp);
    EncodeFrame(sync_input_frame_);
    input_mutex_.unlock();
    return true;
  } else {
    if (!input_free_q_.empty()) {
      frame = input_free_q_.front();
      frame->Fill(data, timestamp);
      input_free_q_.pop();
      input_data_q_.push(frame);
    } else {
      if (input_data_q_.size() < input_queue_size_) {
        frame = NewFrame();
        frame->Fill(data, timestamp);
        input_data_q_.push(frame);
      } else {
        input_frames_dropped++;
        std::cout << "### drop input frame" << std::endl;
        input_mutex_.unlock();
        return false;
      }
    }
    input_mutex_.unlock();
  }
  return true;
}

bool VideoEncoder::PushOutputBuffer(uint8_t *data, size_t size, uint32_t frame_id, int64_t timestamp) {
  if (!running_) return false;
  if (data == nullptr || size <= 0) {
    std::cout << "PushOutputBuffer(): invalid parameters!" << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lk(output_mutex_);

  if (output_circular_buffer_) {
    EncodedFrameHeader efh;
    efh.frame_id = frame_id;
    efh.length = size;
    efh.offset = 0;
    efh.timestamp = timestamp;

    size_t free_size = output_circular_buffer_->capacity() - output_circular_buffer_->size();
    size_t write_size = sizeof(EncodedFrameHeader) + size;

    if (free_size < write_size) {
      output_frames_dropped++;
      // std::cout << "$$$ free_size < write_size drop output frame 1" << std::endl;
      return false;
    }
    output_circular_buffer_->write(reinterpret_cast<uint8_t *>(&efh), sizeof(EncodedFrameHeader));
    output_circular_buffer_->write(data, size);
  } else {
    if (sync_output_frame_new_ == true) {
      output_frames_dropped++;
      // std::cout << "$$$ drop output frame 2" << std::endl;
      return false;
    }
    if (size > sync_output_frame_buffer_length_) {
      if (sync_output_frame_buffer_) delete[] sync_output_frame_buffer_;
      sync_output_frame_buffer_ = new uint8_t[size];
      sync_output_frame_buffer_length_ = size;
    }
    memcpy(sync_output_frame_buffer_, data, size);
    output_frame_header_->length = size;
    output_frame_header_->offset = 0;
    output_frame_header_->frame_id = frame_id;
    output_frame_header_->timestamp = timestamp;
    sync_output_frame_new_ = true;
  }

  return true;
}

bool VideoEncoder::GetFrame(uint8_t *data, uint32_t max_size, uint32_t *size, int64_t *timestamp) {
  if (!running_) return false;
  if (size == nullptr || timestamp == nullptr) {
    std::cout << "GetFrame(): invalid parameters!" << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lk(output_mutex_);

  if (output_circular_buffer_) {
    size_t data_size = output_circular_buffer_->size();
    uint8_t *header = reinterpret_cast<uint8_t *>(output_frame_header_);
    if (data_size > sizeof(EncodedFrameHeader)) {
      if (data == nullptr) {
        output_circular_buffer_->probe(header, sizeof(EncodedFrameHeader));
        *size = output_frame_header_->length;
        *timestamp = output_frame_header_->timestamp;
      } else {
        output_circular_buffer_->read(header, sizeof(EncodedFrameHeader));
        if (output_frame_header_->length <= max_size) {
          output_circular_buffer_->read(data, output_frame_header_->length);
          *size = output_frame_header_->length;
          *timestamp = output_frame_header_->timestamp;
        } else {
          output_circular_buffer_->read(data, max_size);
          *size = max_size;
          *timestamp = output_frame_header_->timestamp;
          std::cout << "Buffer truncated, data_size(" << data_size << ") > max_size(" << max_size << ")" << std::endl;
          output_circular_buffer_->read(nullptr, output_frame_header_->length - max_size);
        }
      }
    } else {
      *size = 0;
      *timestamp = -1;
      return false;
    }
  } else {
    if (sync_output_frame_new_ == false) return false;
    if (data == nullptr) {
      *size = output_frame_header_->length;
      *timestamp = output_frame_header_->timestamp;
    } else {
      uint32_t copy_size = std::min(output_frame_header_->length, max_size);
      memcpy(data, sync_output_frame_buffer_, copy_size);
      *size = copy_size;
      *timestamp = output_frame_header_->timestamp;
      sync_output_frame_new_ = false;
    }
  }

  return true;
}
