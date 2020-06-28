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

#ifndef _CNOSD_H_
#define _CNOSD_H_

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "osd.hpp"

using cv::Mat;
using cv::Point;
using cv::Scalar;
using std::string;
using std::vector;

struct DetectObject {
  /// Object detection label
  int label;
  /// Object detection confidence rate
  float score;
  /// Struct BoundingBox
  float x;
  float y;
  float width;
  float height;
  int track_id;
};

class CnOsd {
 private:
  size_t rows_ = 1;
  size_t cols_ = 1;
  float text_scale_coef_ = 0.002;
  float text_thickness_coef_ = 0.008;
  vector<string> labels_;
  vector<Scalar> colors_;
  int font_ = cv::FONT_HERSHEY_SIMPLEX;

 public:
  CnOsd() = delete;
  CnOsd(size_t rows, size_t cols, const vector<std::string>& labels);

  inline size_t rows() const { return rows_; }
  inline size_t cols() const { return cols_; }
  void SetTextScaleCoef(float coef)  { text_scale_coef_ = coef; }
  inline float GetTextScaleCoef() const { return text_scale_coef_; }
  void SetTextThicknessCoef(float coef)  { text_thickness_coef_ = coef; }
  inline float GetTextThicknessCoef() const { return text_thickness_coef_; }
  inline size_t chn_num() const { return rows() * cols(); }
  inline const std::vector<std::string> labels() const { return labels_; }
  void DrawLabel(Mat image, const vector<DetectObject>& objects, cnstream::CnFont* cn_font = nullptr,
                 bool tiled = false) const;
};

#endif  // _CNOSD_H_
