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
#include <memory>
#include <string>
#include <vector>

using std::to_string;

// Keep 2 digits after decimal
static std::string FloatToString(float number) {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%.2f", number);
  return std::string(buffer);
}

// http://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically

static cv::Scalar HSV2RGB(const float h, const float s, const float v) {
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
  return cv::Scalar(r * 255, g * 255, b * 255);
}

static std::vector<cv::Scalar> GenerateColors(const int n) {
  std::vector<cv::Scalar> colors;
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

CnOsd::CnOsd(size_t rows, size_t cols, const std::vector<std::string>& labels) :
             rows_(rows), cols_(cols), labels_(labels) {
  colors_ = ::GenerateColors(labels_.size());
}

void CnOsd::DrawLogo(cv::Mat *image, std::string logo) const {
  cv::Point logo_pos(5, image->rows - 5);
  uint32_t scale = 1;
  uint32_t thickness = 2;
  cv::Scalar color(200, 200, 200);
  cv::putText(*image, logo, logo_pos, font_, scale, color, thickness);
}

#define CLIP(x) x < 0 ? 0 : (x > 1 ? 1 : x)
void CnOsd::DrawLabel(cv::Mat image, const cnstream::CNObjsVec& objects, cnstream::CnFont* cn_font,
                      bool tiled, std::vector<std::string> attr_keys) const {
  // check input data
  if (image.rows * image.cols == 0) {
    return;
  }

  for (uint32_t i = 0; i < objects.size(); ++i) {
    std::shared_ptr<cnstream::CNInferObject> object = objects[i];
    float x = CLIP(object->bbox.x);
    float y = CLIP(object->bbox.y);
    float w = CLIP(object->bbox.w);
    float h = CLIP(object->bbox.h);
    w = (x + w > 1) ? (1 - x) : w;
    h = (y + h > 1) ? (1 - y) : h;

    float xmin = x * image.cols;
    float ymin = y * image.rows;
    float xmax = (x + w) * image.cols;
    float ymax = (y + h) * image.rows;

    int label = object->id.empty() ? -1 : std::stoi(object->id);
    float score = object->score;
    int track_id = object->track_id.empty() ? -1 : std::stoi(object->track_id);
    std::string text;
    cv::Scalar color;
    if (labels().size() <= static_cast<size_t>(label)) {
      text = "Label not found, id = " + to_string(label);
      color = cv::Scalar(0, 0, 0);
    } else {
      text = labels()[label];
      color = colors_[label];
    }
    // Detection window
    cv::Point tl(xmin, ymin);
    cv::Point br(xmax, ymax);
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
    text += " " + FloatToString(score);

    // Track Id
    if (track_id >= 0) text += " track_id:" + to_string(track_id);
    auto text_size = cv::getTextSize(text, font_, scale, thickness, nullptr);

    int offset = (thickness == 1 ? 0 : -(thickness + 1) / 2);
    cv::Point bl(xmin + offset, ymax + offset);
    cv::Point label_left, label_right;
    label_left = bl;
    label_right = bl + cv::Point(text_size.width + offset, text_size.height * 1.4);
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
      cv::putText(image, text, label_left + cv::Point(0, text_size.height), font_, scale,
                  cv::Scalar(255, 255, 255) - color, thickness, 8, false);
    } else {
      char* str = const_cast<char*>(text.data());
      cn_font->putText(image, str, label_left + cv::Point(0, text_size.height), cv::Scalar(255, 255, 255) - color);
    }
    float second_scale = scale / 4;
    if (second_scale < 0.00001) {
      second_scale = 1;
    }
    int second_thickness = thickness / 4;
    if (second_thickness < 1) {
      second_thickness = 1;
    }
    auto second_text_size = cv::getTextSize(text, font_, second_scale, second_thickness, nullptr);
    // draw secondary infer infomation
    int line_interval = second_text_size.height;
    for (auto& key : attr_keys) {
      cnstream::CNInferAttr infer_attr = object->GetAttribute(key);
      if (infer_attr.value < 0) continue;
      std::string secondary_score = std::to_string(infer_attr.score);
      std::string secondary_lable = secondary_labels_[infer_attr.value];
      std::string secondary_text = secondary_lable + " : " + secondary_score;
      cv::putText(image, secondary_text, cv::Point(xmin, ymin + line_interval),
                  font_, second_scale, cv::Scalar(255, 255, 255) - color, second_thickness, 8, false);
      line_interval += second_text_size.height;
    }
  }
}
