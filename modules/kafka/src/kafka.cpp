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

#include "kafka.hpp"

#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"


#include "kafka_client.h"
#include "kafka_handler.hpp"

namespace cnstream {

struct KafkaContext {
  std::unique_ptr<KafkaHandler> handler = nullptr;
};

Kafka::Kafka(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc(
      "kafka is a module which using rdkafka to produce CNFrameInfo data, or consume data.");
  param_register_.Register("handler", "The name of handler which use to deal CNFrameInfo data.");
  param_register_.Register("brokers", "The Brokers list of Kafka. "
      "It is a ,-separated list of brokers in the format: <host1>[:<port1>],<host2>[:<port2>]....");
  param_register_.Register("topic", "Topic is the basic unit of Kafka data writing operation.");
}

KafkaContext *Kafka::GetContext(CNFrameInfoPtr data) {
  KafkaContext *ctx = nullptr;
  std::lock_guard<std::mutex> lk(mutex_);
  auto search = contexts_.find(data->GetStreamIndex());
  if (search != contexts_.end()) {
    ctx = search->second;
  } else {
    ctx = new KafkaContext;
    std::string topic = topic_ + "_" + std::to_string(data->GetStreamIndex());
    ctx->handler.reset(KafkaHandler::Create(handler_name_));
    if (!ctx->handler) {
      LOGE(Kafka) << "Create handler failed";
      delete ctx;
    }
    ctx->handler->brokers_ = brokers_;
    ctx->handler->topic_ = topic;
    contexts_[data->GetStreamIndex()] = ctx;
  }

  return ctx;
}

Kafka::~Kafka() { Close(); }

bool Kafka::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("brokers") == paramSet.end() || paramSet.find("handler") == paramSet.end()) {
    LOGE(Kafka) << "Miss parameters";
    return false;
  }

  brokers_ = paramSet["brokers"];
  handler_name_ = paramSet["handler"];

  if (paramSet.find("topic") != paramSet.end()) {
    topic_ = paramSet["topic"];
  } else {
    topic_ = "CnstreamData";
  }
  return true;
}

void Kafka::Close() {
  if (contexts_.empty()) {
    return;
  }
  for (auto &c : contexts_) {
    delete c.second;
  }
  contexts_.clear();
}

int Kafka::Process(CNFrameInfoPtr data) {
  if (!data) return -1;

  if (data->IsRemoved()) { /* discard packets from removed-stream */
    return 0;
  }

  KafkaContext *ctx = GetContext(data);
  if (!ctx) {
    LOGE(Kafka) << "Get Kafka Context Failed.";
    return -1;
  }

  if (ctx->handler) {
    if (ctx->handler->UpdateFrame(data)) {
      LOGE(Kafka) << "Update frame failed";
    }
  }

  return 0;
}

}  // namespace cnstream
