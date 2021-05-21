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

#include <opencv2/opencv.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cppcodec/base64_rfc4648.hpp"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_logging.hpp"
#include "kafka_handler.hpp"

namespace cnstream {

using ProduceFunc = std::function<bool(const std::string &)>;
using ConsumeFunc = std::function<bool(std::string *, int)>;
using ConfigFunc = std::function<bool(const std::string &)>;
using base64 = cppcodec::base64_rfc4648;

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;
using CNInferObjectPtr = std::shared_ptr<cnstream::CNInferObject>;

class IALDataProcesser {
 public:
  int Process(const CNFrameInfoPtr &data, std::string *output);

 private:
  void GenerateJsonString(const CNFrameInfoPtr &data, const CNInferObjectPtr &obj);
  std::string GetRoiDataBase64(const CNFrameInfoPtr &data, const CNInferObjectPtr &obj);

  rapidjson::StringBuffer buffer_;
  rapidjson::Writer<rapidjson::StringBuffer> writer_;
};

std::string IALDataProcesser::GetRoiDataBase64(const CNFrameInfoPtr &data, const CNInferObjectPtr &obj) {
  auto frame = cnstream::GetCNDataFramePtr(data);
  cv::Mat src = frame->ImageBGR();
  cv::Mat roi = src(cv::Rect(obj->bbox.x, obj->bbox.y, obj->bbox.w, obj->bbox.h)).clone();
  std::vector<uint8_t> vec(roi.data, roi.data + (roi.cols * roi.rows * 3));
  return base64::encode(vec);
}

int IALDataProcesser::Process(const CNFrameInfoPtr &data, std::string *output) {
  buffer_.Clear();
  writer_.Reset(buffer_);
  auto objs_ptr = cnstream::GetCNInferObjsPtr(data);
  std::vector<std::shared_ptr<CNInferObject>> person_objs;
  std::vector<std::shared_ptr<CNInferObject>> mvehicle_objs;
  std::vector<std::shared_ptr<CNInferObject>> non_mvehicle_objs;
  for (auto &it : objs_ptr->objs_) {
    auto &obj = it;
    std::string json_type = obj->GetExtraAttribute("jsonType");
    if (json_type == "person") {
      person_objs.push_back(obj);
    } else if (json_type == "vehicle") {
      mvehicle_objs.push_back(obj);
    } else if (json_type == "non_vehicle") {
      non_mvehicle_objs.push_back(obj);
    } else {
      LOGE(KAFKA) << " wrong json type";
    }
  }
  // if (!objs_ptr->objs_.empty()) writer_.StartObject();
  writer_.StartObject();
  // write persons

  writer_.Key("person");
  writer_.StartArray();
  for (auto &it : person_objs) {
    GenerateJsonString(data, it);
  }
  writer_.EndArray();

  // write mvehicles
  writer_.Key("mvehicle");
  writer_.StartArray();
  for (auto &it : mvehicle_objs) {
    GenerateJsonString(data, it);
  }
  writer_.EndArray();

  // write non_mvehicles
  writer_.Key("non_mvehicle");
  writer_.StartArray();
  for (auto &it : non_mvehicle_objs) {
    GenerateJsonString(data, it);
  }
  writer_.EndArray();

  // if (!objs_ptr->objs_.empty()) writer_.EndObject();
  writer_.EndObject();
  person_objs.clear();
  mvehicle_objs.clear();
  non_mvehicle_objs.clear();
  *output = std::string(buffer_.GetString(), buffer_.GetLength());
  return 0;
}

void IALDataProcesser::GenerateJsonString(const CNFrameInfoPtr &data, const CNInferObjectPtr &obj) {
  std::string cut_image_base64 = GetRoiDataBase64(data, obj);
  writer_.StartObject();
  auto frame = cnstream::GetCNDataFramePtr(data);
  writer_.Key("leftTopX");
  writer_.Int64(obj->bbox.x * frame->width);
  writer_.Key("leftTopY");
  writer_.Int64(obj->bbox.y * frame->height);
  writer_.Key("rightBtmX");
  writer_.Int64((obj->bbox.x + obj->bbox.w) * frame->width);
  writer_.Key("rightBtmY");
  writer_.Int64((obj->bbox.y + obj->bbox.h) * frame->height);
  writer_.Key("shortCutFileWidth");
  writer_.Int64(obj->bbox.w * frame->width);
  writer_.Key("shortCutFileHeight");
  writer_.Int64(obj->bbox.h * frame->height);
  writer_.Key("cut_image_base64");
  writer_.String(cut_image_base64.c_str());

  auto kv_attribute = obj->GetExtraAttributes();
  for (auto &it : kv_attribute) {
    writer_.Key(it.first.c_str());
    if (!it.second.empty()) {
      writer_.String(it.second.c_str());
    } else {
      writer_.String("");
    }
  }
  writer_.EndObject();
}

class KafkaHandlerIAL : public cnstream::KafkaHandler {
 public:
  ~KafkaHandlerIAL() {}
  int ProduceInfo(const ProduceFunc produce, const std::string &param, const CNFrameInfoPtr &data) override;

 private:
  std::unique_ptr<IALDataProcesser> data_processer_;
  DECLARE_REFLEX_OBJECT_EX(cnstream::KafkaHandlerIAL, cnstream::KafkaHandler);
};  // class KafkaHandlerIAL

IMPLEMENT_REFLEX_OBJECT_EX(cnstream::KafkaHandlerIAL, cnstream::KafkaHandler);

int KafkaHandlerIAL::ProduceInfo(const ProduceFunc produce, const std::string &param, const CNFrameInfoPtr &data) {
  if (!data_processer_) data_processer_.reset(new IALDataProcesser);

  std::string json_str;
  if (data_processer_->Process(data, &json_str)) {
    return -1;
  }

  if (json_str.length() <= 2) {
    return 0;
  }
  if (!produce(json_str)) {
    return -1;
  }

  return 0;
}

}  // namespace cnstream
