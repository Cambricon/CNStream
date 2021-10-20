
/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include <memory>
#include <string>

#include "kafka_client.h"
#include "rapidjson/prettywriter.h"

#include "cnstream_logging.hpp"
#include "kafka_handler.hpp"

class DefaultKafkaHandler : public cnstream::KafkaHandler {
 public:
  ~DefaultKafkaHandler() {}
  int UpdateFrame(const cnstream::CNFrameInfoPtr& data) override;
  DECLARE_REFLEX_OBJECT_EX(DefaultKafkaHandler, cnstream::KafkaHandler)
 private:
  rapidjson::StringBuffer buffer_;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer_;
};

IMPLEMENT_REFLEX_OBJECT_EX(DefaultKafkaHandler, cnstream::KafkaHandler)

int DefaultKafkaHandler::UpdateFrame(const std::shared_ptr<cnstream::CNFrameInfo>& data) {
  buffer_.Clear();
  writer_.Reset(buffer_);

  std::string json_str;

  // Generate stream ID to JSON string.
  std::string stream_id = data->stream_id;
  writer_.StartObject();
  writer_.String("StreamName");
#if RAPIDJSON_HAS_STDSTRING
  writer_.String(stream_id);
#else
  writer_.String(stream_id.c_str(), static_cast<rapidjson::SizeType>(stream_id.length()));
#endif

  // Generate frame count to JSON string if exist.
  if (data->collection.HasValue(cnstream::kCNDataFrameTag)) {
    auto dataframe = data->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
    auto frame_count = dataframe->frame_id;
    writer_.String("FrameCount");
    writer_.Uint64(frame_count);
  }

  // If there are inference objects, stringify them to JSON strings.
  if (data->collection.HasValue(cnstream::kCNInferObjsTag)) {
    writer_.String("Objects");
    writer_.StartArray();

    auto objs_holder = data->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
    for (auto infer_object : objs_holder->objs_) {
      writer_.StartObject();
      writer_.String("Label");
#if RAPIDJSON_HAS_STDSTRING
      writer_.String(infer_object->id);
#else
      writer_.String(infer_object->id.c_str(), static_cast<rapidjson::SizeType>(infer_object->id.length()));
#endif
      writer_.String("Score");
      writer_.Double(infer_object->score);

      writer_.String("BBox");
      writer_.StartArray();
      // x,y,w,h
      writer_.Double(infer_object->bbox.x);
      writer_.Double(infer_object->bbox.y);
      writer_.Double(infer_object->bbox.w);
      writer_.Double(infer_object->bbox.h);
      writer_.EndArray();

      writer_.EndObject();
    }

    writer_.EndArray();
  }

  writer_.EndObject();

  json_str = buffer_.GetString();

  if (json_str.length() <= 2) {
    LOGW(DEFAULTKAFKAHANDLER) << "Produce Kafka message failed!";
    return 0;
  }

  // Produce Kafka Message
  if (!Produce(json_str)) {
    LOGE(DEFAULTKAFKAHANDLER) << "Produce Kafka message failed!";
    return -1;
  }

  return 0;
}
