#include <opencv2/opencv.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "easyinfer/model_loader.h"
#include "easyinfer/shape.h"
#include "postproc.hpp"

class PostprocSR : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const std::shared_ptr<cnstream::CNFrameInfo>& package) {
    int width = package->frame.width;
    int height = package->frame.height;
    auto output_shapes = model->OutputShapes();
    int dst_w = output_shapes[0].w;
    int dst_h = output_shapes[0].h;
    cv::Mat img_h_temp(dst_h, dst_w, CV_32FC1, net_outputs[0]);
    cv::Mat img_h(dst_h, dst_w, CV_8UC1);
    for (int r = 0; r < dst_h; r++) {
      for (int c = 0; c < dst_w; c++) {
        img_h.at<uint8_t>(r, c) = static_cast<int>(img_h_temp.at<float>(r, c) * 255);
        if (img_h.at<uint8_t>(r, c) >= 255) img_h.at<uint8_t>(r, c) = 255;
        if (img_h.at<uint8_t>(r, c) <= 0) img_h.at<uint8_t>(r, c) = 0;
      }
    }
    uint8_t* img_data = new uint8_t[package->frame.GetBytes()];
    uint8_t* t = img_data;
    for (int i = 0; i < package->frame.GetPlanes(); ++i) {
      memcpy(t, package->frame.data[i]->GetCpuData(), package->frame.GetPlaneBytes(i));
      t += package->frame.GetPlaneBytes(i);
    }
    // convert color space
    cv::Mat img;
    cv::Mat img_ycrcb(width, height, CV_8UC3);
    switch (package->frame.fmt) {
      case cnstream::CNDataFormat::CN_PIXEL_FORMAT_BGR24:
        img = cv::Mat(height, width, CV_8UC3, img_data);
        cv::cvtColor(img, img_ycrcb, cv::COLOR_BGR2YCrCb);
        break;
      case cnstream::CNDataFormat::CN_PIXEL_FORMAT_RGB24:
        img = cv::Mat(height, width, CV_8UC3, img_data);
        cv::cvtColor(img, img_ycrcb, cv::COLOR_RGB2YCrCb);
        break;
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
    // resize if needed
    if (height != dst_h || width != dst_w) {
      cv::Mat dst_temp(dst_h, dst_w, CV_8UC3);
      cv::resize(img_ycrcb, dst_temp, cv::Size(dst_w, dst_h));
      img_ycrcb = dst_temp;
    }
    char* path;
    path = getcwd(NULL, 0);
    std::string output_dir_ = path;
    cv::Mat bgr_resize;
    cv::cvtColor(img_ycrcb, bgr_resize, cv::COLOR_YCrCb2BGR);
    std::string save_name1 = output_dir_ + std::string("/output/result") + std::string(".png");
    cv::imwrite(save_name1, bgr_resize);

    cv::Mat dst(dst_h, dst_w, CV_8UC1);
    std::vector<cv::Mat> channels;
    cv::split(img_ycrcb, channels);

    // figure the overflow
    cv::Mat img_h_resize = channels[0];
    for (int r = 0; r < dst_h; r++) {
      for (int c = 0; c < dst_w; c++) {
        if (img_h_resize.at<uint8_t>(r, c) >= 240 || img_h_resize.at<uint8_t>(r, c) <= 15) {
          if (abs(img_h_resize.at<uint8_t>(r, c) - img_h.at<uint8_t>(r, c)) >= 50)
            img_h.at<uint8_t>(r, c) = img_h_resize.at<uint8_t>(r, c);
        }
      }
    }
    img_h.convertTo(dst, CV_8U);

    channels[0] = dst;
    cv::merge(channels, img_ycrcb);
    // cv::imwrite("ycrcb-post-SR.png",img_ycrcb);
    cv::Mat img_dst(dst_h, dst_w, CV_8UC3);
    cv::cvtColor(img_ycrcb, img_dst, cv::COLOR_YCrCb2BGR);
    std::string save_name2 = output_dir_ + std::string("/output/result") + std::string(".png");
    cv::imwrite(save_name2, img_dst);

    return 0;
  }

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocSR, cnstream::Postproc);
};
IMPLEMENT_REFLEX_OBJECT_EX(PostprocSR, cnstream::Postproc);
