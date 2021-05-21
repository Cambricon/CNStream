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

#include "cn_kafka.h"

#include <errno.h>
#include <signal.h>
#include <time.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "cnstream_logging.hpp"
namespace cnstream {

CnKafka::Logger CnKafka::logger_ = nullptr;

CnKafka::CnKafka(Logger logger) {
  message_ = nullptr;
  rk_ = nullptr;
  topic_ = nullptr;
  conf_ = nullptr;
  topic_conf_ = nullptr;
  mode_ = IDEL;
  logger_ = logger;
}

CnKafka::~CnKafka() {
  if (mode_ != IDEL) {
    Stop(true);
  }
}

bool CnKafka::Start(TYPE type, const std::string &brokers, const std::string &topic, int32_t partition) {
  char tmp[16];
  char errstr[512];

  mode_ = (type == CONSUMER) ? PRE_CONSUME : PRE_PRODUCE;

  type_ = type;
  partition_ = partition;
  conf_ = rd_kafka_conf_new();
  if (!conf_) {
    LOGE(Kafka) << "rd_kafka_conf_new.error";
    Stop();
    return false;
  }

  /* Quick termination */
  snprintf(tmp, sizeof(tmp), "%i", SIGIO);
  rd_kafka_conf_set(conf_, "internal.termination.signal", tmp, nullptr, 0);

  /* Topic configuration */
  topic_conf_ = rd_kafka_topic_conf_new();
  if (!topic_conf_) {
    LOGE(Kafka) << "rd_kafka_topic_conf_new.error";
    Stop();
    return false;
  }

  if (type == CONSUMER) {
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
    rd_kafka_conf_set_dr_msg_cb(conf_, CnKafka::msg_delivered);

    /* Create Kafka producer */
    if (!(rk_ = rd_kafka_new(RD_KAFKA_PRODUCER, conf_, errstr, sizeof(errstr)))) {
      LOGE(Kafka) << "Failed to create new producer" << errstr;
      Stop();
      return false;
    }
  }

  /* Set logger */
  rd_kafka_conf_set_log_cb(conf_, logger);
  rd_kafka_set_log_level(rk_, LOG_DEBUG);

  /* Add brokers */
  if (rd_kafka_brokers_add(rk_, brokers.c_str()) == 0) {
    LOGE(Kafka) << "No valid brokers specified";
    Stop();
    return false;
  }

  /* Create topic */
  topic_ = rd_kafka_topic_new(rk_, topic.c_str(), topic_conf_);
  if (!topic_) {
    LOGE(Kafka) << "rd_kafka_topic_new.error";
    Stop();
    return false;
  }

  /* Now topic conf owned by topic */
  topic_conf_ = nullptr;

  if (type == CONSUMER) {
    /* Start consuming */
    int64_t start_offset = RD_KAFKA_OFFSET_STORED;
    if (rd_kafka_consume_start(topic_, partition, start_offset) == -1) {
      LOGE(Kafka) << "Failed to start consuming";
      Stop();
      return false;
    }
    mode_ = CONSUME;
  } else {
    mode_ = PRODUCE;
  }

  return true;
}

bool CnKafka::Stop(bool instant) {
  if (mode_ == IDEL) {
    LOGW(Kafka) << "Already stopped";
    return true;
  }

  if (mode_ == CONSUME) {
    /* Stop consuming */
    if (topic_) {
      rd_kafka_consume_stop(topic_, partition_);
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
  if (topic_) {
    rd_kafka_topic_destroy(topic_);
  }

  /* Destroy handle */
  if (rk_) {
    rd_kafka_destroy(rk_);
  }

  /* Destroy message */
  if (message_) {
    rd_kafka_message_destroy(message_);
  }

  mode_ = IDEL;

  return true;
}

bool CnKafka::Consume(uint8_t **p_payload, size_t *p_length, int timeout_ms) {
  if (mode_ != CONSUME) {
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
    message_ = rd_kafka_consume(topic_, partition_, timeout);
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

bool CnKafka::Produce(const uint8_t *payload, size_t length) {
  if (mode_ != PRODUCE) {
    return false;
  }

  /* Send/Produce message. */
  if (rd_kafka_produce(topic_, partition_, RD_KAFKA_MSG_F_COPY, const_cast<uint8_t *>(payload), length, nullptr, 0,
                       nullptr) == -1) {
    LOGE(Kafka) << "Failed to produce to topic %s partition" << rd_kafka_topic_name(topic_) << partition_;
    return false;
  }

  /* Poll to handle delivery reports */
  rd_kafka_poll(rk_, 0);
  return true;
}

/* Kafka logger callback (optional) */
void CnKafka::logger(const rd_kafka_t *rk, int level, const char *fac, const char *buf) {
  if (logger_ != nullptr) {
    int log_level = LOG_LEVEL_DEBUG;
    if (level <= LOG_LEVEL_ERROR) {
      log_level = LOG_LEVEL_ERROR;
    } else if (level == LOG_WARNING) {
      log_level = LOG_LEVEL_WARNING;
    } else if (level <= LOG_INFO) {
      log_level = LOG_LEVEL_INFO;
    } else {
      log_level = LOG_LEVEL_DEBUG;
    }
    logger_(log_level, buf);
  } else {
    auto n = std::chrono::high_resolution_clock::now();
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(n.time_since_epoch());
    auto us = (usec.count()) % 1000000;
    auto t = std::chrono::system_clock::to_time_t(n);
    auto time = std::localtime(&t);
    char foo[80];
    strftime(foo, 80, "%Y-%m-%d %H:%M:%S", time);
    LOGE(Kafka) << "[" << foo << "." << std::setw(6) << std::setfill('0') << us
                << "] "
                /* << "RDKAFKA-" << level << "-" */
                << fac << (rk ? (": " + std::string(rd_kafka_name(rk))) : "") << ": " << buf << std::endl;
  }
}

void CnKafka::log(const rd_kafka_t *rk, int level, const char *fac, const char *fmt, ...) {
  char buf[2048] = {0};
  va_list vl;

  va_start(vl, fmt);
  vsnprintf(buf, sizeof(buf), fmt, vl);
  va_end(vl);

  logger(rk, level, fac, buf);
}

bool CnKafka::msg_consume(rd_kafka_message_t *msg, uint8_t **p_payload, size_t *p_len) {
  if (msg->err) {
    *p_payload = nullptr;
    *p_len = 0;
    if (msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
      LOGE(Kafka) << " Consumer reached end of " << rd_kafka_topic_name(msg->rkt) << " message queue at offset "
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
void CnKafka::msg_delivered(rd_kafka_t *rk, const rd_kafka_message_t *msg, void *opaque) {
  if (msg->err) {
    log(rk, LOG_LEVEL_ERROR, "ERROR", "%% Message delivery failed: %s", rd_kafka_err2str(msg->err));
  }
}

}  // namespace cnstream
