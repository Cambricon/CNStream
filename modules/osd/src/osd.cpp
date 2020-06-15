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

#include "cnosd.h"
#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

static std::vector<string> LoadLabels(const std::string& label_path) {
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

namespace cnstream {

/**
 *@brief osd context structure
 */
struct OsdContext {
  CnOsd* processer_ = nullptr;
  uint32_t frame_index_;
};

#ifdef HAVE_FREETYPE

CnFont::CnFont(const char* font_path) {
  if (FT_Init_FreeType(&m_library)) {
    LOG(ERROR) << "FreeType init errors";
  }
  if (FT_New_Face(m_library, font_path, 0, &m_face)) {
    LOG(ERROR) << "Can not create a font, please checkout the font path: " << m_library;
  }

  // Set font args
  restoreFont();

  // Set font env
  setlocale(LC_ALL, "");
}

CnFont::~CnFont() {
  FT_Done_Face(m_face);
  FT_Done_FreeType(m_library);
}

// Restore the original font Settings
void CnFont::restoreFont() {
  m_fontType = 0;

  m_fontSize.val[0] = 20;
  m_fontSize.val[1] = 0.5;
  m_fontSize.val[2] = 0.1;
  m_fontSize.val[3] = 0;

  m_fontUnderline = false;

  m_fontDiaphaneity = 1.0;

  // Set character size
  FT_Set_Pixel_Sizes(m_face, static_cast<int>(m_fontSize.val[0]), 0);
}

int CnFont::ToWchar(char*& src, wchar_t*& dest, const char* locale) {
  if (src == NULL) {
    dest = NULL;
    return 0;
  }

  // Set the locale according to the environment variable
  setlocale(LC_CTYPE, locale);

  // Gets the required wide character size to convert to
  int w_size = mbstowcs(NULL, src, 0) + 1;

  if (w_size == 0) {
    dest = NULL;
    return -1;
  }

  dest = new (std::nothrow) wchar_t[w_size];
  if (!dest) {
    return -1;
  }

  int ret = mbstowcs(dest, src, strlen(src) + 1);
  if (ret <= 0) {
    return -1;
  }
  return 0;
}

int CnFont::putText(cv::Mat& img, char* text, cv::Point pos, cv::Scalar color) {
  if (img.data == nullptr) return -1;
  if (text == nullptr) return -1;

  wchar_t* w_str;
  ToWchar(text, w_str);

  int i;
  for (i = 0; w_str[i] != '\0'; ++i) {
    putWChar(img, w_str[i], pos, color);
  }

  return i;
}

// Output the current character and update the m pos position
void CnFont::putWChar(cv::Mat& img, wchar_t wc, cv::Point& pos, cv::Scalar color) {
  // Generate a binary bitmap of a font based on unicode
  FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
  FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
  FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);

  FT_GlyphSlot slot = m_face->glyph;

  // Cols and rows
  int rows = slot->bitmap.rows;
  int cols = slot->bitmap.width;

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      int off = i * slot->bitmap.pitch + j / 8;
      if (slot->bitmap.buffer[off] & (0xC0 >> (j % 8))) {
        int r = pos.y - (rows - 1 - i);
        int c = pos.x + j;

        if (r >= 0 && r < img.rows && c >= 0 && c < img.cols) {
          cv::Vec3b pixel = img.at<cv::Vec3b>(cv::Point(c, r));
          cv::Scalar scalar = cv::Scalar(pixel.val[0], pixel.val[1], pixel.val[2]);

          // Color fusion
          float p = m_fontDiaphaneity;
          for (int k = 0; k < 4; ++k) {
            scalar.val[k] = scalar.val[k] * (1 - p) + color.val[k] * p;
          }
          img.at<cv::Vec3b>(cv::Point(c, r))[0] = (unsigned char)(scalar.val[0]);
          img.at<cv::Vec3b>(cv::Point(c, r))[1] = (unsigned char)(scalar.val[1]);
          img.at<cv::Vec3b>(cv::Point(c, r))[2] = (unsigned char)(scalar.val[2]);
        }
      }
    }
  }
  // Modify the output position of the next word
  double space = m_fontSize.val[0] * m_fontSize.val[1];
  double sep = m_fontSize.val[0] * m_fontSize.val[2];

  pos.x += static_cast<int>((cols ? cols : space) + sep);
}
#endif

Osd::Osd(const std::string& name) : Module(name) {
  param_register_.SetModuleDesc("Osd is a module for drawing objects on image. Output image is BGR24 format.");
  param_register_.Register("label_path", "The path of the label file.");
  param_register_.Register("chinese_label_flag", "Whether chinese label will be used.");
}

Osd::~Osd() { Close(); }

