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

#ifndef _CNOSD_HPP_
#define _CNOSD_HPP_

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "osd.hpp"
#include "cnstream_frame_va.hpp"

namespace cnstream {

class CnFont;

class CnOsd {
 public:
  CnOsd() = delete;
  explicit CnOsd(const std::vector<std::string>& labels);

  inline void SetTextScale(float scale)  { text_scale_ = scale; }
  inline void SetTextThickness(float thickness)  { text_thickness_ = thickness; }
  inline void SetBoxThickness(float thickness)  { box_thickness_ = thickness; }
  inline void SetSecondaryLabels(std::vector<std::string> labels) { secondary_labels_ = labels; }
  inline void SetCnFont(std::shared_ptr<CnFont> cn_font) { cn_font_ = cn_font; }

  void DrawLabel(cv::Mat *image, const CNObjsVec& objects, std::vector<std::string> attr_keys = {}) const;
  void DrawLogo(cv::Mat *image, std::string logo) const;

 private:
  std::pair<cv::Point, cv::Point> GetBboxCorner(const cnstream::CNInferObject &object,
                                                int img_width, int img_height) const;
  bool LabelIsFound(const int &label_id) const;
  int GetLabelId(const std::string &label_id_str) const;
  void DrawBox(cv::Mat* image, const cv::Point &top_left, const cv::Point &bottom_right,
               const cv::Scalar &color) const;
  void DrawText(cv::Mat* image, const cv::Point &bottom_left, const std::string &text, const cv::Scalar &color,
                float scale = 1, int* text_height = nullptr) const;
  int CalcThickness(int image_width, float thickness) const;
  double CalcScale(int image_width, float scale) const;

  float text_scale_ = 1;
  float text_thickness_ = 1;
  float box_thickness_ = 1;
  std::vector<std::string> labels_;
  std::vector<std::string> secondary_labels_;
  std::vector<cv::Scalar> colors_;
  int font_ = cv::FONT_HERSHEY_SIMPLEX;
  std::shared_ptr<CnFont> cn_font_;
};  // class CnOsd

}  // namespace cnstream

#endif  // _CNOSD_HPP_
