#include <opencv2/opencv.hpp>
#include <math.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "postproc.hpp"

namespace cv {
class PostprocDehaze : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float *> &net_outputs,
              const std::shared_ptr<edk::ModelLoader> &model,
              const cnstream::CNFrameInfoPtr &package);

 private:
  void Process(std::shared_ptr<cnstream::CNFrameInfo> data, Mat mat);
  double Acount(cv::Mat dark, cv::Mat img);
  Mat Guidedfilter(cv::Mat gray, cv::Mat mat);
  DECLARE_REFLEX_OBJECT_EX(PostprocDehaze, cnstream::Postproc);
};  // class PostprocDehaze

Mat PostprocDehaze::Guidedfilter(cv::Mat img, cv::Mat mat) {
  Mat gray;
  int height = img.rows;
  int width = img.cols;
  cvtColor(img, gray, CV_BGR2GRAY);
  gray.convertTo(gray, CV_32FC1);
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      gray.at<float>(i, j) = gray.at<float>(i, j) / 255.0;
    }
  }
  int r = 60;
  double eps = 0.0001;
  Mat mean_I;
  mean_I.convertTo(mean_I, CV_32FC1);
  boxFilter(gray, mean_I, -1, Size(r, r), Point(-1, -1), true,
            BORDER_DEFAULT);
  Mat mean_p;
  mean_p.convertTo(mean_p, CV_32FC1);
  boxFilter(mat, mean_p, -1, Size(r, r), Point(-1, -1), true, BORDER_DEFAULT);
  Mat mean_Ip;
  mean_Ip.convertTo(mean_Ip, CV_32FC1);
  Mat GM;
  GM.convertTo(GM, CV_32FC1);
  GM = gray.mul(mat);
  boxFilter(GM, mean_Ip, -1, Size(r, r), Point(-1, -1), true, BORDER_DEFAULT);
  Mat cov_Ip;
  cov_Ip.convertTo(cov_Ip, CV_32FC1);
  Mat MM;
  MM.convertTo(MM, CV_32FC1);
  MM = mean_I.mul(mean_p);
  cov_Ip = mean_Ip - MM;
  Mat mean_II;
  mean_II.convertTo(mean_II, CV_32FC1);
  Mat GG;
  GG.convertTo(GG, CV_32FC1);
  GG = gray.mul(gray);
  boxFilter(GG, mean_II, -1, Size(r, r), Point(-1, -1), true, BORDER_DEFAULT);
  Mat var_I;
  var_I.convertTo(var_I, CV_32FC1);
  Mat II;
  II.convertTo(II, CV_32FC1);
  II = mean_I.mul(mean_I);
  var_I = mean_II - II;
  Mat a;
  a.convertTo(a, CV_32FC1);
  a = cov_Ip / (var_I + eps);
  Mat b;
  b.convertTo(b, CV_32FC1);
  Mat AM;
  AM.convertTo(AM, CV_32FC1);
  AM = a.mul(mean_I);
  b = mean_p - AM;
  Mat mean_a;
  mean_a.convertTo(mean_a, CV_32FC1);
  boxFilter(a, mean_a, -1, Size(r, r), Point(-1, -1), true, BORDER_DEFAULT);
  Mat mean_b;
  mean_b.convertTo(mean_b, CV_32FC1);
  boxFilter(b, mean_b, -1, Size(r, r), Point(-1, -1), true, BORDER_DEFAULT);
  Mat q;
  q.convertTo(q, CV_32FC1);
  Mat MG;
  MG.convertTo(MG, CV_32FC1);
  MG = mean_a.mul(gray);
  q = MG + mean_b;
  q = cv::max(q, 0.1);
  return q;
}

