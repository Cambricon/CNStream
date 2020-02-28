#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "opencv2/opencv.hpp"

#include "easybang/resize.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "../src/easyinfer/mlu_task_queue.h"
#include "test_base.h"

std::string exe_path = GetExePath();  //NOLINT
std::string dir = "../../samples/data/images/";  // NOLINT
std::mutex print_mutex;
edk::MluMemoryOp mem_op;

struct TestResizeParam {
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  int bsize;
  int core;
  bool yuv_nv12;
};

void SaveImg(cv::Mat yuv_img, bool yuv_nv12, int cnt, std::string prefix) {
  cv::Mat rgb_image;

  int CV_MODE = yuv_nv12 ? CV_YUV2RGB_NV12 : CV_YUV2RGB_NV21;

  // transfer from yuv to rgb
  cvtColor(yuv_img, rgb_image, CV_MODE);
  // save RGB image
  cv::imwrite(exe_path + dir + prefix +  std::to_string(cnt) + ".jpg", rgb_image);
}

void Rgb2Yuv(std::string path, TestResizeParam p, char* cpu_input, int frame_cnt) {
  cv::Mat src_image, src_yuv_image;

  // read src image
  src_image = cv::imread(exe_path + dir + path, CV_LOAD_IMAGE_COLOR);

  ASSERT_FALSE(src_image.empty()) << "read \"" << exe_path + dir + path << "\" failed";

  int src_img_area = p.src_w * p.src_h;

  // resize to src h x w
  cv::resize(src_image, src_image, cv::Size(p.src_w, p.src_h));

  // bgr 2 yuv 420
  src_yuv_image.convertTo(src_yuv_image, CV_8UC3, 1.f);
  cvtColor(src_image, src_yuv_image, CV_BGR2YUV_I420);

  // yuv to yuv nv12 or nv21
  char* srcU_ = reinterpret_cast<char*>(src_yuv_image.data) + src_img_area;
  char* srcV_ = srcU_ + src_img_area / 4;
  char* srcUV = cpu_input + src_img_area;
  memcpy(cpu_input, reinterpret_cast<char*>(src_yuv_image.data), src_img_area);

  for (int i = 0; i < src_img_area / 4; i++) {
    if (!p.yuv_nv12) {
      (*srcUV++) = (*srcU_++);
      (*srcUV++) = (*srcV_++);
    } else {
      (*srcUV++) = (*srcV_++);
      (*srcUV++) = (*srcU_++);
    }
  }

  // uncomment to save image
  // src_yuv_image.data = (unsigned char*)cpu_input;
  // SaveImg(src_yuv_image, p.yuv_nv12, frame_cnt, "src_input_");
}


void D2H(char* cpu_output, char* mlu_output, TestResizeParam param) {
  cv::Mat dst_resize_image, dst_rgb_image,  dst_yuv_image;

  int dst_img_size = param.dst_w * param.dst_h * 3 / 2;

  // copy result from mlu to cpu
  mem_op.MemcpyD2H(cpu_output, mlu_output, dst_img_size, param.bsize);

  // resize
  dst_resize_image = cv::Mat(cv::Size(param.dst_w, param.dst_h), CV_8UC3, cv::Scalar::all(0));

  // yuv
  dst_yuv_image.convertTo(dst_yuv_image, CV_8UC3, 1.f);
  cvtColor(dst_resize_image, dst_yuv_image, CV_BGR2YUV_I420);

  // save to rgb
  for (int i = 0; i < param.bsize; i++) {
    dst_yuv_image.data = (unsigned char*) cpu_output + dst_img_size * i;
    SaveImg(dst_yuv_image, param.yuv_nv12, i, "dst_");
  }
}

