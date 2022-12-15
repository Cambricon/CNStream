/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
#include "preprocess_common.hpp"

#include <memory>
#include <string>
#include <vector>

#include "opencv2/imgproc/types_c.h"
#include "opencv2/opencv.hpp"

#include "cnedk_transform.h"

#include "cnstream_logging.hpp"
#include "cnstream_preproc.hpp"

CnedkTransformDataType GetTransformDataType(infer_server::DataType dtype) {
  switch (dtype) {
    case infer_server::DataType::UINT8:
      return CNEDK_TRANSFORM_UINT8;
    case infer_server::DataType::FLOAT32:
      return CNEDK_TRANSFORM_FLOAT32;
    case infer_server::DataType::FLOAT16:
      return CNEDK_TRANSFORM_FLOAT16;
    case infer_server::DataType::INT32:
      return CNEDK_TRANSFORM_INT32;
    case infer_server::DataType::INT16:
      return CNEDK_TRANSFORM_INT16;
    default:
      LOGW(PREPROC) << "Unknown data type, use UINT8 as default";
      return CNEDK_TRANSFORM_UINT8;
  }
}

int GetDataTypeSize(infer_server::DataType dtype) {
  switch (dtype) {
    case infer_server::DataType::UINT8:
      return 1;
    case infer_server::DataType::FLOAT32:
      return 4;
    default:
      LOGW(PREPROC) << "Only support UINT8 and FLOAT32. Unknown data type, use UINT8 as default";
      return 1;
  }
}

CnedkTransformColorFormat GetTransformColorFormat(infer_server::NetworkInputFormat pix_fmt) {
  switch (pix_fmt) {
    case infer_server::NetworkInputFormat::RGB:
      return CNEDK_TRANSFORM_COLOR_FORMAT_RGB;
    case infer_server::NetworkInputFormat::BGR:
      return CNEDK_TRANSFORM_COLOR_FORMAT_BGR;
    default:
      LOGW(PREPROC) << "Unknown input pixel format, use RGB as default";
      return CNEDK_TRANSFORM_COLOR_FORMAT_RGB;
  }
}

CnedkBufSurfaceColorFormat GetBufSurfaceColorFormat(infer_server::NetworkInputFormat pix_fmt) {
  switch (pix_fmt) {
    case infer_server::NetworkInputFormat::RGB:
      return CNEDK_BUF_COLOR_FORMAT_RGB;
    case infer_server::NetworkInputFormat::BGR:
      return CNEDK_BUF_COLOR_FORMAT_BGR;
    default:
      LOGW(PREPROC) << "Unknown input pixel format, use RGB as default";
      return CNEDK_BUF_COLOR_FORMAT_RGB;
  }
}

// ---------------------------------------------------------------------------------------

