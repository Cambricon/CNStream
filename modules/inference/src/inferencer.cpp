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
#include "inferencer.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


#include "private/cnstream_param.hpp"

namespace cnstream {

class InferObserver : public infer_server::Observer {
 public:
  explicit InferObserver(std::function<void(const CNFrameInfoPtr)> callback) : callback_(callback) {}
  void Response(infer_server::Status status, infer_server::PackagePtr result,
                infer_server::any user_data) noexcept override {
    callback_(infer_server::any_cast<const CNFrameInfoPtr>(user_data));
  }
 private:
  std::function<void(const CNFrameInfoPtr)> callback_;
};

Inferencer::Inferencer(const std::string& name) : ModuleEx(name) {
  param_register_.SetModuleDesc(
      "Inferencer is a module for running offline model inference, preprocessing and "
      "postprocessing based on infer_server.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<InferParams>(name));
  // regist param

  auto model_input_pixel_format_parser = [](const ModuleParamSet& param_set, const std::string& param_name,
                                            const std::string& value, void* result) -> bool {
    if ("RGB24" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::RGB;
    } else if ("BGR24" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::BGR;
    } else if ("ARGB32" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::ARGB;
    } else if ("ABGR32" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::ABGR;
    } else if ("RGBA32" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::RGBA;
    } else if ("BGRA32" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::BGRA;
    } else if ("GRAY" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::GRAY;
    } else if ("TENSOR" == value) {
      *(static_cast<InferVideoPixelFmt*>(result)) = infer_server::NetworkInputFormat::TENSOR;
    } else {
      LOGE(Inferencer) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed";
      return false;
    }
    return true;
  };

  auto batch_strategy_parser = [](const ModuleParamSet& param_set, const std::string& param_name,
                                  const std::string& value, void* result) -> bool {
    std::string value_upper = value;
    std::transform(value_upper.begin(), value_upper.end(), value_upper.begin(), ::toupper);
    if (value_upper == "STATIC") {
      *(static_cast<InferBatchStrategy*>(result)) = InferBatchStrategy::STATIC;
      return true;
    } else if (value_upper == "DYNAMIC") {
      *(static_cast<InferBatchStrategy*>(result)) = InferBatchStrategy::DYNAMIC;
      return true;
    }
    LOGE(Inferencer) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed";
    return false;
  };

  auto preproc_parser = [](const ModuleParamSet& param_set, const std::string& param_name, const std::string& value,
                           void* result) -> bool {
    std::unordered_map<std::string, std::string> params_map;
    if (!SplitParams(value, &params_map)) {
      LOGE(Inferencer) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed";
      return false;
    }
    if (params_map.find("name") != params_map.end()) {
      if (params_map["name"].empty()) return false;
      size_t offset = OFFSET(InferParams, preproc_name);
      *(reinterpret_cast<std::string*>(reinterpret_cast<void*>(reinterpret_cast<char*>(result) + offset))) =
          params_map["name"];
    }
    if (params_map.find("use_cpu") != params_map.end()) {
      size_t offset = OFFSET(InferParams, preproc_use_cpu);
      if (!ModuleParamParser<bool>::Parser(param_set, "use_cpu", params_map["use_cpu"],
                                           reinterpret_cast<char*>(result) + offset)) {
        return false;
      }
    }
    return true;
  };

  auto postproc_parser = [](const ModuleParamSet& param_set, const std::string& param_name, const std::string& value,
                            void* result) -> bool {
    std::unordered_map<std::string, std::string> params_map;
    if (!SplitParams(value, &params_map)) {
      LOGE(Inferencer) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed";
      return false;
    }
    if (params_map.find("name") != params_map.end()) {
      if (params_map["name"].empty()) return false;
      size_t offset = OFFSET(InferParams, postproc_name);
      *(static_cast<std::string*>(static_cast<void*>(static_cast<char*>(result) + offset))) = params_map["name"];
    }
    if (params_map.find("threshold") != params_map.end()) {
      size_t offset = OFFSET(InferParams, threshold);
      if (!ModuleParamParser<float>::Parser(param_set, "threshold", params_map["threshold"],
                                            (reinterpret_cast<char*>(result) + offset))) {
        return false;
      }
    }
    return true;
  };

  auto filter_parser = [](const ModuleParamSet& param_set, const std::string& param_name, const std::string& value,
                          void* result) -> bool {
    std::unordered_map<std::string, std::string> params_map;
    if (!SplitParams(value, &params_map)) {
      LOGE(Inferencer) << "[ModuleParamParser] [" << param_name << "]:" << value << " failed";
      return false;
    }
    if (params_map.find("name") != params_map.end()) {
      if (params_map["name"].empty()) return false;
      size_t offset = OFFSET(InferParams, filter_name);
      *(reinterpret_cast<std::string*>(reinterpret_cast<void*>(reinterpret_cast<char*>(result) + offset))) =
          params_map["name"];
    }
    if (params_map.find("categories") != params_map.end()) {
      size_t offset = OFFSET(InferParams, filter_categories);
      if (!ModuleParamParser<std::string>::VectorParser(param_set, "categories", params_map["categories"],
                                                        (reinterpret_cast<char*>(result) + offset))) {
        return false;
      }
    }
    return true;
  };

  auto json_parser = [](const ModuleParamSet& param_set, const std::string& param_name, const std::string& value,
                        void* result) -> bool {
    auto result_tmp = static_cast<std::unordered_map<std::string, std::string>*>(result);
    if (value.empty()) {
      result_tmp->clear();
      return true;
    }
    rapidjson::Document doc;
    if (doc.Parse<rapidjson::kParseCommentsFlag>(value.c_str()).HasParseError()) {
      LOGE(Inferencer) << "Parse custom preprocessing parameters configuration failed. "
                       << "Error code [" << std::to_string(doc.GetParseError()) << "]"
                       << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << value;
      return false;
    }
    result_tmp->clear();
    for (auto iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
      std::string value_tmp;
      if (!iter->value.IsString()) {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
        iter->value.Accept(jwriter);
        value_tmp = sbuf.GetString();
      } else {
        value_tmp = iter->value.GetString();
      }
      (*result_tmp)[iter->name.GetString()] = value_tmp;
    }
    return true;
  };

  static const std::vector<ModuleParamDesc> register_param = {
      {"device_id", "0", "Device ordinal number.", PARAM_REQUIRED, OFFSET(InferParams, device_id),
       ModuleParamParser<uint32_t>::Parser, "uint32_t"},

      {"priority", "0", "Optional. The priority of this infer task in infer server.", PARAM_OPTIONAL,
       OFFSET(InferParams, priority), ModuleParamParser<uint32_t>::Parser, "uint32_t"},

      {"engine_num", "1",
       "Optional. infer server engine number. Increase the engine number to improve performance. "
       "However, more MLU resources will be used. It is important to choose a proper number. "
       "Usually, it could be set to the core number of the device / the core number of the model.",
       PARAM_OPTIONAL, OFFSET(InferParams, engine_num), ModuleParamParser<uint32_t>::Parser, "uint32_t"},

      {"show_stats", "false",
       "Optional. Whether show performance statistics. "
       "1/true/TRUE/True/0/false/FALSE/False these values are accepted.",
       PARAM_OPTIONAL, OFFSET(InferParams, show_stats), ModuleParamParser<bool>::Parser, "bool"},

      {"batch_strategy", "DYNAMIC",
       "Optional. The batch strategy. The options are dynamic and static. "
       "Dynamic strategy: high throughput but high latency. "
       "Static strategy: low latency but low throughput.",
       PARAM_OPTIONAL, OFFSET(InferParams, batch_strategy), batch_strategy_parser, "InferBatchStrategy"},

      {"batch_timeout", "300", "The batching timeout. unit[ms].", PARAM_OPTIONAL, OFFSET(InferParams, batch_timeout),
       ModuleParamParser<uint32_t>::Parser, "uint32_t"},

      {"interval", "1", "Optional. Inferencing one frame every [interval] frames.", PARAM_OPTIONAL,
       OFFSET(InferParams, interval), ModuleParamParser<uint32_t>::Parser, "uint32_t"},

      {"model_input_pixel_format", "RGB24",
       "Optional. The pixel format of the model input image. "
       "For using Custom preproc RGB24/BGR24/GRAY/TENSOR are supported. ",
       PARAM_OPTIONAL, OFFSET(InferParams, input_format), model_input_pixel_format_parser, "InferVideoPixelFmt"},

      {"model_path", "", "The path of the offline model.", PARAM_REQUIRED, OFFSET(InferParams, model_path),
       ModuleParamParser<std::string>::Parser, "string"},

      {"label_path", "", "The label path for model.", PARAM_OPTIONAL, OFFSET(InferParams, label_path),
       ModuleParamParser<std::string>::VectorParser, "std::vector<string>"},

      {"preproc", "",
       "Required. Parameters related to postprocessing including name and use_cpu."
       "name : Required. The class name for postprocess. The class specified by this name "
       "must inherit from class cnstream::Preproc."
       "use_cpu : Optional, default true.",
       PARAM_REQUIRED, 0, preproc_parser, "PreProcParam"},

      {"postproc", "",
       "Required. Parameters related to postprocessing including name and threshold."
       "name : Required. The class name for postprocess. The class specified by this name "
       "must inherit from class cnstream::Postproc."
       "threshold : Optional. The threshold will be set to postprocessing.",
       PARAM_REQUIRED, 0, postproc_parser, "PostProcParam"},

      {"filter", "",
       "Optional. Parameters related to filter including name and categories."
       "name : Optional. The class name for custom object filter. "
       "must inherit from class cnstream::ObjectFilterVideo. "
       "categories : Optional. The categories for object filter."
       "-1 or all/ALL means all the objects of the frame will be the inputs",
       PARAM_OPTIONAL, 0, filter_parser, "ObjectFilterParam"},

       {"custom_preproc_params", "",
       "Optional. Custom preprocessing parameters. After the inferencer module creates an instance of "
       "the preprocessing class specified by preproc_name or obj_preproc_name, the Init function of the specified "
       "preprocessing class will be called, and these parameters will be passed to Init. See Preproc::Init "
       "and ObjPreproc::Init for detail.",
       PARAM_OPTIONAL, OFFSET(InferParams, custom_preproc_params), json_parser, "CustomPreprocParams"},

       {"custom_postproc_params", "",
       "Optional. Parameters related to filter including name and categories."
       "name : Optional. The class name for custom object filter. "
       "must inherit from class cnstream::ObjectFilterVideo. "
       "categories : Optional. The categories for object filter."
       "-1 or all/ALL means all the objects of the frame will be the inputs",
       PARAM_OPTIONAL, OFFSET(InferParams, custom_postproc_params), json_parser, "CustomPostprocParams"}};

  param_helper_->Register(register_param, &param_register_);
}

Inferencer::~Inferencer() { Close(); }

bool Inferencer::Open(ModuleParamSet raw_params) {
  if (!CheckParamSet(raw_params)) {
    return false;
  }

  auto params = param_helper_->GetParams();
  uint32_t dev_cnt = 0;
  if (cnrtGetDeviceCount(&dev_cnt) != cnrtSuccess || params.device_id < 0 ||
      static_cast<uint32_t>(params.device_id) >= dev_cnt) {
    LOGE(Inferencer) << "[" << GetName() << "] device " << params.device_id << " does not exist.";
    return false;
  }

  cnrtSetDevice(params.device_id);

  preproc_ = std::shared_ptr<Preproc>(Preproc::Create(params.preproc_name));
  if (!preproc_) {
    LOGE(Inferencer) << "Can not find Preproc implemention by name: " << params.preproc_name;
    return false;
  }
  if (preproc_->Init(params.custom_preproc_params) != 0) {
    LOGE(Inferencer) << "Preprocessor init failed.";
    return false;
  }
  preproc_->hw_accel_ = (params.preproc_use_cpu == false);

  postproc_ = std::shared_ptr<Postproc>(Postproc::Create(params.postproc_name));
  if (!postproc_) {
    LOGE(Inferencer) << "Can not find Postproc implemention by name: " << params.postproc_name;
    return false;
  }
  if (postproc_->Init(params.custom_postproc_params) != 0) {
    LOGE(Inferencer) << "Postprocessor init failed.";
    return false;
  }
  postproc_->threshold_ = params.threshold;

  if (!params.filter_name.empty() || !params.filter_categories.empty()) {
    if (params.filter_name.empty()) {
      std::string categroy = params.filter_categories[0];
      std::transform(categroy.begin(), categroy.end(), categroy.begin(), ::toupper);
      if (categroy == "-1" || categroy == "ALL") {
        filter_ = std::make_shared<ObjectFilterVideo>();
        if (!filter_) {
          LOGE(Inferencer) << "Create ObjectFilterVideo failed";
          return false;
        }
      } else {
        filter_ = std::make_shared<ObjectFilterVideoCategory>();
        if (!filter_) {
          LOGE(Inferencer) << "Create ObjectFilterVideoCategory failed";
          return false;
        }
        filter_->Config(params.filter_categories);
      }
    } else {
      filter_ = std::shared_ptr<ObjectFilterVideo>(ObjectFilterVideo::Create(params.filter_name));
      if (!filter_) {
        LOGE(Inferencer) << "Can not find ObjectFilter implemention by name: " << params.filter_name;
        return false;
      }
      if (!params.filter_categories.empty()) filter_->Config(params.filter_categories);
    }
  }

  params.model_path = GetPathRelativeToTheJSONFile(params.model_path, raw_params);
  if (!params.label_path.empty()) {
    for (auto& p : params.label_path) {
      std::string path = GetPathRelativeToTheJSONFile(p, raw_params);
      std::ifstream path_fs(path);
      std::vector<std::string> label_string;
      if (path_fs.good()) {
        std::string label((std::istreambuf_iterator<char>(path_fs)), std::istreambuf_iterator<char>());
        label_string = StringSplitT(label, '\n');
      } else {
        LOGE(Inferencer) << "Failed to load label file: " << path;
        return false;
      }
      if (label_string.size()) {
        label_strings_.push_back(label_string);
      }
    }
    if (label_strings_.empty()) {
      LOGE(Inferencer) << "Load labels error";
      return false;
    }
  }

  cnrtSetDevice(params.device_id);
  server_.reset(new infer_server::InferServer(params.device_id));

  infer_server::SessionDesc desc;
  desc.name = this->GetName();
  desc.strategy = params.batch_strategy;
  desc.batch_timeout = params.batch_timeout;
  desc.priority = params.priority;
  desc.engine_num = params.engine_num;
  desc.show_perf = params.show_stats;
  desc.model = server_->LoadModel(params.model_path);
  desc.model_input_format = params.input_format;
  desc.preproc = infer_server::Preprocessor::Create();
  infer_server::SetPreprocHandler(desc.model->GetKey(), this);
  desc.postproc = infer_server::Postprocessor::Create();
  infer_server::SetPostprocHandler(desc.model->GetKey(), this);

  observer_ = std::make_shared<InferObserver>(std::bind(&Inferencer::OnProcessDone, this, std::placeholders::_1));
  session_ = server_->CreateSession(desc, observer_);
  if (!session_) return false;
  return true;
}

void Inferencer::Close() {
  if (server_ && session_) {
    infer_server::RemovePreprocHandler(server_->GetModel(session_)->GetKey());
    infer_server::RemovePostprocHandler(server_->GetModel(session_)->GetKey());
    server_->DestroySession(session_);
    session_ = nullptr;
    server_.reset();
  }
}

int Inferencer::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) {
    LOGE(INFERENCER) << "Process inputdata is nulltpr!";
    return -1;
  }

