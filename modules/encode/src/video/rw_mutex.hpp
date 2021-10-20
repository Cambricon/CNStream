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
#ifndef __RW_MUTEX_HPP__
#define __RW_MUTEX_HPP__

#include <mutex>
#include <condition_variable>
#include <utility>

namespace cnstream {

namespace video {

class UniqueReadLock;
class UniqueWriteLock;
class UniqueRwLock;

class RwMutex {
 public:
  // Prefer writer
  struct PreferWriter { };
  static constexpr PreferWriter prefer_writer { };

  RwMutex() noexcept = default;
  ~RwMutex() = default;
  explicit RwMutex(PreferWriter tag) noexcept : prefer_reader_(false) { }

  void ReadLock() { ReadLock(nullptr, nullptr); }
  void ReadUnlock() { ReadUnlock(nullptr, nullptr); }
  void WriteLock() { WriteLock(nullptr); }
  void WriteUnlock() { WriteUnlock(nullptr); }

  bool Reading() { return reading_count_ > 0; }
  bool Writing() { return writing_; }

  friend class UniqueReadLock;
  friend class UniqueWriteLock;
  friend class UniqueRwLock;

 private:
  void ReadLock(volatile size_t *count, volatile size_t *reading) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++read_count_;
    if (count) ++*count;
    read_cv_.wait(lock, [this] () {
        return (prefer_reader_ && !writing_) || (!prefer_reader_ && write_count_ == 0);
      });
    ++reading_count_;
    if (reading) ++*reading;
  }

  void ReadUnlock(volatile size_t *count, volatile size_t *reading, bool release = false) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (read_count_ == 0) return;
    if (count) {
      if (*count == 0) return;
      if (reading && *reading > *count) return;
      if (release) {
        if (read_count_ >= *count) {
          read_count_ -= *count;
        } else {
          read_count_ = 0;
        }
        *count = 0;
      } else {
        --*count;
        --read_count_;
      }
    } else {
      --read_count_;
    }
    if (reading) {
      if (release) {
        if (reading_count_ >= *reading) {
          reading_count_ -= *reading;
        } else {
          reading_count_ = 0;
        }
        *reading = 0;
      } else {
        if (*reading > 0) --*reading;
        if (reading_count_ > 0) --reading_count_;
      }
    } else {
      if (reading_count_ > 0) --reading_count_;
    }
    if (((prefer_reader_ &&  read_count_ == 0) || (!prefer_reader_ && reading_count_ == 0)) && write_count_ > 0) {
      lock.unlock();
      write_cv_.notify_one();
    }
  }

  void WriteLock(volatile size_t *count) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++write_count_;
    if (count) ++*count;
    write_cv_.wait(lock, [this] () {
        return ((prefer_reader_ && read_count_ == 0) || (!prefer_reader_ && reading_count_ == 0)) && !writing_;
      });
    writing_ = true;
  }

  void WriteUnlock(volatile size_t *count, bool release = false) {
    std::unique_lock<std::mutex> lock(mutex_);
    writing_ = false;
    if (count) {
      if (*count == 0) return;
      if (release) {
        if (write_count_ >= *count) {
          write_count_ -= *count;
        } else {
          write_count_ = 0;
        }
        *count = 0;
      } else {
        --*count;
        --write_count_;
      }
    } else {
      --write_count_;
    }
    if ((prefer_reader_ && read_count_ > 0) || (!prefer_reader_ && write_count_== 0)) {
      lock.unlock();
      read_cv_.notify_all();
    } else if (write_count_ > 0) {
      lock.unlock();
      write_cv_.notify_one();
    }
  }

  RwMutex(const RwMutex &) = delete;
  RwMutex &operator=(const RwMutex &) = delete;

  volatile bool prefer_reader_ = true;
  volatile size_t read_count_ = 0;
  volatile size_t write_count_ = 0;
  volatile size_t reading_count_ = 0;
  volatile bool writing_ = false;
  std::mutex mutex_;
  std::condition_variable read_cv_;
  std::condition_variable write_cv_;
};  // RwMutex

class ReadLockGuard {
 public:
  explicit ReadLockGuard(RwMutex &mutex) noexcept : mutex_(mutex) {
    mutex_.ReadLock();
  }
  ~ReadLockGuard() { mutex_.ReadUnlock(); }

 private:
  ReadLockGuard(const ReadLockGuard &) = delete;
  ReadLockGuard &operator=(const ReadLockGuard &) = delete;

  RwMutex &mutex_;
};  // ReadLockGuard

class WriteLockGuard {
 public:
  explicit WriteLockGuard(RwMutex &mutex) noexcept : mutex_(mutex) {
    mutex_.WriteLock();
  }
  ~WriteLockGuard() { mutex_.WriteUnlock(); }

 private:
  WriteLockGuard(const WriteLockGuard &) = delete;
  WriteLockGuard &operator=(const WriteLockGuard &) = delete;

  RwMutex &mutex_;
};  // WriteLockGuard

class UniqueReadLock {
 public:
  explicit UniqueReadLock(RwMutex &mutex, bool defer_lock = false) noexcept // NOLINT
      : mutex_(std::addressof(mutex)), count_(0), reading_count_(0) {
    if (!defer_lock) mutex_->ReadLock(&count_, &reading_count_);
  }
  ~UniqueReadLock() { mutex_->ReadUnlock(&count_, &reading_count_, true); }

