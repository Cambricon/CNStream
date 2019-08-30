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

#include "data_handler.hpp"
#include "glog/logging.h"

namespace cnstream {

std::mutex DataHandler::index_mutex_;
uint64_t DataHandler::index_mask_ = 0;

/*maxStreamNumber is sizeof(index_mask_) * 8  (bytes->bits)
 */
size_t DataHandler::GetStreamIndex() {
  std::unique_lock<std::mutex> lock(index_mutex_);
  if (streamIndex_ != INVALID_STREAM_ID) {
    return streamIndex_;
  }
  for (size_t i = 0; i < sizeof(index_mask_) * 8; i++) {
    if (!(index_mask_ & ((uint64_t)1 << i))) {
      index_mask_ |= (uint64_t)1 << i;
      streamIndex_ = i;
      return i;
    }
  }
  streamIndex_ = INVALID_STREAM_ID;
  return -1;
}

void DataHandler::ReturnStreamIndex() {
  std::unique_lock<std::mutex> lock(index_mutex_);
  if (streamIndex_ < 0 || streamIndex_ >= sizeof(index_mask_) * 8) {
    return;
  }
  index_mask_ &= ~((uint64_t)1 << streamIndex_);
}

}  // namespace cnstream
