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
#ifndef __CIRCULAR_BUFFER_HPP__
#define __CIRCULAR_BUFFER_HPP__

#include <cstddef>
#include <cstring>

namespace cnstream {

namespace video {

class CircularBuffer {
 public:
  explicit CircularBuffer(size_t capacity = 0x100000)
      : beg_index_(0), end_index_(0), size_(0), capacity_(capacity) {
    data_ = new unsigned char[capacity];
  }

  ~CircularBuffer() { delete[] data_; }

  size_t Size() const { return size_; }

  size_t Capacity() const { return capacity_; }

  /* Return number of bytes written. */
  size_t Write(const unsigned char *data, size_t bytes) {
    if (bytes == 0 || data == nullptr) return 0;

    size_t capacity = capacity_;
    size_t bytes_to_write = bytes < (capacity - size_) ? bytes : (capacity - size_);

    /* Write in a single step */
    if (bytes_to_write <= capacity - end_index_) {
      memcpy(data_ + end_index_, data, bytes_to_write);
      end_index_ += bytes_to_write;
      if (end_index_ == capacity) end_index_ = 0;
    } else {  /* Write in two steps */
      size_t size_1 = capacity - end_index_;
      memcpy(data_ + end_index_, data, size_1);
      size_t size_2 = bytes_to_write - size_1;
      memcpy(data_, data + size_1, size_2);
      end_index_ = size_2;
    }

    size_ += bytes_to_write;
    return bytes_to_write;
  }

  /*
    Return number of bytes read.
    if data is NULL, just move read pointer.
    if probe, read data without moving read pointer.
  */
  size_t Read(unsigned char *data, size_t bytes, bool probe = false) {
    if (bytes == 0) return 0;
    if (probe && data == nullptr) return 0;

    size_t capacity = capacity_;
    size_t bytes_to_read = bytes < size_ ? bytes : size_;

    // Read in a single step
    if (bytes_to_read <= capacity - beg_index_) {
      if (data != nullptr) memcpy(data, data_ + beg_index_, bytes_to_read);
      if (!probe) beg_index_ += bytes_to_read;
      if (beg_index_ == capacity) beg_index_ = 0;
    } else {  // Read in two steps
      size_t size_1 = capacity - beg_index_;
      if (data != nullptr) memcpy(data, data_ + beg_index_, size_1);
      size_t size_2 = bytes_to_read - size_1;
      if (data != nullptr) memcpy(data + size_1, data_, size_2);
      if (!probe) beg_index_ = size_2;
    }

    if (!probe) size_ -= bytes_to_read;
    return bytes_to_read;
  }

 private:
  CircularBuffer(const CircularBuffer &) = delete;
  CircularBuffer &operator=(const CircularBuffer &) = delete;
  CircularBuffer(const CircularBuffer &&) = delete;
  CircularBuffer &operator=(const CircularBuffer &&) = delete;

  size_t beg_index_, end_index_, size_, capacity_;
  unsigned char *data_;
};

}  // namespace video

}  // namespace cnstream

#endif  //  __CIRCULAR_BUFFER_HPP__
