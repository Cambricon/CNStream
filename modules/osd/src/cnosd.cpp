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

#include "cnosd.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnfont.hpp"

#define CLIP(x) x < 0 ? 0 : (x > 1 ? 1 : x)

namespace cnstream {

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
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = 1; g = 1; b = 1; break;
  }
  return cv::Scalar(r * 255, g * 255, b * 255);
}

static std::vector<cv::Scalar> GenerateColorsForCategories(const int n) {
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

CnOsd::CnOsd(const std::vector<std::string>& labels) : labels_(labels) {
  colors_ = GenerateColorsForCategories(labels_.size());
}

void CnOsd::DrawLogo(cv::Mat* image, std::string logo) const {
  cv::Point logo_pos(5, image->rows - 5);
  uint32_t scale = 1;
  uint32_t thickness = 2;
  cv::Scalar color(200, 200, 200);
  cv::putText(*image, logo, logo_pos, font_, scale, color, thickness);
}

void CnOsd::DrawLabel(cv::Mat* image, const cnstream::CNObjsVec& objects,
                      std::vector<std::string> attr_keys) const {
  // check input data
  if (image->cols * image->rows == 0) {
    LOG(ERROR) << "Osd: the image is empty.";
    return;
  }

  for (uint32_t i = 0; i < objects.size(); ++i) {
    std::shared_ptr<cnstream::CNInferObject> object = objects[i];
    if (!object) continue;
    std::pair<cv::Point, cv::Point> corner = GetBboxCorner(*object.get(), image->cols, image->rows);
    cv::Point top_left = corner.first;
    cv::Point bottom_right = corner.second;
    cv::Point bottom_left(top_left.x, bottom_right.y);

    cv::Scalar color(0, 0, 0);

    int label_id = GetLabelId(object->id);

    if (LabelIsFound(label_id)) {
      color = colors_[label_id];
    }

    // Draw Detection window
    DrawBox(image, top_left, bottom_right, color);

    // Draw Text label + score + track id
    std::string text;
    if (LabelIsFound(label_id)) {
      text = labels_[label_id];
    } else {
      text = "Label not found, id = " + std::to_string(label_id);
    }
    text += " " + FloatToString(object->score);
    if (!object->track_id.empty() && object->track_id != "-1") {
      text += " track_id: " + object->track_id;
    }
    DrawText(image, bottom_left, text, color);

    // draw secondary inference infomation
    int label_bottom_y = 0;
    int text_height = 0;
    for (auto& key : attr_keys) {
      cnstream::CNInferAttr infer_attr = object->GetAttribute(key);
      if (infer_attr.value < 0) continue;
      std::string secondary_lable = secondary_labels_[infer_attr.value];
      std::string secondary_score = std::to_string(infer_attr.score);
      std::string secondary_text = secondary_lable + " : " + secondary_score;
      DrawText(image, top_left + cv::Point(0, label_bottom_y), secondary_text, color, 0.5, &text_height);
      label_bottom_y += text_height;
    }
  }
}

std::pair<cv::Point, cv::Point> CnOsd::GetBboxCorner(const cnstream::CNInferObject &object,
                                                     int img_width, int img_height) const {
  float x = CLIP(object.bbox.x);
  float y = CLIP(object.bbox.y);
  float w = CLIP(object.bbox.w);
  float h = CLIP(object.bbox.h);
  w = (x + w > 1) ? (1 - x) : w;
  h = (y + h > 1) ? (1 - y) : h;
  cv::Point top_left(x * img_width, y * img_height);
  cv::Point bottom_right((x + w) * img_width, (y + h) * img_height);
  return std::make_pair(top_left, bottom_right);
}

bool CnOsd::LabelIsFound(const int &label_id) const {
  if (labels_.size() <= static_cast<size_t>(label_id)) {
    return false;
  }
  return true;
}

int CnOsd::GetLabelId(const std::string &label_id_str) const {
  return label_id_str.empty() ? -1 : std::stoi(label_id_str);
}

void CnOsd::DrawBox(cv::Mat* image, const cv::Point &top_left, const cv::Point &bottom_right,
                    const cv::Scalar &color) const {
  cv::rectangle(*image, top_left, bottom_right, color, CalcThickness(image->cols, box_thickness_));
}

void CnOsd::DrawText(cv::Mat* image, const cv::Point &bottom_left, const std::string &text,
                     const cv::Scalar &color, float scale, int* text_height) const {
  double txt_scale = CalcScale(image->cols, text_scale_) * scale;
  int txt_thickness = CalcThickness(image->cols, text_thickness_) * scale;
  int box_thickness = CalcThickness(image->cols, box_thickness_) * scale;

  int baseline = 0;
  cv::Size text_size;
  int label_height;
  if (cn_font_ == nullptr) {
    text_size = cv::getTextSize(text, font_, txt_scale, txt_thickness, &baseline);
    label_height = baseline + txt_thickness + text_size.height;
  } else {
    // TODO(gaoyujia): Get the height and width of chinese character
    text_size = cv::getTextSize(text, font_, 1, 1, &baseline);
    label_height = baseline + text_size.height;
  }
  int offset = (box_thickness == 1 ? 0 : -(box_thickness + 1) / 2);
  cv::Point label_top_left = bottom_left + cv::Point(offset, offset);
  cv::Point label_bottom_right = label_top_left + cv::Point(text_size.width + offset, label_height);
  // move up if the label is beyond the bottom of the image
  if (label_bottom_right.y > image->rows) {
    label_bottom_right.y -= label_height;
    label_top_left.y -= label_height;
  }
  // move left if the label is beyond the right side of the image
  if (label_bottom_right.x > image->cols) {
    label_bottom_right.x = image->cols;
    label_top_left.x = image->cols - text_size.width;
  }
  // draw text background
  cv::rectangle(*image, label_top_left, label_bottom_right, color, CV_FILLED);
  // draw text
  cv::Point text_left_bottom = label_top_left + cv::Point(0, text_size.height + baseline / 2);
  cv::Scalar text_color = cv::Scalar(255, 255, 255) - color;
  if (cn_font_ == nullptr) {
    cv::putText(*image, text, text_left_bottom, font_, txt_scale, text_color, txt_thickness);
  } else {
    char* str = const_cast<char*>(text.data());
    cn_font_->putText(*image, str, text_left_bottom, text_color);
  }
  if (text_height) *text_height = text_size.height + baseline;
}

int CnOsd::CalcThickness(int image_width, float thickness) const {
  int result = thickness * image_width / 300;
  if (result <= 0) result = 1;
  return result;
}

double CnOsd::CalcScale(int image_width, float scale) const {
  return scale * image_width / 1000;
}

}  // namespace cnstream
