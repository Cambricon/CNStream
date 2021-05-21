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
  std::ofstream log_file_;
  bool param_updated = true;
};

static std::vector<std::string> StringSplit(const std::string &s, char c) {
  std::stringstream ss(s);
  std::string piece;
  std::vector<std::string> result;
  while (std::getline(ss, piece, c)) {
    result.push_back(piece);
  }
  return result;
}

static std::vector<std::string> StringSplitT(const std::string &str, char c) {
  auto strings = StringSplit(str, c);

  for (auto &s : strings) {
    if (!s.empty()) {
      s.erase(std::remove_if(s.begin(), s.end(), ::isblank), s.end());
    }
  }
  return strings;
}

KafkaClient::KafkaClient(const std::string &name) : Module(name) {}

KafkaClientContext *KafkaClient::GetContext(CNFrameInfoPtr data) {
  KafkaClientContext *ctx = nullptr;
  auto search = contexts_.find(data->GetStreamIndex());
  if (search != contexts_.end()) {
    // context exists
    ctx = search->second;
  } else {
    ctx = new KafkaClientContext;

    ctx->producer_ =
        new KafkaProducer(std::bind(&KafkaClient::Log, this, std::placeholders::_1, std::placeholders::_2));
    std::string topic = topic_ + std::to_string(data->GetStreamIndex());
    ctx->producer_->Start(CnKafka::PRODUCER, broker_, topic, 0);

    if (data->GetStreamIndex() < stream_ids_.size())
      ctx->stream_id_ = stream_ids_[data->GetStreamIndex()];
    else
      ctx->stream_id_ = "stm_" + std::to_string(data->GetStreamIndex());
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
    topic_ = "IALStruct";
  }
  std::string stream_ids;
  if (paramSet.find("stream_ids") != paramSet.end()) {
    stream_ids = paramSet["stream_ids"];
    stream_ids_ = StringSplitT(stream_ids, ',');
  }
  std::ifstream conf("./kafka.conf");
  if (conf.good()) {
    std::string param((std::istreambuf_iterator<char>(conf)), std::istreambuf_iterator<char>());
    if (!param.empty()) Config(param);
    conf.close();
  }

  handler_ = KafkaHandler::Create(handler_name_);
  if (handler_) {
    producer_ = new KafkaProducer(std::bind(&KafkaClient::Log, this, std::placeholders::_1, std::placeholders::_2));
    producer_->Start(CnKafka::PRODUCER, broker_, topic_, 0);
    consumer_ = new KafkaConsumer(std::bind(&KafkaClient::Log, this, std::placeholders::_1, std::placeholders::_2));
    consumer_->Start(CnKafka::CONSUMER, broker_, topic_, 0);
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

  if (producer_) {
    delete producer_;
  }
  if (consumer_) {
    delete consumer_;
  }
  if (handler_) {
    delete handler_;
  }
}

int KafkaClient::Process(CNFrameInfoPtr data) {
  KafkaClientContext *ctx = GetContext(data);

  if (ctx->handler_) {
    std::string param;
    if (ctx->param_updated) {
      param = "stream_id=" + ctx->stream_id_;
      ctx->param_updated = false;
    }
    auto produce_func =
        std::bind(&KafkaClient::Produce, this, ctx->producer_, std::ref(ctx->log_file_), std::placeholders::_1);
    if (ctx->handler_->ProduceInfo(produce_func, param, data)) {
      LOGE(Kafka) << "[KafkaClient] ProduceInfo failed";
    }
  }

  return 0;
}

bool KafkaClient::Produce(KafkaProducer *producer, std::ofstream &log_file, const std::string &str) {
  if (true != producer->Produce(reinterpret_cast<const uint8_t *>(str.c_str()), str.length())) {
    return false;
  }
  return true;
}

bool KafkaClient::Consume(KafkaConsumer *consumer, std::ofstream &log_file, std::string *str, int timeout) {
  uint8_t *payload;
  size_t length;
  if (true != consumer->Consume(&payload, &length, timeout)) {
    return false;
  }
  std::string consume_str(reinterpret_cast<char *>(payload), length);
  *str = consume_str;
  return true;
}

bool KafkaClient::Config(const std::string &param, bool store) {
  LOGI(Kafka) << "Config param: " << param;

  auto params = StringSplitT(param, ';');
  if (params.empty()) return false;

  for (auto &p : params) {
    auto key_value = StringSplit(p, '=');
    std::string key = key_value[0];
    std::string value = key_value[1];

    if (key == "stream_ids") {
      auto stream_ids = StringSplit(value, ',');
      if (stream_ids.size() < stream_ids_.size()) {
        LOGW(Kafka) << "New stream_ids size less than old one";
      }
      stream_ids_.swap(stream_ids);
    }
  }

  if (store) {
    std::ofstream conf("./kafka.conf");
    if (conf.good()) {
      conf.write(param.c_str(), param.length());
      conf.close();
    }
  }

  for (auto &c : contexts_) {
    c.second->param_updated = true;
  }

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