void H2D(TestResizeParam param, std::vector<std::string> image_name, char** mlu_input, char** mlu_output,
         char** cpu_input, char** cpu_output) {
  int src_img_size = param.src_w * param.src_h * 3 / 2;
  int dst_img_size = param.dst_w * param.dst_h * 3 / 2;

  if (*cpu_input != nullptr) {
    free(*cpu_input);
  }
  if (*cpu_output != nullptr) {
    free(*cpu_output);
  }

  *cpu_input = reinterpret_cast<char*>(malloc(sizeof(char*) * src_img_size * param.bsize));
  *cpu_output = reinterpret_cast<char*>(malloc(sizeof(char*) * dst_img_size * param.bsize));
  memset(*cpu_output, 0, sizeof(char) * dst_img_size * param.bsize);

  for (int i = 0; i < param.bsize; i++) {
    Rgb2Yuv(image_name[i], param, *cpu_input + i * src_img_size, i);
  }

  *mlu_input = reinterpret_cast<char*>(mem_op.AllocMlu(sizeof(char) * src_img_size, param.bsize));
  *mlu_output = reinterpret_cast<char*>(mem_op.AllocMlu(sizeof(char) * dst_img_size, param.bsize));

  mem_op.MemcpyH2D(*mlu_input, *cpu_input, src_img_size, param.bsize);
}

// YUV 2 YUV reisze only
void RunResize(char* mlu_input, char* mlu_output, TestResizeParam param, std::vector<std::string> image_name,
               int batch_num, uint32_t channel_id, bool print_hardware_time, bool print_time) {
  cnrtNotifier_t eventBegin = nullptr;
  cnrtNotifier_t eventEnd = nullptr;
  float total_hardware_time = 0.0;

  std::chrono::time_point<std::chrono::steady_clock> start_time;
  std::chrono::time_point<std::chrono::steady_clock> end_time;

  edk::MluResize *resize = nullptr;

  int src_img_size = param.src_w * param.src_h * 3 / 2;

  // set context
  edk::MluContext context;
  context.SetDeviceId(0);
  context.SetChannelId(channel_id % 2);
  context.ConfigureForThisThread();

  // create notifier
  if (print_hardware_time) {
    if (CNRT_RET_SUCCESS != cnrtCreateNotifier(&eventBegin)) {
      std::cout << "cnrtCreateNotifier eventBegin failed" << std::endl;
    }
    if (CNRT_RET_SUCCESS != cnrtCreateNotifier(&eventEnd)) {
      std::cout << "cnrtCreateNotifier eventEnd failed" << std::endl;
    }
  }

  resize = new edk::MluResize();

  edk::MluResize::Attr attr;
  attr.src_h = param.src_h;
  attr.src_w = param.src_w;
  attr.dst_h = param.dst_h;
  attr.dst_w = param.dst_w;
  attr.batch_size = param.bsize;
  attr.core = param.core;
  attr.channel_id = channel_id;

  if (!resize->Init(attr)) {
    std::cout << "resize->Init() failed" << std::endl;;
  }

  // batching up
  void **src_y_mlu_in_cpu = reinterpret_cast<void**>(malloc(param.bsize * sizeof(char*)));
  void **src_uv_mlu_in_cpu = reinterpret_cast<void**>(malloc(param.bsize * sizeof(char*)));

  for (int cnt = 0; cnt < batch_num; cnt++) {
    for (int i = 0; i < param.bsize; i++) {
      src_y_mlu_in_cpu[i] = mlu_input + i * src_img_size;
      src_uv_mlu_in_cpu[i] = mlu_input + i * src_img_size + param.src_w * param.src_h;

      resize->BatchingUp(src_y_mlu_in_cpu[i], src_uv_mlu_in_cpu[i]);
    }

    if (print_hardware_time) {
      cnrtPlaceNotifier(eventBegin, resize->GetMluQueue()->queue);
    }

    start_time = std::chrono::steady_clock::now();

    // invoke kernel
    bool success = resize->SyncOneOutput(mlu_output);

    if (print_hardware_time) {
      cnrtPlaceNotifier(eventEnd, resize->GetMluQueue()->queue);
      // sync queue
      if (CNRT_RET_SUCCESS != cnrtSyncQueue(resize->GetMluQueue()->queue)) {
        std::cout << "cnrtSyncQueue failed" << std::endl;
      }
      cnrtNotifierDuration(eventBegin, eventEnd, &total_hardware_time);
    }

    end_time = std::chrono::steady_clock::now();

    std::chrono::duration<double, std::micro> diff = end_time - start_time;

    if (success) {
      if (print_time) {
        std::lock_guard<std::mutex> lock_g(print_mutex);
        std::cout << "--------------------software " << diff.count() <<"us ---------------- " << std::endl;
        if (print_hardware_time) {
          std::cout << "--------------------hardware " << total_hardware_time <<"us ---------------- " << std::endl;
        }
      }
    } else {
      ASSERT_TRUE(false) << "invoke resize kernel failed";
    }
  }

  // release resources
  if (resize) {
    resize->Destroy();
    delete resize;
  }
  if (print_hardware_time) {
    if (eventBegin) cnrtDestroyNotifier(&eventBegin);
    if (eventEnd) cnrtDestroyNotifier(&eventEnd);
  }
}