int PreprocessTransform(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                        const std::vector<CnedkTransformRect> &src_rects,
                        const cnstream::CnPreprocNetworkInfo &info, infer_server::NetworkInputFormat pix_fmt,
                        bool keep_aspect_ratio, int pad_value, bool mean_std, std::vector<float> mean,
                        std::vector<float> std) {
  if (src_rects.size() && src_rects.size() != src->GetNumFilled()) {
    return -1;
  }
  CnedkBufSurface* src_buf = src->GetBufSurface();
  CnedkBufSurface* dst_buf = dst->GetBufSurface();

  uint32_t batch_size = src->GetNumFilled();
  std::vector<CnedkTransformRect> src_rect(batch_size);
  std::vector<CnedkTransformRect> dst_rect(batch_size);
  CnedkTransformParams params;
  memset(&params, 0, sizeof(params));
  params.transform_flag = 0;
  if (src_rects.size()) {
    params.transform_flag |= CNEDK_TRANSFORM_CROP_SRC;
    params.src_rect = src_rect.data();
    memset(src_rect.data(), 0, sizeof(CnedkTransformRect) * batch_size);
    for (uint32_t i = 0; i < batch_size; i++) {
      CnedkTransformRect *src_bbox = &src_rect[i];
      *src_bbox = src_rects[i];
      // validate bbox
      src_bbox->left -= src_bbox->left & 1;
      src_bbox->top -= src_bbox->top & 1;
      src_bbox->width -= src_bbox->width & 1;
      src_bbox->height -= src_bbox->height & 1;
      while (src_bbox->left + src_bbox->width > src_buf->surface_list[i].width) src_bbox->width -= 2;
      while (src_bbox->top + src_bbox->height > src_buf->surface_list[i].height) src_bbox->height -= 2;
    }
  }

  if (keep_aspect_ratio) {
    params.transform_flag |= CNEDK_TRANSFORM_CROP_DST;
    params.dst_rect = dst_rect.data();
    memset(dst_rect.data(), 0, sizeof(CnedkTransformRect) * batch_size);
    for (uint32_t i = 0; i < batch_size; i++) {
      CnedkTransformRect *dst_bbox = &dst_rect[i];
      *dst_bbox = cnstream::KeepAspectRatio(src_buf->surface_list[i].width, src_buf->surface_list[i].height,
                                            info.w, info.h);
      // validate bbox
      dst_bbox->left -= dst_bbox->left & 1;
      dst_bbox->top -= dst_bbox->top & 1;
      dst_bbox->width -= dst_bbox->width & 1;
      dst_bbox->height -= dst_bbox->height & 1;

      while (dst_bbox->left + dst_bbox->width > info.w) dst_bbox->width -= 2;
      while (dst_bbox->top + dst_bbox->height > info.h) dst_bbox->height -= 2;
    }
  }

  CnedkTransformMeanStdParams mean_std_params;
  if (mean_std) {
    if (mean.size() < info.c || std.size() < info.c) {
      LOGE(PREPROC) << "[PreprocessTransform] Invalid mean std value size";
      return -1;
    }
    params.transform_flag |= CNEDK_TRANSFORM_MEAN_STD;
    for (uint32_t c_i = 0; c_i < info.c; c_i++) {
      mean_std_params.mean[c_i] = mean[c_i];
      mean_std_params.std[c_i] = std[c_i];
    }
    params.mean_std_params = &mean_std_params;
  }

  // configure dst_desc
  CnedkTransformTensorDesc dst_desc;
  dst_desc.color_format = GetTransformColorFormat(pix_fmt);
  dst_desc.data_type = GetTransformDataType(info.dtype);
  dst_desc.shape.n = info.n;
  dst_desc.shape.c = info.c;
  dst_desc.shape.h = info.h;
  dst_desc.shape.w = info.w;
  params.dst_desc = &dst_desc;

  CnedkBufSurfaceMemSet(dst_buf, -1, -1, pad_value);
  if (CnedkTransform(src_buf, dst_buf, &params) < 0) {
    LOGE(PREPROC) << "[PreprocessTransform] CnedkTransform failed";
    return -1;
  }

  return 0;
}

