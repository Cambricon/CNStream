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

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "cnrt.h"
#include "cnstream_source.hpp"
#include "data_handler_file.hpp"
#include "test_base.hpp"

namespace cnstream {

TEST(SourceFrController, SetAndGetFrameRate) {
  uint32_t seed = (uint32_t)time(0);

  FrController fr_controller(0);
  EXPECT_EQ(fr_controller.GetFrameRate(), (uint32_t)0);
  uint32_t loop_num = 10;
  while (loop_num--) {
    uint32_t frame_rate = rand_r(&seed) % 100;
    fr_controller.SetFrameRate(frame_rate);
    EXPECT_EQ(fr_controller.GetFrameRate(), frame_rate);
  }
}

TEST(SourceFrController, Control) {
  uint32_t frame_rate = 0;
  FrController fr_controller(frame_rate);
  EXPECT_EQ(fr_controller.GetFrameRate(), frame_rate);
  // return directly
  EXPECT_NO_THROW(fr_controller.Control());

  // reset frame rate to 10
  frame_rate = 10;
  fr_controller.SetFrameRate(frame_rate);
  auto start = std::chrono::steady_clock::now();
  fr_controller.Start();

  std::chrono::duration<double, std::milli> diff;
  uint32_t loop_num = 10;
  while (loop_num--) {
    fr_controller.Control();
    auto end = std::chrono::steady_clock::now();
    diff = end - start;

    EXPECT_TRUE(diff.count() >= static_cast<double>(1000) / static_cast<double>(frame_rate));
    start = end;
  }

  // reset frame rate to 30
  frame_rate = 30;
  fr_controller.SetFrameRate(frame_rate);

  loop_num = 20;
  while (loop_num--) {
    fr_controller.Control();
    auto end = std::chrono::steady_clock::now();
    diff = end - start;
    EXPECT_TRUE(diff.count() >= static_cast<double>(1000) / static_cast<double>(frame_rate));
    start = end;
  }
}

}  // namespace cnstream
