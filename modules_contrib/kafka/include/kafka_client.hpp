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

#ifndef MODULES_KAFKA_CLIENT_HPP_
#define MODULES_KAFKA_CLIENT_HPP_

#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cnstream_core.hpp"

namespace cnstream {
struct KafkaClientContext;
class CnKafka;
class KafkaHandler;

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;
using KafkaProducer = CnKafka;
using KafkaConsumer = CnKafka;

class KafkaClient : public cnstream::Module, public cnstream::ModuleCreator<KafkaClient> {
 public:
  explicit KafkaClient(const std::string &name);
  ~KafkaClient();

  bool Open(cnstream::ModuleParamSet paramSet) override;
  void Close() override;
  int Process(CNFrameInfoPtr data) override;

 private:
  KafkaClientContext *GetContext(CNFrameInfoPtr data);

  bool Produce(KafkaProducer *producer, std::ofstream &log_file, const std::string &str);
  bool Consume(KafkaConsumer *consumer, std::ofstream &log_file, std::string *str, int timeout);
  bool Config(const std::string &param, bool store = false);
  void Log(int level, const std::string &str);

  std::unordered_map<int, KafkaClientContext *> contexts_;
  std::vector<std::string> stream_ids_;
  std::string broker_;
  std::string handler_name_;
  std::string topic_;

  KafkaProducer *producer_ = nullptr;
  KafkaConsumer *consumer_ = nullptr;
  KafkaHandler *handler_ = nullptr;
};  // class KafkaClient

}  // namespace cnstream

#endif  // MODULES_KAFKA_CLIENT_HPP_
