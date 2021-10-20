/*************************************************************************
 *  Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include "cnfont.hpp"

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include <string>

#include "cnstream_logging.hpp"

namespace cnstream {

#ifdef HAVE_FREETYPE

bool CnFont::Init(const std::string &font_path, float font_pixel, float space, float step) {
if (FT_Init_FreeType(&m_library)) {
    LOGE(OSD) << "FreeType init errors";
    return false;
  }
  if (FT_New_Face(m_library, font_path.c_str(), 0, &m_face)) {
    FT_Done_FreeType(m_library);
    LOGE(OSD) << "Can not create a font, please checkout the font path: " << m_library;
    return false;
  }
  is_initialized_ = true;

  // Set font args
  restoreFont(font_pixel, space, step);

  // Set font env
  setlocale(LC_ALL, "");

  return true;
}

CnFont::~CnFont() {
  if (is_initialized_) {
    FT_Done_Face(m_face);
    FT_Done_FreeType(m_library);
  }
}

// Restore the original font Settings
void CnFont::restoreFont(float font_pixel, float space, float step) {
  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init CnFont first.";
    return;
  }
  m_fontType = 0;

  m_fontSize.val[0] = font_pixel;
  m_fontSize.val[1] = space;
  m_fontSize.val[2] = step;
  m_fontSize.val[3] = 0;

  m_fontUnderline = false;

  m_fontDiaphaneity = 1.0;

  // Set character size
  FT_Set_Pixel_Sizes(m_face, static_cast<int>(m_fontSize.val[0]), 0);
}

uint32_t CnFont::GetFontPixel() {
  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init CnFont first.";
    return 0;
  }
  return m_fontSize.val[0];
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

bool CnFont::GetTextSize(char* text, uint32_t* width, uint32_t* height) {
  if (!width || !height || !text) {
    LOGE(OSD) << " [CnFont] [GetTextSize] The text, width or height is nullptr.";
    return false;
  }
  if (!is_initialized_) {
    LOGE(OSD) << " [CnFont] [GetTextSize] Please init CnFont first.";
    return false;
  }
  wchar_t* w_str;
  if (ToWchar(text, w_str) == -1) {
    LOGE(OSD) << "[CnFont] [GetTextSize] [ToWchar] failed.";
    return -1;
  }

  uint32_t w_char_width = 0, w_char_height = 0;
  double space = m_fontSize.val[0] * m_fontSize.val[1];
  double sep = m_fontSize.val[0] * m_fontSize.val[2];

  for (int i = 0; w_str[i] != '\0'; ++i) {
    GetWCharSize(w_str[i], &w_char_width, &w_char_height);
    if (*height < w_char_height) {
      *height = w_char_height;
    }
    if (w_char_width) {
      *width += w_char_width;
    } else {
      *width += space;
    }
    *width += sep;
  }
  return true;
}

void CnFont::GetWCharSize(wchar_t wc, uint32_t* width, uint32_t* height) {
  if (!width || !height) {
    LOGE(OSD) << " [CnFont] [GetWCharSize] The width or height is nullptr.";
    return;
  }

  FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
  FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
  FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);

  FT_GlyphSlot slot = m_face->glyph;

  // height and width
  *height = slot->bitmap.rows;
  *width = slot->bitmap.width;
}

int CnFont::putText(cv::Mat& img, char* text, cv::Point pos, cv::Scalar color) {
  if (img.data == nullptr) {
    LOGE(OSD) << "[CnFont] [putText] img.data is nullptr.";
    return -1;
  }

  if (text == nullptr) {
    LOGE(OSD) << "[CnFont] [putText] text is nullptr.";
  }

  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init CnFont first.";
    return -1;
  }

  wchar_t* w_str;
  if (ToWchar(text, w_str) == -1) {
    LOGE(OSD) << "[CnFont] [putText] [ToWchar] failed.";
    return -1;
  }

  for (int i = 0; w_str[i] != '\0'; ++i) {
    putWChar(img, w_str[i], pos, color);
  }

  return 0;
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

}  // namespace cnstream
