#include <gtest/gtest.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include "easycodec/easy_decode.h"
#include "easyinfer/mlu_context.h"
#include "test_base.h"

std::mutex mut;
std::condition_variable cond_main, cond_sub;
bool main_rec = false, sub_rec = false;
const char* jpeg_file = "../../tests/data/1080p.jpg";
const char* h264_file = "../../tests/data/1080p.h264";
char* test_file = NULL;
FILE* p_big_stream = NULL;
FILE* p_small_stream = NULL;

edk::EasyDecode* g_decode;
static uint8_t* g_data_buffer;

#ifndef MAX_INPUT_DATA_SIZE
#define MAX_INPUT_DATA_SIZE (25 << 20)
#endif

void smallstream_callback(bool* condv, std::condition_variable* cond, const edk::CnFrame& frame) {
  std::cout << "smallstream_callback(" << frame.frame_size << ")" << std::endl;
  EXPECT_EQ(static_cast<uint32_t>(300), frame.height);
  EXPECT_EQ(static_cast<uint32_t>(320), frame.width);

  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError& err) {
    std::cout << "set mlu env failed" << std::endl;
    g_decode->ReleaseBuffer(frame.buf_id);
    return;
  }

  if (p_small_stream == NULL) {
    p_small_stream = fopen("small.yuv", "wb");
    if (p_small_stream == NULL) {
      std::cout << "open small.yuv failed" << std::endl;
      g_decode->ReleaseBuffer(frame.buf_id);
      return;
    }
  }

  uint8_t* buffer = NULL;
  uint32_t length = frame.frame_size;
  size_t written;

  if (length == 0) {
    g_decode->ReleaseBuffer(frame.buf_id);
    return;
  }

  buffer = reinterpret_cast<uint8_t*>(malloc(length));
  if (buffer == NULL) {
    std::cout << ("ERROR: malloc for small buffer failed\n");
    g_decode->ReleaseBuffer(frame.buf_id);
    return;
  }

  g_decode->CopyFrame(buffer, frame);

  written = fwrite(buffer, 1, length, p_small_stream);
  if (written != length) {
    printf("ERROR: small written size(%u) != data length(%u)\n", (unsigned int)written, length);
  }

  free(buffer);

  g_decode->ReleaseBuffer(frame.buf_id);

#if 0
  if (*condv == false) {
    std::unique_lock<std::mutex> lk(mut);
    *condv = true;
    cond->notify_one();
  }
#endif
}

void bigstream_callback(bool* condv, std::condition_variable* cond, const edk::CnFrame& frame) {
  std::cout << "bigstream_callback(" << frame.frame_size << ")" << std::endl;
  EXPECT_EQ(static_cast<uint32_t>(1080), frame.height);
  EXPECT_EQ(static_cast<uint32_t>(1920), frame.width);

  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError& err) {
    std::cout << "set mlu env failed" << std::endl;
    g_decode->ReleaseBuffer(frame.buf_id);
    return;
  }

  if (p_big_stream == NULL) {
    p_big_stream = fopen("big.yuv", "wb");
    if (p_big_stream == NULL) {
      std::cout << "open big.yuv failed" << std::endl;
      g_decode->ReleaseBuffer(frame.buf_id);
      return;
    }
  }

  uint8_t* buffer = NULL;
  uint32_t length = frame.frame_size;
  size_t written;

  if (length == 0) {
    g_decode->ReleaseBuffer(frame.buf_id);
    return;
  }

  buffer = reinterpret_cast<uint8_t*>(malloc(length));
  if (buffer == NULL) {
    std::cout << ("ERROR: malloc for big buffer failed\n");
    g_decode->ReleaseBuffer(frame.buf_id);
    return;
  }

  g_decode->CopyFrame(buffer, frame);

  written = fwrite(buffer, 1, length, p_big_stream);
  if (written != length) {
    printf("ERROR: big written size(%u) != data length(%u)\n", (unsigned int)written, length);
  }

  free(buffer);

  g_decode->ReleaseBuffer(frame.buf_id);

#if 0
  if (*condv == false) {
    std::unique_lock<std::mutex> lk(mut);
    *condv = true;
    cond->notify_one();
  }
#endif
}

void eos_callback(bool* condv, std::condition_variable* cond) {
  std::cout << "eos_callback" << std::endl;

  if (p_big_stream) {
    fflush(p_big_stream);
    fclose(p_big_stream);
    p_big_stream = NULL;
  }
  if (p_small_stream) {
    fflush(p_small_stream);
    fclose(p_small_stream);
    p_small_stream = NULL;
  }

  std::unique_lock<std::mutex> lk(mut);
  *condv = true;
  cond->notify_one();
}

void perf_callback(const edk::DecodePerfInfo& perf) {
#if 0
  std::cout << "----------- Decode Performance Info -----------" << std::endl;
  std::cout << "total us: " << perf.total_us << "us" << std::endl;
  std::cout << "decode us: " << perf.decode_us << "us" << std::endl;
  std::cout << "transfer us: " << perf.transfer_us << "us" << std::endl;
  std::cout << "----------- END ------------" << std::endl;
#endif
}

