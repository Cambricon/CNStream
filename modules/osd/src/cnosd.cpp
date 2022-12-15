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

#include "cnedk_osd.h"
#include "cnfont.hpp"
#include "libyuv.h"
#include "opencv2/imgproc/imgproc_c.h"

#define CLIP(x) x < 0 ? 0 : (x > 1 ? 1 : x)

// #define LOCAL_DEBUG_DUMP_IMAGE 1

#ifdef LOCAL_DEBUG_DUMP_IMAGE
#include "opencv2/imgproc/types_c.h"
#include "opencv2/opencv.hpp"
#endif

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

CnOsd::CnOsd(const std::vector<std::string> &labels) : labels_(labels) {
  colors_ = GenerateColorsForCategories(labels_.size());
}

void CnOsd::DrawLogo(CNDataFramePtr frame, std::string logo) /*const*/ {
  cv::Mat image = frame->ImageBGR();
  cv::Point logo_pos(5, image.rows - 5);
  uint32_t scale = 1;
  uint32_t thickness = 2;
  cv::Scalar color(200, 200, 200);
  cv::putText(image, logo, logo_pos, font_, scale, color, thickness);
}

void CnOsd::DrawLabel(CNDataFramePtr frame, const cnstream::CNObjsVec &objects,
                      std::vector<std::string> attr_keys) /*const*/ {
  // check input data
  if (frame->buf_surf->GetWidth() * frame->buf_surf->GetHeight() == 0) {
    LOGE(OSD) << "Osd: the image is empty.";
    return;
  }

  for (uint32_t i = 0; i < objects.size(); ++i) {
    std::shared_ptr<cnstream::CNInferObject> object = objects[i];
    if (!object) continue;
    std::pair<cv::Point, cv::Point> corner =
        GetBboxCorner(*object.get(), frame->buf_surf->GetWidth(), frame->buf_surf->GetHeight());
    cv::Point top_left = corner.first;
    cv::Point bottom_right = corner.second;
    cv::Point bottom_left(top_left.x, bottom_right.y);

    cv::Scalar color(0, 0, 0);

    int label_id = GetLabelId(object->id);

    if (LabelIsFound(label_id)) {
      color = colors_[label_id];
    }

    // Draw Detection window
    DrawBox(frame, top_left, bottom_right, color);

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
      VLOG5(OSD) << "Draw Label, Score and TrackID: " << text;
    } else {
      VLOG5(OSD) << "Draw Label and Score: " << text;
    }
    DrawText(frame, bottom_left, text, color);

    // draw secondary inference information
    int label_bottom_y = 0;
    int text_height = 0;
    for (auto &key : attr_keys) {
      CNInferAttr infer_attr = object->GetAttribute(key);
      if (infer_attr.value < 0 || infer_attr.value > static_cast<int>(secondary_labels_.size()) - 1) {
        std::string attr_value = object->GetExtraAttribute(key);
        if (attr_value.empty()) continue;
        std::string secondary_text = key + " : " + attr_value;
        DrawText(frame, top_left + cv::Point(0, label_bottom_y), secondary_text, color, 0.5, &text_height);
      } else {
        std::string secondary_label = secondary_labels_[infer_attr.value];
        std::string secondary_score = std::to_string(infer_attr.score);
        secondary_score = secondary_score.substr(0, std::min(size_t(4), secondary_score.size()));
        std::string secondary_text = key + " : " + secondary_label + " score[" + secondary_score + "]";
        DrawText(frame, top_left + cv::Point(0, label_bottom_y), secondary_text, color, 0.5, &text_height);
      }
      label_bottom_y += text_height;
    }
  }
}

void CnOsd::DrawLabel(CNDataFramePtr frame, const std::vector<DrawInfo> &info) /*const*/ {
  // check input data
  if (frame->buf_surf->GetWidth() * frame->buf_surf->GetHeight() == 0) {
    LOGE(OSD) << "Osd: the image is empty.";
    return;
  }

  for (uint32_t i = 0; i < info.size(); ++i) {
    auto &item = info[i];

    float x = CLIP(item.bbox.x);
    float y = CLIP(item.bbox.y);
    float w = CLIP(item.bbox.w);
    float h = CLIP(item.bbox.h);
    w = (x + w > 1) ? (1 - x) : w;
    h = (y + h > 1) ? (1 - y) : h;

    int img_width = frame->buf_surf->GetWidth();
    int img_height = frame->buf_surf->GetHeight();
    cv::Point top_left(x * img_width, y * img_height);
    cv::Point bottom_right((x + w) * img_width, (y + h) * img_height);
    cv::Point bottom_left(top_left.x, bottom_right.y);

    cv::Scalar color(0, 0, 0);

    int label_id = item.label_id;

    if (LabelIsFound(label_id)) {
      color = colors_[label_id];
    }

    // Draw Detection window
    DrawBox(frame, top_left, bottom_right, color);

    // Draw Basic Info
    DrawText(frame, bottom_left, item.basic_info, color);

    // draw secondary inference infomation
    int label_bottom_y = 0;
    int text_height = 0;

    for (auto &attribute : item.attributes) {
      DrawText(frame, top_left + cv::Point(0, label_bottom_y), attribute, color, 0.7, &text_height, item.attr_down);
      label_bottom_y += text_height;
    }
  }
}

