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

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "timeout_helper.hpp"
#include "cnstream_logging.hpp"

namespace cnstream {

TimeoutHelper::TimeoutHelper() { handle_th_ = std::thread(&TimeoutHelper::HandleFunc, this); }

TimeoutHelper::~TimeoutHelper() {
  std::unique_lock<std::mutex> lk(mtx_);
  state_ = STATE_EXIT;
  lk.unlock();
  cond_.notify_all();
  if (handle_th_.joinable()) handle_th_.join();
}

int TimeoutHelper::SetTimeout(float timeout) {
  if (timeout < 0) {
    return 1;
  } else {
    std::lock_guard<std::mutex> lk(mtx_);
    timeout_ = timeout;
    return 0;
  }
}

int TimeoutHelper::Reset(const std::function<void()>& func) {
  if (STATE_EXIT == state_) {
    LOGW(INFERENCER) << "Timeout Operator has been exit.";
    return 1;
  }
  func_ = func;
  if (func) {
    if (STATE_NO_FUNC == state_) {
      state_ = STATE_DO;
    } else if (STATE_DO == state_ || STATE_RESET == state_) {
      state_ = STATE_RESET;
    } else {
      LOGF(INFERENCER) << "Unexpected logic.";
    }
  } else {
    state_ = STATE_NO_FUNC;
  }
  cond_.notify_one();
  return 0;
}

void TimeoutHelper::HandleFunc() {
  std::unique_lock<std::mutex> lk(mtx_);
  while (state_ != STATE_EXIT) {
    cond_.wait(lk, [this]() -> bool { return state_ == STATE_EXIT || state_ != STATE_NO_FUNC; });

    auto wait_time = std::chrono::nanoseconds(static_cast<uint64_t>(timeout_ * 1e6));
    cond_.wait_for(lk, wait_time, [this]() -> bool {
      return state_ == STATE_EXIT || state_ == STATE_NO_FUNC || state_ == STATE_RESET;
    });

    if (STATE_RESET == state_) {
      state_ = STATE_DO;
      continue;
    } else if (STATE_NO_FUNC == state_) {
      continue;
    } else if (STATE_EXIT == state_) {
      break;
    } else if (STATE_DO == state_) {
      LOGF_IF(INFERENCER, static_cast<bool>(func_) == false) << "Bad logic: state_ is STATE_DO, but function is NULL.";
      func_();
      timeout_print_cnt_++;
      if (timeout_print_cnt_ == TIMEOUT_PRINT_INTERVAL) {
        timeout_print_cnt_ = 0;
        LOGI(INFERENCER) << "Batching timeout. The trigger frequency of timeout processing can be reduced by"
                     " increasing the timeout time(see batching_timeout parameter of the inferencer module). If the"
                     " decoder memory is reused, the trigger frequency of timeout processing can also be reduced by"
                     " increasing the number of cache blocks output by the decoder(see output_buf_number parameter of"
                     " the source module). ";
      }
      func_ = NULL;  // unbind resources.
      state_ = STATE_NO_FUNC;
    } else {
      LOGF(INFERENCER) << "Unexpected logic.";
      break;
    }
  }
}

}  // namespace cnstream
