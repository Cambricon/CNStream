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

#include <string>

#include "kafka_client.h"
#include "kafka_handler.hpp"

namespace cnstream {

KafkaHandler *KafkaHandler::Create(const std::string &name) {
  return cnstream::ReflexObjectEx<KafkaHandler>::CreateObject(name);
}

bool KafkaHandler::Produce(const std::string &content) {
  if (!producer_) {
    producer_.reset(new KafkaClient(KafkaClient::TYPE::PRODUCER, brokers_, topic_, 0));
    producer_->Start();
  }
  const uint8_t *payload = reinterpret_cast<const uint8_t *>(content.c_str());
  size_t length = content.length();
  return producer_->Produce(payload, length);
}

bool KafkaHandler::Consume(std::string *content, int timeout_ms) {
  if (!consumer_) {
    consumer_.reset(new KafkaClient(KafkaClient::TYPE::CONSUMER, brokers_, topic_, 0));
    consumer_->Start();
  }
  uint8_t *payload;
  size_t length;
  if (!consumer_->Consume(&payload, &length, timeout_ms)) return false;
  *content = std::string(reinterpret_cast<char *>(payload), length);
  return true;
}

}  // namespace cnstream