  UniqueReadLock(UniqueReadLock &&lock) noexcept
      : mutex_(lock.mutex_), count_(lock.count_), reading_count_(lock.reading_count_) {
    lock.mutex_ = nullptr;
    lock.count_ = 0;
    lock.reading_count_ = 0;
  }
  UniqueReadLock &operator=(UniqueReadLock &&lock) noexcept {
    if (count_ > 0) mutex_->ReadUnlock(&count_, &reading_count_, true);
    std::swap(mutex_, lock.mutex_);
    std::swap(count_, lock.count_);
    std::swap(reading_count_, lock.reading_count_);
    lock.mutex_ = nullptr;
    lock.count_ = 0;
    lock.reading_count_ = 0;
    return *this;
  }

  void Lock() { mutex_->ReadLock(&count_, &reading_count_); }
  void Unlock() { mutex_->ReadUnlock(&count_, &reading_count_); }
  bool Reading() { return mutex_->Reading(); }

 private:
  UniqueReadLock() = delete;
  UniqueReadLock(const UniqueReadLock &) = delete;
  UniqueReadLock &operator=(const UniqueReadLock &) = delete;

  RwMutex *mutex_ = nullptr;
  volatile size_t count_ = 0;
  volatile size_t reading_count_ = 0;
};  // UniqueReadLock

class UniqueWriteLock {
 public:
  explicit UniqueWriteLock(RwMutex &mutex, bool defer_lock = false) noexcept // NOLINT
      : mutex_(std::addressof(mutex)), count_(0) {
    if (!defer_lock) mutex_->WriteLock(&count_);
  }
  ~UniqueWriteLock() { mutex_->WriteUnlock(&count_, true); }

  UniqueWriteLock(UniqueWriteLock &&lock) noexcept
      : mutex_(lock.mutex_), count_(lock.count_) {
    lock.mutex_ = nullptr;
    lock.count_ = 0;
  }
  UniqueWriteLock &operator=(UniqueWriteLock &&lock) noexcept {
    if (count_ > 0) mutex_->WriteUnlock(&count_, true);
    std::swap(mutex_, lock.mutex_);
    std::swap(count_, lock.count_);
    lock.mutex_ = nullptr;
    lock.count_ = 0;
    return *this;
  }

  void Lock() { mutex_->WriteLock(&count_); }
  void Unlock() { mutex_->WriteUnlock(&count_); }
  bool Writing() { return mutex_->Writing(); }

 private:
  UniqueWriteLock() = delete;
  UniqueWriteLock(const UniqueWriteLock &) = delete;
  UniqueWriteLock &operator=(const UniqueWriteLock &) = delete;

  RwMutex *mutex_ = nullptr;
  volatile size_t count_ = 0;
};  // UniqueWriteLock

class UniqueRwLock {
 public:
  explicit UniqueRwLock(RwMutex &mutex, bool read_lock, bool defer_lock = false) // NOLINT
      : mutex_(std::addressof(mutex)), read_count_(0), write_count_(0) {
    if (mutex_ && !defer_lock) {
      if (read_lock) {
        mutex_->ReadLock(&read_count_, &reading_count_);
      } else {
        mutex_->WriteLock(&write_count_);
      }
    }
  }
  ~UniqueRwLock() {
    mutex_->ReadUnlock(&read_count_, &reading_count_, true);
    mutex_->WriteUnlock(&write_count_, true);
  }

  UniqueRwLock(UniqueRwLock &&lock) noexcept
      : mutex_(lock.mutex_), read_count_(lock.read_count_),
        write_count_(lock.write_count_), reading_count_(lock.reading_count_) {
    lock.mutex_ = nullptr;
    lock.read_count_ = 0;
    lock.write_count_ = 0;
    lock.reading_count_ = 0;
  }
  UniqueRwLock &operator=(UniqueRwLock &&lock) noexcept {
    if (read_count_ > 0) mutex_->ReadUnlock(&read_count_, &reading_count_, true);
    if (write_count_ > 0) mutex_->WriteUnlock(&write_count_, true);
    std::swap(mutex_, lock.mutex_);
    std::swap(read_count_, lock.read_count_);
    std::swap(write_count_, lock.write_count_);
    std::swap(reading_count_, lock.reading_count_);
    lock.mutex_ = nullptr;
    lock.read_count_ = 0;
    lock.write_count_ = 0;
    lock.reading_count_ = 0;
    return *this;
  }

  void ReadLock() { mutex_->ReadLock(&read_count_, &reading_count_); }
  void ReadUnlock() { mutex_->ReadUnlock(&read_count_, &reading_count_); }
  void WriteLock() { mutex_->WriteLock(&write_count_); }
  void WriteUnlock() { mutex_->WriteUnlock(&write_count_); }

  bool Reading() { return mutex_->Reading(); }
  bool Writing() { return mutex_->Writing(); }

 private:
  UniqueRwLock() = delete;
  UniqueRwLock(const UniqueRwLock &) = delete;
  UniqueRwLock &operator=(const UniqueRwLock &) = delete;

  RwMutex *mutex_ = nullptr;
  volatile size_t read_count_ = 0;
  volatile size_t write_count_ = 0;
  volatile size_t reading_count_ = 0;
};  // UniqueRwLock

}  // namespace video

}  // namespace cnstream

#endif  //  __RW_MUTEX_HPP__
