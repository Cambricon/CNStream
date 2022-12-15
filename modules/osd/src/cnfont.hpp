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

#ifndef MODULES_CNFONT_HPP_
#define MODULES_CNFONT_HPP_

#ifdef HAVE_FREETYPE
#include <ctype.h>
#include <ft2build.h>
#include <locale.h>
#include <wchar.h>
#include <cmath>
#include <string>
#include FT_FREETYPE_H
#endif

#include "cnstream_frame_va.hpp"
#include "opencv2/imgproc/imgproc.hpp"

namespace cnstream {

/**
 * @brief Show chinese label in the image
 */
class CnFont {
#ifdef HAVE_FREETYPE

 public:
  /**
   * @brief Constructor of CnFont
   */
  CnFont() {}
  /**
   * @brief Release font resource
   */
  ~CnFont();
  /**
   * @brief Initialize the display font
   * @param
   *   font_path: the font of path
   */
  bool Init(const std::string& font_path, float font_pixel = 30, float space = 0.4, float step = 0.15);

  /**
   * @brief Configure font Settings
   */
  void restoreFont(float font_pixel = 30, float space = 0.4, float step = 0.15);
  /**
   * @brief Displays the string on the image
   * @param
   *   img: source image
   *   text: the show of message
   *   pos: the show of position
   *   color: the color of font
   * @return Size of the string
   */
  int putText(CNDataFramePtr frame, char* text, cv::Point pos, cv::Scalar color);
  bool GetTextSize(char* text, uint32_t* width, uint32_t* height);
  uint32_t GetFontPixel();

  int putText(char* text, cv::Scalar color, cv::Scalar bg_color, void* argb1555, cv::Size size);

 private:
  void GetWCharSize(wchar_t wc, uint32_t* width, uint32_t* height);
  /**
   * @brief Converts character to wide character
   * @param
   *   src: The original string
   *   dst: The Destination wide string
   *   locale: Coded form
   * @return
   *   -1: Conversion failure
   *    0: Conversion success
   */
  int ToWchar(char*& src, wchar_t*& dest, const char* locale = "C.UTF-8");  // NOLINT

  /**
   * @brief Print single wide character in the image
   * @param
   *   img: source image
   *   wc: single wide character
   *   pos: the show of position
   *   color: the color of font
   */
  void putWChar(CNDataFramePtr frame, wchar_t wc, cv::Point& pos, cv::Scalar color);  // NOLINT
  void putWChar(wchar_t wc, cv::Point& pos, cv::Scalar color, cv::Scalar bg_color, void* bitmap,  // NOLINT
                cv::Size size);
  CnFont& operator=(const CnFont&);

  FT_Library m_library;
  FT_Face m_face;
  bool is_initialized_ = false;

  // Default font output parameters
  int m_fontType;
  cv::Scalar m_fontSize;
  bool m_fontUnderline;
  float m_fontDiaphaneity;

  std::mutex mutex_;
#else

 public:
  explicit CnFont(const char* font_path) {}
  ~CnFont() {}
  int putText(CNDataFramePtr frame, char* text, cv::Point pos, cv::Scalar color) { return 0; }  // NOLINT
  bool GetTextSize(char* text, uint32_t* width, uint32_t* height) { return true; }
  uint32_t GetFontPixel() { return 0; }
  int putText(char* text, cv::Scalar color, cv::Scalar bg_color, void* argb1555, cv::Size size) { return 0; }  // NOLINT
#endif
};  // class CnFont

}  // namespace cnstream

#endif  // MODULES_CNFONT_HPP_
