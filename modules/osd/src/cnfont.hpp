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
#include FT_FREETYPE_H
#endif

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#else
#error OpenCV required
#endif

namespace cnstream {

/**
 * @brief Show chinese label in the image
 */
class CnFont {
#ifdef HAVE_FREETYPE

 public:
  /**
   * @brief Initialize the display font
   * @param
   *   font_path: the font of path
   */
  explicit CnFont(const char* font_path);
  /**
   * @brief Release font resource
   */
  ~CnFont();
  /**
   * @brief Configure font Settings
   */
  void restoreFont();
  /**
   * @brief Displays the string on the image
   * @param
   *   img: source image
   *   text: the show of message
   *   pos: the show of position
   *   color: the color of font
   * @return Size of the string
   */
  int putText(cv::Mat& img, char* text, cv::Point pos, cv::Scalar color);  // NOLINT

 private:
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
  void putWChar(cv::Mat& img, wchar_t wc, cv::Point& pos, cv::Scalar color);  // NOLINT
  CnFont& operator=(const CnFont&);

  FT_Library m_library;
  FT_Face m_face;

  // Default font output parameters
  int m_fontType;
  cv::Scalar m_fontSize;
  bool m_fontUnderline;
  float m_fontDiaphaneity;
#else

 public:
  explicit CnFont(const char* font_path) {}
  ~CnFont() {}
  int putText(cv::Mat& img, char* text, cv::Point pos, cv::Scalar color) { return 0; };  // NOLINT
#endif
};  // class CnFont

}  // namespace cnstream

#endif  // MODULES_CNFONT_HPP_
