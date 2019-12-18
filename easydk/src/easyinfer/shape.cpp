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

#include "easyinfer/shape.h"

namespace edk {

Shape::Shape(uint32_t _n, uint32_t _h, uint32_t _w, uint32_t _c, uint32_t _stride)
    : n(_n), h(_h), w(_w), c(_c), stride_(_stride) {}

bool Shape::operator==(const Shape &other) {
  return n == other.n && c == other.c && h == other.h && w == other.w && Stride() == other.Stride();
}

bool Shape::operator!=(const Shape &other) { return !(*this == other); }

std::ostream &operator<<(std::ostream &os, const Shape &shape) {
  os << "NHWC+STRIDE(" << shape.n << ", " << shape.h << ", " << shape.w << ", " << shape.c << ", " << shape.Stride()
     << ")";
  return os;
}

}  // namespace edk
