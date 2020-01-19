#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <opencv2/opencv.hpp>
#include <string>
#include <tuple>
#include <vector>

#include "easybang/resize_and_colorcvt.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "test_base.h"
#include "../src/easyinfer/mlu_task_queue.h"

static constexpr const char *jpg_1080p = "../../tests/data/1080p.jpg";
static constexpr const char *jpg_500x500 = "../../tests/data/500x500.jpg";

static std::map<edk::MluResizeConvertOp::ColorMode, const char*> color_mode_map = {
  {edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV12, "YUV2RGBA_NV12"},
  {edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV21, "YUV2RGBA_NV21"},
  {edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV12, "YUV2BGRA_NV12"},
  {edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV21, "YUV2BGRA_NV21"},
  {edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV12, "YUV2ARGB_NV12"},
  {edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV21, "YUV2ARGB_NV21"},
  {edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV12, "YUV2ABGR_NV12"},
  {edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV21, "YUV2ABGR_NV21"},
};

#define ALIGN(w, a) ((w + a - 1) & ~(a - 1))
#define ALIGN_16(w) (ALIGN(w, 16))
#define ALIGN_64(w) (ALIGN(w, 64))

class ResizeConvertParam : public testing::TestWithParam<std::tuple<const char*, edk::MluResizeConvertOp::ColorMode>> {};

static bool cvt_bgr_to_yuv420sp(const cv::Mat &bgr_image, uint32_t alignment, bool nv21, uint8_t *yuv_2planes_data) {
  cv::Mat yuv_i420_image;
  uint32_t width, height, stride;
  uint8_t *src_y, *src_u, *src_v, *dst_y, *dst_uv;

  cv::cvtColor(bgr_image, yuv_i420_image, cv::COLOR_BGR2YUV_I420);

  width = bgr_image.cols;
  height = bgr_image.rows;
  if (alignment > 0)
    stride = ALIGN(width, alignment);
  else
    stride = width;
  src_y = yuv_i420_image.data;
  src_u = yuv_i420_image.data + width * height;
  src_v = yuv_i420_image.data + width * height * 5 / 4;
  dst_y = yuv_2planes_data;
  dst_uv = yuv_2planes_data + stride * height;

  for (uint32_t i = 0; i < height; i++) {
    // y data
    memcpy(dst_y + i * stride, src_y + i * width, width);
    // uv data
    if (i % 2 == 0) {
      for (uint32_t j = 0; j < width / 2; j++) {
        if (nv21 == true) {
          *(dst_uv + i * stride / 2 + 2 * j) = *(src_v + i * width / 4 + j);
          *(dst_uv + i * stride / 2 + 2 * j + 1) = *(src_u + i * width / 4 + j);
        } else {
          *(dst_uv + i * stride / 2 + 2 * j) = *(src_u + i * width / 4 + j);
          *(dst_uv + i * stride / 2 + 2 * j + 1) = *(src_v + i * width / 4 + j);
        }
      }
    }
  }

  return true;
}

static bool compare_data(const uint8_t *bgr_data, const uint8_t *mlu_data, int width, int height, int stride,
                         edk::MluResizeConvertOp::ColorMode color_mode) {
  bool ret = true;
  float thres = 0.02;
  float diff = 0.0;
  float diffSum = 0.0;
  float max = 0.0;
  float mae = 0.0;
  float mse = 0.0;
  float ma = 0.0;
  float ms = 0.0;

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      for (int k = 0; k < 3; k++) {
        int rgbIdx = (((static_cast<int>(color_mode) - 1) / 2) == 0) * (2 - k) +
                     (((static_cast<int>(color_mode) - 1) / 2) == 1) * (k) +
                     (((static_cast<int>(color_mode) - 1) / 2) == 2) * (3 - k) +
                     (((static_cast<int>(color_mode) - 1) / 2) == 3) * (k + 1);

        diff = static_cast<float>(mlu_data[i * stride * 4 + j * 4 + rgbIdx]) -
               static_cast<float>(bgr_data[i * width * 3 + j * 3 + k]);

        ma += static_cast<float>(bgr_data[i * width * 3 + j * 3 + k]);
        ms += static_cast<float>(bgr_data[i * width * 3 + j * 3 + k]) *
              static_cast<float>(bgr_data[i * width * 3 + j * 3 + k]);

        if (std::abs(diff) > max) max = std::abs(diff);
        mae += std::abs(diff);
        diffSum += diff;
        mse += diff * diff;
      }
    }
  }

  mae /= ma;
  mse = std::sqrt(mse);
  ms = std::sqrt(ms);
  mse /= ms;

  if (mae > thres || mse > thres) {
    ret = false;
    std::cout << "FAILED!"
              << "mae:" << mae << " mse:" << mse << std::endl;
  } else {
    ret = true;
    std::cout << "PASSED!" << std::endl;
  }

  // std::cout << "mae: " << mae * 100 << "%, ";
  // std::cout << "mse: " << mse * 100 << "%" << std::endl;

  return ret;
}

