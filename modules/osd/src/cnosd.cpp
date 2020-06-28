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

#include "cnosd.h"
#include <algorithm>
#include <string>
#include <vector>

using std::to_string;

// Keep 2 digits after decimal
static string FloatToString(float number) {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%.2f", number);
  return string(buffer);
}

// http://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically

static Scalar HSV2RGB(const float h, const float s, const float v) {
  const int h_i = static_cast<int>(h * 6);
  const float f = h * 6 - h_i;
  const float p = v * (1 - s);
  const float q = v * (1 - f * s);
  const float t = v * (1 - (1 - f) * s);
  float r, g, b;
  switch (h_i) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    case 5:
      r = v;
      g = p;
      b = q;
      break;
    default:
      r = 1;
      g = 1;
      b = 1;
      break;
  }
  return Scalar(r * 255, g * 255, b * 255);
}

static vector<Scalar> GenerateColors(const int n) {
  vector<Scalar> colors;
  cv::RNG rng(12345);
  const float golden_ratio_conjugate = 0.618033988749895f;
  const float s = 0.3f;
  const float v = 0.99f;
  for (int i = 0; i < n; ++i) {
    const float h = std::fmod(rng.uniform(0.0f, 1.0f) + golden_ratio_conjugate, 1.0f);
    colors.push_back(HSV2RGB(h, s, v));
  }
  return colors;
}

CnOsd::CnOsd(size_t rows, size_t cols, const vector<string>& labels) : rows_(rows), cols_(cols), labels_(labels) {
  colors_ = ::GenerateColors(labels_.size());
}

void CnOsd::DrawLabel(Mat image, const vector<DetectObject>& objects, cnstream::CnFont* cn_font, bool tiled) const {
  // check input data
  if (image.rows * image.cols == 0) {
    return;
  }

  for (auto& object : objects) {
    float xmin = object.x * image.cols;
    float ymin = object.y * image.rows;
    float xmax = (object.x + object.width) * image.cols;
    float ymax = (object.y + object.height) * image.rows;

    string text;
    Scalar color;
    if (labels().size() <= static_cast<size_t>(object.label)) {
      text = "Label not found, id = " + to_string(object.label);
      color = Scalar(0, 0, 0);
    } else {
      text = labels()[object.label];
      color = colors_[object.label];
    }
    // Detection window
    Point tl(xmin, ymin);
    Point br(xmax, ymax);
    float scale_coef = GetTextScaleCoef();
    double scale = scale_coef * image.cols;
    float thickness_coef = GetTextThicknessCoef();
    int thickness = static_cast<int>(thickness_coef * image.cols);
    if (thickness < 1) {
        thickness = static_cast<int>(0.008 * image.cols);
        if (thickness < 1) {
          thickness = 1;
        }
    }
    if (scale < 0.00001) {
       scale = 0.002 * image.cols;
    }
    cv::rectangle(image, tl, br, color, thickness);
    // Label and Score
    text += " " + FloatToString(object.score);

    // Track Id
    if (object.track_id >= 0) text += " track_id:" + to_string(object.track_id);
    auto text_size = cv::getTextSize(text, font_, scale, thickness, nullptr);

    int offset = (thickness == 1 ? 0 : -(thickness + 1) / 2);
    Point bl(xmin + offset, ymax + offset);
    Point label_left, label_right;
    label_left = bl;
    label_right = bl + Point(text_size.width + offset, text_size.height * 1.4);
    if (label_right.y > image.rows) {
      label_right.y -= text_size.height * 1.4;
      label_left.y -= text_size.height * 1.4;
    }
    if (label_right.x > image.cols) {
      label_right.x = image.cols;
      label_left.x = image.cols - text_size.width;
    }
    cv::rectangle(image, label_left, label_right, color, CV_FILLED);
    if (cn_font == nullptr) {
      cv::putText(image, text, label_left + Point(0, text_size.height), font_, scale, Scalar(255, 255, 255) - color,
                  thickness, 8, false);
    } else {
      char* str = const_cast<char*>(text.data());
      cn_font->putText(image, str, label_left + Point(0, text_size.height), Scalar(255, 255, 255) - color);
    }
  }
}
