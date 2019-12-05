#include <opencv2/opencv.hpp>

#include <memory>
#include <vector>

#include "easyinfer/shape.h"
#include "easyinfer/model_loader.h"
#include "preproc.hpp"

// adjust the value of image by network
class PreprocSR : public cnstream::Preproc {
 public:
  int Execute(const std::vector<float*>& net_inputs, const std::shared_ptr<edk::ModelLoader>& model,
              const std::shared_ptr<cnstream::CNFrameInfo>& package) {
    auto input_shapes = model->InputShapes();
    if (net_inputs.size() != 1 || input_shapes[0].c != 1) {
      LOG(ERROR) << "[PreprocCpu] model input shape not supported";
      return -1;
    }
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
    cv::Mat img_ycrcb(img.rows, img.cols, CV_8UC3);
    switch (package->frame.fmt) {
      case cnstream::CNDataFormat::CN_PIXEL_FORMAT_BGR24:
        img = cv::Mat(height, width, CV_8UC3, img_data);
        cv::cvtColor(img, img_ycrcb, cv::COLOR_BGR2YCrCb);
        break;
      case cnstream::CNDataFormat::CN_PIXEL_FORMAT_RGB24:
        img = cv::Mat(height, width, CV_8UC3, img_data);
        cv::cvtColor(img, img_ycrcb, cv::COLOR_RGB2YCrCb);
        break;
      // y：u：v为4：1：1
      case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
        img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
        cv::Mat bgr(height, width, CV_8UC3);
        cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
        cv::cvtColor(bgr, img_ycrcb, cv::COLOR_BGR2YCrCb);
      } break;
      case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
        img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
        cv::Mat bgr(height, width, CV_8UC3);
        cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
        cv::cvtColor(bgr, img_ycrcb, cv::COLOR_BGR2YCrCb);
      } break;
      default:
        LOG(WARNING) << "[Encoder] Unsupport pixel format.";
        delete[] img_data;
        return -1;
    }

    cv::Mat img_h(img_ycrcb.rows, img_ycrcb.cols, CV_8UC1);
    std::vector<cv::Mat> channels;
    cv::split(img_ycrcb, channels);
    img_h = channels[0];
    if (height != dst_h || width != dst_w) {
      cv::Mat dst(dst_h, dst_w, CV_8UC1);
      cv::resize(img_h, dst, cv::Size(dst_w, dst_h));
      img_h = dst;
    }
    cv::Mat img_h_temp(dst_h, dst_w, CV_32FC1);
    for (int r = 0; r < dst_h; r++) {
      for (int c = 0; c < dst_w; c++) {
        img_h_temp.at<float>(r, c) = static_cast<float>(img_h.at<uint8_t>(r, c)) / 255;
      }
    }
    // since model input data type is float, convert image to float
    cv::Mat dst(dst_h, dst_w, CV_32FC1, net_inputs[0]);
    img_h_temp.convertTo(dst, CV_32F);
    delete[] img_data;
    return 0;
  }  // class PreprocSRCpu

 private:
  DECLARE_REFLEX_OBJECT_EX(PreprocSR, cnstream::Preproc);
};
IMPLEMENT_REFLEX_OBJECT_EX(PreprocSR, cnstream::Preproc);