std::pair<cv::Point, cv::Point> CnOsd::GetBboxCorner(const cnstream::CNInferObject &object, int img_width,
                                                     int img_height) const {
  auto bbox = GetFullFovBbox(&object);
  float x = CLIP(bbox.x);
  float y = CLIP(bbox.y);
  float w = CLIP(bbox.w);
  float h = CLIP(bbox.h);
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

void CnOsd::DrawBox(CNDataFramePtr frame, const cv::Point &top_left, const cv::Point &bottom_right,
                    const cv::Scalar &color) /* const*/ {
  if (hw_accel_) {
    BBoxInfo bbox_info;
    bbox_info.top_left = top_left;
    bbox_info.bottom_right = bottom_right;
    bbox_info.color = color;
    bbox_info.thickness = CalcThickness(frame->buf_surf->GetWidth(), box_thickness_);
    DoDrawRect(frame, &bbox_info);
    return;
  }
  cv::Mat image = frame->ImageBGR();
  cv::rectangle(image, top_left, bottom_right, color, CalcThickness(frame->buf_surf->GetWidth(), box_thickness_));
}

void CnOsd::DrawText(CNDataFramePtr frame, const cv::Point &bottom_left, const std::string &text,
                     const cv::Scalar &color, float scale, int *text_height, bool down) /*const*/ {
  if (text.empty()) {
    return;
  }
  int txt_thickness = CalcThickness(frame->buf_surf->GetWidth(), text_thickness_) * scale;
  int box_thickness = CalcThickness(frame->buf_surf->GetWidth(), box_thickness_) * scale;

  int baseline = 0;
  int space_before = 0;
  cv::Size text_size;
  int label_height;
  if (cn_font_ == nullptr) {
    double txt_scale = CalcScale(frame->buf_surf->GetWidth(), text_scale_) * scale;
    text_size = cv::getTextSize(text, font_, txt_scale, txt_thickness, &baseline);
    space_before = 3 * txt_scale;
    text_size.width += space_before * 2;
  } else {
    uint32_t text_h = 0, text_w = 0;
    char *str = const_cast<char *>(text.data());
    cn_font_->GetTextSize(str, &text_w, &text_h);
    baseline = cn_font_->GetFontPixel() / 4;
    space_before = baseline / 2;
    text_size.height = text_h;
    text_size.width = text_w + space_before * 2;
    if (hw_accel_) {
      text_size.width = (text_size.width + 31) / 32 * 32;
      text_size.height = (text_size.height + 15) / 16 * 16;
    }
  }
  label_height = baseline + txt_thickness + text_size.height;
  if (hw_accel_) label_height = (label_height + 15) / 16 * 16;

  int offset = (box_thickness == 1 ? 0 : -(box_thickness + 1) / 2);
  cv::Point label_top_left = bottom_left + cv::Point(offset, down ? offset : -offset - label_height);
  cv::Point label_bottom_right = label_top_left + cv::Point(text_size.width + offset, label_height);
  // move up if the label is beyond the bottom of the image
  if (label_bottom_right.y >= static_cast<int>(frame->buf_surf->GetHeight())) {
    label_bottom_right.y -= label_height;
    label_top_left.y -= label_height;
  }
  // move left if the label is beyond the right side of the image
  if (label_bottom_right.x >= static_cast<int>(frame->buf_surf->GetWidth())) {
    label_bottom_right.x = frame->buf_surf->GetWidth() - 1;
    label_top_left.x = frame->buf_surf->GetWidth() - text_size.width;
  }

  // correct coords
  if (label_top_left.x < 0) label_top_left.x = 0;
  if (label_top_left.y < 0) label_top_left.y = 0;

  // draw text background
  if (hw_accel_) {
    TextBackground bg;
    bg.top_left = label_top_left;
    bg.bottom_right = label_bottom_right;
    bg.color = color;
    DoFillRect(frame, &bg);
  } else {
    cv::Mat image = frame->ImageBGR();
    cv::rectangle(image, label_top_left, label_bottom_right, color, CV_FILLED);
  }
  // draw text
  cv::Point text_left_bottom =
      label_top_left + cv::Point(space_before, label_height - baseline / 2 - txt_thickness / 2);
  cv::Scalar text_color = cv::Scalar(255, 255, 255) - color;
  if (cn_font_ == nullptr) {
    double txt_scale = CalcScale(frame->buf_surf->GetWidth(), text_scale_) * scale;
    cv::Mat image = frame->ImageBGR();
    cv::putText(image, text, text_left_bottom, font_, txt_scale, text_color, txt_thickness);
  } else {
    char *str = const_cast<char *>(text.data());
    if (hw_accel_) {
      int text_bitmap_size = text_size.width * 2 * text_size.height;
      cnedk::BufSurfWrapperPtr text_bitmap = GetMem(text_bitmap_size);
      if (!text_bitmap) return;

      cn_font_->putText(str, text_color, color, text_bitmap->GetMappedData(0), text_size);
      // correct text_left, due to alignment adjust
      if (text_left_bottom.x >= static_cast<int>(frame->buf_surf->GetWidth()) - text_size.width) {
        text_left_bottom.x = frame->buf_surf->GetWidth() - text_size.width - 1;
      }
      text_left_bottom.x -= (text_left_bottom.x & 1);  // make sure x is at least 2-aligned

      if (text_left_bottom.x >= 0) {
        TextInfo textInfo(text_size, text_bitmap, text_left_bottom, color);
        DoDrawBitmap(frame, &textInfo);
      } else {
        LOGW(OSD) << "Text is too long, discard it";
        // abort();
      }
    } else {
      cn_font_->putText(frame, str, text_left_bottom, text_color);
    }
  }
  if (text_height) *text_height = down ? text_size.height + baseline : -text_size.height - baseline;
}

void CnOsd::DoDrawRect(CNDataFramePtr frame, BBoxInfo *info, bool last) {
  if (info) {
    if (rect_num_ == kMaxRectNum) {
      CnedkDrawRect(frame->buf_surf->GetBufSurface(), rectParams_, rect_num_);
      rect_num_ = 0;
    }
    if (rect_num_ < kMaxRectNum) {
      CnedkOsdRectParams &param = rectParams_[rect_num_++];
      memset(&param, 0, sizeof(CnedkOsdRectParams));
      param.x = info->top_left.x;
      param.y = info->top_left.y;
      param.w = info->bottom_right.x - info->top_left.x + 1;
      param.h = info->bottom_right.y - info->top_left.y + 1;
      uint32_t color = 0xff000000;
      color |= (uint32_t)info->color.val[0] << 16;  // R
      color |= (uint32_t)info->color.val[1] << 8;   // G
      color |= (uint32_t)info->color.val[2] << 0;   // B
      param.color = color;
      param.line_width = info->thickness;
    }
  }

  if (last && rect_num_ > 0) {
    CnedkDrawRect(frame->buf_surf->GetBufSurface(), rectParams_, rect_num_);
    rect_num_ = 0;
  }
}

void CnOsd::DoFillRect(CNDataFramePtr frame, TextBackground *info, bool last) {
  if (info) {
    if (rectBg_num_ == kMaxRectNum) {
      CnedkFillRect(frame->buf_surf->GetBufSurface(), rectBgParams_, rectBg_num_);
      rectBg_num_ = 0;
    }
    if (rectBg_num_ < kMaxRectNum) {
      CnedkOsdRectParams &param = rectBgParams_[rectBg_num_++];
      memset(&param, 0, sizeof(CnedkOsdRectParams));
      param.x = info->top_left.x;
      param.y = info->top_left.y;
      param.w = info->bottom_right.x - info->top_left.x + 1;
      param.h = info->bottom_right.y - info->top_left.y + 1;
      uint32_t color = 0xff000000;
      color |= (uint32_t)info->color.val[0] << 16;  // R
      color |= (uint32_t)info->color.val[1] << 8;   // G
      color |= (uint32_t)info->color.val[2] << 0;   // B
      param.color = color;
    }
  }

  if (last && rectBg_num_ > 0) {
    CnedkFillRect(frame->buf_surf->GetBufSurface(), rectBgParams_, rectBg_num_);
    rectBg_num_ = 0;
  }
}

void CnOsd::DoDrawBitmap(CNDataFramePtr frame, TextInfo *info, bool last) {
  if (info) {
    if (texts.size() == kMaxTextNum) {
      for (uint32_t j = 0; j < kMaxTextNum; j++) {
        CnedkOsdBitmapParams &param = bitmapParams_[j];
        TextInfo &textInfo = texts[j];
        memset(&param, 0, sizeof(CnedkOsdBitmapParams));
        param.x = textInfo.left_bottom.x;
        param.y = textInfo.left_bottom.y - textInfo.size.height + 1;
        param.w = textInfo.size.width;
        param.h = textInfo.size.height;
        uint32_t color = 0xff000000;
        color |= (uint32_t)textInfo.bg_color.val[0] << 16;  // R
        color |= (uint32_t)textInfo.bg_color.val[1] << 8;   // G
        color |= (uint32_t)textInfo.bg_color.val[2] << 0;   // B
        param.bg_color = color;
        param.bitmap_argb1555 = textInfo.bitmap->GetData(0);
      }
      CnedkDrawBitmap(frame->buf_surf->GetBufSurface(), bitmapParams_, kMaxTextNum);
      texts.clear();
    }
    texts.push_back(*info);
  }

  if (last && texts.size()) {
    for (uint32_t j = 0; j < texts.size(); j++) {
      CnedkOsdBitmapParams &param = bitmapParams_[j];
      TextInfo &textInfo = texts[j];
      memset(&param, 0, sizeof(CnedkOsdBitmapParams));
      param.x = textInfo.left_bottom.x;
      param.y = textInfo.left_bottom.y - textInfo.size.height + 1;
      param.w = textInfo.size.width;
      param.h = textInfo.size.height;
      uint32_t color = 0xff000000;
      color |= (uint32_t)textInfo.bg_color.val[0] << 16;  // R
      color |= (uint32_t)textInfo.bg_color.val[1] << 8;   // G
      color |= (uint32_t)textInfo.bg_color.val[2] << 0;   // B
      param.bg_color = color;
      param.bitmap_argb1555 = textInfo.bitmap->GetData(0);
    }
    CnedkDrawBitmap(frame->buf_surf->GetBufSurface(), bitmapParams_, texts.size());
    texts.clear();
  }
}

void CnOsd::update_vframe(CNDataFramePtr frame) {
  if (!hw_accel_) {
    /*update frame->vframe for vout, venc etc...  FIXME
     *  BGR->yuv420sp
     */
    cv::Mat img = frame->ImageBGR();
    int h = img.rows;
    int w = img.cols;

    // BGR covert to yuv420sp
    unsigned char *dst_y = static_cast<unsigned char *>(frame->buf_surf->GetHostData(0));
    unsigned char *dst_uv = static_cast<unsigned char *>(frame->buf_surf->GetHostData(1));
    int y_stride = frame->buf_surf->GetStride(0);
    int uv_stride = frame->buf_surf->GetStride(1);
    if (frame->buf_surf->GetColorFormat() == CNEDK_BUF_COLOR_FORMAT_NV21) {
      libyuv::RGB24ToNV21(img.data, w * 3, dst_y, y_stride, dst_uv, uv_stride, w, h);
    } else if (frame->buf_surf->GetColorFormat() == CNEDK_BUF_COLOR_FORMAT_NV12) {
      libyuv::RGB24ToNV12(img.data, w * 3, dst_y, y_stride, dst_uv, uv_stride, w, h);
    } else {
      LOGE(OSD) << "fmt not supported yet.";
    }

    // sync on platform MLUxxx/CExxxx, in case device will access the context
    frame->buf_surf->SyncHostToDevice();
    return;
  }

  // hw-accel osd
  //
  // draw bounding-boxes
  DoDrawRect(frame, nullptr, true);
  // draw text-background
  DoFillRect(frame, nullptr, true);
  // draw text
  DoDrawBitmap(frame, nullptr, true);

#ifdef LOCAL_DEBUG_DUMP_IMAGE
  static std::atomic<unsigned int> count{0};
  if (count >= 100 && count <= 120) {
    std::string fname = "test_osd";
    fname += std::to_string(frame->frame_id);
    fname += ".jpg";
    cv::imwrite(fname, frame->ImageBGR());
  }
  ++count;
#endif
}

}  // namespace cnstream