int PreprocessCpu(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                  const std::vector<CnedkTransformRect> &src_rects,
                  const cnstream::CnPreprocNetworkInfo &info, infer_server::NetworkInputFormat pix_fmt,
                  bool keep_aspect_ratio, int pad_value, bool mean_std, std::vector<float> mean,
                  std::vector<float> std) {
  if (src_rects.size() && src_rects.size() != src->GetNumFilled()) {
    return -1;
  }

  CnedkBufSurface* src_buf = src->GetBufSurface();
  if ((src->GetColorFormat() != CNEDK_BUF_COLOR_FORMAT_NV12 &&
       src->GetColorFormat() != CNEDK_BUF_COLOR_FORMAT_NV21) ||
      (pix_fmt != infer_server::NetworkInputFormat::RGB && pix_fmt != infer_server::NetworkInputFormat::BGR)) {
    LOGE(PREPROC) << "[PreprocessCpu] Unsupported pixel format convertion";
    return -1;
  }

  if (info.dtype == infer_server::DataType::UINT8 && mean_std) {
    LOGW(PREPROC) << "[PreprocessCpu] not support uint8 with mean std.";
  }

  uint32_t batch_size = src->GetNumFilled();

  CnedkBufSurfaceSyncForCpu(src_buf, -1, -1);
  size_t img_size = info.w * info.h * info.c;
  std::unique_ptr<uint8_t[]> img_tmp = nullptr;

  for (uint32_t batch_idx = 0; batch_idx < batch_size; ++batch_idx) {
    uint8_t *y_plane = static_cast<uint8_t *>(src->GetHostData(0, batch_idx));
    uint8_t *uv_plane = static_cast<uint8_t *>(src->GetHostData(1, batch_idx));
    CnedkTransformRect src_bbox;
    if (src_rects.size()) {
      src_bbox = src_rects[batch_idx];
      // validate bbox
      src_bbox.left -= src_bbox.left & 1;
      src_bbox.top -= src_bbox.top & 1;
      src_bbox.width -= src_bbox.width & 1;
      src_bbox.height -= src_bbox.height & 1;
      while (src_bbox.left + src_bbox.width > src_buf->surface_list[batch_idx].width) src_bbox.width -= 2;
      while (src_bbox.top + src_bbox.height > src_buf->surface_list[batch_idx].height) src_bbox.height -= 2;
    } else {
      src_bbox.left = 0;
      src_bbox.top = 0;
      src_bbox.width = src_buf->surface_list[batch_idx].width;
      src_bbox.height = src_buf->surface_list[batch_idx].height;
    }
    // apply src_buf roi
    int y_stride = src_buf->surface_list[batch_idx].plane_params.pitch[0];
    int uv_stride = src_buf->surface_list[batch_idx].plane_params.pitch[1];
    CnedkBufSurfaceColorFormat src_fmt = src_buf->surface_list[batch_idx].color_format;
    CnedkBufSurfaceColorFormat dst_fmt = GetBufSurfaceColorFormat(pix_fmt);

    y_plane += src_bbox.left + src_bbox.top * y_stride;
    uv_plane += src_bbox.left + src_bbox.top / 2 * uv_stride;

    void *dst_img = dst->GetHostData(0, batch_idx);

    uint8_t *dst_img_u8, *dst_img_roi;
    CnedkTransformRect dst_bbox;
    if (info.dtype == infer_server::DataType::UINT8) {
      dst_img_u8 = reinterpret_cast<uint8_t *>(dst_img);
    } else if (info.dtype == infer_server::DataType::FLOAT32) {
      img_tmp.reset(new uint8_t[img_size]);
      dst_img_u8 = img_tmp.get();
    } else {
      return -1;
    }

    memset(dst_img_u8, pad_value, img_size);
    if (keep_aspect_ratio) {
      dst_bbox = cnstream::KeepAspectRatio(src_bbox.width, src_bbox.height, info.w, info.h);
      // validate bbox
      dst_bbox.left -= dst_bbox.left & 1;
      dst_bbox.top -= dst_bbox.top & 1;
      dst_bbox.width -= dst_bbox.width & 1;
      dst_bbox.height -= dst_bbox.height & 1;

      while (dst_bbox.left + dst_bbox.width > info.w) dst_bbox.width -= 2;
      while (dst_bbox.top + dst_bbox.height > info.h) dst_bbox.height -= 2;

      dst_img_roi = dst_img_u8 + dst_bbox.left * info.c + dst_bbox.top * info.w * info.c;
    } else {
      dst_bbox.left = 0;
      dst_bbox.top = 0;
      dst_bbox.width = info.w;
      dst_bbox.height = info.h;
      dst_img_roi = dst_img_u8;
    }

    cnstream::YUV420spToRGBx(y_plane, uv_plane, src_bbox.width, src_bbox.height, y_stride, uv_stride, src_fmt,
                             dst_img_roi, dst_bbox.width, dst_bbox.height, info.w * info.c, dst_fmt);

    if (info.dtype == infer_server::DataType::FLOAT32) {
      float *dst_img_fp32 = reinterpret_cast<float *>(dst_img);
      if (mean_std) {
        for (uint32_t i = 0; i < info.w * info.h; i++) {
          for (uint32_t c_i = 0; c_i < info.c; c_i++) {
            dst_img_fp32[i * info.c + c_i] = (dst_img_u8[i * info.c + c_i] - mean[c_i]) / std[c_i];
          }
        }
      } else {
        for (uint32_t i = 0; i < img_size; i++) {
          dst_img_fp32[i] = static_cast<float>(dst_img_u8[i]);
        }
      }
    }
    dst->SyncHostToDevice(-1, batch_idx);
  }
  return 0;
}

// -----------------------------------------------------------------------------------------------
int GetCvDataType(infer_server::DataType dtype) {
  switch (dtype) {
    case infer_server::DataType::UINT8:
      return CV_8U;
    case infer_server::DataType::FLOAT32:
      return CV_32F;
    default:
      LOGW(PREPROC) << "Only support UINT8 and FLOAT32. Unknown data type, use UINT8 as default";
      return CV_8U;
  }
}

void SaveResult(const std::string &filename, int count, uint32_t batch_size, cnedk::BufSurfWrapperPtr dst_buf,
                const cnstream::CnPreprocNetworkInfo &info) {
  uint32_t data_size = info.w * info.h * info.c * GetDataTypeSize(info.dtype);
  for (size_t batch_idx = 0; batch_idx < batch_size; batch_idx++) {
    std::unique_ptr<unsigned char[]> cpu_data(new unsigned char[data_size]);
    void *dev_addr = dst_buf->GetData(0, batch_idx);
    if (cnrtMemcpy(cpu_data.get(), dev_addr, data_size, cnrtMemcpyDevToHost) != cnrtSuccess) {
      LOGE(PREPROC) << "SaveResult(): cnrtMemcpy failed";
      return;
    }
    cv::Mat image = cv::Mat(cvSize(info.w, info.h), CV_MAKETYPE(GetCvDataType(info.dtype), info.c),
                            reinterpret_cast<void *>(cpu_data.get()), cv::Mat::AUTO_STEP);
    cv::imwrite(filename + std::to_string(count) + "_" + std::to_string(batch_idx) + ".jpg", image);
  }
}
