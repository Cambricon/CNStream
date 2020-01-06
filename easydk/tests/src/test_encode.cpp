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

static std::mutex enc_mutex;
static std::condition_variable enc_cond;

static edk::EasyEncode *g_encoder = NULL;
static edk::PixelFmt input_pixel_format = PixelFmt::NON_FORMAT;
static FILE *p_output_file;
static uint32_t frame_count = 0;

#define VIDEO_ENCODE_FRAME_COUNT 100

#define TEST_1080P_JPG "../../tests/data/1080p.jpg"
#define TEST_500x500_JPG "../../tests/data/500x500.jpg"

static const char *pf_str(const PixelFmt &fmt) {
  switch (fmt) {
    case PixelFmt::NON_FORMAT:
      return "NON_FORMAT";
    case PixelFmt::YUV420SP_NV21:
      return "YUV420SP_NV21";
    case PixelFmt::YUV420SP_NV12:
      return "YUV420SP_NV12";
    case PixelFmt::BGR24:
      return "BGR24";
    case PixelFmt::RGB24:
      return "RGB24";
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

void eos_callback() {
  printf("eos_callback()\n");
  if (p_output_file) {
    fflush(p_output_file);
    fclose(p_output_file);
    p_output_file = NULL;
  }
  frames_output = 0;
  if (g_encoder->GetAttr().codec_type != CodecType::JPEG) {
    printf("encode video ok\n");
  } else {
    printf("encode jpeg ok\n");
  }
  std::unique_lock<std::mutex> lk(enc_mutex);
  enc_cond.notify_one();
}

void perf_callback(const edk::EncodePerfInfo &perf) {
#if 0
  std::cout << "----------- Encode Performance Info -----------" << std::endl;
  std::cout << "input transfer us: " << perf.input_transfer_us << "us" << std::endl;
  std::cout << "encode us: " << perf.encode_us << "us" << std::endl;
  std::cout << "transfer us: " << perf.transfer_us << "us" << std::endl;
  std::cout << "----------- END ------------" << std::endl;
#endif
}

void packet_callback(const edk::CnPacket &packet) {
  char *output_file = NULL;
  char str[256] = {0};
  size_t written;

  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    printf("set mlu env failed\n");
#ifdef CNSTK_MLU100
    g_encoder->ReleaseBuffer(packet.buf_id);
#endif
    return;
  }

  /* printf("packet_callback(data=%p,length=%ld,buf_id=%u,"
      "pts=%ld,codec_type=%s,frame_count=%u,frames_output=%u)\n",
      packet.data, packet.length, packet.buf_id, packet.pts,
      cc_str[packet.codec_type], frame_count, frames_output + 1); */

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

  uint8_t *buffer = NULL;
  uint32_t length = packet.length;

  buffer = reinterpret_cast<uint8_t *>(malloc(length));
  if (buffer == NULL) {
    printf("ERROR: malloc for output buffer failed\n");
    return;
  }

  g_encoder->CopyPacket(buffer, packet);

  written = fwrite(buffer, 1, length, p_output_file);
  if (written != length) {
    printf("ERROR: written size(%u) != data length(%u)\n", (unsigned int)written, length);
  }

  free(buffer);

#ifdef CNSTK_MLU100
  g_encoder->ReleaseBuffer(packet.buf_id);
#endif
}

bool SendData(edk::EasyEncode *encoder, PixelFmt pixel_format, CodecType codec_type, bool end,
              const std::string &image_path) {
  edk::CnFrame frame;
  uint8_t *p_data_buffer = NULL;
  cv::Mat cv_image;
  int width, height;
  unsigned int input_length;

  cv_image = cv::imread(image_path);
  if (cv_image.empty()) return false;

  width = cv_image.cols;
  height = cv_image.rows;

  memset(&frame, 0, sizeof(frame));

  if (pixel_format == PixelFmt::BGR24 || pixel_format == PixelFmt::RGB24) {
    if (pixel_format == PixelFmt::RGB24) cv::cvtColor(cv_image, cv_image, cv::COLOR_BGR2RGB);

    input_length = width * height * 3;
    frame.ptrs[0] = reinterpret_cast<void *>(cv_image.data);
    frame.frame_size = input_length;
    frame.width = width;
    frame.height = height;
  } else if (pixel_format == PixelFmt::YUV420SP_NV21 || pixel_format == PixelFmt::YUV420SP_NV12) {
    input_length = width * height * 3 / 2;
    p_data_buffer = reinterpret_cast<uint8_t *>(malloc(input_length));
    if (p_data_buffer == NULL) {
      printf("ERROR: malloc buffer for input file failed\n");
      return false;
    }

    cvt_bgr_to_yuv420sp(cv_image, 0, pixel_format == PixelFmt::YUV420SP_NV21, p_data_buffer);

    frame.ptrs[0] = reinterpret_cast<void *>(p_data_buffer);
    frame.frame_size = input_length;
    frame.width = width;
    frame.height = height;
  } else {
    printf("ERROR: Input pixel format(%d) invalid\n", static_cast<int>(pixel_format));
    return false;
  }

  input_pixel_format = pixel_format;

  uint32_t one_frame_size;
  if (pixel_format == PixelFmt::BGR24 || pixel_format == PixelFmt::RGB24) {
    one_frame_size = frame.width * frame.height * 3;
  } else if (pixel_format == PixelFmt::YUV420SP_NV21 || pixel_format == PixelFmt::YUV420SP_NV12) {
    one_frame_size = frame.width * frame.height * 3 / 2;
  } else {
    printf("ERROR: unsupported format\n");
    return false;
  }

  bool ret = true;
  uint8_t *frame_data = reinterpret_cast<uint8_t *>(frame.ptrs[0]);
  uint32_t frame_length = frame.frame_size;
  for (uint32_t i = 0; i < frame_length; i += one_frame_size) {
    frame.ptrs[0] = reinterpret_cast<void *>(frame_data + i);
    frame.frame_size = one_frame_size;
    frame.pts = frame_count;
    bool eos = false;
    if (end && (i + one_frame_size) >= frame_length) {
      printf("Set EOS flag to encoder\n");
      eos = true;
    }
    ret = encoder->SendData(frame, eos);
    frame_count++;
    if (ret == false) break;
  }

  if (p_data_buffer) free(p_data_buffer);

  return ret;
}

bool test_EasyEncode(const char *input_file, uint32_t w, uint32_t h, uint32_t dw, uint32_t dh, PixelFmt pixel_format,
                     CodecType codec_type) {
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
  attr.maximum_geometry.w = w;
  attr.maximum_geometry.h = h;
  attr.output_geometry.w = dw;
  attr.output_geometry.h = dh;
  attr.codec_type = codec_type;
  attr.pixel_format = pixel_format;
  attr.packet_callback = packet_callback;
  attr.eos_callback = eos_callback;
  attr.perf_callback = perf_callback;
  attr.color2gray = false;
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = 30;
  attr.rate_control.stat_time = 1;
  attr.rate_control.src_frame_rate_num = 30;
  attr.rate_control.src_frame_rate_den = 1;
  attr.rate_control.dst_frame_rate_num = 30;
  attr.rate_control.dst_frame_rate_den = 1;
  attr.rate_control.bit_rate = 1024;
  attr.rate_control.fluctuate_level = 0;
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.crop_config.enable = false;
  attr.silent = false;
  attr.output_on_cpu = false;
  attr.profile = edk::VideoProfile::MAIN;
  attr.jpeg_qfactor = 80;

  try {
    bool ret;

    g_encoder = edk::EasyEncode::Create(attr);
    edk::EasyEncode *encoder = g_encoder;
    if (codec_type == CodecType::H264 || codec_type == CodecType::H265) {
      // encode multi frames for video encoder
      for (int i = 0; i < VIDEO_ENCODE_FRAME_COUNT; i++) {
        bool end = i < (VIDEO_ENCODE_FRAME_COUNT - 1) ? false : true;
        ret = SendData(encoder, pixel_format, codec_type, end, input_path);
        if (ret != true) {
          printf("SendData failed\n");
          break;
        }
      }
    } else {
      ret = SendData(encoder, pixel_format, codec_type, true, input_path);
    }
    if (!ret) {
      std::cout << "Send data failed" << std::endl;
      return false;
    }

    std::unique_lock<std::mutex> lk(enc_mutex);
    enc_cond.wait(lk);
  } catch (edk::EasyEncodeError &err) {
    std::cout << err.what() << std::endl;
    return false;
  }

  if (g_encoder) {
    delete g_encoder;
    g_encoder = nullptr;
  }

  return true;
}

TEST(Codec, Encode) {
  bool ret = false;
#ifdef CNSTK_MLU100
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::YUV420SP_NV21, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::BGR24, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::RGB24, CodecType::JPEG);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::YUV420SP_NV21, CodecType::H264);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::BGR24, CodecType::H264);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::RGB24, CodecType::H264);
  EXPECT_TRUE(ret);

#elif CNSTK_MLU270
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::YUV420SP_NV12, CodecType::H264);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::YUV420SP_NV12, CodecType::H265);
  EXPECT_TRUE(ret);
  ret = test_EasyEncode(TEST_1080P_JPG, 1920, 1080, 1920, 1080, PixelFmt::YUV420SP_NV12, CodecType::JPEG);
  EXPECT_TRUE(ret);

#endif  // CNSTK_MLU100
}
