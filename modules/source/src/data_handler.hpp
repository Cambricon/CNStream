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

#ifndef MODULES_SOURCE_DATA_HANDLER_HPP_
#define MODULES_SOURCE_DATA_HANDLER_HPP_

#include <string>
#include <thread>

#include "data_source.hpp"

namespace cnstream {

class DataHandler {
 public:
  explicit DataHandler(DataSource *module) : module_(module) { streamIndex_ = this->GetStreamIndex(); }
  virtual ~DataHandler() { this->ReturnStreamIndex(); }

  virtual bool Open() = 0;
  virtual void Close() = 0;

 protected:
  DataSource *module_ = nullptr;
  size_t GetStreamIndex();
  static const size_t INVALID_STREAM_ID = -1;

 private:
  size_t streamIndex_ = INVALID_STREAM_ID;
  void ReturnStreamIndex();
  static std::mutex index_mutex_;
  static uint64_t index_mask_;
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_DATA_HANDLER_HPP_
