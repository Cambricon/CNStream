/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif
#include "cnstream_frame.hpp"
#include "postproc.hpp"

DECLARE_bool(get_faces_lib_flag);
DECLARE_string(person_name);
DECLARE_double(face_recognize_socre_threshold);
DECLARE_string(faces_lib_file_path);

// json file buf_size
#define FILE_BUF_SIZE 1024 * 100
#define FACE_FEATURE_DIMEN 4096
#define VGGFACE_DEBUG 1

void saveToFacesLib(const float *featureVec);

class PostprocVggface : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const cnstream::CNFrameInfoPtr &package, const std::shared_ptr<cnstream::CNInferObject> &obj) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocVggface, cnstream::Postproc)
};  // classd PostprocVggface

IMPLEMENT_REFLEX_OBJECT_EX(PostprocVggface, cnstream::Postproc)

int PostprocVggface::Execute(const std::vector<float *> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
                             const cnstream::CNFrameInfoPtr &package,
                             const std::shared_ptr<cnstream::CNInferObject> &obj) {
  float recognizeScore = 0.0f;
  std::string recognizeName;
  int faceId = 0;
  float recognizeThreshold = FLAGS_face_recognize_socre_threshold;

  // get network output feature map
  float *featureVec = net_outputs[0];

  // get face template mode
  if (FLAGS_get_faces_lib_flag) {
    saveToFacesLib(featureVec);
  } else {  // face match mode
    // read faces_lib
    rapidjson::Document document;
    FILE *fp = fopen("faces_lib.json", "r");
    char *buf = new char[FILE_BUF_SIZE];
    rapidjson::FileReadStream input(fp, buf, sizeof(buf));
    document.ParseStream(input);
    fclose(fp);

    // traverse faces_lib, match the faces
    rapidjson::Value::ConstMemberIterator it;
    int loopCount = 1;
    for (it = document.MemberBegin(); it != document.MemberEnd(); ++it) {
      float faceSimilarity = 0.0f;
      float vecValue_x = 0.0f;
      float vecValue_y = 0.0f;
      float denominator = 0.0f;
      float numerator = 0.0f;
      float denominatorPartA = 0.0f;
      float denominatorPartB = 0.0f;
      size_t vecSize = it->value.Size();
      assert((it->value.IsArray()) && (vecSize == FACE_FEATURE_DIMEN));
      float mean_xy = 0.0f;
      float sum_x = 0.0f;
      float sum_y = 0.0f;
      float sum_xy = 0.0f;
      size_t count_x = 0;
      size_t count_y = 0;

      // compute mean value
      for (rapidjson::SizeType index = 0; index < vecSize; ++index) {
        vecValue_x = featureVec[index];
        if (vecValue_x != 0) {
          sum_x += vecValue_x;
          count_x++;
        }
        vecValue_y = it->value[index].GetFloat();
        if (vecValue_y != 0) {
          sum_y += vecValue_y;
          count_y++;
        }
      }
      sum_xy = sum_x + sum_y;
      if (sum_xy != 0) {
        mean_xy = sum_xy / (count_x + count_y);
      }

      // Compute adjust cosine similarity
      for (rapidjson::SizeType index = 0; index < vecSize; ++index) {
        // compute numerator
        vecValue_x = featureVec[index];
        if (vecValue_x != 0) vecValue_x -= mean_xy;
        vecValue_y = it->value[index].GetFloat();
        if (vecValue_y != 0) vecValue_y -= mean_xy;
        numerator += vecValue_x * vecValue_y;
        if (vecValue_x != 0) {
          denominatorPartA += std::pow(vecValue_x, 2);
        }
        if (vecValue_y != 0) {
          denominatorPartB += std::pow(vecValue_y, 2);
        }
      }

      // compute denominator
      denominator = std::sqrt(denominatorPartA) * std::sqrt(denominatorPartB);
      if (denominator != 0) {
        faceSimilarity = numerator / denominator;
      }
#ifdef VGGFACE_DEBUG
      std::cout << "frame id :" << package->frame.frame_id << " faceSimilarity to " << it->name.GetString() << ": "
                << faceSimilarity << "........................" << std::endl;
#endif
      // record the max probility person
      if (faceSimilarity > recognizeScore) {
        recognizeScore = faceSimilarity;
        recognizeName = it->name.GetString();
        faceId = loopCount;
      }
      loopCount++;
    }
    delete[] buf;
  }

  // save the detectObj
  auto outObj = std::make_shared<cnstream::CNInferObject>();
  if (recognizeScore < recognizeThreshold) {
    recognizeName = "unknow";
    faceId = 0;
  }
  outObj->AddExtraAttribute("faceSocre", std::to_string(recognizeScore));
  outObj->AddExtraAttribute("name", recognizeName);

  // convert to float scale value for adapting osd module
  outObj->bbox.x = obj->bbox.x / package->frame.width;
  outObj->bbox.y = obj->bbox.y / package->frame.height;
  outObj->bbox.w = obj->bbox.w / package->frame.width;
  outObj->bbox.h = obj->bbox.h / package->frame.height;
  outObj->id = std::to_string(faceId);
#ifdef VGGFACE_DEBUG
  std::cout << "faceId :" << faceId << std::endl;
#endif
  outObj->score = recognizeScore;
  package->objs.push_back(outObj);
  return 0;
}

// Save face template to json face library
void saveToFacesLib(const float *featureVec) {
  rapidjson::Document document;
  rapidjson::Document::AllocatorType &allocator = document.GetAllocator();
  document.SetObject();
  rapidjson::Value name;
  name.SetString(FLAGS_person_name.c_str(), FLAGS_person_name.size(), document.GetAllocator());

  // open and write to faces lib json file
  const char *faces_lib_file = FLAGS_faces_lib_file_path.c_str();
  FILE *inputFp = fopen(faces_lib_file, "r+");
  char *inputBuf = new char[FILE_BUF_SIZE];

  rapidjson::FileReadStream input(inputFp, inputBuf, FILE_BUF_SIZE);
  document.ParseStream(input);
  fclose(inputFp);

  if (document.HasMember(name)) {
    std::cout << "already has the name, remove :" << FLAGS_person_name << std::endl;
    document.RemoveMember(name);
  }
  rapidjson::Value feature;
  feature.SetArray();
  for (int i = 0; i < FACE_FEATURE_DIMEN; ++i) {
    feature.PushBack(*(featureVec + i), allocator);
  }
  document.AddMember(name, feature, allocator);
  char *outputBuf = new char[FILE_BUF_SIZE];
  FILE *outputFp = fopen(faces_lib_file, "w+");
  rapidjson::FileWriteStream output(outputFp, outputBuf, FILE_BUF_SIZE);
  rapidjson::Writer<rapidjson::FileWriteStream> writer(output);
  document.Accept(writer);
  fclose(outputFp);

  // delete buf
  delete[] inputBuf;
  delete[] outputBuf;
}