//bool test_resize_colorcvt(const char *image_path, uint32_t dst_w, uint32_t dst_h, edk::MluResizeConvertOp::ColorMode cmode) {
TEST_P(ResizeConvertParam, Execute) {
  std::string path = GetExePath();
  auto params = GetParam();

  std::string image_path = path + std::get<0>(params);
  edk::MluResizeConvertOp::ColorMode cmode = std::get<1>(params);
  // uint32_t dst_w = std::get<1>(params);
  // uint32_t dst_h = std::get<2>(params);
  uint32_t dst_w = 300;
  uint32_t dst_h = 300;

  cv::Mat src_image, mlu_rc_image, cv_rc_image;
  uint32_t width, height, stride, dst_stride;
  uint32_t input_size, output_size;
  uint8_t *cpu_input = NULL, *cpu_output = NULL;
  edk::MluMemoryOp mem_op;
  void *mlu_input, *mlu_output;
  edk::MluResizeConvertOp *rc_op = NULL;
  cnrtQueue_t rc_queue;

  ASSERT_TRUE(color_mode_map.find(cmode) != color_mode_map.end()) << "invalid color mode";

  std::cout << "Convert " << color_mode_map[cmode] << std::endl;

  src_image = cv::imread(image_path);

  ASSERT_FALSE(src_image.empty()) << "read \"" << image_path << "\" failed";

  width = src_image.cols;
  height = src_image.rows;
  stride = ALIGN_64(width);
  input_size = stride * height * 3 / 2;
  dst_stride = dst_w;
  output_size = dst_stride * dst_h * 4;

  cpu_input = new uint8_t[input_size];
  cpu_output = new uint8_t[output_size];

  cvt_bgr_to_yuv420sp(src_image, 64, (static_cast<int>(cmode) % 2) == 0, cpu_input);

  edk::MluContext context;
  context.SetDeviceId(0);
  context.ConfigureForThisThread();

  if (CNRT_RET_SUCCESS != cnrtCreateQueue(&rc_queue)) {
    delete[] cpu_input;
    delete[] cpu_output;
    ASSERT_TRUE(false) << "cnrtCreateStream failed";
  }

  auto mlu_queue = std::make_shared<edk::MluTaskQueue>();
  mlu_queue->queue = rc_queue;

  rc_op = new edk::MluResizeConvertOp();
  rc_op->SetMluQueue(mlu_queue);

  edk::MluResizeConvertOp::Attr attr;
  attr.src_h = height;
  attr.src_w = width;
  attr.src_stride = stride;
  attr.dst_h = dst_h;
  attr.dst_w = dst_w;
  attr.data_mode = edk::MluResizeConvertOp::DataMode::UINT8ToUINT8;
  attr.color_mode = cmode;
  attr.core_version = context.GetCoreVersion();

  if (!rc_op->Init(attr)) {
    rc_op->Destroy();
    delete[] cpu_input;
    delete[] cpu_output;
    delete rc_op;
    ASSERT_TRUE(false) << "rc_op->Init() failed";
  }

  mlu_input = mem_op.AllocMlu(input_size, 1);
  mlu_output = mem_op.AllocMlu(output_size, 1);

  mem_op.MemcpyH2D(mlu_input, cpu_input, input_size, 1);

  void *src_y = mlu_input;
  auto src_uv = reinterpret_cast<uint8_t *>(mlu_input) + stride * height;
  rc_op->InvokeOp(mlu_output, src_y, src_uv);
  cnrtSyncQueue(rc_queue);

  mem_op.MemcpyD2H(cpu_output, mlu_output, output_size, 1);

  cv::resize(src_image, cv_rc_image, cv::Size(dst_w, dst_h));

  // compare
  EXPECT_TRUE(compare_data(cv_rc_image.data, cpu_output, dst_w, dst_h, dst_stride, attr.color_mode));

  // release resources
  mem_op.FreeMlu(mlu_input);
  mem_op.FreeMlu(mlu_output);
  rc_op->Destroy();
  delete[] cpu_input;
  delete[] cpu_output;
  delete rc_op;
}

INSTANTIATE_TEST_CASE_P(
        Bang,
        ResizeConvertParam,
        testing::Combine(testing::Values(jpg_1080p, jpg_500x500),
                         testing::Values(edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV12,
                                         edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV21,
                                         edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV12,
                                         edk::MluResizeConvertOp::ColorMode::YUV2BGRA_NV21,
                                         edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV12,
                                         edk::MluResizeConvertOp::ColorMode::YUV2ARGB_NV21,
                                         edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV12,
                                         edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV21)));
