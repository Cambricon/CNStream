/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#include "osd.hpp"

#include <cassert>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cnosd/cnosd.h"
#include "opencv2/opencv.hpp"

static std::vector<string> LoadLabels(const std::string& label_path) {
  std::vector<std::string> labels;
  std::ifstream ifs(label_path);
  if (!ifs.is_open()) return labels;

  while (!ifs.eof()) {
    std::string label_name;
    std::getline(ifs, label_name);
    labels.push_back(label_name);
  }

  ifs.close();
  return labels;
}

namespace cnstream {

Osd::Osd(const std::string& name) : Module(name) {}

bool Osd::Open(cnstream::ModuleParamSet paramSet) {
  if (paramSet.find("label_path") == paramSet.end()) {
    return false;
  }
  std::string label_path = paramSet.find("label_path")->second;
  labels_ = ::LoadLabels(label_path);
  return true;
}

void Osd::Close() { /*empty*/
}

int Osd::Process(std::shared_ptr<CNFrameInfo> data) {
  libstream::CnOsd processor(1, 1, labels_);

  int width = data->frame.width;
  int height = data->frame.height;

  CNDataFormat format = data->frame.fmt;

  uint8_t* img_data = new uint8_t[data->frame.GetBytes()];
  uint8_t* t = img_data;
  for (int i = 0; i < data->frame.GetPlanes(); ++i) {
    memcpy(t, data->frame.data[i]->GetCpuData(), data->frame.GetPlaneBytes(i));
    t += data->frame.GetPlaneBytes(i);
  }

  cv::Mat img;

  switch (format) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      img = cv::Mat(height, width, CV_8UC3, img_data).clone();
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      img = cv::Mat(height, width, CV_8UC3, img_data).clone();
      cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
      break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
      img = bgr;
      data->frame.ReallocMemory(CNDataFormat::CN_PIXEL_FORMAT_BGR24);
    } break;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21: {
      img = cv::Mat(height * 3 / 2, width, CV_8UC1, img_data);
      cv::Mat bgr(height, width, CV_8UC3);
      cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV21);
      img = bgr;
      data->frame.ReallocMemory(CNDataFormat::CN_PIXEL_FORMAT_BGR24);
    } break;
    default:
      LOG(WARNING) << "[Osd] Unsupport pixel format.";
      delete[] img_data;
      return -1;
  }

  delete[] img_data;

  std::vector<CnDetectObject> objs;
  for (const auto& it : data->objs) {
    CnDetectObject cn_obj;
    cn_obj.label = std::stoi(it->id);
    cn_obj.score = it->score;
    cn_obj.x = it->bbox.x;
    cn_obj.y = it->bbox.y;
    cn_obj.w = it->bbox.w;
    cn_obj.h = it->bbox.h;
    cn_obj.track_id = it->track_id.empty() ? -1 : std::stoi(it->track_id);
    objs.push_back(cn_obj);
  }
  processor.DrawLabel(img, objs);

  cv::Mat frame(height, width, CV_8UC3, data->frame.data[0]->GetMutableCpuData());
  img.copyTo(frame);

  return 0;
}

}  // namespace cnstream
