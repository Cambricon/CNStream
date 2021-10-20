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
 *********************************************************************/

#include <errno.h>
#include <signal.h>
#include <time.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "cnstream_logging.hpp"

#include "kafka_client.h"

namespace cnstream {

/* log levels from librdkafka */
#define RDKAFKA_LOG_EMERG   0
#define RDKAFKA_LOG_ALERT   1
#define RDKAFKA_LOG_CRIT    2
#define RDKAFKA_LOG_ERR     3
#define RDKAFKA_LOG_WARNING 4
#define RDKAFKA_LOG_NOTICE  5
#define RDKAFKA_LOG_INFO    6
#define RDKAFKA_LOG_DEBUG   7

KafkaClient::KafkaClient(TYPE type, const std::string &brokers, const std::string &topic, int32_t partition)
    : type_(type), brokers_(brokers), topic_(topic), partition_(partition) {
  state_ = STATE::IDLE;
  rk_ = nullptr;
  rkt_ = nullptr;
  conf_ = nullptr;
  topic_conf_ = nullptr;
  message_ = nullptr;
}

KafkaClient::~KafkaClient() {
  if (state_ != STATE::IDLE) Stop(true);
}

bool KafkaClient::Start() {
  char signal[16];
  char errstr[512];

  state_ = (type_ == TYPE::CONSUMER) ? STATE::PRE_CONSUME : STATE::PRE_PRODUCE;

  conf_ = rd_kafka_conf_new();
  if (!conf_) {
    LOGE(Kafka) << "rd_kafka_conf_new.error";
    Stop();
    return false;
  }

  /* Set logger */
  rd_kafka_conf_set_log_cb(conf_, logger);

  /* Quick termination */
  snprintf(signal, sizeof(signal), "%i", SIGIO);
  rd_kafka_conf_set(conf_, "internal.termination.signal", signal, nullptr, 0);

  /* Topic configuration */
  topic_conf_ = rd_kafka_topic_conf_new();
  if (!topic_conf_) {
    LOGE(Kafka) << "rd_kafka_topic_conf_new.error";
    Stop();
    return false;
  }

  if (type_ == TYPE::CONSUMER) {
    rd_kafka_conf_set(conf_, "enable.partition.eof", "true", nullptr, 0);
    rd_kafka_conf_set(conf_, "group.id", "kafka_test_group", nullptr, 0);

    /* Create Kafka consumer */
    if (!(rk_ = rd_kafka_new(RD_KAFKA_CONSUMER, conf_, errstr, sizeof(errstr)))) {
      LOGE(Kafka) << "Failed to create new consumer:" << errstr;
      Stop();
      return false;
    }
  } else {
    /* Set up a mesage delivery report callback.
     * It will be called once for each message, either on successful
     * delivery to brokers, or upon failure to deliver to brokers */
    rd_kafka_conf_set_dr_msg_cb(conf_, KafkaClient::msg_delivered);

    /* Create Kafka producer */
    if (!(rk_ = rd_kafka_new(RD_KAFKA_PRODUCER, conf_, errstr, sizeof(errstr)))) {
      LOGE(Kafka) << "Failed to create new producer" << errstr;
      Stop();
      return false;
    }
  }

  /* Add brokers */
  if (rd_kafka_brokers_add(rk_, brokers_.c_str()) == 0) {
    LOGE(Kafka) << "No valid brokers specified";
    Stop();
    return false;
  }

  /* Create topic */
  rkt_ = rd_kafka_topic_new(rk_, topic_.c_str(), topic_conf_);
  if (!rkt_) {
    LOGE(Kafka) << "rd_kafka_topic_new.error";
    Stop();
    return false;
  }

  /* Now topic conf owned by topic */
  topic_conf_ = nullptr;

  if (type_ == TYPE::CONSUMER) {
    /* Start consuming */
    int64_t start_offset = RD_KAFKA_OFFSET_STORED;
    if (rd_kafka_consume_start(rkt_, partition_, start_offset) == -1) {
      LOGE(Kafka) << "Failed to start consuming";
      Stop();
      return false;
    }
    state_ = STATE::CONSUME;
  } else {
    state_ = STATE::PRODUCE;
  }

  return true;
}

bool KafkaClient::Stop(bool instant) {
  if (state_ == STATE::IDLE) {
    LOGW(Kafka) << "Already stopped";
    return true;
  }

  if (state_ == STATE::CONSUME) {
    /* Stop consuming */
    if (rkt_) {
      rd_kafka_consume_stop(rkt_, partition_);
    }
  } else {
    if (rk_) {
      /* Poll to handle delivery reports */
      rd_kafka_poll(rk_, 0);
      /* Wait for messages to be delivered */
      while (!instant && rd_kafka_outq_len(rk_) > 0) {
        rd_kafka_poll(rk_, 100);
      }
    }
  }

  /* Destroy topic */
  if (rkt_) {
    rd_kafka_topic_destroy(rkt_);
  }

  /* Destroy handle */
  if (rk_) {
    rd_kafka_destroy(rk_);
  }

  /* Destroy message */
  if (message_) {
    rd_kafka_message_destroy(message_);
  }

  state_ = STATE::IDLE;

  return true;
}

bool KafkaClient::Consume(uint8_t **p_payload, size_t *p_length, int timeout_ms) {
  if (state_ != STATE::CONSUME) {
    return false;
  }

  if (message_) {
    rd_kafka_message_destroy(message_);
    message_ = nullptr;
  }

  int timeout = timeout_ms == -1 ? 1000 : timeout_ms;

  do {
    /* Poll for errors, etc. */
    rd_kafka_poll(rk_, 0);

    /* Consume single message.
     * See rdkafka_performance.c for high speed
     * consuming of messages. */
    message_ = rd_kafka_consume(rkt_, partition_, timeout);
    if (!message_) { /* timeout */
      if (timeout_ms == -1) {
        continue;
      } else {
        *p_payload = nullptr;
        *p_length = 0;
        return false;
      }
    }

    return msg_consume(message_, p_payload, p_length);
  } while (1);
}

bool KafkaClient::Produce(const uint8_t *payload, size_t length) {
  if (state_ != STATE::PRODUCE) {
    return false;
  }

  /* Send/Produce message. */
  if (rd_kafka_produce(rkt_, partition_, RD_KAFKA_MSG_F_COPY, const_cast<uint8_t *>(payload),
                       length, nullptr, 0, nullptr) == -1) {
    LOGE(Kafka) << "Failed to produce to topic: " << rd_kafka_topic_name(rkt_) << " partition: " << partition_;
    return false;
  }

  /* Poll to handle delivery reports */
  rd_kafka_poll(rk_, 0);
  return true;
}

/* Kafka logger callback (optional) */
void KafkaClient::logger(const rd_kafka_t *rk, int level, const char *fac, const char *buf) {
  LOGD(Kafka) << "logger";
  if (level <= RDKAFKA_LOG_ERR) {
    LOGE(Kafka) << fac << (rk ? (": " + std::string(rd_kafka_name(rk))) : "") << ": " << buf;
  } else if (level == RDKAFKA_LOG_NOTICE) {
    LOGW(Kafka) << fac << (rk ? (": " + std::string(rd_kafka_name(rk))) : "") << ": " << buf;
  } else if (level == RDKAFKA_LOG_INFO) {
    LOGI(Kafka) << fac << (rk ? (": " + std::string(rd_kafka_name(rk))) : "") << ": " << buf;
  } else {
    LOGD(Kafka) << fac << (rk ? (": " + std::string(rd_kafka_name(rk))) : "") << ": " << buf;
  }
}

bool KafkaClient::msg_consume(rd_kafka_message_t *msg, uint8_t **p_payload, size_t *p_len) {
  if (msg->err) {
    *p_payload = nullptr;
    *p_len = 0;
    if (msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
      LOGE(Kafka) << "Consumer reached end of " << rd_kafka_topic_name(msg->rkt) << " message queue at offset "
                  << msg->offset;
      return false;
    }

    LOGE(Kafka) << "Consume error for topic:" << rd_kafka_topic_name(msg->rkt) << " offset:" << msg->offset << " "
                << rd_kafka_message_errstr(msg);

    if (msg->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION || msg->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC) {
      LOGE(Kafka) << "%% Exit read process";
    }

    return false;
  }

  *p_payload = reinterpret_cast<uint8_t *>(msg->payload);
  *p_len = msg->len;

  return true;
}

/* Message delivery report callback using the richer rd_kafka_message_t object. */
void KafkaClient::msg_delivered(rd_kafka_t *rk, const rd_kafka_message_t *msg, void *opaque) {
  if (msg->err) {
    LOGE(Kafka) << "%% Message delivery failed: " << rd_kafka_err2str(msg->err);
  }
}

}  // namespace cnstream
