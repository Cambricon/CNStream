/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include <signal.h>
#include <stdio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "opencv2/opencv.hpp"
#if (CV_MAJOR_VERSION >= 4)
#include "opencv2/videoio/videoio_c.h"
#endif

#include "cnedk_platform.h"
#include "cnstream_frame_va.hpp"
#include "data_source.hpp"
#include "cnstream_postproc.hpp"
#include "cnstream_preproc.hpp"

DEFINE_string(input_url, "", "video file or images. e.g. /your/path/to/file.mp4, /your/path/to/images/%d.jpg.");
DEFINE_int32(input_num, 1, "input number");
DEFINE_string(how_to_show, "video", "image/video, otherwise do not show");
DEFINE_string(model_path, "", "/your/path/to/model_name.magicmind");
DEFINE_string(model_type, "yolov5", "yolov5/resnet50.");
DEFINE_string(label_path, "", "/your/path/to/label.txt");
DEFINE_string(output_dir, "./", "/your/path/to/output_dir");
DEFINE_int32(output_frame_rate, 25, "output frame rate");
DEFINE_bool(keep_aspect_ratio, false, "keep aspect ratio for image scaling");
DEFINE_string(pad_value, "114, 114, 114", "pad value in model input pixel format order");
DEFINE_string(mean_value, "0, 0, 0", "mean value in model input pixel format order");
DEFINE_string(std, "1.0, 1.0, 1.0", "std in model input pixel format order");
DEFINE_string(model_input_pixel_format, "RGB", "BGR/RGB");
DEFINE_int32(dev_id, 0, "device ordinal index");
DEFINE_int32(codec_id_start, 0, "vdec/venc first id, for CE3226 only");

