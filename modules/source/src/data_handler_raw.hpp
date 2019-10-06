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

#ifndef MODULES_SOURCE_HANDLER_RAW_HPP_
#define MODULES_SOURCE_HANDLER_RAW_HPP_

#include <string>
#include <thread>
#include "data_handler.hpp"
#include "data_source.hpp"
#include "raw_decoder.hpp"

namespace cnstream {

class DataHandlerRaw : public DataHandler {
 public:
  explicit DataHandlerRaw(DataSource* module, const std::string& stream_id, const std::string& filename, int framerate,
                          bool loop)
      : DataHandler(module, stream_id, framerate, loop), filename_(filename) {}
  ~DataHandlerRaw() {}

 private:
  std::string filename_;
  RawPacket packet_;
  uint8_t* chunk_ = nullptr;  // for chunk mode
  size_t chunk_size_ = 0;
  uint64_t pts_ = 0;
  int fd_ = -1;

 private:
  bool PrepareResources() override;
  void ClearResources() override;
  bool Process() override;
  bool Extract();

 private:
  std::shared_ptr<RawDecoder> decoder_ = nullptr;
};

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_RAW_HPP_
