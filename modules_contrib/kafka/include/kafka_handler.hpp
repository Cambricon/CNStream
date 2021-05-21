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

#include "cnstream_frame.hpp"
#include "kafka_client.hpp"
#include "reflex_object.h"

namespace cnstream {

class KafkaHandler : virtual public ReflexObjectEx<cnstream::KafkaHandler> {
 public:
  static KafkaHandler *Create(const std::string &name);
  virtual ~KafkaHandler() {}

  using ProduceFunc = std::function<bool(const std::string &)>;
  using ConsumeFunc = std::function<bool(std::string *, int)>;
  using ConfigFunc = std::function<bool(const std::string &)>;

  virtual int ProduceInfo(const ProduceFunc produce, const std::shared_ptr<cnstream::CNFrameInfo> &data) { return 0; }
};  // class KafkaHandler
}  // namespace cnstream

#endif  // ifndef MODULES_KAFKA_INCLUDE_HANDLER_HPP_
