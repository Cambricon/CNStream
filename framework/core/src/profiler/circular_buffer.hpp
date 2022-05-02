/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
 *
 * This source code is licensed under the Apache-2.0 license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * A part of this source code is referenced from mediapipe project.
 * https://github.com/google/mediapipe/blob/master/mediapipe/framework/profiler/circular_buffer.h
 *
 * Copyright (C) 2018, MediaPipe Team.
 *
 * This source code is licensed under the Apache-2.0 license found in the
 * LICENSE file in the root directory of this source tree.
 *
 *************************************************************************/

#ifndef CNSTREAM_FRAMEWORK_PROFILER_CIRCULAR_BUFFER_HPP_
#define CNSTREAM_FRAMEWORK_PROFILER_CIRCULAR_BUFFER_HPP_

#include <atomic>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace cnstream {

/**
 * A circular buffer for lock-free event storage.
 * This class is thread-safe and writing using "push_back" is lock-free.
 * Multiple writers and readers are supported. All writes and reads
 * will succeed as long as the buffer does not grow by more than
 * |buffer_margin| during a read.
 **/
template <typename T>
class CircularBuffer {
 public:
  class iterator;

  /**
   * Create a circular buffer to hold up to |capacity| events.
   * Buffer writers are separated from readers by |buffer_margin|.
   **/
  explicit CircularBuffer(size_t capacity, double buffer_margin = 0.2);

  /**
   * Appends one event to the buffer.
   * Returns true if the buffer is free and writing succeeds.
   **/
  bool push_back(const T& event);

  /**
   * Appends one event to the buffer.
   * Returns true if the buffer is free and writing succeeds.
   **/
  bool push_back(T&& event);

  /**
   * Returns the i-th event in the buffer from the current beginning location.
   * Reading blocks until buffer is free.
   **/
  T Get(size_t i) const;

  /**
   * Returns the i-th event in the absolute buffer coordinates. Wrapping from
   * the beginning must be implemented separately.
   * Reading blocks until buffer is free.
   **/
  T GetAbsolute(size_t i) const;

  /**
   * Returns the first available index in the buffer.
   **/
  iterator begin() const;

  /**
   * Returns one past the last available index in the buffer.
   **/
  iterator end() const;

 private:
  // Marks an atom busy and returns its previous value.
  static char AcquireForWrite(std::atomic_char* patom);

  // After an atom reaches |lap|, marks it busy and returns its previous value.
  static char AcquireForRead(std::atomic_char* patom, char lap);

  // Marks an atom as not busy at |lap|.
  static void Release(std::atomic_char* patom, char lap);

  // Returns the modulo lap for a buffer index.
  static char GetLap(size_t i, size_t buffer_size);

  // Returns the greater of two modulo laps.
  static char MaxLap(char u, char v);

 private:
  double buffer_margin_;
  size_t capacity_;
  size_t buffer_size_;
  std::vector<T> buffer_;
  mutable std::vector<std::atomic_char> lap_;
  std::atomic<size_t> current_;
  static constexpr char kBusy = 0xFF;
  static constexpr char kMask = 0x7F;
};  // class CircularBuffer

template <typename T>
CircularBuffer<T>::CircularBuffer(size_t capacity, double buffer_margin)
    : buffer_margin_(buffer_margin),
      capacity_(capacity),
      buffer_size_((size_t)capacity * (1 + buffer_margin)),
      buffer_(buffer_size_),
      lap_(buffer_size_),
      current_(0) {}

template <typename T>
bool CircularBuffer<T>::push_back(const T& event) {
  size_t i = current_++;
  char lap = GetLap(i, buffer_size_);
  size_t index = i % buffer_size_;
  char prev = AcquireForWrite(&lap_[index]);
  buffer_[index] = event;
  Release(&lap_[index], MaxLap(prev, lap));
  return true;
}

template <typename T>
bool CircularBuffer<T>::push_back(T&& event) {
  size_t i = current_++;
  char lap = GetLap(i, buffer_size_);
  size_t index = i % buffer_size_;
  char prev = AcquireForWrite(&lap_[index]);
  buffer_[index] = std::forward<T>(event);
  Release(&lap_[index], MaxLap(prev, lap));
  return true;
}

template <typename T>
inline T CircularBuffer<T>::GetAbsolute(size_t i) const {
  char lap = GetLap(i, buffer_size_);
  size_t index = i % buffer_size_;
  char prev = AcquireForRead(&lap_[index], lap);
  T result = buffer_[index];
  Release(&lap_[index], prev);
  return result;
}

template <typename T>
inline T CircularBuffer<T>::Get(size_t i) const {
  if (current_ > capacity_) {
    return GetAbsolute(i + current_ - capacity_);
  } else {
    return GetAbsolute(i);
  }
}

template <typename T>
inline char CircularBuffer<T>::AcquireForWrite(std::atomic_char* patom) {
  std::atomic_char& atom = *patom;
  char prev;
  for (bool done = false; !done;) {
    prev = atom;
    if (prev != kBusy) {
      done =
          atom.compare_exchange_strong(prev, kBusy, std::memory_order_acquire);
    }
  }
  return prev;
}

template <typename T>
inline char CircularBuffer<T>::AcquireForRead(std::atomic_char* patom, char lap) {
  std::atomic_char& atom = *patom;
  char prev;
  for (bool done = false; !done;) {
    prev = atom;
    if (prev != kBusy && prev == MaxLap(prev, lap)) {
      done =
          atom.compare_exchange_strong(prev, kBusy, std::memory_order_acquire);
    }
  }
  return prev;
}

template <typename T>
inline void CircularBuffer<T>::Release(std::atomic_char* patom, char lap) {
  patom->store(lap, std::memory_order_release);
}

template <typename T>
inline char CircularBuffer<T>::GetLap(size_t index, size_t buffer_size) {
  return (index / buffer_size + 1) & kMask;
}

template <typename T>
inline char CircularBuffer<T>::MaxLap(char u, char v) {
  return ((u - v) & kMask) <= (kMask / 2) ? u : v;
}

template <typename T>
inline typename CircularBuffer<T>::iterator CircularBuffer<T>::begin() const {
  return iterator(this, current_ < capacity_ ? 0 : current_ - capacity_);
}

template <typename T>
inline typename CircularBuffer<T>::iterator CircularBuffer<T>::end() const {
  return iterator(this, current_);
}

template <typename T>
class CircularBuffer<T>::iterator
    : public std::iterator<std::random_access_iterator_tag, T, int64_t> {
 public:
  explicit iterator(const CircularBuffer* buffer, size_t index)
      : buffer_(buffer), index_(index) {}
  bool operator==(iterator other) const { return index_ == other.index_; }
  bool operator!=(iterator other) const { return !(*this == other); }
  bool operator<(iterator other) const { return index_ < other.index_; }
  T operator*() const { return buffer_->GetAbsolute(index_); }
  T* operator->() const { &buffer_->GetAbsolute(index_); }
  iterator& operator++() { return (*this) += 1; }
  iterator& operator+=(const int64_t& num) { return index_ += num, *this; }
  int64_t operator-(const iterator& it) const { return index_ - it.index_; }
  iterator& operator+(const int64_t& num) { return iterator(*this) += num; }
  iterator& operator-(const int64_t& num) { return iterator(*this) += -num; }

 private:
  const CircularBuffer* buffer_;
  size_t index_;
};

}  // namespace cnstream

#endif  // CNSTREAM_FRAMEWORK_PROFILER_CIRCULAR_BUFFER_HPP_
