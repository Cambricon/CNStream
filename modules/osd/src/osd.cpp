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

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#include "cnfont.hpp"
#include "cnosd.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

static std::vector<std::string> StringSplit(const std::string &s, char c) {
  std::stringstream ss(s);
  std::string piece;
  std::vector<std::string> result;
  while (std::getline(ss, piece, c)) {
    result.push_back(piece);
  }
  return result;
}

static std::vector<std::string> LoadLabels(const std::string& label_path) {
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

Osd::Osd(const std::string& name) : Module(name) {
  param_register_.SetModuleDesc("Osd is a module for drawing objects on image. Output image is BGR24 format.");
  param_register_.Register("label_path", "The path of the label file.");
  param_register_.Register("font_path", "The path of font.");
  param_register_.Register("label_size", " The size of the label, support value: "
                           "normal, large, larger, small, smaller and number. The default value is normal");
  param_register_.Register("text_scale", "The scale of the text, which can change the size of text put on image. "
                           "The default value is 1. scale = label_size * text_scale");
  param_register_.Register("text_thickness", "The thickness of the text, which can change the thickness of text put on "
                           "image. The default value is 1. thickness = label_size * text_thickness");
  param_register_.Register("box_thickness", "The thickness of the box drawn on the image. "
                           "thickness = label_size * box_thickness");
  param_register_.Register("secondary_label_path", "The path of the secondary label file");
  param_register_.Register("attr_keys", "The keys of attribute which you want to draw on image");
  param_register_.Register("logo", "draw 'logo' on each frame");
}

Osd::~Osd() { Close(); }

std::shared_ptr<CnOsd> Osd::GetOsdContext() {
  std::shared_ptr<CnOsd> ctx = nullptr;
  std::thread::id thread_id = std::this_thread::get_id();
  {
    RwLockReadGuard lg(ctx_lock_);
    if (osd_ctxs_.find(thread_id) != osd_ctxs_.end()) {
      ctx = osd_ctxs_[thread_id];
    }
  }
  if (!ctx) {
    ctx = std::make_shared<CnOsd>(labels_);
    if (!ctx) {
      LOG(ERROR) << "Osd::GetOsdContext() create Osd Context Failed";
      return nullptr;
    }
    ctx->SetTextScale(label_size_ * text_scale_);
    ctx->SetTextThickness(label_size_ * text_thickness_);
    ctx->SetBoxThickness(label_size_ * box_thickness_);

    ctx->SetSecondaryLabels(secondary_labels_);

#ifdef HAVE_FREETYPE
    if (!font_path_.empty()) {
      std::shared_ptr<CnFont> font = std::make_shared<CnFont>();
      float font_size = label_size_ * text_scale_ * 30;
      float space = font_size / 75;
      float step = font_size / 200;
      if (font && font->Init(font_path_, font_size, space, step)) {
        ctx->SetCnFont(font);
      } else {
        LOG(ERROR) << "Create and initialize CnFont failed.";
      }
    }
#endif
    RwLockWriteGuard lg(ctx_lock_);
    osd_ctxs_[thread_id] = ctx;
  }
  return ctx;
}

bool Osd::Open(cnstream::ModuleParamSet paramSet) {
  std::string label_path = "";
  if (paramSet.find("label_path") == paramSet.end()) {
    LOG(WARNING) << "Can not find label_path from module parameters.";
  } else {
    label_path = paramSet["label_path"];
    label_path = GetPathRelativeToTheJSONFile(label_path, paramSet);
    labels_ = LoadLabels(label_path);
    if (labels_.empty()) {
      LOG(WARNING) << "Empty label file or wrong file path.";
    } else {
#ifdef HAVE_FREETYPE
      if (paramSet.find("font_path") != paramSet.end()) {
        std::string font_path = paramSet["font_path"];
        font_path_ = GetPathRelativeToTheJSONFile(font_path, paramSet);
      }
#endif
    }
  }

  if (paramSet.find("secondary_label_path") != paramSet.end()) {
    label_path = paramSet["secondary_label_path"];
    label_path = GetPathRelativeToTheJSONFile(label_path, paramSet);
    secondary_labels_ = LoadLabels(label_path);
    if (paramSet.find("attr_keys") != paramSet.end()) {
      std::string attr_key = paramSet["attr_keys"];
      attr_key.erase(std::remove_if(attr_key.begin(), attr_key.end(), ::isspace), attr_key.end());
      attr_keys_ = StringSplit(attr_key, ',');
    }
  }
  if (paramSet.find("label_size") != paramSet.end()) {
    std::string label_size = paramSet["label_size"];
    if (label_size == "large") {
      label_size_ = 1.5;
    } else if (label_size == "larger") {
      label_size_ = 2;
    } else if (label_size == "small") {
      label_size_ = 0.75;
    } else if (label_size == "smaller") {
      label_size_ = 0.5;
    } else if (label_size != "normal") {
      float size = std::stof(paramSet["label_size"]);
      label_size_ = size;
    }
  }

  if (paramSet.find("text_scale") != paramSet.end()) {
    text_scale_ = std::stof(paramSet["text_scale"]);
  }

  if (paramSet.find("text_thickness") != paramSet.end()) {
    text_thickness_ = std::stof(paramSet["text_thickness"]);
  }

  if (paramSet.find("box_thickness") != paramSet.end()) {
    box_thickness_ = std::stof(paramSet["box_thickness"]);
  }

  if (paramSet.find("logo") != paramSet.end()) {
    logo_ = paramSet["logo"];
  }
  return true;
}

void Osd::Close() {
  osd_ctxs_.clear();
}

int Osd::Process(std::shared_ptr<CNFrameInfo> data) {
  std::shared_ptr<CnOsd> ctx = GetOsdContext();
  if (ctx == nullptr) {
    LOG(ERROR) << "Get Osd Context Failed.";
    return -1;
  }

  CNDataFramePtr frame = cnstream::GetCNDataFramePtr(data);
  if (frame->width < 0 || frame->height < 0) {
    LOG(ERROR) << "OSD module processed illegal frame: width or height may < 0.";
    return -1;
  }
  if (frame->ptr_cpu[0] == nullptr && frame->ptr_mlu[0] == nullptr && frame->cpu_data == nullptr &&
      frame->mlu_data == nullptr) {
    LOG(ERROR) << "OSD module processed illegal frame: data ptr point to nullptr.";
    return -1;
  }

  CNInferObjsPtr objs_holder = nullptr;
  if (data->datas.find(CNInferObjsPtrKey) != data->datas.end()) {
    objs_holder = cnstream::GetCNInferObjsPtr(data);
  }

  if (!logo_.empty()) {
    ctx->DrawLogo(frame->ImageBGR(), logo_);
  }
  ctx->DrawLabel(frame->ImageBGR(), objs_holder, attr_keys_);
  return 0;
}

bool Osd::CheckParamSet(const ModuleParamSet& paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Osd] Unknown param: " << it.first;
    }
  }
  if (paramSet.find("label_path") != paramSet.end()) {
    if (!checker.CheckPath(paramSet.at("label_path"), paramSet)) {
      LOG(ERROR) << "[Osd] [label_path] : " << paramSet.at("label_path") << " non-existence.";
      ret = false;
    }
  }
  if (paramSet.find("font_path") != paramSet.end()) {
    if (!checker.CheckPath(paramSet.at("font_path"), paramSet)) {
      LOG(ERROR) << "[Osd] [font_path] : " << paramSet.at("font_path") << " non-existence.";
      ret = false;
    }
  }
  if (paramSet.find("secondary_label_path") != paramSet.end()) {
    if (!checker.CheckPath(paramSet.at("secondary_label_path"), paramSet)) {
      LOG(ERROR) << "[Osd] [secondary_label_path] : " << paramSet.at("secondary_label_path") << " non-existence.";
      ret = false;
    }
  }
  std::string err_msg;
  if (!checker.IsNum({"text_scale", "text_thickness", "box_thickness"}, paramSet, err_msg)) {
    LOG(ERROR) << "[Osd] " << err_msg;
    ret = false;
  }
  if (paramSet.find("label_size") != paramSet.end()) {
    std::string label_size = paramSet.at("label_size");
    if (label_size != "normal" && label_size != "large" && label_size != "larger" &&
        label_size != "small" && label_size != "smaller") {
      if (!checker.IsNum({"label_size"}, paramSet, err_msg)) {
        LOG(ERROR) << "[Osd] " << err_msg << " Please choose from 'normal', 'large', 'larger', 'small', 'smaller'."
                   << " Or set a number to it.";
        ret = false;
      }
    }
  }
  return ret;
}

}  // namespace cnstream
