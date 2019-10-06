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

#ifndef CNSTREAM_TIMER_HPP_
#define CNSTREAM_TIMER_HPP_

/**
 * @file cnstream_timer.hpp
 *
 * This file contains a declaration of class CNTimer.
 */

#include <chrono>
#include <string>

namespace cnstream {

/***********************************************************
 * Calculate the average time in ms for each frame.
 * Calculate fps, as well.
 ***********************************************************/
class CNTimer {
 public:
  void Dot(uint32_t cnt_step);
  /**************************************************************
   * Record one step time info
   *
   * @param time The cost time in ms of a cnt_step.
   * @param cnt_step Process step. As once processing batch frames,
   *        in most case, cnt_step is equal to batch size.
   **************************************************************/
  void Dot(double time, uint32_t cnt_step);
  /**************************************************************
   * Calculate and print fps
   *
   * @param head Print head string before fps information.
   **************************************************************/
  void PrintFps(const std::string& head) const;
  /**************************************************************
   * Clear all record.
   **************************************************************/
  void Clear();
  /**************************************************************
   * Mix other record into this timer.
   **************************************************************/
  void MixUp(const CNTimer& other);
  /**************************************************************
   * Get average cost time one step.
   *
   * @return Return average cost time.
   **************************************************************/
  double GetAvg() const;

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> last_t_;
  uint64_t cnt_ = 0;
  double avg_ = 0;
  bool first_dot_ = true;
};  // class CNTimer

}  // namespace cnstream

#endif  // CNSTREAM_TIMER_HPP_
