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
std::condition_variable cond;
bool rec = false;
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

void bigstream_callback(bool* condv, std::condition_variable* cond, const edk::CnFrame& frame) {
  std::cout << "bigstream_callback(" << frame.frame_size << ")" << std::endl;
  EXPECT_EQ(static_cast<uint32_t>(1080), frame.height);
  EXPECT_EQ(static_cast<uint32_t>(1920), frame.width);

  try {
    edk::MluContext context;
    context.SetDeviceId(0);
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

  g_decode->CopyFrameD2H(buffer, frame);

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
  try {
    edk::MluContext context;
    context.SetDeviceId(0);
    context.ConfigureForThisThread();
  } catch (edk::MluContextError &err) {
    printf("set mlu env failed\n");
    return;
  }
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

#if 0
void perf_callback(const edk::DecodePerfInfo& perf) {
  std::cout << "----------- Decode Performance Info -----------" << std::endl;
  std::cout << "total us: " << perf.total_us << "us" << std::endl;
  std::cout << "decode us: " << perf.decode_us << "us" << std::endl;
  std::cout << "transfer us: " << perf.transfer_us << "us" << std::endl;
  std::cout << "----------- END ------------" << std::endl;
}
#endif

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

bool test_decode(edk::CodecType ctype, edk::PixelFmt pf, uint32_t frame_w, uint32_t frame_h,
                 std::function<void(const edk::CnFrame&)> frame_cb) {
  try {
    edk::MluContext context;
    context.SetDeviceId(0);
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

  rec = false;
  edk::EasyDecode::Attr attr;
  attr.frame_geometry.w = 1920;
  attr.frame_geometry.h = 1080;
  attr.codec_type = ctype;
  attr.buf_strategy = edk::BufferStrategy::CNCODEC;
  /* attr.video_mode = edk::VideoMode::FRAME_MODE; */
  attr.pixel_format = pf;
  attr.frame_callback = frame_cb;
  attr.eos_callback =
      std::bind(eos_callback, &rec, &cond);
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
    if (NULL != frame_cb) {
      // substream is not open but main stream callback is set,
      // wait for main stream receive is ok.
      cond.wait(lk, []() -> bool { return rec; });
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

TEST(Codec, DecodeH264) {
  bool ret = false;
  g_data_buffer = new uint8_t[MAX_INPUT_DATA_SIZE];

  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::NV21, 1920, 1080,
                    std::bind(bigstream_callback, &rec, &cond, std::placeholders::_1));
  EXPECT_TRUE(ret);

  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::NV12, 1920, 1080,
                    std::bind(bigstream_callback, &rec, &cond, std::placeholders::_1));
  EXPECT_TRUE(ret);

  ret = test_decode(edk::CodecType::H264, edk::PixelFmt::I420, 1920, 1080,
                    std::bind(bigstream_callback, &rec, &cond, std::placeholders::_1));
  EXPECT_TRUE(ret);
  delete[] g_data_buffer;
}

TEST(Codec, DecodeJpeg) {
  bool ret = false;
  g_data_buffer = new uint8_t[MAX_INPUT_DATA_SIZE];

  ret = test_decode(edk::CodecType::JPEG, edk::PixelFmt::NV21, 1920, 1080,
                    std::bind(bigstream_callback, &rec, &cond, std::placeholders::_1));
  EXPECT_TRUE(ret);
  ret = test_decode(edk::CodecType::JPEG, edk::PixelFmt::NV12, 1920, 1080,
                    std::bind(bigstream_callback, &rec, &cond, std::placeholders::_1));
  EXPECT_TRUE(ret);
  delete[] g_data_buffer;
}
