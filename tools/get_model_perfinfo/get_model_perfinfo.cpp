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
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "device/mlu_context.h"
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"

DEFINE_string(offline_model, "", "path of offline-model");
DEFINE_string(function_name, "subnet0", "model defined function name");
DEFINE_int32(th_num, 1, "thread number");
DEFINE_int32(iterations, 1, "invoke time per thread");
DEFINE_int32(dev_id, 0, "device id");

static volatile bool g_run = false;

std::pair<double, double> ThreadFunc() {
  edk::MluContext ctx;
  ctx.SetDeviceId(FLAGS_dev_id);
  ctx.BindDevice();

  auto msptr = std::make_shared<edk::ModelLoader>(FLAGS_offline_model, FLAGS_function_name);
  edk::EasyInfer infer;
  infer.Init(msptr, FLAGS_dev_id);

  edk::MluMemoryOp mem_op;
  mem_op.SetModel(msptr);
  void** input = mem_op.AllocMluInput();
  void** output = mem_op.AllocMluOutput();

  while (!g_run) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  double sw_total_time = 0, hw_total_time = 0;
  int run_num = FLAGS_iterations;
  while (run_num--) {
    float hw_time = 0;
    auto stime = std::chrono::steady_clock::now();
    infer.Run(input, output, &hw_time);
    auto etime = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> diff = etime - stime;
    sw_total_time += diff.count();
    hw_total_time += hw_time;
  }

  mem_op.FreeMluInput(input);
  mem_op.FreeMluOutput(output);

  if (FLAGS_iterations) {
    return std::make_pair(sw_total_time / FLAGS_iterations, hw_total_time / FLAGS_iterations);
  }
  return std::make_pair(0.0, 0.0);
}

int main(int argc, char *argv[]) {
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_offline_model.size() == 0) {
    std::cout << "offline model size is 0\n";
    return 0;
  }
  if (FLAGS_function_name.size() == 0) {
    std::cout << "function name size is 0\n";
    return 0;
  }
  if (FLAGS_th_num <= 0) {
    std::cout << "thread number <= 0\n";
    return 0;
  }
  if (FLAGS_iterations <= 0) {
    std::cout << "invoke time per thread <= 0\n";
    return 0;
  }

  int batchsize = 0;
  {
    edk::ModelLoader model(FLAGS_offline_model, FLAGS_function_name);

    std::cout << "----------------------input num: " << model.InputNum() << '\n';
    for (uint32_t i = 0; i < model.InputNum(); ++i) {
      std::cout << "model input shape " << i << ": " << model.InputShape(i) << std::endl;
    }

    std::cout << "---------------------output num: " << model.OutputNum() << '\n';
    for (uint32_t i = 0; i < model.OutputNum(); ++i) {
      std::cout << "model output shape " << i << ": " << model.OutputShape(i) << std::endl;
    }

    batchsize = model.InputShape(0).N();
  }

  std::vector<std::future<std::pair<double, double>>> th_perf_infos;
  for (int thi = 0; thi < FLAGS_th_num; ++thi) {
    th_perf_infos.push_back(std::async(std::launch::async, &::ThreadFunc));
  }

  std::this_thread::sleep_for(std::chrono::seconds(2));
  g_run = true;

  double total_sw_time = 0, total_hw_time = 0;
  for (auto& it : th_perf_infos) {
    auto time_pair = it.get();
    total_sw_time += time_pair.first;
    total_hw_time += time_pair.second;
  }

  std::cout << "Avg hardware time: " << total_hw_time / FLAGS_th_num << std::endl;
  std::cout << "Avg software time: " << total_sw_time / FLAGS_th_num << std::endl;
  std::cout << "Fps: " << 1000.0 * FLAGS_th_num * batchsize * FLAGS_th_num / total_sw_time << std::endl;

  return 0;
}

