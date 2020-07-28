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

#ifndef CNSTREAM_THREADSAFE_VECTOR_HPP_
#define CNSTREAM_THREADSAFE_VECTOR_HPP_

#include <unistd.h>
#include <vector>

#include "cnstream_spinlock.hpp"

namespace cnstream {

/**
 * @brief thread safe vector
 */
template <typename T>
class ThreadSafeVector {
 public:
  ThreadSafeVector() {}
  ThreadSafeVector(const ThreadSafeVector& other) {
    SpinLockGuard lk(data_m_);
    SpinLockGuard lk_other(other.data_m_);
    v_ = other.v_;
  }
  ThreadSafeVector& operator=(const ThreadSafeVector& other) {
    SpinLockGuard lk(data_m_);
    SpinLockGuard lk_other(other.data_m_);
    v_ = other.v_;
    return *this;
  }
  /**
   * @brief Appends the given element value to the end of the container.
   *
   * @param new_value The value of the element to append.
   *
   */
  void push_back(const T& new_value);

  /**
   * @brief Appends the given element value to the end of the container.
   *
   * @param new_value The value of the element to append.
   *
   */
  void emplace_back(const T& new_value);

  /**
   * @brief Removes the last element of the container.
   *        Calling pop_back on an empty container is undefined.
   *        Iterators and references to the last element, as well as the end() iterator, are invalidated
   */
  void pop_back();

  /**
   * @brief Returns a reference to the element at specified location pos. No bounds checking is performed.
   *
   * @param pos position of the element to return.
   *
   * @return T  Reference to the requested element
   */
  T& operator[](typename std::vector<T>::size_type pos);
  const T& operator[](typename std::vector<T>::size_type pos) const;

  /**
   * @brief Erases all elements from the container. After this call, size() returns zero.
   */
  void clear();

  /**
   * @brief Checks if the container has no elements, i.e. whether begin() == end()
   *
   * @return Flag true if the container is empty, false otherwise
   */
  bool empty() const {
    SpinLockGuard lk(data_m_);
    return v_.empty();
  }

  /**
   * @brief Returns the number of elements in the container, i.e. std::distance(begin(), end())
   *
   * @return The number of elements in the container
   */
  typename std::vector<T>::size_type size() const {
    SpinLockGuard lk(data_m_);
    return v_.size();
  }

  /**
   *  @brief Returns pointer to the underlying array serving as element storage.
   *  The pointer is such that range [data(); data() + size()) is always a valid range,
   *  even if the container is empty (data() is not dereferenceable in that case).
   *
   *  @return Pointer to the underlying element storage.
   *
   */
  T* data() noexcept {
    SpinLockGuard lk(data_m_);
    return v_.data();
  }

  /**
   *  @brief reserve the container.
   *  @param sz target size 
   *
   */
  void reserve(typename std::vector<T>::size_type sz) {
    SpinLockGuard lk(data_m_);
    v_.reserve(sz);
  }

  /**
   *  @brief resize the container.
   *  @param sz target size
   *
   */
  void resize(typename std::vector<T>::size_type sz) {
    SpinLockGuard lk(data_m_);
    v_.resize(sz);
  }

  /**
   * @brief Erases the specified elements from the container
   * @param pos Iterator to the element to remove
   * @param begin Range of elements to remove
   * @param end Range of elements to remove
   *
   */
  typename std::vector<T>::iterator erase(typename std::vector<T>::iterator pos);
  typename std::vector<T>::iterator erase(typename std::vector<T>::iterator begin,
                                          typename std::vector<T>::iterator end);
  /**
   *  @brief Inserts elements at the specified location in the container.
   *
   *  @param pos Iterator before which the content will be inserted. pos may be the end() iterator
   *  @param value Element value to insert.
   *  @param first  The range of elements to insert, can't be iterators into container for which insert is Called.
   *  @param last The range of elements to insert, can't be iterators into container for which insert is Called.
   *
   *  @return Iterator pointing to the inserted value.
   *
   */
  typename std::vector<T>::iterator insert(const typename std::vector<T>::iterator& pos, const T& value);
  typename std::vector<T>::iterator insert(typename std::vector<T>::iterator pos, const T& value);
  template <class InputIt>
  void insert(const typename std::vector<T>::iterator& pos, InputIt first, InputIt last);

  typename std::vector<T>::iterator emplace(const typename std::vector<T>::iterator& pos, const T& value);

 private:
  mutable SpinLock data_m_;
  std::vector<T> v_;
};

template <typename T>
typename std::vector<T>::iterator ThreadSafeVector<T>::erase(typename std::vector<T>::iterator pos) {
  SpinLockGuard lk(data_m_);
  return v_.erase(pos);
}

template <typename T>
typename std::vector<T>::iterator ThreadSafeVector<T>::erase(typename std::vector<T>::iterator begin,
                                                             typename std::vector<T>::iterator end) {
  SpinLockGuard lk(data_m_);
  return v_.erase(begin, end);
}

template <typename T>
template <class InputIt>
void ThreadSafeVector<T>::insert(const typename std::vector<T>::iterator& pos, InputIt first, InputIt last) {
  SpinLockGuard lk(data_m_);
  v_.insert(pos, first, last);
}

template <typename T>
typename std::vector<T>::iterator ThreadSafeVector<T>::insert(const typename std::vector<T>::iterator& pos,
                                                              const T& value) {
  SpinLockGuard lk(data_m_);
  return v_.insert(pos, value);
}

template <typename T>
typename std::vector<T>::iterator ThreadSafeVector<T>::insert(typename std::vector<T>::iterator pos, const T& value) {
  SpinLockGuard lk(data_m_);
  return v_.insert(pos, value);
}

template <typename T>
typename std::vector<T>::iterator ThreadSafeVector<T>::emplace(const typename std::vector<T>::iterator& pos,
                                                               const T& value) {
  SpinLockGuard lk(data_m_);
  return v_.emplace(pos, value);
}

template <typename T>
T& ThreadSafeVector<T>::operator[](typename std::vector<T>::size_type pos) {
  SpinLockGuard lk(data_m_);
  return v_[pos];
}

template <typename T>
const T& ThreadSafeVector<T>::operator[](typename std::vector<T>::size_type pos) const {
  SpinLockGuard lk(data_m_);
  return v_[pos];
}

template <typename T>
void ThreadSafeVector<T>::pop_back() {
  SpinLockGuard lk(data_m_);
  v_.pop_back();
}

template <typename T>
void ThreadSafeVector<T>::clear() {
  SpinLockGuard lk(data_m_);
  v_.clear();
}

template <typename T>
void ThreadSafeVector<T>::push_back(const T& new_value) {
  SpinLockGuard lk(data_m_);
  v_.push_back(new_value);
}

template <typename T>
void ThreadSafeVector<T>::emplace_back(const T& new_value) {
  SpinLockGuard lk(data_m_);
  v_.emplace_back(new_value);
}

}  // namespace cnstream

#endif  // CNSTREAM_THREADSAFE_VECTOR_HPP_
