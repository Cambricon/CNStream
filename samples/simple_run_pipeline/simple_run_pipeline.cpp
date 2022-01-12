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

#include <gflags/gflags.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame_va.hpp"
#include "data_source.hpp"
#include "postproc.hpp"
#include "preproc.hpp"

DEFINE_string(input_url, "", "video file or images. "
  "eg. /your/path/to/file.mp4, /your/path/to/images/%d.jpg.");
DEFINE_string(how_to_show, "display", "image/video/display");
DEFINE_string(model_path, "", "/your/path/to/model_name.cambricon");
DEFINE_string(func_name, "subnet0", "function name in your offline model.");
DEFINE_string(model_type, "detector", "detector/classifier.");
DEFINE_string(label_path, "", "/your/path/to/label.txt");
DEFINE_string(output_dir, "./", "/your/path/to/output_dir");
DEFINE_int32(output_frame_rate, 25, "output frame rate");
DEFINE_bool(keep_aspect_ratio, false, "keep aspect ratio for image scaling");
DEFINE_string(mean_value, "0, 0, 0", "mean value in BGR order");
DEFINE_string(std, "1.0, 1.0, 1.0", "std in BGR order");
DEFINE_string(model_input_pixel_format, "BGRA", "BGRA/RGBA/ARGB/ABGR/BGR/RGB");
DEFINE_int32(dev_id, 0, "device ordinal index");

namespace simple_pipeline {

static std::array<float, 3> gmean_value;
static std::array<float, 3> gstd;
static std::vector<int> gchn_order;

// Init mean values and std. init channel order for color convert(eg. bgr to rgba)
bool InitGlobalValues() {
  if (3 != sscanf(FLAGS_mean_value.c_str(), "%f, %f, %f", &gmean_value[0], &gmean_value[1], &gmean_value[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse mean value failed. mean value should be the "
      "following format :\"100.2, 100.2, 100.2\"";
    return false;
  }
  if (3 != sscanf(FLAGS_std.c_str(), "%f, %f, %f", &gstd[0], &gstd[1], &gstd[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse std failed. std should be the following format :\"100.2, 100.2, 100.2\"";
    return false;
  }
  if ("BGRA" == FLAGS_model_input_pixel_format) {
    gchn_order = {0, 1, 2, 3};
  } else if ("RGBA" == FLAGS_model_input_pixel_format) {
    gchn_order = {2, 1, 0, 3};
  } else if ("ARGB" == FLAGS_model_input_pixel_format) {
    gchn_order = {3, 2, 1, 0};
  } else if ("ABGR" == FLAGS_model_input_pixel_format) {
    gchn_order = {3, 0, 1, 2};
  } else if ("BGR" == FLAGS_model_input_pixel_format) {
    gchn_order = {0, 1, 2};
  } else if ("RGB" == FLAGS_model_input_pixel_format) {
    gchn_order = {2, 1, 0};
  } else {
    LOGE(SIMPLE_PIPELINE) << "Parse model_input_pixel_format failed. Must be one of [BGRA/RGBA/ARGB/ABGR/BGR/RGB].";
    return false;
  }
  if ("detector" != FLAGS_model_type && "classifier" != FLAGS_model_type) return false;
  return true;
}

using CNFrameInfoSptr = std::shared_ptr<cnstream::CNFrameInfo>;
using SourceHandlerSptr = std::shared_ptr<cnstream::SourceHandler>;

// Reflex object, used to do image preprocessing. See parameter named preproc_name in Inferencer module.
class Preprocessor : public cnstream::Preproc {
 public:
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const CNFrameInfoSptr& frame_info) override;

 private:
  cv::Mat Resize(cv::Mat img, const cv::Size& dst_size);
  DECLARE_REFLEX_OBJECT_EX(simple_pipeline::Preprocessor, cnstream::Preproc);
};  // class Preprocessor

IMPLEMENT_REFLEX_OBJECT_EX(simple_pipeline::Preprocessor, cnstream::Preproc);

// Reflex object, used to do postprocessing. See parameter named postproc_name in Inferencer module.
// Supports classification models and detection models. eg. vgg, resnet, ssd, yolo-vx...
class Postprocessor : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const CNFrameInfoSptr& frame_info) override;

 private:
  int ExecuteDetect(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                    const CNFrameInfoSptr& frame_info);
  int ExecuteClassify(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                      const CNFrameInfoSptr& frame_info);
  DECLARE_REFLEX_OBJECT_EX(simple_pipeline::Postprocessor, cnstream::Postproc)
};  // class Postprocessor

IMPLEMENT_REFLEX_OBJECT_EX(simple_pipeline::Postprocessor, cnstream::Postproc);

// Base class to do visualizion
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
  void OnFrame(const CNFrameInfoSptr& frame_info) override;

 private:
  uint64_t frame_index_ = 0;
};  // class ImageSaver

// Encode pictures with the detection result or classification result into avi video file.
class VideoSaver : public VisualizerBase {
 public:
  explicit VideoSaver(int frame_rate) : fr_(frame_rate) {}
  void OnStart() override;
  void OnFrame(const CNFrameInfoSptr& frame_info) override;
  void OnStop() override;

