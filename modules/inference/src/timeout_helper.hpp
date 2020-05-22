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

#ifndef MODULES_INFERENCE_SRC_FRAME_TIMEOUT_HELPER_HPP_
#define MODULES_INFERENCE_SRC_FRAME_TIMEOUT_HELPER_HPP_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#define TIMEOUT_PRINT_INTERVAL 100

namespace cnstream {
class TimeoutHelperTest;
class TimeoutHelper {
 public:
  friend class TimeoutHelperTest;
  TimeoutHelper();

  ~TimeoutHelper();

  void LockOperator() { mtx_.lock(); }

  void UnlockOperator() { mtx_.unlock(); }

  int SetTimeout(float timeout);

  int Reset(const std::function<void()>& func);

 private:
  enum State { STATE_NO_FUNC = 0, STATE_RESET, STATE_DO, STATE_EXIT } state_ = STATE_NO_FUNC;
  void HandleFunc();

  std::mutex mtx_;
  std::condition_variable cond_;
  std::function<void()> func_;
  std::thread handle_th_;
  float timeout_ = 0;
  uint32_t timeout_print_cnt_ = 0;
};  // class TimeoutHelper

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_FRAME_TIMEOUT_HELPER_HPP_
