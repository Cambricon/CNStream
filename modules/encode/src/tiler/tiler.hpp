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

#ifndef __TILER_H__
#define __TILER_H__

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "scaler/scaler.hpp"

namespace cnstream {

class Tiler {
 public:
  using ColorFormat = Scaler::ColorFormat;
  using Buffer = Scaler::Buffer;
  using Rect = Scaler::Rect;

  explicit Tiler(uint32_t cols, uint32_t rows, ColorFormat color, uint32_t width, uint32_t height, uint32_t stride = 0);
  explicit Tiler(const std::vector<Rect> &grids, ColorFormat color, uint32_t width, uint32_t height,
                 uint32_t stride = 0);
  ~Tiler();

  bool Blit(const Buffer *buffer, int position);
  Buffer *GetCanvas(Buffer *buffer = nullptr);
  void ReleaseCanvas();

 private:
  void Init();
  void DumpCanvas();

  uint32_t cols_, rows_;
  std::vector<Rect> grids_;
  ColorFormat color_;
  uint32_t width_, height_, stride_;

  std::mutex mtx_;
  int last_position_ = 0;
  int grid_buffer_count_ = 0;
  std::mutex buf_mtx_;
  std::atomic<int> canvas_index_{0};
  std::atomic<bool> canvas_locked_{false};
  int canvas_diff_ = 0;
  Buffer canvas_buffers_[2];
};

}  // namespace cnstream

#endif  // __TILER_H__
