#include <gtest/gtest.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include "easycodec/easy_encode.h"
#include "easyinfer/mlu_context.h"
#include "test_base.h"

using edk::PixelFmt;
using edk::CodecType;
using edk::VideoProfile;

static std::mutex enc_mutex;
static std::condition_variable enc_cond;

static edk::EasyEncode *g_encoder = NULL;
static edk::PixelFmt input_pixel_format = PixelFmt::NV12;
static FILE *p_output_file;
static uint32_t frame_count = 0;

#define VIDEO_ENCODE_FRAME_COUNT 100

#define TEST_1080P_JPG "../../tests/data/1080p.jpg"
#define TEST_500x500_JPG "../../tests/data/500x500.jpg"

static const char *pf_str(const PixelFmt &fmt) {
  switch (fmt) {
    case PixelFmt::NV21:
      return "NV21";
    case PixelFmt::NV12:
      return "NV12";
    case PixelFmt::I420:
      return "I420";
    default:
      return "UnknownType";
  }
}

static const char *cc_str(const CodecType &mode) {
  switch (mode) {
    case CodecType::MPEG4:
      return "MPEG4";
    case CodecType::H264:
      return "H264";
    case CodecType::H265:
      return "H265";
    case CodecType::JPEG:
      return "JPEG";
    case CodecType::MJPEG:
      return "MJPEG";
    default:
      return "UnknownType";
  }
}

static int frames_output = 0;

#define ALIGN(w, a) ((w + a - 1) & ~(a - 1))

static bool cvt_bgr_to_yuv420sp(const cv::Mat &bgr_image, uint32_t alignment, edk::PixelFmt pixel_fmt, uint8_t *yuv_2planes_data) {
  cv::Mat yuv_i420_image;
  uint32_t width, height, stride;
  uint8_t *src_y, *src_u, *src_v, *dst_y, *dst_u, *dst_v;

  cv::cvtColor(bgr_image, yuv_i420_image, cv::COLOR_BGR2YUV_I420);

  width = bgr_image.cols;
  height = bgr_image.rows;
  if (alignment > 0)
    stride = ALIGN(width, alignment);
  else
    stride = width;

  uint32_t y_len = width * height;
  src_y = yuv_i420_image.data;
  src_u = yuv_i420_image.data + y_len;
  src_v = yuv_i420_image.data + y_len * 5 / 4;
  dst_y = yuv_2planes_data;
  dst_u = yuv_2planes_data + stride * height;
  dst_v = yuv_2planes_data + stride * height * 5 / 4;

  for (uint32_t i = 0; i < height; i++) {
    // y data
    memcpy(dst_y + i * stride, src_y + i * width, width);
    // uv data
    if (i % 2 == 0) {
      if (pixel_fmt == edk::PixelFmt::I420) {
        memcpy(dst_u + i * stride / 4, src_u + i * width / 4, width / 2);
        memcpy(dst_v + i * stride / 4, src_v + i * width / 4, width / 2);
        continue;
      }
      for (uint32_t j = 0; j < width / 2; j++) {
        if (pixel_fmt == edk::PixelFmt::NV21) {
          *(dst_u + i * stride / 2 + 2 * j) = *(src_v + i * width / 4 + j);
          *(dst_u + i * stride / 2 + 2 * j + 1) = *(src_u + i * width / 4 + j);
        } else {
          *(dst_u + i * stride / 2 + 2 * j) = *(src_u + i * width / 4 + j);
          *(dst_u + i * stride / 2 + 2 * j + 1) = *(src_v + i * width / 4 + j);
        }
      }
    }
  }

  return true;
}

void eos_callback() {
  printf("eos_callback()\n");
  if (p_output_file) {
    fflush(p_output_file);
    fclose(p_output_file);
    p_output_file = NULL;
  }
  frames_output = 0;
  if (g_encoder->GetAttr().codec_type != CodecType::JPEG) {
    printf("encode video pass\n");
  } else {
    printf("encode jpeg pass\n");
  }
  std::unique_lock<std::mutex> lk(enc_mutex);
  enc_cond.notify_one();
}

void packet_callback(const edk::CnPacket &packet) {
  char *output_file = NULL;
  char str[256] = {0};
  size_t written;

  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    printf("set mlu env failed\n");
    return;
  }

  if (packet.codec_type == CodecType::JPEG) {
    snprintf(str, sizeof(str), "./encoded_%s_%02d.jpg", pf_str(input_pixel_format), frames_output);
    output_file = str;
  } else if (packet.codec_type == CodecType::H264) {
    snprintf(str, sizeof(str), "./encoded_%s.h264", pf_str(input_pixel_format));
    output_file = str;
  } else if (packet.codec_type == CodecType::H265) {
    snprintf(str, sizeof(str), "./encoded_%s.h265", pf_str(input_pixel_format));
    output_file = str;
  } else {
    printf("ERROR: unknown output codec type <%d>\n", static_cast<int>(packet.codec_type));
  }

  if (p_output_file == NULL) p_output_file = fopen(output_file, "wb");
  if (p_output_file == NULL) {
    printf("ERROR: open output file failed\n");
  }

  frames_output++;
  uint32_t length = packet.length;

  written = fwrite(packet.data, 1, length, p_output_file);
  if (written != length) {
    printf("ERROR: written size(%u) != data length(%u)\n", (unsigned int)written, length);
  }
}