namespace simple_pipeline {

static std::array<uint32_t, 3> gPadValue;
static std::array<float, 3> gMeanValue;
static std::array<float, 3> gStd;
static infer_server::NetworkInputFormat gFmt;
static bool gMeanStd;

// Init mean values and std. init channel order for color convert(eg. bgr to rgba)
bool InitGlobalValues() {
  if (3 != sscanf(FLAGS_pad_value.c_str(), "%d, %d, %d", &gPadValue[0], &gPadValue[1], &gPadValue[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse pad value failed. pad value should be the "
      "following format :\"114, 114, 114\"";
    return false;
  }
  if (3 != sscanf(FLAGS_mean_value.c_str(), "%f, %f, %f", &gMeanValue[0], &gMeanValue[1], &gMeanValue[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse mean value failed. mean value should be the "
      "following format :\"100.2, 100.2, 100.2\"";
    return false;
  }
  if (3 != sscanf(FLAGS_std.c_str(), "%f, %f, %f", &gStd[0], &gStd[1], &gStd[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse std failed. std should be the following format :\"100.2, 100.2, 100.2\"";
    return false;
  }

  if (abs(gMeanValue[0]) < 1e-6 && abs(gMeanValue[1]) < 1e-6 && abs(gMeanValue[2]) < 1e-6 &&
      abs(gStd[0] - 1) < 1e-6 && abs(gStd[1] - 1) < 1e-6 && abs(gStd[2] - 1) < 1e-6) {
    gMeanStd = false;
  } else {
    gMeanStd = true;
  }

  if (FLAGS_model_input_pixel_format == "RGB") {
    gFmt = infer_server::NetworkInputFormat::RGB;
  } else if (FLAGS_model_input_pixel_format == "BGR") {
    gFmt = infer_server::NetworkInputFormat::RGB;
  } else {
    LOGE(SIMPLE_PIPELINE) << "Parse model input pixel format failed, Must be one of [BGR/RGB], but "
                          << FLAGS_model_input_pixel_format;
    return false;
  }

  if ("yolov5" != FLAGS_model_type && "resnet50" != FLAGS_model_type) return false;
  return true;
}

using CNFrameInfoSptr = std::shared_ptr<cnstream::CNFrameInfo>;
using SourceHandlerSptr = std::shared_ptr<cnstream::SourceHandler>;

// Reflex object, used to do image preprocessing. See parameter named preproc_name in Inferencer module.
class Preprocessor : public cnstream::Preproc {
 public:
  int OnTensorParams(const infer_server::CnPreprocTensorParams *params) override;
  int Execute(cnedk::BufSurfWrapperPtr src_buf, cnedk::BufSurfWrapperPtr dst,
              const std::vector<CnedkTransformRect> &src_rects) override;

 private:
  std::mutex mutex_;
  cnstream::CnPreprocNetworkInfo info_;
  DECLARE_REFLEX_OBJECT_EX(simple_pipeline::Preprocessor, cnstream::Preproc);
};  // class Preprocessor

IMPLEMENT_REFLEX_OBJECT_EX(simple_pipeline::Preprocessor, cnstream::Preproc);

// Reflex object, used to do postprocessing. See parameter named postproc_name in Inferencer module.
// Supports classification models and detection models. eg. vgg, resnet, ssd, yolo-vx...
class Postprocessor : public cnstream::Postproc {
 public:
  int Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<cnstream::CNFrameInfoPtr>& packages,
              const cnstream::LabelStrings& labels) override;

 private:
  int ExecuteYolov5(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                    const std::vector<cnstream::CNFrameInfoPtr>& packages,
                    const cnstream::LabelStrings& labels);
  int ExecuteResnet50(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                      const std::vector<cnstream::CNFrameInfoPtr>& packages,
                      const cnstream::LabelStrings& labels);
  DECLARE_REFLEX_OBJECT_EX(simple_pipeline::Postprocessor, cnstream::Postproc)
};  // class Postprocessor

IMPLEMENT_REFLEX_OBJECT_EX(simple_pipeline::Postprocessor, cnstream::Postproc);

// Base class to do visualization
class VisualizerBase {
 public:
  virtual ~VisualizerBase() {}
  virtual void OnStart() {}
  virtual void OnFrame(const CNFrameInfoSptr& frame_info) = 0;
  virtual void OnStop() {}
};  // class Visualizer

// Save the picture with the detection result or classification result to the disk.
class ImageSaver : public VisualizerBase {
 public:
  explicit ImageSaver(const std::string& stream_id) : stream_id_(stream_id) {}
  void OnFrame(const CNFrameInfoSptr& frame_info) override;

 private:
  std::string stream_id_ = "";
  uint64_t frame_index_ = 0;
};  // class ImageSaver

// Encode pictures with the detection result or classification result into avi video file.
class VideoSaver : public VisualizerBase {
 public:
  explicit VideoSaver(int frame_rate, const std::string& stream_id)
      : fr_(frame_rate), stream_id_(stream_id) {}
  void OnStart() override;
  void OnFrame(const CNFrameInfoSptr& frame_info) override;
  void OnStop() override;

 private:
  int fr_ = 25;
  std::string stream_id_ = "";
  cv::VideoWriter writer_;
  cv::Size video_size_ = {1920, 1080};
};  // class VideoSaver

// Use OpenCV to show the picture with the detection result or classification result.
class OpencvDisplayer : public VisualizerBase {
 public:
  explicit OpencvDisplayer(int frame_rate, const std::string& stream_id)
      : fr_(frame_rate), stream_id_(stream_id) {}
  void OnStart() override;
  void OnFrame(const CNFrameInfoSptr& frame_info) override;
  void OnStop() override;

 private:
  int fr_ = 25;
  std::string stream_id_ = "";
  std::chrono::steady_clock::time_point last_show_time_;
};  // class OpencvDisplayer

// Pipeline runner.
// This class will show you how to build a pipeline,
// how to load images or videos into the pipeline and perform decoding, detection, and classification tasks
// and how to get the pipeline execution results.
class SimplePipelineRunner : public cnstream::Pipeline, public cnstream::StreamMsgObserver,
                             public cnstream::IModuleObserver {
 public:
  SimplePipelineRunner();
  int StartPipeline();
  int AddStream(const std::string& url, const std::string& stream_id, VisualizerBase* visualizer = nullptr);
  int RemoveStream(const std::string& stream_id);
  void WaitPipelineDone();
  void ForceStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    force_exit_.store(true);
  }

 private:
  void Notify(CNFrameInfoSptr frame_info) override;
  void Update(const cnstream::StreamMsg& msg) override;

 private:
  void IncreaseStream(std::string stream_id) {
    if (stream_set_.find(stream_id) != stream_set_.end()) {
      LOGF(SIMPLE_PIPELINE) << "IncreaseStream() The stream is ongoing []" << stream_id;
    }
    stream_set_.insert(stream_id);
    if (stop_) stop_ = false;
  }

 private:
  cnstream::DataSource* source_ = nullptr;
  std::unordered_map<std::string, VisualizerBase*> visualizer_map_;
  std::atomic<bool> stop_{false};
  std::set<std::string> stream_set_;
  std::condition_variable wakener_;
  mutable std::mutex mutex_;
  std::atomic<bool> force_exit_{false};
};  // class SimplePipelineRunner

int Preprocessor::OnTensorParams(const infer_server::CnPreprocTensorParams *params) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (GetNetworkInfo(params, &info_) < 0) {
    LOGE(SIMPLE_PIPELINE) << "[Preproc] get network information failed.";
    return -1;
  }

  VLOG1(SIMPLE_PIPELINE) << "[Preproc] Model input : w = " << info_.w << ", h = " << info_.h << ", c = " << info_.c
                         << ", dtype = " << static_cast<int>(info_.dtype)
                         << ", pixel_format = " << static_cast<int>(info_.format);
  return 0;
}

CnedkBufSurfaceColorFormat GetBufSurfaceColorFormat(infer_server::NetworkInputFormat pix_fmt) {
  switch (pix_fmt) {
    case infer_server::NetworkInputFormat::RGB:
      return CNEDK_BUF_COLOR_FORMAT_RGB;
    case infer_server::NetworkInputFormat::BGR:
      return CNEDK_BUF_COLOR_FORMAT_BGR;
    default:
      LOGW(SIMPLE_PIPELINE) << "Unknown input pixel format, use RGB as default";
      return CNEDK_BUF_COLOR_FORMAT_RGB;
  }
}

int PreprocessCpu(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                  const std::vector<CnedkTransformRect> &src_rects,
                  const cnstream::CnPreprocNetworkInfo &info) {
  if (src_rects.size() && src_rects.size() != src->GetNumFilled()) {
    return -1;
  }

  if ((src->GetColorFormat() != CNEDK_BUF_COLOR_FORMAT_NV12 &&
       src->GetColorFormat() != CNEDK_BUF_COLOR_FORMAT_NV21) ||
      (gFmt != infer_server::NetworkInputFormat::RGB && gFmt != infer_server::NetworkInputFormat::BGR)) {
    LOGE(SIMPLE_PIPELINE) << "[PreprocessCpu] Unsupported pixel format convertion";
    return -1;
  }

  if (info.dtype == infer_server::DataType::UINT8 && gMeanStd) {
    LOGW(SIMPLE_PIPELINE) << "[PreprocessCpu] not support uint8 with mean std.";
  }

  uint32_t batch_size = src->GetNumFilled();

  CnedkBufSurface* src_buf = src->GetBufSurface();
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
    CnedkBufSurfaceColorFormat dst_fmt = GetBufSurfaceColorFormat(gFmt);

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

    if (gPadValue[0] == gPadValue[1] && gPadValue[0] == gPadValue[2]) {
      memset(dst_img_u8, gPadValue[0], img_size);
    } else {
      for (uint32_t i = 0; i < info.w * info.h; i++) {
        for (uint32_t c_i = 0; c_i < info.c; c_i++) {
          dst_img_u8[i * info.c + c_i] = gPadValue[c_i];
        }
      }
    }
    if (FLAGS_keep_aspect_ratio) {
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
      if (gMeanStd) {
        for (uint32_t i = 0; i < info.w * info.h; i++) {
          for (uint32_t c_i = 0; c_i < info.c; c_i++) {
            dst_img_fp32[i * info.c + c_i] = (dst_img_u8[i * info.c + c_i] - gMeanValue[c_i]) / gStd[c_i];
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

int Preprocessor::Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                          const std::vector<CnedkTransformRect> &src_rects) {
  LOGF_IF(SIMPLE_PIPELINE, info_.c != 3) << "[Preproc] model input channel is not equal to 3";
  if (PreprocessCpu(src, dst, src_rects, info_) != 0) {
    LOGE(SIMPLE_PIPELINE) << "[Preprocessor] preprocess failed.";
    return -1;
  }
  return 0;
}

int Postprocessor::Execute(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                           const std::vector<cnstream::CNFrameInfoPtr>& packages,
                           const cnstream::LabelStrings& labels) {
  if ("yolov5" == FLAGS_model_type) {
    ExecuteYolov5(net_outputs, model_info, packages, labels);
  } else if ("resnet50" == FLAGS_model_type) {
    ExecuteResnet50(net_outputs, model_info, packages, labels);
  } else {
    LOGF(SIMPLE_PIPELINE) << "Never be here.";
  }
  return 0;
}

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

int Postprocessor::ExecuteYolov5(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                                 const std::vector<cnstream::CNFrameInfoPtr>& packages,
                                 const cnstream::LabelStrings& labels) {
  LOGF_IF(SIMPLE_PIPELINE, net_outputs.size() != 2) << "[Postprocessor] net outputs size is not valid";
  LOGF_IF(SIMPLE_PIPELINE, model_info.OutputNum() != 2) << "[Postprocessor] output number is not valid";

  cnedk::BufSurfWrapperPtr output0 = net_outputs[0].first;  // data
  cnedk::BufSurfWrapperPtr output1 = net_outputs[1].first;  // bbox
  if (!output0->GetHostData(0)) {
    LOGE(SIMPLE_PIPELINE) << "[Postprocessor] copy data to host first.";
    return -1;
  }
  if (!output1->GetHostData(0)) {
    LOGE(SIMPLE_PIPELINE) << "[Postprocessor] copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output0->GetBufSurface(), -1, -1);
  CnedkBufSurfaceSyncForCpu(output1->GetBufSurface(), -1, -1);

  infer_server::DimOrder input_order = model_info.InputLayout(0).order;
  auto s = model_info.InputShape(0);
  int model_input_w, model_input_h;
  if (input_order == infer_server::DimOrder::NCHW) {
    model_input_w = s[3];
    model_input_h = s[2];
  } else if (input_order == infer_server::DimOrder::NHWC) {
    model_input_w = s[2];
    model_input_h = s[1];
  } else {
    LOGE(SIMPLE_PIPELINE) << "[Postprocessor] not supported dim order";
    return -1;
  }

  auto range_0_w = [model_input_w](float num) {
    return std::max(.0f, std::min(static_cast<float>(model_input_w), num));
  };
  auto range_0_h = [model_input_h](float num) {
    return std::max(.0f, std::min(static_cast<float>(model_input_h), num));
  };

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output0->GetHostData(0, batch_idx));
    int box_num = static_cast<int*>(output1->GetHostData(0, batch_idx))[0];
    if (!box_num) {
      continue;  // no bboxes
    }

    cnstream::CNFrameInfoPtr package = packages[batch_idx];
    const auto frame = package->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
    cnstream::CNInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(cnstream::kCNInferObjsTag)) {
      objs_holder = package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
    }

    if (!objs_holder) {
      return -1;
    }

    const float scaling_w = 1.0f * model_input_w / frame->buf_surf->GetWidth();
    const float scaling_h = 1.0f * model_input_h / frame->buf_surf->GetHeight();
    const float scaling = std::min(scaling_w, scaling_h);
    float scaling_factor_w, scaling_factor_h;
    scaling_factor_w = scaling_w / scaling;
    scaling_factor_h = scaling_h / scaling;

    std::lock_guard<std::mutex> lk(objs_holder->mutex_);
    for (int bi = 0; bi < box_num; ++bi) {
      if (threshold_ > 0 && data[2] < threshold_) {
        data += 7;
        continue;
      }

      float l = range_0_w(data[3]);
      float t = range_0_h(data[4]);
      float r = range_0_w(data[5]);
      float b = range_0_h(data[6]);
      l = CLIP((l / model_input_w - 0.5f) * scaling_factor_w + 0.5f);
      t = CLIP((t / model_input_h - 0.5f) * scaling_factor_h + 0.5f);
      r = CLIP((r / model_input_w - 0.5f) * scaling_factor_w + 0.5f);
      b = CLIP((b / model_input_h - 0.5f) * scaling_factor_h + 0.5f);
      if (r <= l || b <= t) {
        data += 7;
        continue;
      }

      auto obj = std::make_shared<cnstream::CNInferObject>();
      uint32_t id = static_cast<uint32_t>(data[1]);
      obj->id = std::to_string(id);
      obj->score = data[2];
      obj->bbox.x = l;
      obj->bbox.y = t;
      obj->bbox.w = std::min(1.0f - l, r - l);
      obj->bbox.h = std::min(1.0f - t, b - t);

      if (!labels.empty() && id < labels[0].size()) {
        obj->AddExtraAttribute("Category", labels[0][id]);
      }

      objs_holder->objs_.push_back(obj);
      data += 7;
    }
  }  // for(batch_idx)
  return 0;
}

int Postprocessor::ExecuteResnet50(const cnstream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                                   const std::vector<cnstream::CNFrameInfoPtr>& packages,
                                   const cnstream::LabelStrings& labels) {
  LOGF_IF(SIMPLE_PIPELINE, net_outputs.size() != 1) << "[Postprocessor] model output size is not valid";
  LOGF_IF(SIMPLE_PIPELINE, model_info.OutputNum() != 1)
      << "[Postprocessor] model output number is not valid";

  cnedk::BufSurfWrapperPtr output = net_outputs[0].first;  // data
  if (!output->GetHostData(0)) {
    LOGE(SIMPLE_PIPELINE) << "[Postprocessor] copy data to host first.";
    return -1;
  }
  CnedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);

  auto len = model_info.OutputShape(0).DataCount();

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output->GetHostData(0, batch_idx));
    auto score_ptr = data;

    float max_score = 0;
    uint32_t label = 0;
    for (decltype(len) i = 0; i < len; ++i) {
      auto score = *(score_ptr + i);
      if (score > max_score) {
        max_score = score;
        label = i;
      }
    }
    if (threshold_ > 0 && max_score < threshold_) continue;

    cnstream::CNFrameInfoPtr package = packages[batch_idx];
    cnstream::CNInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(cnstream::kCNInferObjsTag)) {
      objs_holder = package->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
    }

    if (!objs_holder) {
      LOGE(SIMPLE_PIPELINE) << "[Postprocessor] object holder is nullptr.";
      return -1;
    }

    auto obj = std::make_shared<cnstream::CNInferObject>();
    obj->id = std::to_string(label);
    obj->score = max_score;

    if (!labels.empty() && label < labels[0].size()) {
      obj->AddExtraAttribute("Category", labels[0][label]);
    }

    std::lock_guard<std::mutex> lk(objs_holder->mutex_);
    objs_holder->objs_.push_back(obj);
  }  // for(batch_idx)

  return 0;
}

inline
void ImageSaver::OnFrame(const CNFrameInfoSptr& frame_info) {
  const std::string output_file_name =
      FLAGS_output_dir + "/output_" + stream_id_ + "_" + std::to_string(frame_index_++) + ".jpg";
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  cv::imwrite(output_file_name, frame->ImageBGR());
}

inline
void VideoSaver::OnStart() {
  LOGF_IF(SIMPLE_PIPELINE, !writer_.open(FLAGS_output_dir + "/output_" + stream_id_ + ".avi",
      CV_FOURCC('M', 'J', 'P', 'G'), fr_, video_size_)) << "Open video writer failed.";
}

inline
void VideoSaver::OnFrame(const CNFrameInfoSptr& frame_info) {
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  cv::Mat origin_img = frame->ImageBGR();
  cv::Mat resized_img;
  if (origin_img.size() != video_size_) {
    cv::resize(origin_img, resized_img, video_size_);
  } else {
    resized_img = origin_img;
  }
  writer_.write(resized_img);
}

inline
void VideoSaver::OnStop() {
  writer_.release();
}

inline
void OpencvDisplayer::OnStart() {
  last_show_time_ = std::chrono::steady_clock::now();
}

inline
void OpencvDisplayer::OnFrame(const CNFrameInfoSptr& frame_info) {
  std::chrono::duration<double, std::milli> dura = std::chrono::steady_clock::now() - last_show_time_;
  double sleep_time = dura.count() - 1e3 / fr_;
  if (sleep_time > 0) usleep(sleep_time * 1e3);
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  cv::imshow("simple pipeline " + stream_id_, frame->ImageBGR());
  last_show_time_ = std::chrono::steady_clock::now();
  cv::waitKey(1);
}

inline
void OpencvDisplayer::OnStop() {
  cv::destroyAllWindows();
}

SimplePipelineRunner::SimplePipelineRunner() : cnstream::Pipeline("simple_pipeline") {
  // Use cnstream::CNModuleConfig to load modules statically.
  // You can also load modules by a json config file,
  // see Pipeline::BuildPipelineByJsonFile for details
  // and there are some samples used json file to config a pipeline
  // you can see in directory CNStream/samples/cns_launcher/
  // See https://www.cambricon.com/docs/cnstream/user_guide_html/application/how_to_build_apps.html#id3
  // for more informations.
  // For how to implement your own module:
  // https://www.cambricon.com/docs/cnstream/user_guide_html/customize_module/how_to_implement_module.html
  std::vector<cnstream::CNModuleConfig> configs;
  cnstream::CNModuleConfig decoder_config;
  decoder_config.parallelism = 0;
  decoder_config.name = "decoder";
  decoder_config.class_name = "cnstream::DataSource";
  decoder_config.next = {"inferencer"};
  decoder_config.parameters = {
    std::make_pair("bufpool_size", "16"),
    std::make_pair("interval", "1"),
    std::make_pair("device_id", std::to_string(FLAGS_dev_id))
  };
  configs.push_back(decoder_config);
  cnstream::CNModuleConfig inferencer_config;
  inferencer_config.parallelism = 1;
  inferencer_config.name = "inferencer";
  inferencer_config.class_name = "cnstream::Inferencer";
  inferencer_config.max_input_queue_size = 20;
  inferencer_config.next = {"osd"};
  inferencer_config.parameters = {
    std::make_pair("model_path", FLAGS_model_path),  // cambricon offline model path
    std::make_pair("preproc", "name=simple_pipeline::Preprocessor"),  // the preprocessor
    std::make_pair("postproc", "name=simple_pipeline::Postprocessor;threshold=0.5"),  // the postprocessor
    std::make_pair("batch_timeout", "300"),
    std::make_pair("engine_num", "4"),
    std::make_pair("model_input_pixel_format", "TENSOR"),
    std::make_pair("device_id", std::to_string(FLAGS_dev_id))
  };
  configs.push_back(inferencer_config);
  // Osd module used to draw detection results and classification results in origin images.
  cnstream::CNModuleConfig osd_config;
  osd_config.parallelism = 1;
  osd_config.name = "osd";
  osd_config.class_name = "cnstream::Osd";
  osd_config.max_input_queue_size = 20;
  osd_config.parameters = {
    std::make_pair("label_path", FLAGS_label_path)
  };
  configs.push_back(osd_config);
  LOGF_IF(SIMPLE_PIPELINE, !BuildPipeline(configs)) << "Build pipeline failed.";

  // Gets source module, then we can add data into pipeline in SimplePipelineRunner::Start.
  source_ = dynamic_cast<cnstream::DataSource*>(GetModule("decoder"));
  // This show you one way to get pipeline results.
  // Set a module observer for the last module named 'osd',
  // then we can get every data from `notifier` function which is overwritted from cnstream::IModuleObserver.
  // Another more recommended way is to implement your own module and place it to the last of the pipeline,
  // then you can get every data in the module implemented by yourself.
  GetModule("osd")->SetObserver(this);

  // Set a stream message observer, then we can get message from pipeline, and we especially need to pay attention to
  // EOS message which tells us the input stream is ended.
  SetStreamMsgObserver(this);
}

int SimplePipelineRunner::StartPipeline() {
  if (!Start()) return -1;
  return 0;
}

int SimplePipelineRunner::AddStream(const std::string& url, const std::string& stream_id, VisualizerBase* visualizer) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (url.size() > 4 && "rtsp" == url.substr(0, 4)) {
    cnstream::RtspSourceParam param;
    param.url_name = url;
    param.use_ffmpeg = false;
    param.reconnect = 10;
    if (source_ && !source_->AddSource(cnstream::CreateSource(source_, stream_id, param))) {
      IncreaseStream(stream_id);
      visualizer_map_[stream_id] = visualizer;
      if (visualizer) {
        visualizer->OnStart();
      }
      return 0;
    }
  } else {
    cnstream::FileSourceParam param;
    param.filename = url;
    param.framerate = -1;
    param.loop = false;
    if (source_ && !source_->AddSource(cnstream::CreateSource(source_, stream_id, param))) {
      IncreaseStream(stream_id);
      visualizer_map_[stream_id] = visualizer;
      if (visualizer) {
        visualizer->OnStart();
      }
      return 0;
    }
  }
  return -1;
}

int SimplePipelineRunner::RemoveStream(const std::string& stream_id) {
  if (source_ && !source_->RemoveSource(stream_id)) {
    return 0;
  }
  return -1;
}

void SimplePipelineRunner::WaitPipelineDone() {
  while (1) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (force_exit_) break;
    if (stream_set_.empty()) {
      stop_ = true;
      force_exit_ = true;  // exit when all streams done
    }
    wakener_.wait_for(lk, std::chrono::milliseconds(100), [this]() {
      return stop_.load() || force_exit_.load();
    });
    lk.unlock();
  }
  LOGI(SIMPLE_PIPELINE) << "WaitForStop(): before pipeline Stop";
  if (!stop_.load()) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (nullptr != source_) {
      source_->RemoveSources();
    }
    wakener_.wait_for(lk, std::chrono::seconds(10), [this]() { return stop_.load(); });
  }
  this->Stop();
  CnedkPlatformUninit();
  source_ = nullptr;
  LOGI(SIMPLE_PIPELINE) << "WaitForStop(): pipeline Stop";
}

