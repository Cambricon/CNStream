#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <vector>
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"
#include "easytrack/easy_track.h"
#include "test_base.h"

static std::vector<float> feature_1 = {
    1.94531,   -0.734863, -0.657715,  -1.95215,   0.342773,  0.0381165,  -0.115356, 0.380615,     -0.0448914,
    -0.381592, -0.758789, -0.443604,  -0.0842285, -0.229736, -0.703613,  -1.53613,  2.06445,      0.227417,
    0.847656,  -0.893066, -0.159302,  2.42383,    0.821289,  0.989746,   0.427246,  -0.287354,    -0.577637,
    0.956055,  0.490967,  -0.387207,  -0.630371,  0.873535,  0.925293,   0.558105,  0.385498,     0.325439,
    0.959473,  -0.687012, 1.08203,    -0.379883,  0.818359,  -0.547852,  -1.18066,  0.494141,     0.5625,
    0.165527,  0.485596,  -0.806152,  0.549805,   0.294434,  -1.22852,   -0.27417,  -0.114319,    1.3125,
    1.12109,   1.46191,   -1.13672,   0.13855,    0.79834,   -0.902832,  0.918945,  0.763672,     1.16309,
    -1.0166,   0.156616,  1.46875,    -0.235474,  -0.257568, -0.137329,  1.10156,   -0.000878811, -0.670898,
    -0.296631, -0.692871, 0.814941,   1.57617,    -0.375,    -0.0352173, 0.39502,   -0.542969,    1.375,
    1.25,      -0.970703, -0.0493774, -0.453369,  -0.484863, -0.0697021, 1.35547,   0.519531,     -0.977539,
    -0.189575, 0.134155,  2.16016,    -0.536133,  0.529785,  0.741699,   -0.471924, -0.755371,    -0.0770874,
    -0.136597, 0.882812,  0.0347595,  -0.615234,  0.714844,  -0.292725,  -0.518066, -0.186279,    0.0632324,
    -0.774414, 1.22168,   -0.28125,   -0.818359,  0.0375671, 0.840332,   0.321533,  -0.00410843,  0.458008,
    -0.720703, -0.803223, -0.850098,  -0.527832,  -0.327637, 0.283691,   -0.437988, 0.378662,     -0.108887,
    0.13269};

static std::vector<float> feature_2 = {
    1.93945,   -0.732422, -0.653809,  -1.94434,   0.339111,  0.0434875,  -0.115356, 0.385986,    -0.0465393,
    -0.376465, -0.756348, -0.44043,   -0.0834961, -0.232788, -0.703613,  -1.53223,  2.06055,     0.223633,
    0.847656,  -0.88916,  -0.156372,  2.41797,    0.81543,   0.989746,   0.424316,  -0.281738,   -0.580078,
    0.952637,  0.494629,  -0.387207,  -0.624512,  0.878418,  0.918945,   0.554199,  0.384766,    0.326416,
    0.961426,  -0.683105, 1.08203,    -0.384766,  0.812012,  -0.547852,  -1.17969,  0.495361,    0.559082,
    0.164185,  0.483154,  -0.804199,  0.554199,   0.295654,  -1.22363,   -0.27417,  -0.114319,   1.30469,
    1.11914,   1.46094,   -1.13281,   0.134644,   0.79834,   -0.902344,  0.915527,  0.766602,    1.1582,
    -1.0166,   0.156616,  1.45801,    -0.234619,  -0.260986, -0.137329,  1.10742,   0.00474548,  -0.670898,
    -0.298584, -0.692871, 0.815918,   1.57617,    -0.36792,  -0.0352173, 0.395508,  -0.544434,   1.37793,
    1.24805,   -0.969727, -0.0443115, -0.447998,  -0.486328, -0.0667114, 1.34766,   0.514648,    -0.975586,
    -0.184448, 0.134155,  2.15039,    -0.536133,  0.527832,  0.737793,   -0.469238, -0.755371,   -0.076355,
    -0.136597, 0.879883,  0.0350342,  -0.61084,   0.708496,  -0.297363,  -0.515625, -0.189087,   0.0632324,
    -0.77002,  1.21777,   -0.28125,   -0.818359,  0.0401917, 0.841309,   0.321777,  -0.00931549, 0.459961,
    -0.716309, -0.803223, -0.845215,  -0.524902,  -0.332764, 0.281494,   -0.435059, 0.378662,    -0.103271,
    0.13269};