bool SendData(edk::EasyEncode *encoder, PixelFmt pixel_format, CodecType codec_type, bool end,
              const std::string &image_path) {
  edk::CnFrame frame;
  uint8_t *p_data_buffer = NULL;
  cv::Mat cv_image;
  int width, height;
  unsigned int input_length;

  cv_image = cv::imread(image_path);
  if (cv_image.empty()) {
    std::cerr << "Invalid image, image path" << image_path << std::endl;
    return false;
  }

  width = cv_image.cols;
  height = cv_image.rows;

  memset(&frame, 0, sizeof(frame));

  uint32_t align = 0;

  if (pixel_format == PixelFmt::NV21 || pixel_format == PixelFmt::NV12 || pixel_format == PixelFmt::I420) {
    input_length = width * height * 3 / 2;
    p_data_buffer = new uint8_t[input_length];
    if (p_data_buffer == NULL) {
      printf("ERROR: malloc buffer for input file failed\n");
      return false;
    }

    cvt_bgr_to_yuv420sp(cv_image, align, pixel_format, p_data_buffer);

    frame.ptrs[0] = reinterpret_cast<void *>(p_data_buffer);
    frame.ptrs[1] = reinterpret_cast<void *>(p_data_buffer + width * height);
    frame.n_planes = 2;
    if (pixel_format == PixelFmt::I420) {
      frame.n_planes = 3;
      frame.ptrs[2] = reinterpret_cast<void *>(p_data_buffer + width * height * 5 / 4);
    }
    frame.frame_size = input_length;
    frame.width = width;
    frame.height = height;
    frame.pts = frame_count++;
  } else {
    printf("ERROR: Input pixel format(%d) invalid\n", static_cast<int>(pixel_format));
    return false;
  }

  input_pixel_format = pixel_format;

  bool ret = true;
  bool eos = false;
  if (end) {
    printf("Set EOS flag to encoder\n");
    eos = true;
  }
  ret = encoder->SendDataCPU(frame, eos);

  if (p_data_buffer) delete[] p_data_buffer;

  return ret;
}

bool test_EasyEncode(const char *input_file, uint32_t w, uint32_t h, PixelFmt pixel_format, CodecType codec_type) {
  printf("\nTesting encode %s image to %s\n", pf_str(pixel_format), cc_str(codec_type));

  p_output_file = NULL;
  frame_count = 0;
  frames_output = 0;
  std::string input_path = GetExePath() + input_file;

  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    printf("set mlu env failed\n");
    return false;
  }

  edk::EasyEncode::Attr attr;
  attr.dev_id = 0;
  attr.frame_geometry.w = w;
  attr.frame_geometry.h = h;
  attr.codec_type = codec_type;
  attr.pixel_format = pixel_format;
  attr.packet_callback = packet_callback;
  attr.eos_callback = eos_callback;
  attr.input_buffer_num = 4;
  attr.output_buffer_num = 4;
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = 20;
  attr.rate_control.src_frame_rate_num = 30;
  attr.rate_control.src_frame_rate_den = 1;
  attr.rate_control.bit_rate = 1024;
  attr.rate_control.max_bit_rate = 1024;
  attr.silent = false;
  attr.jpeg_qfactor = 50;
  // attr.max_mb_per_slice = 1;
  // attr.ir_count = 5;
  switch(codec_type) {
    case CodecType::H264: attr.profile = VideoProfile::H264_MAIN; break;
    case CodecType::H265: attr.profile = VideoProfile::H265_MAIN; break;
    default: break;
  }

  try {
    bool ret = false;
    g_encoder = edk::EasyEncode::Create(attr);
    if (!g_encoder) throw edk::EasyEncodeError("Create EasyEncode failed");
    edk::EasyEncode *encoder = g_encoder;
    if (codec_type == CodecType::H264 || codec_type == CodecType::H265 || codec_type == CodecType::JPEG) {
      // encode multi frames for video encoder
      for (int i = 0; i < VIDEO_ENCODE_FRAME_COUNT; i++) {
        bool end = i < (VIDEO_ENCODE_FRAME_COUNT - 1) ? false : true;
        ret = SendData(encoder, pixel_format, codec_type, end, input_path);
        if (!ret) {
          break;
        }
      }
    } else {
      throw edk::EasyEncodeError("Unsupport format");
    }
    if (!ret) {
      throw edk::EasyEncodeError("Send data failed");
    }

    std::unique_lock<std::mutex> lk(enc_mutex);
    enc_cond.wait(lk);
  } catch (edk::EasyEncodeError &err) {
    std::cerr << err.what() << std::endl;
    if (g_encoder) {
      delete g_encoder;
      g_encoder = nullptr;
    }
    return false;
  }

  if (g_encoder) {
    delete g_encoder;
    g_encoder = nullptr;
  }

  return true;
}

TEST(Codec, EncodeVideo) {
  bool ret = false;
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::NV12, CodecType::H264);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::NV21, CodecType::H264);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::I420, CodecType::H264);
  EXPECT_TRUE(ret);

  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::NV12, CodecType::H265);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::NV21, CodecType::H265);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::I420, CodecType::H265);
  EXPECT_TRUE(ret);


  ret = test_EasyEncode(TEST_500x500_JPG, 500, 500, PixelFmt::NV21, CodecType::H264);
  EXPECT_TRUE(ret);
}

TEST(Codec, EncodeJpeg) {
  bool ret = false;
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::NV21, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::NV12, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_500x500_JPG, 500, 500, PixelFmt::NV21, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_500x500_JPG, 500, 500, PixelFmt::NV12, CodecType::JPEG);
  EXPECT_TRUE(ret);

  // FIXME: BUG, create encode failed, error code 999
  #if 0
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, PixelFmt::I420, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_500x500_JPG, 500, 500, PixelFmt::I420, CodecType::JPEG);
  EXPECT_TRUE(ret);
  #endif
}
