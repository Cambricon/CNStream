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

#ifndef MODULES_KAFKA_INCLUDE_HANDLER_HPP_
#define MODULES_KAFKA_INCLUDE_HANDLER_HPP_

#include <memory>
#include <string>

#include "cnstream_frame_va.hpp"

#include "reflex_object.h"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

class KafkaClient;

class KafkaHandler : virtual public ReflexObjectEx<KafkaHandler> {
 public:
  static KafkaHandler *Create(const std::string &name);
  virtual ~KafkaHandler() {}

  virtual int UpdateFrame(const CNFrameInfoPtr &data) { return 0; }

  friend class Kafka;

 protected:
  bool Produce(const std::string &content);
  bool Consume(std::string *content, int timeout_ms);

 private:
  std::string brokers_;
  std::string topic_;
  std::unique_ptr<KafkaClient> producer_ = nullptr;
  std::unique_ptr<KafkaClient> consumer_ = nullptr;
};  // class KafkaHandler

}  // namespace cnstream

#endif  // ifndef MODULES_KAFKA_INCLUDE_HANDLER_HPP_
