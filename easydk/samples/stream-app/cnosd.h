#ifndef CNOSD_H_
#define CNOSD_H_

#include <cstring>
#include <fstream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <string>
#include <vector>
#include "easytrack/easy_track.h"

using cv::Mat;
using cv::Point;
using cv::Scalar;
using std::string;
using std::vector;

class CnOsd {
 private:
  size_t rows_ = 1;
  size_t cols_ = 1;
  int box_thickness_ = 2;
  vector<string> labels_;
  vector<Scalar> colors_;
  int font_ = cv::FONT_HERSHEY_SIMPLEX;
  cv::Size bm_size_ = {1920, 1080};  // benchmark size,used to calculate scale.
  float bm_rate_ = 1.0f;             // benchmark rate, used to calculate scale.
  inline float CalScale(uint64_t area) const {
    float c = 0.3f;
    float a = (c - bm_rate_) / std::pow(bm_size_.width * bm_size_.height, 2);
    float b = 2 * (bm_rate_ - c) / (bm_size_.width * bm_size_.height);
    float scale = a * area * area + b * area + c;
    if (scale < 0) return 0;
    return scale;
  }

 public:
  CnOsd() {}

  CnOsd(size_t rows, size_t cols, const vector<std::string>& labels);

  CnOsd(size_t rows, size_t cols, const std::string& label_fname);

  inline void set_rows(size_t rows) { rows_ = rows; }
  inline size_t rows() const { return rows_; }
  inline void set_cols(size_t cols) { cols_ = cols; }
  inline size_t cols() const { return cols_; }
  inline void set_box_thickness(int box_thickness) { box_thickness_ = box_thickness; }
  inline int get_box_thickness() const { return box_thickness_; }
  inline size_t chn_num() const { return rows() * cols(); }
  void LoadLabels(const std::string& fname);
  inline const std::vector<std::string> labels() const { return labels_; }
  inline void set_benchmark_size(cv::Size size) { bm_size_ = size; }
  inline cv::Size benchmark_size() const { return bm_size_; }
  inline void set_benchmark_rate(float rate) { bm_rate_ = rate; }
  inline float benchmark_rate() const { return bm_rate_; }
  void set_font(int font);
  void DrawId(Mat image, string text) const;
  void DrawId(Mat image, size_t chn_id) const;
  void DrawFps(Mat image, float fps) const;
  void DrawChannel(Mat image, size_t chn_id) const;
  void DrawChannels(Mat image) const;
  void DrawChannelFps(Mat image, const std::vector<float>& fps) const;
  void DrawChannelFps(Mat image, float* fps, size_t len) const;
  void DrawLabel(Mat image, const vector<edk::DetectObject>& objects, bool tiled = false) const;
};

#endif  // CNOSD_H_