  if (data->IsEos()) {
    if (IsStreamRemoved(data->stream_id)) {
      server_->DiscardTask(session_, data->stream_id);
      server_->WaitTaskDone(session_, data->stream_id);
    } else {
      server_->WaitTaskDone(session_, data->stream_id);
    }
    TransmitData(data);
    std::unique_lock<std::mutex> lock(drop_cnt_map_mtx_);
    if (drop_cnt_map_.count(data->stream_id) != 0) {
      drop_cnt_map_.erase(data->stream_id);
    }
    lock.unlock();
    return 0;
  }
  if (data->IsRemoved()) { /* discard packets from removed-stream */
    return 0;
  }

  auto frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  auto params = param_helper_->GetParams();
  if (params.interval > 0) {
    // for interval
    uint32_t interval = params.interval;
    std::unique_lock<std::mutex> lock(drop_cnt_map_mtx_);
    if (drop_cnt_map_.count(data->stream_id) == 0) {
      drop_cnt_map_.insert(std::make_pair(data->stream_id, interval - 1));
    }
    bool drop_data = drop_cnt_map_[data->stream_id]++ != interval - 1;
    lock.unlock();

    if (drop_data) {
      // to keep data in sequence, we pass empty package to infer_server, with CNFrameInfo as user data.
      // frame won't be inferred, and CNFrameInfo will be responsed in sequence
      infer_server::PackagePtr in = infer_server::Package::Create(0, data->stream_id);
      if (!server_->Request(session_, in, data, -1)) {
        LOGE(INFERENCER) << "[" << this->GetName() << "] Process failed."
                        << " stream id: " << data->stream_id << " frame id: " << frame->frame_id;
        return -1;
      }
      return 0;
    }

    drop_cnt_map_[data->stream_id] = 0;
  }

