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
#include <string>

#include "easyinfer/model_loader.h"

DEFINE_string(offline_model, "", "path of offline-model");
DEFINE_string(function_name, "subnet0", "model defined function name");

int main(int argc, char *argv[]) {
  ::google::InitGoogleLogging(argv[0]);
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  CHECK_NE(FLAGS_offline_model.size(), 0);
  CHECK_NE(FLAGS_function_name.size(), 0);

  edk::ModelLoader model(FLAGS_offline_model, FLAGS_function_name);

  std::cout << "----------------------input num: " << model.InputNum() << '\n';
  for (uint32_t i = 0; i < model.InputNum(); ++i) {
    std::cout << "model input shape " << i << ": " << model.InputShapes()[i] << std::endl;
  }

  std::cout << "---------------------output num: " << model.OutputNum() << '\n';
  for (uint32_t i = 0; i < model.OutputNum(); ++i) {
    std::cout << "model output shape " << i << ": " << model.OutputShapes()[i] << std::endl;
  }

  std::cout << "[INFO] succeed in getting input & output format\n";
  ::google::ShutdownGoogleLogging();
  return 0;
}