 private:
  int fr_ = 25;
  cv::VideoWriter writer_;
  cv::Size video_size_ = {1920, 1080};
};  // class VideoSaver

// Use OpenCV to show the picture with the detection result or classification result.
class OpencvDisplayer : public VisualizerBase {
 public:
  explicit OpencvDisplayer(int frame_rate) : fr_(frame_rate) {}
  void OnStart() override;
  void OnFrame(const CNFrameInfoSptr& frame_info) override;
  void OnStop() override;

 private:
  int fr_ = 25;
  std::chrono::steady_clock::time_point last_show_time_;
};  // class OpencvDisplayer

// Pipeline runner.
// This class will show you how to build a pipeline,
// how to load images or videos into the pipeline and perform decoding, detection, and classification tasks
// and how to get the pipeline execution results.
class SimplePipelineRunner : public cnstream::StreamMsgObserver, cnstream::IModuleObserver {
 public:
  explicit SimplePipelineRunner(VisualizerBase* visualizer);
  bool Start(const std::string& url);
  void WaitPipelineDone();

 private:
  void notify(CNFrameInfoSptr frame_info) override;
  void Update(const cnstream::StreamMsg& msg) override;

 private:
  cnstream::Pipeline pipeline_;
  cnstream::DataSource* source_ = nullptr;
  VisualizerBase* visualizer_ = nullptr;
  std::promise<void> done_;
};  // class SimplePipelineRunner

int Preprocessor::Execute(const std::vector<float*>& net_inputs,
                          const std::shared_ptr<edk::ModelLoader>& model,
                          const CNFrameInfoSptr& frame_info) {
  LOGF_IF(SIMPLE_PIPELINE, net_inputs.size() != 1) << "model input number is not equal to 1";
  auto input_shape = model->InputShape(0);
  LOGF_IF(SIMPLE_PIPELINE, input_shape.C() != 3 && input_shape.C() != 4)
      << "model input channel is not equal to 3 or 4";
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  // resize
  cv::Mat resized_img = Resize(frame->ImageBGR(), cv::Size(input_shape.W(), input_shape.H()));
  // normalize
  cv::Mat float_mat;
  resized_img.convertTo(float_mat, CV_32FC3);
  cv::Mat mean_mat(float_mat.cols, float_mat.rows, CV_32FC3,
    cv::Scalar(gmean_value[0], gmean_value[1], gmean_value[2]));
  float_mat -= mean_mat;
  cv::Mat std_mat(float_mat.cols, float_mat.rows, CV_32FC3, cv::Scalar(gstd[0], gstd[1], gstd[2]));
  float_mat /= std_mat;
  // convert color
  std::vector<cv::Mat> split_mats;
  cv::split(float_mat, split_mats);
  if (gchn_order.size() == 4) {
    split_mats.emplace_back(cv::Mat(float_mat.cols, float_mat.rows, CV_32FC1, cv::Scalar(0.0f)));
  } else if (gchn_order.size() != 3) {
    LOGF(SIMPLE_PIPELINE) << "Never be here.";
  }
  std::vector<cv::Mat> ordered_mats;
  for (size_t chn_idx = 0; chn_idx < gchn_order.size(); ++chn_idx) {
    ordered_mats.emplace_back(std::move(split_mats[gchn_order[chn_idx]]));
  }
  cv::Mat dst_mat(float_mat.cols, float_mat.rows, gchn_order.size() == 4 ? CV_32FC4 : CV_32FC3, net_inputs[0]);
  cv::merge(ordered_mats, dst_mat);
  return 0;
}

cv::Mat Preprocessor::Resize(cv::Mat img, const cv::Size& dst_size) {
  cv::Mat dst(dst_size, CV_8UC3, cv::Scalar(128, 128, 128));
  if (FLAGS_keep_aspect_ratio) {
    float scaling_factors = std::min(1.0f * dst_size.width / img.cols, 1.0f * dst_size.height / img.rows);
    cv::Mat resized_mat = cv::Mat(img.rows * scaling_factors, img.cols * scaling_factors, CV_8UC3);
    cv::resize(img, resized_mat, resized_mat.size());
    cv::Rect roi;
    roi.x = (dst.cols - resized_mat.cols) / 2;
    roi.y = (dst.rows - resized_mat.rows) / 2;
    roi.width = resized_mat.cols;
    roi.height = resized_mat.rows;
    resized_mat.copyTo(dst(roi));
  } else {
    cv::resize(img, dst, dst.size());
  }
  return dst;
}

int Postprocessor::Execute(const std::vector<float*>& net_outputs,
                           const std::shared_ptr<edk::ModelLoader>& model,
                           const CNFrameInfoSptr& frame_info) {
  LOGF_IF(SIMPLE_PIPELINE, net_outputs.size() != 1) << "Can not deal with this model, "
    "output num : [" << net_outputs.size() << "].";
  if ("detector" == FLAGS_model_type) {
    ExecuteDetect(net_outputs, model, frame_info);
  } else if ("classifier" == FLAGS_model_type) {
    ExecuteClassify(net_outputs, model, frame_info);
  } else {
    LOGF(SIMPLE_PIPELINE) << "Never be here.";
  }
  return 0;
}

int Postprocessor::ExecuteDetect(const std::vector<float*>& net_outputs,
                                 const std::shared_ptr<edk::ModelLoader>& model,
                                 const CNFrameInfoSptr& frame_info) {
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  const auto input_sp = model->InputShape(0);
  const int img_w = frame->width;
  const int img_h = frame->height;
  const int model_input_w = static_cast<int>(input_sp.W());
  const int model_input_h = static_cast<int>(input_sp.H());
  const float* net_output = net_outputs[0];
  const uint64_t data_count = model->OutputShape(0).DataCount();
  const uint32_t box_num = static_cast<uint32_t>(net_output[0]);
  const int bbox_offset = 64;  // offset for yolo and ssd
  const int box_step = 7;  // nop, label, score, left, top, right, bottom. 7 values for an object.
  if (box_num * 7 + bbox_offset > data_count) {
    LOGE(SIMPLE_PIPELINE) << "Can not deal with this model, we get [" << box_num << "] detections, "
      "but model output len is [" << data_count << "].";
    return 0;
  }

  // scaled size
  float scaling_factors = 1.0;
  if (FLAGS_keep_aspect_ratio) scaling_factors = std::min(1.0 * model_input_w / img_w, 1.0 * model_input_h / img_h);
  const int scaled_w = scaling_factors * img_w;
  const int scaled_h = scaling_factors * img_h;

  // bboxes
  auto range_0_1 = [](float num) { return std::max(.0f, std::min(1.0f, num)); };
  cnstream::CNInferObjsPtr objs_holder =
    frame_info->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  cnstream::CNObjsVec &objs = objs_holder->objs_;  // stores detection objects here for other modules.
  for (uint32_t box_idx = 0; box_idx < box_num; ++box_idx) {
    float left = range_0_1(net_output[64 + box_idx * box_step + 3]);
    float right = range_0_1(net_output[64 + box_idx * box_step + 5]);
    float top = range_0_1(net_output[64 + box_idx * box_step + 4]);
    float bottom = range_0_1(net_output[64 + box_idx * box_step + 6]);

    // rectify
    left = (left * model_input_w - (model_input_w - scaled_w) / 2) / scaled_w;
    right = (right * model_input_w - (model_input_w - scaled_w) / 2) / scaled_w;
    top = (top * model_input_h - (model_input_h - scaled_h) / 2) / scaled_h;
    bottom = (bottom * model_input_h - (model_input_h - scaled_h) / 2) / scaled_h;
    left = std::max(0.0f, left);
    right = std::max(0.0f, right);
    top = std::max(0.0f, top);
    bottom = std::max(0.0f, bottom);

    auto obj = std::make_shared<cnstream::CNInferObject>();
    obj->id = std::to_string(static_cast<int>(net_output[64 + box_idx * box_step + 1]));
    obj->score = net_output[64 + box_idx * box_step + 2];

    obj->bbox.x = left;
    obj->bbox.y = top;
    obj->bbox.w = std::min(1.0f - obj->bbox.x, right - left);
    obj->bbox.h = std::min(1.0f - obj->bbox.y, bottom - top);

    if (obj->bbox.h <= 0 || obj->bbox.w <= 0 || (obj->score < threshold_ && threshold_ > 0)) continue;
    std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
    objs.push_back(obj);
  }
  return 0;
}

int Postprocessor::ExecuteClassify(const std::vector<float*>& net_outputs,
                                   const std::shared_ptr<edk::ModelLoader>& model,
                                   const CNFrameInfoSptr& frame_info) {
  auto data = net_outputs[0];
  auto len = model->OutputShape(0).DataCount();
  auto pscore = data;

  float mscore = 0;
  int label = 0;
  for (decltype(len) i = 0; i < len; ++i) {
    auto score = *(pscore + i);
    if (score > mscore) {
      mscore = score;
      label = i;
    }
  }

  auto obj = std::make_shared<cnstream::CNInferObject>();
  obj->id = std::to_string(label);
  obj->score = mscore;

  cnstream::CNInferObjsPtr objs_holder =
    frame_info->collection.Get<cnstream::CNInferObjsPtr>(cnstream::kCNInferObjsTag);
  std::lock_guard<std::mutex> objs_mutex(objs_holder->mutex_);
  objs_holder->objs_.push_back(obj);  // stores detection objects here for other modules.
  return 0;
}

inline
void ImageSaver::OnFrame(const CNFrameInfoSptr& frame_info) {
  const std::string output_file_name = FLAGS_output_dir + "/" + std::to_string(frame_index_++) + ".jpg";
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  cv::imwrite(output_file_name, frame->ImageBGR());
}

inline
void VideoSaver::OnStart() {
  LOGF_IF(SIMPLE_PIPELINE, !writer_.open(FLAGS_output_dir + "/output.avi",
                                         CV_FOURCC('M', 'J', 'P', 'G'),
                                         fr_, video_size_)) << "Open video writer failed.";
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
  double sleep_time = dura.count() - 1e3 / FLAGS_output_frame_rate;
  if (sleep_time > 0) usleep(sleep_time * 1e3);
  auto frame = frame_info->collection.Get<cnstream::CNDataFramePtr>(cnstream::kCNDataFrameTag);
  cv::imshow("simple pipeline", frame->ImageBGR());
  last_show_time_ = std::chrono::steady_clock::now();
  cv::waitKey(1);
}

inline
void OpencvDisplayer::OnStop() {
  cv::destroyAllWindows();
}

SimplePipelineRunner::SimplePipelineRunner(VisualizerBase* visualizer)
    : pipeline_("simple_pipeline"), visualizer_(visualizer) {
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
  decoder_config.className = "cnstream::DataSource";
  decoder_config.next = {"inferencer"};
  decoder_config.parameters = {
    std::make_pair("output_type", "cpu"),
    std::make_pair("decoder_type", "mlu"),  // use mlu to do decode
    std::make_pair("reuse_cndec_buf", "false"),
    std::make_pair("input_buf_number", "6"),
    std::make_pair("output_buf_number", "16"),
    std::make_pair("device_id", std::to_string(FLAGS_dev_id))
  };
  configs.push_back(decoder_config);
  cnstream::CNModuleConfig inferencer_config;
  inferencer_config.parallelism = 1;
  inferencer_config.name = "inferencer";
  inferencer_config.className = "cnstream::Inferencer";
  inferencer_config.maxInputQueueSize = 20;
  inferencer_config.next = {"osd"};
  inferencer_config.parameters = {
    std::make_pair("model_path", FLAGS_model_path),  // cambricon offline model path
    std::make_pair("func_name", FLAGS_func_name),
    std::make_pair("preproc_name", "simple_pipeline::Preprocessor"),  // the image preprocessor
    std::make_pair("postproc_name", "simple_pipeline::Postprocessor"),  // the postprocessor for model
    std::make_pair("threshold", "0.5"),
    std::make_pair("device_id", std::to_string(FLAGS_dev_id))
  };
  configs.push_back(inferencer_config);
  // Osd module used to draw detection results and classification results in origin images.
  cnstream::CNModuleConfig osd_config;
  osd_config.parallelism = 1;
  osd_config.name = "osd";
  osd_config.className = "cnstream::Osd";
  osd_config.maxInputQueueSize = 20;
  osd_config.parameters = {
    std::make_pair("label_path", FLAGS_label_path)
  };
  configs.push_back(osd_config);
  LOGF_IF(SIMPLE_PIPELINE, !pipeline_.BuildPipeline(configs)) << "Build pipeline failed.";

  // Gets source module, then we can add data into pipeline in SimplePipelineRunner::Start.
  source_ = dynamic_cast<cnstream::DataSource*>(pipeline_.GetModule("decoder"));
  // This show you one way to get pipeline results.
  // Set a module observer for the last module named 'osd',
  // then we can get every data from `notifier` function which is overwritted from cnstream::IModuleObserver.
  // Another more recommended way is to implement your own module and place it to the last of the pipeline,
  // then you can get every data in the module implemented by yourself.
  pipeline_.GetModule("osd")->SetObserver(this);

  // Set a stream message observer, then we can get message from pipeline, and we especially need to pay attention to
  // EOS message which tells us the input stream is ended.
  pipeline_.SetStreamMsgObserver(this);
}

bool SimplePipelineRunner::Start(const std::string& url) {
  if (!pipeline_.Start()) return false;
  visualizer_->OnStart();
  if (url.size() > 4 && "rtsp" == url.substr(0, 4)) {
    if (source_->AddSource(cnstream::RtspHandler::Create(source_, url, url))) {
      LOGE(SIMPLE_PIPELINE) << "Create data handler failed.";
      return false;
    }
  } else {
    if (source_->AddSource(cnstream::FileHandler::Create(source_, url, url, -1, false))) {
      LOGE(SIMPLE_PIPELINE) << "Create data handler failed.";
      return false;
    }
  }
  return true;
}

void SimplePipelineRunner::WaitPipelineDone() {
  done_.get_future().wait();
  pipeline_.Stop();
  visualizer_->OnStop();
}

inline
void SimplePipelineRunner::notify(CNFrameInfoSptr frame_info) {
  if (!frame_info->IsEos())  // eos frame has no data needed to be processed.
    visualizer_->OnFrame(frame_info);
}

void SimplePipelineRunner::Update(const cnstream::StreamMsg& msg) {
  switch (msg.type) {
    case cnstream::StreamMsgType::EOS_MSG:
      LOGI(SIMPLE_PIPELINE) << "End of stream.";
      visualizer_->OnStop();
      done_.set_value();
      break;
    case cnstream::StreamMsgType::FRAME_ERR_MSG:
      LOGW(SIMPLE_PIPELINE) << "Frame error, pts [" << msg.pts << "].";
      break;
    default:
      LOGF(SIMPLE_PIPELINE) << "Something wrong happend, msg type [" << static_cast<int>(msg.type) << "].";
  }
}

}  // namespace simple_pipeline

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  cnstream::InitCNStreamLogging(nullptr);
  if (!simple_pipeline::InitGlobalValues()) return 1;

  simple_pipeline::VisualizerBase* visualizer = nullptr;
  if ("image" == FLAGS_how_to_show) {
    visualizer = new simple_pipeline::ImageSaver();
  } else if ("video" == FLAGS_how_to_show) {
    visualizer = new simple_pipeline::VideoSaver(FLAGS_output_frame_rate);
  } else if ("display" == FLAGS_how_to_show) {
    visualizer = new simple_pipeline::OpencvDisplayer(FLAGS_output_frame_rate);
  } else {
    LOGE(SIMPLE_PIPELINE) << "Invalid value for flag [how_to_show]. image, video, display are valid.";
    return 1;
  }

  simple_pipeline::SimplePipelineRunner runner(visualizer);
  if (!runner.Start(FLAGS_input_url)) {
    LOGE(SIMPLE_PIPELINE) << "Start pipeline failed.";
    delete visualizer;
    return 1;
  }

  LOGI(SIMPLE_PIPELINE) << "Running...";
  runner.WaitPipelineDone();
  delete visualizer;

  cnstream::ShutdownCNStreamLogging();
  return 0;
}
