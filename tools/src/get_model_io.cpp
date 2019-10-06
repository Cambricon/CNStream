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

#include <glog/logging.h>
#include <iostream>
#include <mutex>
#include <string>

#include "cnrt.h"

#define CHECK_CNRT(RET) LOG_IF(FATAL, CNRT_RET_SUCCESS != (RET)) << "Cnrt failed with error: "

DEFINE_string(offline_model, "", "path of offline-model");
DEFINE_string(function_name, "subnet0", "model defined function name");

class GetIoForm {
 public:
  GetIoForm(const fLS::clstring& model_path, const fLS::clstring &func_name);
  ~GetIoForm();

 private:
  int i_num_, o_num_;
  cnrtModel_t model_;
  cnrtFunction_t function_;
  cnrtDataDescArray_t i_desc_array_, o_desc_array_;
};

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  CHECK_NE(FLAGS_offline_model.size(), 0);
  CHECK_NE(FLAGS_function_name.size(), 0);

  GetIoForm model(FLAGS_offline_model, FLAGS_function_name);
  std::cout << "[INFO] succeed in getting input & output format\n";
  ::google::ShutdownGoogleLogging();
  return 0;
}

class CnrtInitTool {
 public:
  static CnrtInitTool* instance() {
    static CnrtInitTool instance;
    return &instance;
  }

  void init() {
    mutex_.lock();
    if (!isInitialized_) {
      CNRT_CHECK(cnrtInit(0));
      unsigned int dev_cnt;
      CNRT_CHECK(cnrtGetDeviceCount(&dev_cnt));
      if (0 == dev_cnt) exit(0);
      isInitialized_ = true;
    }
    mutex_.unlock();
  }

 private:
  CnrtInitTool() : isInitialized_(false) {}
  ~CnrtInitTool() {
    if (isInitialized_)
      cnrtDestroy();
  }
  bool isInitialized_;
  std::mutex mutex_;

  CnrtInitTool(const CnrtInitTool&) = delete;     \
  const CnrtInitTool& operator=(const CnrtInitTool&) = delete;
};  // class CnrtInitTool

GetIoForm::GetIoForm(const fLS::clstring& model_path,
    const fLS::clstring& func_name) {
  ::CnrtInitTool::instance()->init();

  CNRT_CHECK(cnrtLoadModel(&model_, model_path.c_str()));

  CNRT_CHECK(cnrtCreateFunction(&function_));

  CNRT_CHECK(cnrtExtractFunction(&function_, model_, func_name.c_str()));

  CNRT_CHECK(cnrtGetInputDataDesc(&i_desc_array_, &i_num_, function_));

  CNRT_CHECK(cnrtGetOutputDataDesc(&o_desc_array_, &o_num_, function_));

  std::cout << "----------------------input num: " << i_num_ << '\n';
  for (int i = 0; i < i_num_; ++i) {
    cnrtDataDesc_t data_desc = i_desc_array_[i];
    unsigned int n, c, h, w;
    CNRT_CHECK(cnrtGetDataShape(data_desc, &n, &c, &h, &w));
    std::cout << "model input shape " << i << ": "
      << n << " " << c << " " << h << " " << w << '\n';
  }

  std::cout << "---------------------output num: " << o_num_ << '\n';
  for (int i = 0; i < o_num_; ++i) {
    cnrtDataDesc_t data_desc = o_desc_array_[i];
    unsigned int n, c, h, w;
    CNRT_CHECK(cnrtGetDataShape(data_desc, &n, &c, &h, &w));
    std::cout << "model output shape " << i << ": "
      << n << " " << c << " " << h << " " << w << '\n';
  }
}

GetIoForm::~GetIoForm() {
  CNRT_CHECK(cnrtDestroyFunction(function_));
  CNRT_CHECK(cnrtUnloadModel(model_));
}