static std::vector<float> feature_3 = {
    1.93652,   -0.742188, -0.661621,  -1.95801,   0.341064,  0.0381165,  -0.115356, 0.380859,    -0.0401917,
    -0.384033, -0.760254, -0.435791,  -0.0888672, -0.232788, -0.703613,  -1.54785,  2.07422,     0.231323,
    0.841309,  -0.897949, -0.159302,  2.42188,    0.823242,  0.993164,   0.429199,  -0.290283,   -0.570312,
    0.956055,  0.493652,  -0.390137,  -0.634766,  0.880859,  0.923828,   0.560059,  0.376709,    0.314697,
    0.956543,  -0.69043,  1.08008,    -0.384766,  0.823242,  -0.550293,  -1.18066,  0.485352,    0.569336,
    0.167969,  0.485352,  -0.806152,  0.541992,   0.298584,  -1.22852,   -0.27124,  -0.117188,   1.31934,
    1.125,     1.45801,   -1.13281,   0.134644,   0.79834,   -0.904785,  0.917969,  0.758789,    1.16992,
    -1.01855,  0.156616,  1.48145,    -0.236084,  -0.257568, -0.137329,  1.10156,   0.00474548,  -0.667969,
    -0.296631, -0.688965, 0.805664,   1.58105,    -0.379883, -0.0402527, 0.386475,  -0.547852,   1.38574,
    1.24902,   -0.971191, -0.0493774, -0.453369,  -0.479004, -0.0755615, 1.36133,   0.527832,    -0.977539,
    -0.189575, 0.131104,  2.16211,    -0.536133,  0.526367,  0.749512,   -0.469238, -0.757812,   -0.0690308,
    -0.140503, 0.888672,  0.041626,   -0.617676,  0.718262,  -0.292725,  -0.515625, -0.177734,   0.0621643,
    -0.775879, 1.2334,    -0.28125,   -0.818359,  0.0293427, 0.848633,   0.321045,  -0.00410843, 0.448975,
    -0.720703, -0.804688, -0.848633,  -0.521973,  -0.327637, 0.295654,   -0.437988, 0.382568,    -0.111694,
    0.132446};

void data_gen(std::vector<edk::DetectObject>* objs, int det) {
  float d = static_cast<float>(det) / 100;
  objs->clear();
  if (det % 2 == 0) {
    objs->push_back(edk::DetectObject({1, 0.9f, {0.2f + d, 0.2f + d, 0.2f, 0.2f}}));
    objs->push_back(edk::DetectObject({2, 0.78f, {0.6f - d, 0.55f - d, 0.3f, 0.4f}}));
    objs->push_back(edk::DetectObject({3, 0.87f, {0.2f, 0.3f, 0.4f - d, 0.2f + d}}));
  } else {
    objs->push_back(edk::DetectObject({1, 0.9f, {0.2f + d, 0.2f + d, 0.2f, 0.2f}}));
    objs->push_back(edk::DetectObject({3, 0.87f, {0.2f, 0.3f, 0.4f - d, 0.2f + d}}));
    objs->push_back(edk::DetectObject({2, 0.78f, {0.6f - d, 0.55f - d, 0.3f, 0.4f}}));
  }
  for (int i = 0; i < 128; ++i) {
    feature_1[i] += 1e-3;
    feature_2[i] += 1e-3;
    feature_3[i] += 1e-3;
  }
}

TEST(Easytrack, FeatureMatch) {
  std::vector<edk::DetectObject> detects;
  std::vector<edk::DetectObject> tracks;

  int width = 1920, height = 1080;
  cv::Mat image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

  edk::TrackFrame frame;
  frame.data = image.data;
  frame.width = width;
  frame.height = height;
  frame.format = edk::TrackFrame::ColorSpace::RGB24;
  frame.dev_type = edk::TrackFrame::DevType::CPU;

  for (int j = 0; j < 3; j++) {
    edk::FeatureMatchTrack tracker;
    tracker.SetParams(0.2, 100, 0.7, 30, 3);
    for (int i = 0; i < 10; i++) {
      tracks.clear();
      frame.frame_id = i;
      data_gen(&detects, i);
      detects[0].feature = feature_1;
      detects[i % 2 + 1].feature = feature_2;
      detects[(i + 1) % 2 + 1].feature = feature_3;
      tracker.UpdateFrame(frame, detects, &tracks);
      EXPECT_EQ(tracks.size(), detects.size());
    }
  }
}

