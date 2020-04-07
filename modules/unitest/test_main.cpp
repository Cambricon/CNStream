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
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "sys/stat.h"
#include "sys/types.h"

#include "easyinfer/mlu_context.h"
#define PATH_MAX_SIZE 1024

class TestEnvironment : public testing::Environment {
 public:
  virtual void SetUp() {
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(0);
    mlu_ctx.SetChannelId(0);
    mlu_ctx.ConfigureForThisThread();
    LOG(INFO) << "Set Up global environment.";
  }
};

inline bool exists_file(const std::string &name) { return (access(name.c_str(), F_OK) != -1); }

std::string GetExecPath() {
  char path[PATH_MAX_SIZE];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_SIZE);
  if (cnt < 0 || cnt >= PATH_MAX_SIZE) {
    return "";
  }
  for (int i = cnt; i >= 0; --i) {
    if ('/' == path[i]) {
      path[i + 1] = '\0';
      break;
    }
  }
  std::string result(path);
  return result;
}

// split the path for make it
std::vector<std::string> split_path(std::string path) {
  char path_split[PATH_MAX_SIZE];
  char* saved_path;
  // strncpy(path_split, path.c_str(), PATH_MAX_SIZE);
  snprintf(path_split, PATH_MAX_SIZE, "%s", path.c_str());
  std::vector<std::string> chip_path;
  char * str = strtok_r(path_split, "/", &saved_path);
  while (str != nullptr) {
    chip_path.push_back(std::string(str));
    str = strtok_r(NULL, "/", &saved_path);
  }
  delete[] str;
  return chip_path;
}

void GetModuleExists(const std::vector<std::string> model_names, \
const std::map<std::string, std::pair<std::string, std::string> > model_pair) {
  std::string get_execute_path = GetExecPath();
  // std::cout << "program execute path is :" << get_execute_path << std::endl;
  std::string model_path;
  std::string model_file_path;

  std::string tmp = get_execute_path + "../../data/models";
  // if path not exists, return -1 and build it
  if (access(tmp.c_str(), F_OK) != 0) {
    mkdir(tmp.c_str(), 0777);
  }
  tmp += "/MLU270";
  if (access(tmp.c_str(), F_OK) != 0) {
    mkdir(tmp.c_str(), 0777);
  }

  for (int i = 0; i < static_cast<int>(model_names.size()); i++) {
    std::string model_name = model_names[i];
    std::string tmp_path = get_execute_path + "../../data/models/MLU270";
    model_path = tmp_path + (model_pair.find(model_name))->second.first;
    model_file_path = model_path + model_name;
    // model file does not exists
    if (!exists_file(model_file_path)) {
      std::vector<std::string> chip_path = split_path(model_pair.find(model_name)->second.first);
      for (int i = 0; i< static_cast<int>(chip_path.size()); i++) {
        tmp_path = tmp_path + "/" + chip_path[i];
        if (access(tmp_path.c_str(), F_OK) != 0) {
          mkdir(tmp_path.c_str(), 0777);
        }
      }
      std::string cmd ="wget -P " + model_path + " " + model_pair.find(model_name)->second.second;
      // std::cout << cmd << std::endl;
      if (system(cmd.c_str()) != 0) {
        std::cerr << "shell execute failed" << std::endl;
      }
    }
  }
}

int main(int argc, char **argv) {
  // check module exists
  std::map<std::string, std::pair<std::string, std::string> > modulepath_pair;

  modulepath_pair.insert(std::pair<std::string, std::pair<std::string, \
  std::string>>("resnet50_offline.cambricon", std::pair<std::string, std::string>("/Classification/resnet50/", \
  "http://video.cambricon.com/models/MLU270/Classification/resnet50/resnet50_offline.cambricon")));
  modulepath_pair.insert(std::make_pair("yuv2gray.cambricon", std::make_pair("/KCF/", \
  "http://video.cambricon.com/models/MLU270/KCF/yuv2gray.cambricon")));
  std::vector<std::string> model_name = {"resnet50_offline.cambricon", "yuv2gray.cambricon"};
  GetModuleExists(model_name, modulepath_pair);
  ::google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  ::gflags::ParseCommandLineFlags(&argc, &argv, false);
  // FLAGS_alsologtostderr = true;
  testing::AddGlobalTestEnvironment(new TestEnvironment);
  int ret = RUN_ALL_TESTS();
  ::google::ShutdownGoogleLogging();
  return ret;
}