OsdContext* Osd::GetOsdContext(CNFrameInfoPtr data) {
  if (data->channel_idx >= GetMaxStreamNumber()) {
    return nullptr;
  }

  OsdContext* ctx = nullptr;
  auto it = osd_ctxs_.find(data->channel_idx);
  if (it != osd_ctxs_.end()) {
    ctx = it->second;
  } else {
    ctx = new (std::nothrow) OsdContext;
    if (!ctx) {
      LOG(ERROR) << "Osd::GetOsdContext() new OsdContext Failed";
      return nullptr;
    }
    ctx->frame_index_ = 0;
    osd_ctxs_[data->channel_idx] = ctx;
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
    labels_ = ::LoadLabels(label_path);
    if (labels_.empty()) {
      LOG(WARNING) << "Empty label file or wrong file path.";
    } else {
#ifdef HAVE_FREETYPE
      if (paramSet.find("chinese_label_flag") != paramSet.end()) {
        if (paramSet.find("chinese_label_flag")->second == "true") {
          chinese_label_flag_ = true;
        } else if (paramSet.find("chinese_label_flag")->second == "false") {
          chinese_label_flag_ = false;
        } else {
          LOG(ERROR) << "chinese_label_flag must be set to true or false";
          return false;
        }
      }
#endif
    }
  }
  // 1, one channel binded to one thread, it can't be one channel binded to multi threads.
  // 2, the hash value, each channel_idx (key) mapped to, is unique. So, each bucket stores one value.
  // 3, set the buckets number of the unordered map to the maximum channel number before the threads are started,
  //    thus, it doesn't need to be rehashed after.
  // The three conditions above will guarantee, multi threads will write the different buckets of the unordered map,
  // and the unordered map will not be rehashed after, so, it will not cause thread safe issue, when multi threads write
  // the unordered map at the same time without locks
  osd_ctxs_.rehash(GetMaxStreamNumber());
  return true;
}

void Osd::Close() {
  for (auto& pair : osd_ctxs_) {
    if (pair.second->processer_) {
      delete pair.second->processer_;
      pair.second->processer_ = nullptr;
    }
    delete pair.second;
  }
  osd_ctxs_.clear();
}

#define CLIP(x) x < 0 ? 0 : (x > 1 ? 1 : x)
static thread_local auto font_ =
    static_cast<std::shared_ptr<CnFont>>(new (std::nothrow) CnFont("/usr/include/wqy-zenhei.ttc"));

int Osd::Process(std::shared_ptr<CNFrameInfo> data) {
  OsdContext* ctx = GetOsdContext(data);
  if (ctx == nullptr) {
    LOG(ERROR) << "Get Osd Context Failed.";
    return -1;
  }
  if (data->frame.width < 0 || data->frame.height < 0) {
    LOG(ERROR) << "OSD module processed illegal frame: width or height may < 0.";
    return -1;
  }
  if (data->frame.ptr_cpu[0] == nullptr && data->frame.ptr_mlu[0] == nullptr
      && data->frame.cpu_data == nullptr && data->frame.mlu_data == nullptr) {
    LOG(ERROR) << "OSD module processed illegal frame: data ptr point to nullptr.";
    return -1;
  }

  if (!ctx->processer_) {
    ctx->processer_ = new (std::nothrow) CnOsd(1, 1, labels_);
    if (!ctx->processer_) {
      LOG(ERROR) << "Osd::Process() new CnOsd failed";
      return -1;
    }
  }

  std::vector<DetectObject> objs;
  for (const auto& it : data->objs) {
    DetectObject obj;
    obj.label = it->id.empty() ? -1 : std::stoi(it->id);
    obj.score = it->score;
    obj.x = CLIP(it->bbox.x);
    obj.y = CLIP(it->bbox.y);
    obj.width = CLIP(it->bbox.w);
    obj.height = CLIP(it->bbox.h);
    obj.width = (obj.x + obj.width > 1) ? (1 - obj.x) : obj.width;
    obj.height = (obj.y + obj.height > 1) ? (1 - obj.y) : obj.height;
    obj.track_id = it->track_id.empty() ? -1 : std::stoi(it->track_id);
    objs.push_back(obj);
  }

  if (!chinese_label_flag_) {
    ctx->processer_->DrawLabel(*data->frame.ImageBGR(), objs);
  } else {
    ctx->processer_->DrawLabel(*data->frame.ImageBGR(), objs, font_.get());
  }
  return 0;
}

bool Osd::CheckParamSet(const ModuleParamSet& paramSet) const {
  ParametersChecker checker;
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Osd] Unknown param: " << it.first;
    }
  }
  if (paramSet.find("label_path") != paramSet.end()) {
    if (!checker.CheckPath(paramSet.at("label_path"), paramSet)) {
      LOG(ERROR) << "[Osd] [label_path] : " << paramSet.at("label_path") << " non-existence.";
      return false;
    }
  }
  if (paramSet.find("chinese_label_flag") != paramSet.end()) {
    if (paramSet.at("chinese_label_flag") != "true" && paramSet.at("chinese_label_flag") != "false") {
      LOG(ERROR) << "[Osd] [chinese_label_flag] must be true or false.";
      return false;
    }
  }
  return true;
}

}  // namespace cnstream