double PostprocDehaze::Acount(cv::Mat dark, cv::Mat img) {
  double sum = 0;  // The sum of the pixel points according to condition A
  int pointNum = 0;  // Number of pixels that meet the requirements
  double A = 0;  // Atmospheric light intensity A
  double pix = 0;  // Pixel value in the first 0.1% range of luminance in dark channel image
  CvScalar pixel;  // According to the point a in the figure, the corresponding pixel in the fog figure
  float stretch_p[256], stretch_p1[256], stretch_num[256];
  int height = img.rows;
  int width = img.cols;

  /**
   Empty three arrays and initialize the filled array element to 0
   */
  memset(stretch_p, 0, sizeof(stretch_p));
  memset(stretch_p1, 0, sizeof(stretch_p1));
  memset(stretch_num, 0, sizeof(stretch_num));

  IplImage tmp = IplImage(dark);
  CvArr *arr = reinterpret_cast<CvArr *>(&tmp);
  IplImage tmp1 = IplImage(img);
  CvArr *arr1 = reinterpret_cast<CvArr *>(&tmp1);

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      double pixel0 = cvGetReal2D(arr, i, j);
      int pixel = static_cast<int>(pixel0);
      stretch_num[pixel]++;
    }
  }
  /**
   Count the probability of each gray level
   */
  for (int i = 0; i < 256; i++) {
    stretch_p[i] = stretch_num[i] / (height * width);
  }

  /**
   Count the probability of each gray level, take the first 0.1% of pixels
   from the dark channel image according to the brightness,and pix is the 
   dividing point
   */
  for (int i = 0; i < 256; i++) {
    for (int j = 0; j <= i; j++) {
      stretch_p1[i] += stretch_p[j];
      if (stretch_p1[i] > 0.999) {
        pix = static_cast<double>(i);
        i = 256;
        break;
      }
    }
  }

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      double temp = cvGetReal2D(arr, i, j);
      if (temp > pix) {
        pixel = cvGet2D(arr1, i, j);
        pointNum++;
        sum += pixel.val[0];
        sum += pixel.val[1];
        sum += pixel.val[2];
      }
    }
  }
  A = sum / (3 * pointNum);
  A = A / 255.0;
  if (A > 220.0) {
    A = 220.0;
  }
  printf("float: %f\n", A);
  return A;
}

int PostprocDehaze::Execute(
    const std::vector<float *> &net_outputs,
    const std::shared_ptr<edk::ModelLoader> &model,
    const cnstream::CNFrameInfoPtr &package) {
  if (net_outputs.size() != 1) {
    std::cerr << "[Warnning] Ssd neuron network only has one output,"
                 " but get " +
                     std::to_string(net_outputs.size()) + "\n";
    return -1;
  }

  auto sp = model->OutputShapes()[0];
  auto data = net_outputs[0];
  int height = sp.h;
  int width = sp.w;
  Mat mat = Mat(height, width, CV_32FC1, data);  // Network output transmission diagram
  Process(package, mat);
  return 0;
}

void PostprocDehaze::Process(std::shared_ptr<cnstream::CNFrameInfo> data,
                             Mat mat) {
  int width = mat.cols;
  int height = mat.rows;
  cv::Mat img;
  img = *data->frame.ImageBGR();
  Mat q, Dehaze;
  double A = 0.0;  // Atmospheric optical value
  imwrite("img.jpg", img);  // Get the original picture
  cv::resize(img, img, Size(width, height));
  std::vector<Mat> rgb;
  split(img, rgb);

  /**
   In fact, the dark channel is composed of gray-scale image 
   by taking the minimum value from three RGB channels
  */
  Mat dc = cv::min(cv::min(rgb.at(0), rgb.at(1)),
                   rgb.at(2));
  Mat kernel = getStructuringElement(cv::MORPH_RECT, Size(15, 15));
  Mat dark;
  erode(dc, dark, kernel);  // Corrosion is minimum filtering

  /**
   Take the first 0.1% of pixels from the dark channel image according to 
   the brightness. 2. In these positions, find the corresponding value 
   with the highest brightness point in the original image as a value.
   */
  A = Acount(dark, img);
  q = Guidedfilter(img, mat);
  Dehaze = Mat(height, width, CV_32FC3);
  img.convertTo(img, CV_32FC3);
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      img.at<Vec3f>(i, j)[0] = img.at<Vec3f>(i, j)[0] / 255.0;
      img.at<Vec3f>(i, j)[1] = img.at<Vec3f>(i, j)[1] / 255.0;
      img.at<Vec3f>(i, j)[2] = img.at<Vec3f>(i, j)[2] / 255.0;
    }
  }
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      Dehaze.at<Vec3f>(i, j)[0] = (((img.at<Vec3f>(i, j)[0] - A) / q.at<float>(i, j)) + A) * 255;
      Dehaze.at<Vec3f>(i, j)[1] = (((img.at<Vec3f>(i, j)[1] - A) / q.at<float>(i, j)) + A) * 255;
      Dehaze.at<Vec3f>(i, j)[2] = (((img.at<Vec3f>(i, j)[2] - A) / q.at<float>(i, j)) + A) * 255;
    }
  }
  static int i = 0;
  std::string img_name = std::to_string(i++) + ".jpg";
  cv::imwrite(img_name, Dehaze);
}
IMPLEMENT_REFLEX_OBJECT_EX(PostprocDehaze, cnstream::Postproc);
}  // namespace cv
