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
#include "opencv2/opencv.hpp"
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

  dest = new wchar_t[w_size];
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

Osd::Osd(const std::string& name) : Module(name) {}

bool Osd::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("label_path") == paramSet.end()) {
    LOG(ERROR) << "Can not find label_path from module parameters.";
    return false;
  }
  std::string label_path = GetFullPath(paramSet.find("label_path")->second);
  labels_ = ::LoadLabels(label_path);
  if (labels_.empty()) {
    return false;
  }
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
  return true;
}

void Osd::Close() { /* empty */
}

#define CLIP(x) x < 0 ? 0 : (x > 1 ? 1 : x)
static thread_local auto font_ = static_cast<std::shared_ptr<CnFont>>(new CnFont("/usr/include/wqy-zenhei.ttc"));

int Osd::Process(std::shared_ptr<CNFrameInfo> data) {
  CnOsd processor(1, 1, labels_);

  std::vector<CnDetectObject> objs;
  for (const auto& it : data->objs) {
    CnDetectObject cn_obj;
    cn_obj.label = std::stoi(it->id);
    cn_obj.score = it->score;
    cn_obj.x = CLIP(it->bbox.x);
    cn_obj.y = CLIP(it->bbox.y);
    cn_obj.w = CLIP(it->bbox.w);
    cn_obj.h = CLIP(it->bbox.h);
    cn_obj.w = (it->bbox.x + cn_obj.w > 1) ? (1 - cn_obj.x) : cn_obj.w;
    cn_obj.h = (it->bbox.y + cn_obj.h > 1) ? (1 - cn_obj.y) : cn_obj.h;
    cn_obj.track_id = it->track_id.empty() ? -1 : std::stoi(it->track_id);
    objs.push_back(cn_obj);
  }
  if (!chinese_label_flag_) {
    processor.DrawLabel(*data->frame.ImageBGR(), objs);
  } else {
    processor.DrawLabel(*data->frame.ImageBGR(), objs, font_.get());
  }
  return 0;
}

}  // namespace cnstream
