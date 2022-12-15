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
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_logging.hpp"
#include "gtest/gtest.h"
#include "sys/stat.h"
#include "sys/types.h"

#include "cnedk_platform.h"
#include "test_base.hpp"

#define PATH_MAX_SIZE 1024

class TestEnvironment : public testing::Environment {
 public:
  virtual void SetUp() {
    CnedkSensorParams sensor_params[4];
    memset(sensor_params, 0, sizeof(CnedkSensorParams) * 4);

    CnedkPlatformConfig config;
    memset(&config, 0, sizeof(config));


    CnedkVoutParams vout_params;
    memset(&vout_params, 0, sizeof(CnedkVoutParams));

    CnedkPlatformInit(&config);
  }
  ~TestEnvironment() {
    CnedkPlatformUninit();
  }
};

inline bool check_file_existence(const std::string &name) { return (access(name.c_str(), F_OK) == 0); }

std::string GetExecPath() {
  char path[PATH_MAX_SIZE];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_SIZE);
  if (cnt < 0 || cnt >= PATH_MAX_SIZE) {
    return "";
  }
  if (path[cnt - 1] == '/') {
    path[cnt - 1] = '\0';
  } else {
    path[cnt] = '\0';
  }
  std::string result = std::string(path);
  return result.substr(0, result.rfind('/') + 1);
}

// split the path for make it
std::vector<std::string> split_path(const std::string &s, char c) {
  std::stringstream ss(s);
  std::string piece;
  std::vector<std::string> chip_path;
  while (std::getline(ss, piece, c)) {
    chip_path.push_back(piece);
  }
  return chip_path;
}

void GetModuleExists() {
  std::string get_execute_path = GetExecPath();
  // std::cout << "program execute path is :" << get_execute_path << std::endl;
  std::string model_path;
  std::string model_file_path;

  std::string tmp = get_execute_path + "../../data/models";
  // if path not exists, return -1 and build it
  if (access(tmp.c_str(), F_OK) != 0) {
    mkdir(tmp.c_str(), 0777);
  }

  std::string tmp_path = get_execute_path + "../../data/models";
  std::vector<std::string> model_name = {
      GetModelInfoStr("resnet50", "name"),
      GetModelInfoStr("feature_extract", "name"),
      GetModelInfoStr("yolov3", "name"),
      GetLabelInfoStr("map_coco", "name"),
      GetLabelInfoStr("synset_word", "name")};
  std::vector<std::string> model_url = {
      GetModelInfoStr("resnet50", "url"),
      GetModelInfoStr("feature_extract", "url"),
      GetModelInfoStr("yolov3", "url"),
      GetLabelInfoStr("map_coco", "url"),
      GetLabelInfoStr("synset_word", "url")};
  model_path = tmp_path + "/";
  for (unsigned i = 0; i < model_name.size(); i++) {
    model_file_path = model_path + model_name[i];

    if (!check_file_existence(model_file_path)) {
      if (access(tmp_path.c_str(), F_OK) != 0) {
        mkdir(tmp_path.c_str(), 0777);
      }
      std::string cmd = "wget -P " + model_path + " " + model_url[i];
      if (system(cmd.c_str()) != 0) {
        std::cerr << "shell execute failed" << std::endl;
      }
    }
  }
}

int main(int argc, char **argv) {
  // GetModuleExists(model_name, modulepath_pair);
  GetModuleExists();
  // fork process and write log to file in child proccess is not supported in log system
  // google::InitGoogleLogging(GetExecPath().c_str());
  testing::InitGoogleTest(&argc, argv);
  ::gflags::ParseCommandLineFlags(&argc, &argv, false);
  // FLAGS_alsologtostderr = true;
  testing::AddGlobalTestEnvironment(new TestEnvironment);
  int ret = RUN_ALL_TESTS();
  // google::ShutdownGoogleLogging();
  return ret;
}