bool SendData(edk::EasyDecode* decode) {
  edk::CnPacket packet;
  FILE* fid;

  if (test_file == NULL) {
    std::cout << "test_file == NULL" << std::endl;
    return false;
  }

  std::string test_path = GetExePath() + test_file;
  fid = fopen(test_path.c_str(), "rb");
  if (fid == NULL) {
    return false;
  }
  fseek(fid, 0, SEEK_END);
  int64_t file_len = ftell(fid);
  rewind(fid);
  if ((file_len == 0) || (file_len > MAX_INPUT_DATA_SIZE)) {
    fclose(fid);
    return false;
  }
  memset(&packet, 0, sizeof(edk::CnPacket));
  packet.length = fread(g_data_buffer, 1, MAX_INPUT_DATA_SIZE, fid);
  fclose(fid);
  packet.data = g_data_buffer;
  packet.pts = 0;
  return decode->SendData(packet, true);
}

bool test_decode(edk::CodecType ctype, edk::PixelFmt pf, uint32_t mainstream_w, uint32_t mainstream_h,
                 uint32_t substream_w, uint32_t substream_h, std::function<void(const edk::CnFrame&)> m_cb,
                 std::function<void(const edk::CnFrame&)> s_cb) {
  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError& err) {
    std::cout << "set mlu env failed" << std::endl;
    return false;
  }

  if (ctype == edk::CodecType::H264) {
    test_file = const_cast<char*>(h264_file);
  } else if (ctype == edk::CodecType::JPEG) {
    test_file = const_cast<char*>(jpeg_file);
  } else {
    std::cout << "unknown codec type" << std::endl;
    return false;
  }

  main_rec = false;
  sub_rec = false;
  edk::EasyDecode::Attr attr;
  attr.drop_rate = 0;
  attr.maximum_geometry.w = 1920;
  attr.maximum_geometry.h = 1080;
  attr.output_geometry.w = mainstream_w;
  attr.output_geometry.h = mainstream_h;
  attr.substream_geometry.w = substream_w;
  attr.substream_geometry.h = substream_h;
  attr.codec_type = ctype;
  attr.video_mode = edk::VideoMode::FRAME_MODE;
  attr.pixel_format = pf;
  attr.frame_callback = m_cb;
  attr.substream_callback = s_cb;
  attr.perf_callback = perf_callback;
  attr.eos_callback =
      std::bind(eos_callback, (NULL != s_cb) ? (&sub_rec) : (&main_rec), (NULL != s_cb) ? (&cond_sub) : (&cond_main));
  attr.silent = false;
  edk::EasyDecode* decode = nullptr;
  try {
    bool ret;
    decode = edk::EasyDecode::Create(attr);
    g_decode = decode;
    decode->Pause();
    decode->Resume();
    ret = SendData(g_decode);
    if (!ret) {
      std::cout << "Send Data failed" << std::endl;
      delete decode;
      return false;
    }
    std::unique_lock<std::mutex> lk(mut);
    if (NULL != s_cb) {
      // substream is opened, wait for substream receive is ok.
      cond_sub.wait(lk, []() -> bool { return sub_rec; });
    } else if (NULL != m_cb) {
      // substream is not open but main stream callback is set,
      // wait for main stream receive is ok.
      cond_main.wait(lk, []() -> bool { return main_rec; });
    }
    delete decode;
    decode = nullptr;
    g_decode = nullptr;
  } catch (edk::EasyDecodeError& err) {
    std::cout << err.what() << std::endl;
    if (nullptr != decode) {
      delete decode;
      g_decode = nullptr;
    }
    return false;
  }
  return true;
}

TEST(Codec, Decode) {
  bool ret = false;
  g_data_buffer = new uint8_t[MAX_INPUT_DATA_SIZE];
#ifdef CNSTK_MLU100
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::BGR24, 320, 300, 0, 0,
                    std::bind(smallstream_callback, &main_rec, &cond_main, std::placeholders::_1), NULL);
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::RGB24, 320, 300, 0, 0,
                    std::bind(smallstream_callback, &main_rec, &cond_main, std::placeholders::_1), NULL);
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::YUV420SP_NV21, 1920, 1080, 320, 300,
                    std::bind(bigstream_callback, &main_rec, &cond_main, std::placeholders::_1),
                    std::bind(smallstream_callback, &sub_rec, &cond_sub, std::placeholders::_1));
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::YUV420SP_NV21, 1920, 1080, 320, 300, NULL,
                    std::bind(smallstream_callback, &sub_rec, &cond_sub, std::placeholders::_1));
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::YUV420SP_NV21, 1920, 1080, 3840, 2160,
                    std::bind(bigstream_callback, &main_rec, &cond_main, std::placeholders::_1),
                    std::bind(smallstream_callback, &sub_rec, &cond_sub, std::placeholders::_1));
  EXPECT_FALSE(ret);
#endif  // CNSTK_MLU100
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::YUV420SP_NV21, 320, 300, 0, 0,
                    std::bind(smallstream_callback, &main_rec, &cond_main, std::placeholders::_1), NULL);
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::NON_FORMAT, 320, 300, 0, 0,
                    std::bind(smallstream_callback, &main_rec, &cond_main, std::placeholders::_1), NULL);
  EXPECT_FALSE(ret);
  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::YUV420SP_NV21, 1920, 1080, 0, 0,
                    std::bind(bigstream_callback, &main_rec, &cond_main, std::placeholders::_1), NULL);
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::JPEG, edk::PixelFmt::YUV420SP_NV21, 1920, 1080, 0, 0,
                    std::bind(bigstream_callback, &main_rec, &cond_main, std::placeholders::_1), NULL);
  EXPECT_TRUE(ret);
  delete[] g_data_buffer;
}
