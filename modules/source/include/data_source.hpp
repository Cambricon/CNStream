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

#ifndef MODULES_DATA_SOURCE_HPP_
#define MODULES_DATA_SOURCE_HPP_

#include <memory>
#include <utility>
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

class DataHandler;
class DataSource : public Module, public ModuleCreator<DataSource> {
 public:
  explicit DataSource(const std::string &name);
  ~DataSource();

  /* paramSet:
   *    "decoder_type": "mlu",...
   *    "decoder_interval": handle data every "interval"
   */
  bool Open(ModuleParamSet paramSet) override;
  void Close() override;
  int Process(std::shared_ptr<CNFrameInfo> data) override;

 public:
  int AddVideoSource(const std::string &stream_id, const std::string &filename, int framerate, bool loop = false);
  int AddImageSource(const std::string &stream_id, const std::string &filename, bool loop = false);
  int RemoveSource(const std::string &stream_id);

 public:
  /*SendData() will be called by DataHandler*/
  bool SendData(std::shared_ptr<CNFrameInfo> data);
  ModuleParamSet ParamSet() const { return param_set_; }

 private:
  ModuleParamSet param_set_;
  std::mutex mutex_;
  std::map<std::string /*stream_id*/, std::shared_ptr<DataHandler>> source_map_;
};  // class DataSource

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
