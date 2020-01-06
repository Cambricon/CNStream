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

#ifndef EASYINFER_SHAPE_HPP_
#define EASYINFER_SHAPE_HPP_

#include <iostream>

namespace edk {

/**
 * @brief Shape to describe inference model input and output data
 */
class Shape {
 public:
  /**
   * @brief Construct a new Shape object
   *
   * @param n[in] data number
   * @param h[in] height
   * @param w[in] width
   * @param c[in] channel
   * @param stride[in] aligned width
   */
  explicit Shape(uint32_t n = 1, uint32_t h = 1, uint32_t w = 1, uint32_t c = 1, uint32_t stride = 1);

  /**
   * @brief Get stride, which is aligned width
   *
   * @return Stride
   */
  inline uint32_t Stride() const { return w > stride_ ? w : stride_; }

  /**
   * @brief Set the stride
   *
   * @param s[in] Stride
   */
  inline void SetStride(uint32_t s) { stride_ = s; }

  /**
   * @brief Get Step, row length, equals to stride multiply c
   *
   * @return Step
   */
  inline uint64_t Step() const { return Stride() * c; }

  /**
   * @brief Get total data count, equal to memory size
   *
   * @return Data count
   */
  inline uint64_t DataCount() const { return n * h * Step(); }

  /**
   * @brief Get n * h * w * c, which is unaligned data size
   *
   * @return nhwc
   */
  inline uint64_t nhwc() const { return n * h * w * c; }

  /**
   * @brief Get h * w * c, which is size of one data part
   *
   * @return hwc
   */
  inline uint64_t hwc() const { return h * w * c; }

  /**
   * @brief Get h * w, which is size of one channel in one data part
   *
   * @return hw
   */
  inline uint64_t hw() const { return h * w; }

  /**
   * @brief Print shape
   *
   * @param os[in] Output stream
   * @param shape[in] Shape to be printed
   * @return Output stream
   */
  friend std::ostream &operator<<(std::ostream &os, const Shape &shape);

  /**
   * @brief Judge whether two shapes are equal
   *
   * @param other[in] Another shape
   * @return Return true if two shapes are equal
   */
  bool operator==(const Shape &other);

  /**
   * @brief Judge whether two shapes are not equal
   *
   * @param other[in] Another shape
   * @return Return true if two shapes are not equal
   */
  bool operator!=(const Shape &other);

  /// data number
  uint32_t n;
  /// height
  uint32_t h;
  /// width
  uint32_t w;
  /// channel
  uint32_t c;

 private:
  uint32_t stride_;
};  // class Shape

}  // namespace edk

#endif  // EASYINFER_SHAPE_HPP_
