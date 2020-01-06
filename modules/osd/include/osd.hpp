/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_OSD_H_
#define MODULES_OSD_H_
/**
 *  @file osd.hpp
 *
 *  This file contains a declaration of class Osd
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef HAVE_FREETYPE
#include <ctype.h>
#include <ft2build.h>
#include <locale.h>
#include <wchar.h>
#include <cmath>
#include FT_FREETYPE_H
#endif

#include "cnstream_core.hpp"
#include "cnstream_module.hpp"

namespace cnstream {

// Pointer for frame info
using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

struct OsdContext;

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
};

/**
 * @brief Draw objects on image,output is bgr24 images
 */
class Osd : public Module, public ModuleCreator<Osd> {
 public:
  /**
   *  @brief  Generate osd
   *
   *  @param  Name : Module name
   *
   *  @return None
   */
  explicit Osd(const std::string& name);

  /**
   * @brief Release osd
   * @param None
   * @return None
   */
  ~Osd();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet :
   * @verbatim
   *   label_path: label path
   * @endverbatim
   *
   * @return if module open succeed
   */
  bool Open(cnstream::ModuleParamSet paramSet) override;

  /**
   * @brief  Called by pipeline when pipeline stop
   *
   * @param  None
   *
   * @return  None
   */
  void Close() override;

  /**
   * @brief Do for each frame
   *
   * @param data : Pointer to the frame info
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   *
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(ModuleParamSet paramSet) override;

 private:
  OsdContext* GetOsdContext(CNFrameInfoPtr data);
  std::unordered_map<int, OsdContext*> osd_ctxs_;
  std::vector<std::string> labels_;
  bool chinese_label_flag_ = false;
};  // class osd

}  // namespace cnstream

#endif  // MODULES_OSD_H_