TEST(resize, resize) {
  edk::MluContext context;

  std::chrono::time_point<std::chrono::steady_clock> start_time;
  std::chrono::time_point<std::chrono::steady_clock> end_time;

  char *cpu_input = nullptr, *cpu_output = nullptr;
  std::vector<char*> mlu_outputs, mlu_inputs;

  int src_width = 1920;
  int src_height = 1080;
  int dst_width = 352;
  int dst_height = 288;
  int batch_size = 16;
  int core_num = 4;  // if u1 set core = 4, u2 set core = 8
  bool yuv_nv12 = false;

  bool print_hw_time = false;
  bool print_time = false;

  int batch_num = 2000;
  uint32_t thread_num = 2;

  std::vector<std::string> image_name;
  std::string image_0 = "0.jpg";
  std::string image_1 = "1.jpg";
  for (int i = 0; i < batch_size / 2 + 1; i++) {
    image_name.push_back(image_0);
    image_name.push_back(image_1);
  }

  TestResizeParam
     param = {src_width, src_height, dst_width, dst_height, batch_size, core_num, yuv_nv12};

  mlu_inputs.clear();
  mlu_outputs.clear();
  for (uint32_t th_i = 0; th_i < thread_num ; ++th_i) {
    // set context
    context.SetChannelId(th_i % 2);
    context.ConfigureForThisThread();
    mlu_inputs.push_back(nullptr);
    mlu_outputs.push_back(nullptr);
    H2D(param, image_name, &mlu_inputs[th_i], &mlu_outputs[th_i], &cpu_input, &cpu_output);
  }
  std::vector<std::thread> ths;

  start_time = end_time = std::chrono::steady_clock::now();

  for (uint32_t th_i = 0; th_i < thread_num ; ++th_i) {
    ths.push_back(std::thread(&RunResize, mlu_inputs[th_i], mlu_outputs[th_i], param, image_name, batch_num, th_i,
                              print_hw_time, print_time));
  }

  for (auto &it : ths) {
    if (it.joinable()) {
      it.join();
    }
  }

  end_time = std::chrono::steady_clock::now();

  std::chrono::duration<double, std::milli> diff = end_time - start_time;
  std::cout << "========================== U " << param.core / 4 << " =============================" << std::endl;
  std::cout << std::endl;
  std::cout << "****** bsize = " << param.bsize << " ****** " << thread_num << " threads ***** "
            << batch_num << " batch ******" << std::endl << std::endl;
  std::cout << "  src_h = " << param.src_h << " src_w = " << param.src_w << " dst_h = " << param.dst_h
            << " dst_w = " << param.dst_w << std::endl << std::endl;
  std::cout << "=================== total time " << diff.count() << "ms =====================" <<std::endl << std::endl;

  // uncomment to copy result to host and save image
  // D2H(cpu_output, mlu_outputs[0], param);

  for (uint32_t th_i = 0; th_i < thread_num ; ++th_i) {
    // set context
    context.SetChannelId(th_i % 2);
    context.ConfigureForThisThread();
    mem_op.FreeMlu(mlu_inputs[th_i]);
    mem_op.FreeMlu(mlu_outputs[th_i]);
  }
  if (cpu_input) free(cpu_input);
  if (cpu_output) free(cpu_output);
}
