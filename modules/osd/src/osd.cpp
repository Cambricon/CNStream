/*************************************************************************
 *  Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include "osd.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnfont.hpp"
#include "cnosd.hpp"
#include "cnstream_frame_va.hpp"
#include "osd_handler.hpp"
#include "private/cnstream_param.hpp"

namespace cnstream {

// local var,
//   init in Open() and destroy in Close()
static std::shared_ptr<CnFont> s_font = nullptr;

struct OsdThreadLocalContext {
  std::shared_ptr<CnOsd> processor_ = nullptr;
};

/**
 *@brief osd context structure
 */
struct OsdContext {
  OsdHandler *handler_ = nullptr;
};

static thread_local OsdThreadLocalContext g_osd_tl_context;

static std::vector<std::string> LoadLabels(const std::string &label_path) {
  std::vector<std::string> labels;
  std::ifstream ifs(label_path);
  if (!ifs.is_open()) return labels;

  while (!ifs.eof()) {
    std::string label_name;
    std::getline(ifs, label_name);
    labels.push_back(label_name);
  }

  ifs.close();
  return labels;
}

Osd::Osd(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("Osd is a module for drawing objects on image. Output image is BGR24 format.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<OsdParams>(name));
  auto label_path_parser = [](const ModuleParamSet &param_set, const std::string &param_name, const std::string &value,
                              void *result) -> bool {
    if (value.empty()) {
      LOGW(OSD) << "Can not find " << param_name << " from module parameters.";
      return true;
    }
    std::string label_path = GetPathRelativeToTheJSONFile(value, param_set);
    std::vector<std::string> labels = LoadLabels(label_path);
    if (labels.empty()) {
      LOGW(OSD) << "Empty label file or wrong file path.";
    }
    *static_cast<std::vector<std::string> *>(result) = labels;
    return true;
  };

  auto font_path_parser = [](const ModuleParamSet &param_set, const std::string &param_name, const std::string &value,
                             void *result) -> bool {
    if (value.empty()) {
      LOGW(OSD) << "Can not find " << param_name << " from module parameters.";
      return true;
    }
#ifdef HAVE_FREETYPE
    *static_cast<std::string *>(result) = GetPathRelativeToTheJSONFile(value, param_set);
#endif
    return true;
  };

  auto label_size_parser = [](const ModuleParamSet &param_set, const std::string &param_name, const std::string &value,
                              void *result) -> bool {
    if (value == "large") {
      *static_cast<float *>(result) = 1.5;
    } else if (value == "larger") {
      *static_cast<float *>(result) = 2;
    } else if (value == "small") {
      *static_cast<float *>(result) = 0.75;
    } else if (value == "smaller") {
      *static_cast<float *>(result) = 0.5;
    } else if (value == "normal") {
      *static_cast<float *>(result) = 1;
    } else {
      ParametersChecker checker;
      std::string err_msg;
      if (!checker.IsNum({"label_size"}, param_set, err_msg)) {
        LOGE(OSD) << "[Osd] " << err_msg << " Please choose from 'normal', 'large', 'larger', 'small', 'smaller'."
                  << " Or set a number to it.";
        return false;
      }
      *static_cast<float *>(result) = std::stof(value);
    }
    return true;
  };

  static const std::vector<ModuleParamDesc> register_param = {
      {"hw_accel", "false",
       "use hardware to draw bboxes,lables, and features,etc."
       "The default value is false.",
       PARAM_OPTIONAL, OFFSET(OsdParams, hw_accel), ModuleParamParser<bool>::Parser, "bool"},
      {"label_path", "", "The path of the label file.", PARAM_OPTIONAL, OFFSET(OsdParams, labels), label_path_parser,
       "std::vector<std::string>"},
      {"secondary_label_path", "", "The path of the secondary inference file.", PARAM_OPTIONAL,
       OFFSET(OsdParams, secondary_labels), label_path_parser, "vector<string>"},
      {"attr_keys", "", "The keys of attribute which you want to draw on image.", PARAM_OPTIONAL,
       OFFSET(OsdParams, attr_keys), ModuleParamParser<std::string>::VectorParser, "vector<string>"},
      {"font_path", "", "The path of the font file.", PARAM_OPTIONAL, OFFSET(OsdParams, font_path), font_path_parser,
       "std::string"},
      {"logo", "", "draw 'logo' on each frame", PARAM_OPTIONAL, OFFSET(OsdParams, logo),
       ModuleParamParser<std::string>::Parser, "string"},
      {"osd_handler", "", "The name of osd handler.", PARAM_OPTIONAL, OFFSET(OsdParams, osd_handler_name),
       ModuleParamParser<std::string>::Parser, "string"},
      {"text_scale", "1",
       "The scale of the text, which can change the size of text put on image. "
       "The default value is 1.",
       PARAM_OPTIONAL, OFFSET(OsdParams, text_scale), ModuleParamParser<float>::Parser, "float"},
      {"text_thickness", "1",
       "The thickness of the text, which can change "
       "the thickness of text put on image. The default value is 1.",
       PARAM_OPTIONAL, OFFSET(OsdParams, text_thickness), ModuleParamParser<float>::Parser, "float"},
      {"box_thickness", "1", "The thickness of the box drawed on the image.", PARAM_OPTIONAL,
       OFFSET(OsdParams, box_thickness), ModuleParamParser<float>::Parser, "float"},
      {"label_size", "normal",
       "The size of the label, support value: "
       "normal, large, larger, small, smaller and number. The default value is normal.",
       PARAM_OPTIONAL, OFFSET(OsdParams, label_size), label_size_parser, "float"}};
  param_helper_->Register(register_param, &param_register_);
}

Osd::~Osd() { Close(); }

std::shared_ptr<OsdContext> Osd::GetOsdContext(CNFrameInfoPtr data) {
  auto params = param_helper_->GetParams();
  if (!g_osd_tl_context.processor_) {
    g_osd_tl_context.processor_ = std::make_shared<CnOsd>(params.labels);
    if (!g_osd_tl_context.processor_) {
      LOGE(OSD) << "Osd::GetOsdContext() create g_osd_tl_context processor Failed";
      return nullptr;
    }
    g_osd_tl_context.processor_->SetTextScale(params.label_size * params.text_scale);
    g_osd_tl_context.processor_->SetTextThickness(params.label_size * params.text_thickness);
    g_osd_tl_context.processor_->SetBoxThickness(params.label_size * params.box_thickness);
    g_osd_tl_context.processor_->SetSecondaryLabels(params.secondary_labels);

    if (s_font) {
      g_osd_tl_context.processor_->SetCnFont(s_font);
    }
    if (params.hw_accel) {
      g_osd_tl_context.processor_->SetHwAccel(true);
    }
  }

  std::shared_ptr<OsdContext> ctx = nullptr;
  std::string stream_id = data->stream_id;
  {
    RwLockReadGuard lg(ctx_lock_);
    auto search = osd_ctxs_.find(stream_id);
    if (search != osd_ctxs_.end()) {
      // context exists
      ctx = search->second;
    }
  }
  if (!ctx) {
    ctx = std::make_shared<OsdContext>();
    RwLockWriteGuard lg(ctx_lock_);
    osd_ctxs_[stream_id] = ctx;
  }

  return ctx;
}

bool Osd::Open(cnstream::ModuleParamSet param_set) {
  if (false == CheckParamSet(param_set)) {
    return false;
  }

  // preload font library
#ifdef HAVE_FREETYPE
  auto params = param_helper_->GetParams();
  if (!params.font_path.empty()) {
    s_font = std::make_shared<CnFont>();
    float font_size = params.label_size * params.text_scale * 30;
    float space = font_size / 75;
    float step = font_size / 200;
    LOGI(OSD) << "FontPath = " << params.font_path << std::endl;
    if (s_font && s_font->Init(params.font_path, font_size, space, step)) {
      // do nothing
    } else {
      LOGE(OSD) << "Create and initialize CnFont failed.";
      s_font.reset();
      s_font = nullptr;
    }
  }
#endif
  return true;
}

void Osd::Close() {
  osd_ctxs_.clear();
  if (s_font) {
    s_font.reset();
    s_font = nullptr;
  }
}

int Osd::Process(std::shared_ptr<CNFrameInfo> data) {
  if (!data) {
    LOGE(OSD) << "Process input data is nulltpr!";
    return -1;
  }
  if (data->IsRemoved()) {
    return 0;
  }

  std::shared_ptr<OsdContext> ctx = GetOsdContext(data);
  if (ctx == nullptr) {
    LOGE(OSD) << "Get Osd Context Failed.";
    return -1;
  }

  CNDataFramePtr frame = data->collection.Get<CNDataFramePtr>(kCNDataFrameTag);

  auto params = param_helper_->GetParams();
  CNInferObjsPtr objs_holder = nullptr;
  if (data->collection.HasValue(kCNInferObjsTag)) {
    objs_holder = data->collection.Get<CNInferObjsPtr>(kCNInferObjsTag);
  }
  if (!objs_holder) {
    return 0;
  }

  if (!params.logo.empty()) {
    g_osd_tl_context.processor_->DrawLogo(frame, params.logo);
  }

  if (!ctx->handler_ && !params.osd_handler_name.empty()) {
    ctx->handler_ = OsdHandler::Create(params.osd_handler_name);
  }

  if (ctx->handler_) {
    std::vector<OsdHandler::DrawInfo> info;
    std::unique_lock<std::mutex> lk(objs_holder->mutex_);
    const CNObjsVec &input_objs = objs_holder->objs_;
    if (0 == ctx->handler_->GetDrawInfo(input_objs, params.labels, &info)) {
      g_osd_tl_context.processor_->DrawLabel(frame, info);
      g_osd_tl_context.processor_->update_vframe(frame);
    }
  } else {
    std::unique_lock<std::mutex> lk(objs_holder->mutex_);
    const CNObjsVec &input_objs = objs_holder->objs_;
    g_osd_tl_context.processor_->DrawLabel(frame, input_objs, params.attr_keys);
    g_osd_tl_context.processor_->update_vframe(frame);
  }
  return 0;
}

void Osd::OnEos(const std::string &stream_id) {
  LOGI(OSD) << this->GetName() << " OnEos flow-EOS arrived:  " << stream_id;
  {
    RwLockWriteGuard lg(ctx_lock_);
    auto search = osd_ctxs_.find(stream_id);
    if (search != osd_ctxs_.end()) {
      if (search->second->handler_) {
        delete search->second->handler_;
        search->second->handler_ = nullptr;
      }
      osd_ctxs_.erase(stream_id);
    }
  }
}

bool Osd::CheckParamSet(const ModuleParamSet& param_set) const {
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(OSD) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }

  auto params = param_helper_->GetParams();

  // ParametersChecker checker;
  bool ret = true;

  if (param_set.find("label_path") != param_set.end() && params.labels.empty()) {
    ret = false;
  }

  if (param_set.find("secondary_label_path") != param_set.end() && params.secondary_labels.empty()) {
    ret = false;
  }
  // for (auto& label : params.labels) {
  //   if (!checker.CheckPath(label, param_set)) {
  //     LOGE(OSD) << "[Osd] [label_path] : " << label << " non-existence.";
  //     ret = false;
  //   }
  // }

  // for (auto& label : params.secondary_labels) {
  //   if (!checker.CheckPath(label, param_set)) {
  //     LOGE(OSD) << "[Osd] [secondary label_path] : " << label << " non-existence.";
  //     ret = false;
  //   }
  // }

  return ret;
}

}  // namespace cnstream
