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

#ifndef MODULES_INFERENCE_SRC_INFER_TRANS_DATA_HELPER_HPP_
#define MODULES_INFERENCE_SRC_INFER_TRANS_DATA_HELPER_HPP_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include "infer_engine.hpp"

namespace cnstream {

class Inferencer;
class CNFrameInfo;

class InferTransDataHelper {
 public:
  explicit InferTransDataHelper(Inferencer* infer, int batchsize);
  ~InferTransDataHelper();

  void SubmitData(const std::pair<std::shared_ptr<CNFrameInfo>, InferEngine::ResultWaitingCard>& data);

 private:
  void Loop();
  std::mutex mtx_;
  std::condition_variable cond_not_full_;
  std::condition_variable cond_not_empty_;
  std::queue<std::pair<std::shared_ptr<CNFrameInfo>, InferEngine::ResultWaitingCard>> queue_;
  Inferencer* infer_ = nullptr;
  std::thread th_;
  std::atomic<bool> running_;
  int batchsize_ = 1;
};  // class InferTransDataHelper

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_TRANS_DATA_HELPER_HPP_
