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

#include "kafka_client.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "cn_kafka.h"
#include "cnstream_logging.hpp"
#include "kafka_handler.hpp"

namespace cnstream {

struct KafkaClientContext {
  KafkaProducer *producer_ = nullptr;
  KafkaHandler *handler_ = nullptr;
  std::string stream_id_;
};

KafkaClient::KafkaClient(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc(
      "kafka is a module which using rdkafka to produce CNFrameInfo data,"
      " or consume data.");
  param_register_.Register("handler", "The name of handler which use to deal CNFrameInfo data.");
  param_register_.Register("broker", "The message broker of kafka.");
  param_register_.Register("topic", "Topic is the basic unit of Kafka data writing operation.");
}

KafkaClientContext *KafkaClient::GetContext(CNFrameInfoPtr data) {
  KafkaClientContext *ctx = nullptr;
  auto search = contexts_.find(data->GetStreamIndex());
  if (search != contexts_.end()) {
    ctx = search->second;
  } else {
    ctx = new KafkaClientContext;
    ctx->producer_ =
        new KafkaProducer(std::bind(&KafkaClient::Log, this, std::placeholders::_1, std::placeholders::_2));
    std::string topic = topic_ + "_" + std::to_string(data->GetStreamIndex());
    ctx->producer_->Start(CnKafka::PRODUCER, broker_, topic, 0);
    ctx->stream_id_ = std::to_string(data->GetStreamIndex());
    ctx->handler_ = KafkaHandler::Create(handler_name_);
    if (!ctx->handler_) {
      LOGE(Kafka) << "[KafkaClient] handler create failed";
    }
    contexts_[data->GetStreamIndex()] = ctx;
  }

  return ctx;
}

KafkaClient::~KafkaClient() { Close(); }

bool KafkaClient::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("broker") == paramSet.end() || paramSet.find("handler") == paramSet.end()) {
    LOGE(Kafka) << "[KafkaClient] Miss parameters";
    return false;
  }

  broker_ = paramSet["broker"];
  handler_name_ = paramSet["handler"];

  if (paramSet.find("topic") != paramSet.end()) {
    topic_ = paramSet["topic"];
  } else {
    topic_ = "CnstreamData";
  }
  return true;
}

void KafkaClient::Close() {
  if (contexts_.empty()) {
    return;
  }
  for (auto &pair : contexts_) {
    auto &ctx = pair.second;
    if (ctx->producer_) {
      delete ctx->producer_;
    }
    if (ctx->handler_) {
      delete ctx->handler_;
    }
    delete ctx;
  }
  contexts_.clear();
}

int KafkaClient::Process(CNFrameInfoPtr data) {
  KafkaClientContext *ctx = GetContext(data);

  if (ctx->handler_) {
    std::string param;
    auto produce_func = std::bind(&KafkaClient::Produce, this, ctx->producer_, std::placeholders::_1);
    if (ctx->handler_->ProduceInfo(produce_func, data)) {
      LOGE(Kafka) << "[KafkaClient] ProduceInfo failed";
    }
  }

  return 0;
}

bool KafkaClient::Produce(KafkaProducer *producer, const std::string &str) {
  if (true != producer->Produce(reinterpret_cast<const uint8_t *>(str.c_str()), str.length())) {
    return false;
  }
  return true;
}

bool KafkaClient::Consume(KafkaConsumer *consumer, std::string *str, int timeout) {
  uint8_t *payload;
  size_t length;
  if (true != consumer->Consume(&payload, &length, timeout)) {
    return false;
  }
  std::string consume_str(reinterpret_cast<char *>(payload), length);
  *str = consume_str;
  return true;
}

void KafkaClient::Log(int level, const std::string &str) {
  if (level == CnKafka::LOG_LEVEL_ERROR) {
    LOGE(Kafka) << str;
  } else if (level == CnKafka::LOG_LEVEL_WARNING) {
    LOGW(Kafka) << str;
  } else if (level == CnKafka::LOG_LEVEL_INFO) {
    LOGI(Kafka) << str;
  } else if (level == CnKafka::LOG_LEVEL_DEBUG) {
    LOGI(Kafka) << str;
  } else {
    return;
  }
}

}  // namespace cnstream