  // Async inference:
  // 1. send data to the inferserver
  // 2. the infer-result will be notified via OnPostproc()
  // 3. process_done() will be invoked when finished
  infer_server::PackagePtr request = std::make_shared<infer_server::Package>();
  request->tag = data->stream_id;
  if (filter_) {
    CNInferObjsPtr objs_holder = nullptr;
    if (data->collection.HasValue(kCNInferObjsTag)) {
      objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
      std::lock_guard<std::mutex> lk(objs_holder->mutex_);
      auto& objs = objs_holder->objs_;
      for (auto& obj : objs) {
        if (!filter_->Filter(data, obj)) continue;
        infer_server::PreprocInput tmp;
        tmp.surf = frame->buf_surf;
        tmp.bbox = GetFullFovBbox(obj.get());
        // validate bbox to meet hw requirements,
        //   move to the handlers, some networks do have the cases ... for example, lprnet 94x24
        /*
        if (tmp.bbox.w * frame->buf_surf->GetWidth() < 64 || tmp.bbox.h * frame->buf_surf->GetHeight() < 64 ) {
          continue;
        }
        */
        tmp.has_bbox = true;
        request->data.emplace_back(new infer_server::InferData);
        auto& request_data = request->data.back();
        request_data->Set(std::move(tmp));
        request_data->SetUserData(std::make_pair(data, obj));
      }
    }
  } else {
    request->data.emplace_back(new infer_server::InferData);
    auto& request_data = request->data.back();
    infer_server::PreprocInput tmp;
    tmp.surf = frame->buf_surf;
    tmp.has_bbox = false;
    request_data->Set(std::move(tmp));
    request_data->SetUserData(std::make_pair(data, CNInferObjectPtr(nullptr)));
  }