TEST(Easytrack, IouMatch) {
  std::vector<edk::DetectObject> detects;
  std::vector<edk::DetectObject> tracks;

  int width = 1920, height = 1080;
  cv::Mat image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

  edk::TrackFrame frame;
  frame.data = image.data;
  frame.width = width;
  frame.height = height;
  frame.format = edk::TrackFrame::ColorSpace::RGB24;
  frame.dev_type = edk::TrackFrame::DevType::CPU;

  for (int j = 0; j < 3; j++) {
    edk::FeatureMatchTrack tracker;
    tracker.SetParams(0.2, 100, 0.7, 30, 3);
    for (int i = 0; i < 10; i++) {
      tracks.clear();
      frame.frame_id = i;
      data_gen(&detects, i);
      tracker.UpdateFrame(frame, detects, &tracks);
      EXPECT_EQ(tracks.size(), detects.size());
    }
  }
}

TEST(Easytrack, KCF) {
  bool ret = true;
#ifdef CNSTK_MLU100
  int width = 500;
  int height = 500;
  std::vector<edk::DetectObject> detects;
  std::vector<edk::DetectObject> tracks;
  edk::KcfTrack *tracker = nullptr;
  edk::MluMemoryOp mem_op;

  std::string model_path = GetExePath() + "../../samples/data/models/MLU100/resnet34_ssd.cambricon";
  std::string func_name = "subnet0";
  tracker = new edk::KcfTrack;
  auto loader = std::make_shared<edk::ModelLoader>(model_path, func_name);

  tracker->SetModel(loader);
  tracker->SetParams(0.2);
  int* size = new int[10];
  void *output = mem_op.AllocMlu(width * height, 1);

  std::string image_path = GetExePath() + "../../tests/data/500x500.jpg";
  cv::Mat image = cv::imread(image_path);
  cv::resize(image, image, cv::Size(width, height));
  cv::cvtColor(image, image, CV_BGRA2GRAY);
  mem_op.MemcpyH2D(output, reinterpret_cast<void *>(image.data), width * height, 1);

  for (int i = 0; i < 10; i++) {
    detects.clear();

    edk::TrackFrame frame;
    frame.data = output;
    frame.width = width;
    frame.height = height;
    frame.format = edk::TrackFrame::ColorSpace::NV21;
    frame.frame_id = i;
    frame.dev_type = edk::TrackFrame::DevType::MLU;
    frame.device_id = 0;

    float d = static_cast<float>(i) / 100;
    if (i == 0) {
      detects.push_back(edk::DetectObject({1, 0.9f, {0.2f - d, 0.2f - d, 0.2f, 0.2f}}));
    }
    if (i == 4) {
      detects.push_back(edk::DetectObject({1, 0.9f, {0.2f - d, 0.2f - d, 0.2f, 0.2f}}));
      detects.push_back(edk::DetectObject({2, 0.78f, {0.6f - d, 0.55f - d, 0.3f, 0.4f}}));
      detects.push_back(edk::DetectObject({3, 0.87f, {0.2f, 0.3f, 0.4f - d, 0.2f + d}}));
      detects.push_back(edk::DetectObject({4, 0.78f, {0.6f, 0.3f, 0.4f - d, 0.2f + d}}));
      }
    if (i == 8) {
      detects.push_back(edk::DetectObject({1, 0.9f, {0.2f - d, 0.2f - d, 0.2f, 0.2f}}));
      detects.push_back(edk::DetectObject({2, 0.78f, {0.6f - d, 0.55f - d, 0.3f, 0.4f}}));
    }
    tracker->UpdateFrame(frame, detects, &tracks);
    size[i] = tracks.size();
  }
  for (int i = 0; i < 10; i++) {
    if (size[i] != size[i/4*4]) {
      ret = false;
      break;
    }
  }
  mem_op.FreeMlu(output);
  delete tracker;
  delete[] size;
#else
  ret = true;
#endif
  EXPECT_TRUE(ret);
}
