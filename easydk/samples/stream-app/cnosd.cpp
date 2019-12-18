#include "cnosd.h"
#include <algorithm>
#include <string>
#include <vector>

using std::to_string;

// Keep 2 digits after decimal
static string FloatToString(float number) {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%.2f", number);
  return string(buffer);
}

// http://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically

static Scalar HSV2RGB(const float h, const float s, const float v) {
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
  return Scalar(r * 255, g * 255, b * 255);
}

static vector<Scalar> GenerateColors(const int n) {
  vector<Scalar> colors;
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

static vector<string> LoadLabels(const string& filename) {
  vector<string> labels;
  std::ifstream file(filename);
  if (file.is_open()) {
    string line;
    while (std::getline(file, line)) {
      labels.push_back(string(line));
    }
    file.close();
  } else {
    printf("[Warning]: Load labels failed: %s\n", filename.c_str());
  }
  return labels;
}

CnOsd::CnOsd(size_t rows, size_t cols, const vector<string>& labels) : rows_(rows), cols_(cols), labels_(labels) {
  colors_ = ::GenerateColors(labels_.size());
}

CnOsd::CnOsd(size_t rows, size_t cols, const string& label_fname) : rows_(rows), cols_(cols) {
  LoadLabels(label_fname);
}

void CnOsd::LoadLabels(const std::string& fname) {
  labels_ = ::LoadLabels(fname);
  colors_ = ::GenerateColors(labels_.size());
}

void CnOsd::set_font(int font) { font_ = font; }

void CnOsd::DrawId(Mat image, string text) const {
  float scale = CalScale(image.cols * image.rows);
  cv::Size text_size = cv::getTextSize(text, font_, scale, 1, nullptr);
  Scalar color(0, 255, 255);
  cv::putText(image, text, Point(0, text_size.height), font_, scale, color, 1, 8, false);
}

void CnOsd::DrawId(Mat image, size_t chn_id) const {
  string text = "CHN:" + std::to_string(chn_id);
  DrawId(image, text);
}

void CnOsd::DrawFps(Mat image, float fps) const {
  // check input data
  if (image.cols * image.rows == 0) {
    return;
  }

  string text = "fps: " + ::FloatToString(fps);
  float scale = CalScale(image.cols * image.rows);
  cv::Size text_size = cv::getTextSize(text, font_, scale, 1, nullptr);
  Scalar color(0, 0, 255);
  cv::putText(image, text, Point(image.cols - text_size.width, text_size.height), font_, scale, color, 1, 8, false);
}

void CnOsd::DrawChannelFps(Mat image, const vector<float>& fps) const {
  // check input data
  if (cols() * rows() == 0) {
    return;
  }

  if (image.cols * image.rows == 0) {
    return;
  }

  // draw
  size_t width = image.cols / cols();
  size_t height = image.rows / rows();
  size_t chns = chn_num();
  size_t process_chn_num = std::min(chns, fps.size());

  for (decltype(process_chn_num) chn = 0; chn < process_chn_num; ++chn) {
    size_t r = chn / cols();  // channel on which row.
    size_t c = chn % cols();  // channel on which col.
    Mat roi = image(cv::Rect(c * width, r * height, width, height));
    DrawFps(roi, fps[chn]);
  }
}

void CnOsd::DrawChannelFps(Mat image, float* fps, size_t len) const {
  vector<float> vec;
  vec.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    vec.push_back(fps[i]);
  }
  DrawChannelFps(image, vec);
}

void CnOsd::DrawChannels(Mat image) const {
  // check input data
  if (cols() * rows() == 0) {
    // Invalid tiling cols() * rows()
    return;
  }

  if (image.cols * image.rows == 0) {
    // Invalid image size [image.cols * image.rows]
    return;
  }

  // draw
  size_t width = image.cols / cols();
  size_t height = image.rows / rows();
  for (size_t chn = 0; chn < chn_num(); ++chn) {
    size_t r = chn / cols();  // channel on which row.
    size_t c = chn % cols();  // channel on which col.
    cv::Mat roi = image(cv::Rect(c * width, r * height, width, height));
    DrawId(roi, chn);
  }
}

void CnOsd::DrawChannel(Mat image, size_t chn_id) const {
  // check input data
  if (cols() * rows() == 0) {
    // Invalid tiling cols() * rows()
    return;
  }

  if (image.cols * image.rows == 0) {
    // Invalid image size [image.cols * image.rows]
    return;
  }

  if (chn_id >= chn_num()) {
    // Invalid channel id: chn_id while there are chn_num() channels
    return;
  }

  // draw
  size_t width = image.cols / cols();
  size_t height = image.rows / rows();
  size_t r = chn_id / cols();  // channel on which row.
  size_t c = chn_id % cols();  // channel on which col.
  cv::Mat roi = image(cv::Rect(c * width, r * height, width, height));
  DrawId(roi, chn_id);
}

// tl: top left
// br: bottom right
// bl: bottom left
void CnOsd::DrawLabel(Mat image, const vector<edk::DetectObject>& objects, bool tiled) const {
  // check input data
  if (image.rows * image.cols == 0) {
    return;
  }

  for (auto& object : objects) {
    float xmin = object.bbox.x * image.cols;
    float ymin = object.bbox.y * image.rows;
    float xmax = (object.bbox.x + object.bbox.width) * image.cols;
    float ymax = (object.bbox.y + object.bbox.height) * image.rows;

    string text;
    Scalar color;
    if (labels().size() <= static_cast<size_t>(object.label)) {
      text = "Label not found, id = " + to_string(object.label);
      color = Scalar(0, 0, 0);
    } else {
      text = labels()[object.label];
      color = colors_[object.label];
    }

    // Detection window
    Point tl(xmin, ymin);
    Point br(xmax, ymax);
    int box_thickness = get_box_thickness();
    cv::rectangle(image, tl, br, color, box_thickness);

    // Label and Score
    text += " " + FloatToString(object.score);

    // Track Id
    if (object.track_id >= 0) text += " track_id:" + to_string(object.track_id);

    float scale = CalScale(image.cols * image.rows);

    if (tiled && (cols() * rows() != 0)) {
      scale = scale / (cols() * rows());
    }

    int text_thickness = 1;
    cv::Size text_size = cv::getTextSize(text, font_, scale, text_thickness, nullptr);

    int offset = (box_thickness == 1 ? 0 : -(box_thickness + 1) / 2);
    Point bl(xmin + offset, ymax + offset);
    Point label_left, label_right;
    label_left = bl;
    label_right = bl + Point(text_size.width + offset, text_size.height * 1.4);
    if (label_right.y > image.rows) {
      label_right.y -= text_size.height * 1.4;
      label_left.y -= text_size.height * 1.4;
    }
    if (label_right.x > image.cols) {
      label_right.x = image.cols;
      label_left.x = image.cols - text_size.width;
    }
    cv::rectangle(image, label_left, label_right, color, CV_FILLED);
    cv::putText(image, text, label_left + Point(0, text_size.height), font_, scale, Scalar(255, 255, 255) - color,
                text_thickness, 8, false);
  }
}
