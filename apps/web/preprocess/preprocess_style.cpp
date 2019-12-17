#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "easyinfer/model_loader.h"
#include "reflex_object.h"

#include "cnstream_frame.hpp"
#include "preproc.hpp"

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

class PreprocStyle_transfer : public cnstream::Preproc {
 public:
  /**
   * @brief Execute preproc on origin data
   *
   * @param net_inputs: neural network inputs
   * @param model: model information(you can get input shape and output shape from model)
   * @param package: smart pointer of struct to store origin data
   *
   * @return return 0 if succeed
   *
   * @attention net_inputs is a pointer to pre-allocated cpu memory
   */
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

  DECLARE_REFLEX_OBJECT_EX(PreprocStyle_transfer, cnstream::Preproc);
};  // class PreprocStyle_transfer

int PreprocStyle_transfer::Execute(const std::vector<float*>& net_inputs,
                                   const std::shared_ptr<edk::ModelLoader>& model,
                                   const cnstream::CNFrameInfoPtr& package) {
  // check params
  auto input_shapes = model->InputShapes();
  if (net_inputs.size() != 1 || input_shapes[0].c != 3) {
    LOG(ERROR) << "[PreprocCpu] model input shape not supported";
    return -1;
  }

  DLOG(INFO) << "[PreprocCpu] do preproc...";

  int width = package->frame.width;
  int height = package->frame.height;
  int dst_w = input_shapes[0].w;
  int dst_h = input_shapes[0].h;

  uint8_t* img_data = new uint8_t[package->frame.GetBytes()];
  uint8_t* t = img_data;

  for (int i = 0; i < package->frame.GetPlanes(); ++i) {
    memcpy(t, package->frame.data[i]->GetCpuData(), package->frame.GetPlaneBytes(i));
    t += package->frame.GetPlaneBytes(i);
  }

  // convert color space
  cv::Mat img;
  switch (package->frame.fmt) {
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      img = cv::Mat(height, width, CV_8UC3, img_data);
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      img = cv::Mat(height, width, CV_8UC3, img_data);
      cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
      img = bgr;
    } break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
      img = bgr;
    } break;
    default:
      LOG(WARNING) << "[Encoder] Unsupport pixel format.";
      delete[] img_data;
      return -1;
  }

  // resize if needed
  if (height != dst_h || width != dst_w) {
    cv::Mat dst(dst_h, dst_w, CV_8UC3);
    cv::resize(img, dst, cv::Size(dst_w, dst_h));
    img.release();
    img = dst;
  }

  // since model input data type is float, convert image to float
  cv::Mat dst(dst_h, dst_w, CV_32FC3, net_inputs[0]);
  img.convertTo(dst, CV_32FC3);

  float mean_value[3] = {
      122.5814138,
      116.5541927,
      103.8942281,
  };
  cv::Mat mean(512, 512, CV_32FC3, cv::Scalar(mean_value[2], mean_value[1], mean_value[0]));
  cv::Mat subtracted;
  cv::subtract(dst, mean, subtracted);

  auto input_data = net_inputs[0];
  std::vector<cv::Mat> channels(3);
  for (int j = 0; j < 3; j++) {
    cv::Mat split_image(512, 512, CV_32FC1, input_data);
    channels.push_back(split_image);
    input_data += 512 * 512;
  }
  cv::split(dst, channels);

  delete[] img_data;
  return 0;
}

IMPLEMENT_REFLEX_OBJECT_EX(PreprocStyle_transfer, cnstream::Preproc)
