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

#include "fr_controller.hpp"

#include <thread>

void FrController::Start() { start_ = std::chrono::high_resolution_clock::now(); }

void FrController::Control() {
  if (0 == frame_rate_) return;
  double delay = 1000.0 / frame_rate_;
  end_ = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> diff = end_ - start_;
  auto gap = delay - diff.count();
  if (gap > 0) {
    std::chrono::duration<double, std::milli> dura(gap);
    std::this_thread::sleep_for(dura);
  }
  Start();
}