inline
void SimplePipelineRunner::Notify(CNFrameInfoSptr frame_info) {
  // eos frame has no data needed to be processed.
  if (!frame_info->IsEos()) {
    std::unique_lock<std::mutex> lk(mutex_);
    VisualizerBase* visualizer = nullptr;
    if (visualizer_map_.find(frame_info->stream_id) != visualizer_map_.end()) {
      visualizer = visualizer_map_[frame_info->stream_id];
    }
    lk.unlock();
    if (visualizer) {
      visualizer->OnFrame(frame_info);
    }
  }
}

void SimplePipelineRunner::Update(const cnstream::StreamMsg& msg) {
  std::lock_guard<std::mutex> lg(mutex_);
  switch (msg.type) {
    case cnstream::StreamMsgType::EOS_MSG:
      LOGI(SIMPLE_PIPELINE) << "[" << this->GetName() << "] End of stream [" << msg.stream_id << "].";
      if (stream_set_.find(msg.stream_id) != stream_set_.end()) {
        if (source_) source_->RemoveSource(msg.stream_id);
        if (visualizer_map_[msg.stream_id]) visualizer_map_[msg.stream_id]->OnStop();
        stream_set_.erase(msg.stream_id);
      }
      if (stream_set_.empty()) {
        LOGI(SIMPLE_PIPELINE) << "[" << this->GetName() << "] received all EOS";
        stop_ = true;
      }
      break;
    case cnstream::StreamMsgType::FRAME_ERR_MSG:
      LOGW(SIMPLE_PIPELINE) << "Frame error, pts [" << msg.pts << "].";
      break;
    default:
      LOGF(SIMPLE_PIPELINE) << "Something wrong happend, msg type [" << static_cast<int>(msg.type) << "].";
  }
  if (stop_) {
    wakener_.notify_one();
  }
}

}  // namespace simple_pipeline

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::INFO;
  FLAGS_colorlogtostderr = true;

  if (!simple_pipeline::InitGlobalValues()) return 1;

  CnedkPlatformConfig config;
  memset(&config, 0, sizeof(config));
  if (FLAGS_codec_id_start) {
    config.codec_id_start = FLAGS_codec_id_start;
  }
  if (CnedkPlatformInit(&config) < 0) {
    LOGE(CNS_LAUNCHER) << "Init platform failed";
    return -1;
  }

  std::vector<std::unique_ptr<simple_pipeline::VisualizerBase>> visualizer_vec;
  std::vector<std::string> stream_id_vec;
  for (int i = 0; i < FLAGS_input_num; i++) {
    stream_id_vec.push_back("stream_" + std::to_string(i));
    std::unique_ptr<simple_pipeline::VisualizerBase> visualizer = nullptr;
    if ("image" == FLAGS_how_to_show) {
      visualizer.reset(new simple_pipeline::ImageSaver(stream_id_vec[i]));
    } else if ("video" == FLAGS_how_to_show) {
      visualizer.reset(new simple_pipeline::VideoSaver(FLAGS_output_frame_rate, stream_id_vec[i]));
    // } else if ("display" == FLAGS_how_to_show) {
    //   visualizer.reset(new simple_pipeline::OpencvDisplayer(FLAGS_output_frame_rate, stream_id_vec[i]));
    } else {
      LOGW(SIMPLE_PIPELINE) << "Result will not show. Set flag [how_to_show] to [image/video] if show";
    }
    visualizer_vec.push_back(std::move(visualizer));
  }

  simple_pipeline::SimplePipelineRunner runner;
  if (runner.StartPipeline() != 0) {
    LOGE(SIMPLE_PIPELINE) << "Start pipeline failed.";
    return 1;
  }

  for (int i = 0; i < FLAGS_input_num; i++) {
    if (runner.AddStream(FLAGS_input_url, stream_id_vec[i], visualizer_vec[i].get()) != 0) {
      LOGE(SIMPLE_PIPELINE) << "Add stream failed.";
      return 1;
    }
  }

  LOGI(SIMPLE_PIPELINE) << "Running...";
  runner.WaitPipelineDone();

  google::ShutdownGoogleLogging();
  return 0;
}