  if (!server_->Request(session_, request, data, -1)) {
    LOGE(INFERENCER) << "[" << this->GetName() << "] Process failed."
                     << " stream id: " << data->stream_id << " frame id: " << frame->frame_id;
    return -1;
  }
  return 0;
}

bool Inferencer::CheckParamSet(const ModuleParamSet& param_set) const {
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(Inferencer) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  bool ret = true;
  auto params = param_helper_->GetParams();

  // check preprocess
  if (params.preproc_name.empty()) {
    LOGE(Inferencer) << "Preproc name can't be empty string. Please set preproc_name.";
    ret = false;
  }

  // check post_process
  if (params.postproc_name.empty()) {
    LOGE(Inferencer) << "Postproc name can't be empty string. Please set postproc_name.";
    ret = false;
  }

  ParametersChecker checker;

  if (!checker.CheckPath(params.model_path, param_set)) {
    LOGE(Inferencer) << "[model_path] : " << params.model_path << " non-existence.";
    ret = false;
  }

  return ret;
}

int Inferencer::OnPostproc(const std::vector<infer_server::InferData*>& data_vec,
                           const infer_server::ModelIO& model_output,
                           const infer_server::ModelInfo* model_info) {
  if (!data_vec.size()) return 0;

  auto params = param_helper_->GetParams();
  cnrtSetDevice(params.device_id);

  NetOutputs net_outputs;
  for (size_t i = 0; i < model_output.surfs.size(); i++) {
    net_outputs.emplace_back(model_output.surfs[i], model_output.shapes[i]);
  }

  std::vector<CNFrameInfoPtr> packages;
  std::vector<CNInferObjectPtr> objects;
  for (auto& it : data_vec) {
    auto user_data = it->GetUserData<std::pair<CNFrameInfoPtr, CNInferObjectPtr>>();
    packages.push_back(user_data.first);
    if (user_data.second) objects.push_back(user_data.second);
  }

  if (postproc_) {
    if (objects.size()) {
      return postproc_->Execute(net_outputs, *model_info, packages, objects, label_strings_);
    }
    return postproc_->Execute(net_outputs, *model_info, packages, label_strings_);
  }
  return -1;
}

}  // namespace cnstream
