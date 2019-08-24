/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
#ifndef CNSHAPE_HPP_
#define CNSHAPE_HPP_

#include <unistd.h>
#include <iostream>  // NOLINT

namespace libstream {

/*****************************************************
 * @brief NHWC
 *****************************************************/
class CnShape {
 public:
  explicit CnShape(uint32_t n = 1, uint32_t h = 1,
      uint32_t w = 1, uint32_t c = 1, uint32_t stride = 1);

  inline void set_n(uint32_t n) {
    n_ = n;
  }
  inline void set_h(uint32_t h) {
    h_ = h;
  }
  inline void set_w(uint32_t w) {
    w_ = w;
  }
  inline void set_c(uint32_t c) {
    c_ = c;
  }
  inline uint32_t n() const {
    return n_;
  }
  inline uint32_t h() const {
    return h_;
  }
  inline uint32_t w() const {
    return w_;
  }
  inline uint32_t c() const {
    return c_;
  }
  inline uint32_t stride() const {
    return w() > stride_ ? w() : stride_;
  }
  inline void set_stride(uint32_t s) {
    stride_ = s;
  }
  inline uint64_t step() const {
    return stride() * c();
  }

  inline uint64_t DataCount() const {
    return n() * h() * step();
  }
  inline uint64_t nhwc() const {
    return n() * h() * w() * c();
  }
  inline uint64_t hwc() const {
    return h() * w() * c();
  }
  inline uint64_t hw() const {
    return h() * w();
  }
  inline uint64_t wc() const {
    return w() * c();
  }

  friend std::ostream &operator<<(std::ostream &os, const CnShape &shape);
  bool operator==(const CnShape& other);
  bool operator!=(const CnShape& other);

 private:
  //  -----------------MEMBERS-----------------  //
  uint32_t n_;
  uint32_t h_;
  uint32_t w_;
  uint32_t c_;
  uint32_t stride_;
};  // class CnShape

}  // namespace libstream

#endif  // CNSHAPE_HPP_

