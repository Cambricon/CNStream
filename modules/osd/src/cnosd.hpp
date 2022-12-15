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

#include <opencv2/imgproc/imgproc.hpp>

#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnedk_osd.h"
#include "cnstream_frame_va.hpp"
#include "osd.hpp"
#include "osd_handler.hpp"

using DrawInfo = cnstream::OsdHandler::DrawInfo;

constexpr uint32_t kOsdBlockSize = 64 * 1024;
constexpr uint32_t kOsdBlockNum = 32;
constexpr uint32_t kMaxRectNum = 20;
constexpr uint32_t kMaxTextNum = kOsdBlockNum;

namespace cnstream {

class CnFont;

class CnOsd {
 public:
  CnOsd() = delete;
  explicit CnOsd(const std::vector<std::string> &labels);
  ~CnOsd() {
    std::unique_lock<std::mutex> lk(pool_mutex_);
    if (hw_accel_) mempool_.DestroyPool(5000);
  }

  inline void SetTextScale(float scale) { text_scale_ = scale; }
  inline void SetTextThickness(float thickness) { text_thickness_ = thickness; }
  inline void SetBoxThickness(float thickness) { box_thickness_ = thickness; }
  inline void SetSecondaryLabels(std::vector<std::string> labels) { secondary_labels_ = labels; }
  inline void SetCnFont(std::shared_ptr<CnFont> cn_font) { cn_font_ = cn_font; }
  inline void SetHwAccel(bool hw_accel) {
    hw_accel_ = hw_accel;
    if (hw_accel_) InitMempool();
  }

  void DrawLabel(CNDataFramePtr frame, const CNObjsVec &objects, std::vector<std::string> attr_keys = {}) /*const*/;
  void DrawLabel(CNDataFramePtr frame, const std::vector<DrawInfo> &info) /*const*/;
  void DrawLogo(CNDataFramePtr frame, std::string logo) /*const*/;
  void update_vframe(CNDataFramePtr frame);

 private:
  std::pair<cv::Point, cv::Point> GetBboxCorner(const cnstream::CNInferObject &object, int img_width,
                                                int img_height) const;
  bool LabelIsFound(const int &label_id) const;
  int GetLabelId(const std::string &label_id_str) const;
  void DrawBox(CNDataFramePtr frame, const cv::Point &top_left, const cv::Point &bottom_right,
               const cv::Scalar &color);  // const;
  void DrawText(CNDataFramePtr frame, const cv::Point &bottom_left, const std::string &text, const cv::Scalar &color,
                float scale = 1, int *text_height = nullptr, bool down = true);  // const;
  int CalcThickness(int image_width, float thickness) const {
    int result = thickness * image_width / 300;
    if (result <= 0) result = 1;
    return result;
  }
  double CalcScale(int image_width, float scale) const { return scale * image_width / 1000; }

  float text_scale_ = 1;
  float text_thickness_ = 1;
  float box_thickness_ = 1;
  std::vector<std::string> labels_;
  std::vector<std::string> secondary_labels_;
  std::vector<cv::Scalar> colors_;
  int font_ = cv::FONT_HERSHEY_SIMPLEX;
  std::shared_ptr<CnFont> cn_font_;
  bool hw_accel_ = false;

 private:
  cnedk::BufPool mempool_;
  std::mutex pool_mutex_;

  int InitMempool() {
    std::unique_lock<std::mutex> lk(pool_mutex_);
    CnedkBufSurfaceCreateParams create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.device_id = 0;  // TODO(gaoyujia)
    create_params.batch_size = 1;
    create_params.size = kOsdBlockSize;
    create_params.mem_type = CNEDK_BUF_MEM_UNIFIED;
    mempool_.CreatePool(&create_params, kOsdBlockNum);
    return 0;
  }
  cnedk::BufSurfWrapperPtr GetMem(size_t nSize) {
    std::unique_lock<std::mutex> lk(pool_mutex_);
    cnedk::BufSurfWrapperPtr surfPtr = nullptr;
    if (nSize <= kOsdBlockSize) {
      surfPtr = mempool_.GetBufSurfaceWrapper(0);
    }

    if (!surfPtr) {
      CnedkBufSurfaceCreateParams create_params;
      memset(&create_params, 0, sizeof(create_params));
      create_params.device_id = 0;
      create_params.batch_size = 1;
      create_params.size = nSize;
      create_params.mem_type = CNEDK_BUF_MEM_UNIFIED;
      CnedkBufSurface *surf = nullptr;
      if (CnedkBufSurfaceCreate(&surf, &create_params) < 0) {
        LOGE(OSD) << "GetMem(): Create BufSurface failed";
        return nullptr;
      }
      surfPtr = std::make_shared<cnedk::BufSurfaceWrapper>(surf);
    }

    memset(surfPtr->GetMappedData(0), 0, nSize);
    return surfPtr;
  }

 private:
  struct BBoxInfo {
    cv::Point top_left;
    cv::Point bottom_right;
    cv::Scalar color;
    int thickness = 1;
    int lineType = 8;
    int shift = 0;
  };
  CnedkOsdRectParams rectParams_[kMaxRectNum];
  uint32_t rect_num_ = 0;
  void DoDrawRect(CNDataFramePtr frame, BBoxInfo *info, bool last = false);

  struct TextBackground {
    cv::Point top_left;
    cv::Point bottom_right;
    cv::Scalar color;
  };
  CnedkOsdRectParams rectBgParams_[kMaxRectNum];
  uint32_t rectBg_num_ = 0;
  void DoFillRect(CNDataFramePtr frame, TextBackground *info, bool last = false);

  struct TextInfo {
    cv::Size size;
    cnedk::BufSurfWrapperPtr bitmap;
    cv::Point left_bottom;
    cv::Scalar bg_color;
    TextInfo(cv::Size size, cnedk::BufSurfWrapperPtr bitmap, cv::Point left_bottom, cv::Scalar bg_color)
        : size(size), bitmap(bitmap), left_bottom(left_bottom), bg_color(bg_color) {}
  };
  std::vector<TextInfo> texts;
  CnedkOsdBitmapParams bitmapParams_[kMaxTextNum];
  void DoDrawBitmap(CNDataFramePtr frame, TextInfo *info, bool last = false);
};  // class CnOsd

}  // namespace cnstream

#endif  // _CNOSD_HPP_
