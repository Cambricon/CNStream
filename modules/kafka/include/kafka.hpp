/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_KAFKA_HPP_
#define MODULES_KAFKA_HPP_

#include <memory>
#include <mutex>
#include <string>
#include <map>

#include "cnstream_module.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

struct KafkaContext;

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

class Kafka : public cnstream::Module, public cnstream::ModuleCreator<Kafka> {
 public:
  explicit Kafka(const std::string &name);
  ~Kafka();

  bool Open(cnstream::ModuleParamSet paramSet) override;
  void Close() override;
  int Process(CNFrameInfoPtr data) override;

 private:
  KafkaContext *GetContext(CNFrameInfoPtr data);

  std::mutex mutex_;
  std::map<int, KafkaContext *> contexts_;
  std::string brokers_;
  std::string handler_name_;
  // the topic_ is prefix of a real topic. eg: if you set topic in json is "cndata",
  // the stream_id 0`s real topic is "cndata_0"
  std::string topic_;
};  // class Kafka

}  // namespace cnstream

#endif  // MODULES_KAFKA_HPP_
