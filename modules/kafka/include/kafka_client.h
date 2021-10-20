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

#ifndef __KAFKA_CLIENT_H__
#define __KAFKA_CLIENT_H__

#include <librdkafka/rdkafka.h>
#include <stdarg.h>

#include <functional>
#include <string>

namespace cnstream {

class KafkaClient {
 public:
  enum class TYPE {
    PRODUCER = 0,
    CONSUMER,
  };

  explicit KafkaClient(TYPE type, const std::string &brokers, const std::string &topic, int32_t partition);
  ~KafkaClient();

  bool Start();
  bool Stop(bool instant = false);
  bool Produce(const uint8_t *p_payload, size_t length);
  bool Consume(uint8_t **p_payload, size_t *p_length, int timeout_ms = 0);

 private:
  enum class STATE {
    IDLE = 0,
    PRE_PRODUCE,
    PRE_CONSUME,
    PRODUCE,
    CONSUME,
  };

  static void logger(const rd_kafka_t *rk, int level, const char *fac, const char *buf);
  static void msg_delivered(rd_kafka_t *rk, const rd_kafka_message_t *msg, void *opaque);
  bool msg_consume(rd_kafka_message_t *msg, uint8_t **p_payload, size_t *p_len);

  TYPE type_;
  const std::string brokers_;
  const std::string topic_;
  int32_t partition_ = 0;
  STATE state_ = STATE::IDLE;

  rd_kafka_t *rk_ = nullptr;
  rd_kafka_topic_t *rkt_ = nullptr;
  rd_kafka_conf_t *conf_ = nullptr;
  rd_kafka_topic_conf_t *topic_conf_ = nullptr;
  rd_kafka_message_t *message_ = nullptr;
};
}  // namespace cnstream

#endif  // __KAFKA_CLIENT_H__
