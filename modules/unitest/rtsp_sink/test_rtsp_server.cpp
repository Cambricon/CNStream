/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <mutex>
#include <string>
#include <thread>

#include "cnstream_logging.hpp"
#include "gtest/gtest.h"
#include "rtsp_server.hpp"
#include "test_base.hpp"
#include "video/circular_buffer.hpp"
#include "video/frame_rate_controller.hpp"

namespace cnstream {

static constexpr const char *test_file = "../../modules/unitest/source/data/img.mp4";

bool TestRtspServer(const std::string &file) {
  bool ret = true;
  AVFormatContext *fmt_ctx = nullptr;
  AVBitStreamFilterContext *bf_ctx = nullptr;
  AVPacket packet;
  int video_index = -1;
  bool first_frame = true;
  bool find_pts = true;  // set it to true by default!
  AVStream *vstream = nullptr;
  AVCodecID codec_id = AV_CODEC_ID_H264;
  int width = 0, height = 0;
  double frame_rate;
  FrameRateController *frc = nullptr;
  std::mutex mtx;
  video::CircularBuffer *buffer = new video::CircularBuffer();
  RtspServer::Param param;
  RtspServer *rtsp_server = nullptr;

  auto get_packet = [&](uint8_t *data, int size, double *timestamp, int *buffer_percent) -> int {
    int ret = -1;
    AVPacket packet;
    std::unique_lock<std::mutex> lk(mtx);
    if (buffer->Size() <= sizeof(packet)) return 0;
    buffer->Read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet), true);
    if (size < 0) {  // skip packet
      buffer->Read(nullptr, sizeof(packet) + packet.size);
      return packet.size;
    } else if (!data) {  // get packet size, timestamp
      ret = packet.size;
    } else {  // read out packet data
      if (size < packet.size) return -1;
      buffer->Read(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
      ret = buffer->Read(data, packet.size);
      if (ret > 0) {
        if (timestamp) *timestamp = packet.pts / 1000.0;
        if (buffer_percent) *buffer_percent = buffer->Size() * 100 / buffer->Capacity();
      }
    }
    return ret;
  };

  av_register_all();
  avformat_network_init();

  // format context
  fmt_ctx = avformat_alloc_context();
  // open input
  int ret_code = avformat_open_input(&fmt_ctx, file.c_str(), nullptr, nullptr);
  if (0 != ret_code) {
    LOGE(RTSP_SERVER_UNITTEST) << "Couldn't open input stream.";
    ret = false;
    goto end;
  }
  // find video stream information
  ret_code = avformat_find_stream_info(fmt_ctx, nullptr);
  if (ret_code < 0) {
    LOGE(RTSP_SERVER_UNITTEST) << "Couldn't find stream information.";
    ret = false;
    goto end;
  }
  video_index = -1;
  for (uint32_t loop_i = 0; loop_i < fmt_ctx->nb_streams; loop_i++) {
    vstream = fmt_ctx->streams[loop_i];
    AVMediaType media_type;
    media_type = vstream->codec->codec_type;
    if (media_type == AVMEDIA_TYPE_VIDEO) {
      video_index = loop_i;
      break;
    }
  }
  if (video_index == -1) {
    LOGE(RTSP_SERVER_UNITTEST) << "Didn't find a video stream.";
    ret = false;
    goto end;
  }

  codec_id = vstream->codec->codec_id;
  width = vstream->codec->width;
  height = vstream->codec->height;
  frame_rate = av_q2d(vstream->r_frame_rate);

  // bitstream filter
  bf_ctx = nullptr;
  if (strstr(fmt_ctx->iformat->name, "mp4") || strstr(fmt_ctx->iformat->name, "flv") ||
      strstr(fmt_ctx->iformat->name, "matroska")) {
    if (AV_CODEC_ID_H264 == codec_id) {
      bf_ctx = av_bitstream_filter_init("h264_mp4toannexb");
    } else if (AV_CODEC_ID_HEVC == codec_id) {
      bf_ctx = av_bitstream_filter_init("hevc_mp4toannexb");
    } else {
    }
  }

  param.port = 8554;
  param.authentication = false;
  param.width = width;
  param.height = height;
  param.bit_rate = vstream->codec->bit_rate;
  switch (codec_id) {
    case AV_CODEC_ID_H264:
      param.codec_type = RtspServer::CodecType::H264;
      break;
    case AV_CODEC_ID_HEVC:
      param.codec_type = RtspServer::CodecType::H265;
      break;
    case AV_CODEC_ID_MPEG4:
      param.codec_type = RtspServer::CodecType::MPEG4;
      break;
    default:
      LOGE(RTSP_SERVER_UNITTEST) << "Unsupported codec id: " << codec_id;
      goto end;
  }
  param.get_packet = get_packet;
  rtsp_server = new RtspServer(param);
  if (!rtsp_server) {
    ret = false;
    goto end;
  }
  if (!rtsp_server->Start()) {
    ret = false;
    goto end;
  }

  av_init_packet(&packet);
  packet.data = nullptr;
  packet.size = 0;

  LOGI(RTSP_SERVER_UNITTEST) << "Stream frame rate is " << frame_rate;
  frc = new FrameRateController(frame_rate);
  frc->Start();

  while (true) {
    if (av_read_frame(fmt_ctx, &packet) < 0) {
      LOGI(RTSP_SERVER_UNITTEST) << "Reach file end.";
      rtsp_server->OnEvent(RtspServer::Event::EVENT_EOS);
      break;
    }

    if (packet.stream_index != video_index) {
      av_packet_unref(&packet);
      continue;
    }

    AVStream *vstream = fmt_ctx->streams[video_index];
    if (first_frame) {
      if (packet.flags & AV_PKT_FLAG_KEY) {
        first_frame = false;
      } else {
        av_packet_unref(&packet);
        continue;
      }
    }

    if (bf_ctx) {
      av_bitstream_filter_filter(bf_ctx, vstream->codec, nullptr, &packet.data, &packet.size, packet.data, packet.size,
                                 0);
    }
    // find pts information
    if (AV_NOPTS_VALUE == packet.pts && find_pts) {
      find_pts = false;
      LOGW(RTSP_SERVER_UNITTEST) << "Didn't find pts informations, use ordered numbers instead. "
                                 << "stream url: " << file.c_str();
    } else if (AV_NOPTS_VALUE != packet.pts) {
      find_pts = true;
      packet.pts = av_rescale_q(packet.pts, vstream->time_base, {1, 1000});
      packet.dts = av_rescale_q(packet.dts, vstream->time_base, {1, 1000});
    }

    while (true) {
      std::unique_lock<std::mutex> lk(mtx);
      size_t free_size = buffer->Capacity() - buffer->Size();
      if (free_size <= packet.size + sizeof(AVPacket)) {
        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }

      buffer->Write(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
      buffer->Write(packet.data, packet.size);
      // LOGI(RTSP_SERVER_UNITTEST) << "Send packet, size=" << packet.size << ", pts=" << packet.pts << ", dts=" <<
      // packet.dts;
      break;
    }

    rtsp_server->OnEvent(RtspServer::Event::EVENT_DATA);

    if (bf_ctx) {
      av_freep(&packet.data);
    }
    av_packet_unref(&packet);

    frc->Control();
  }

end:
  if (frc) {
    delete frc;
    frc = nullptr;
  }
  if (rtsp_server) {
    rtsp_server->Stop();
    delete rtsp_server;
    rtsp_server = nullptr;
  }
  av_packet_unref(&packet);
  if (fmt_ctx) {
    avformat_close_input(&fmt_ctx);
    avformat_free_context(fmt_ctx);
    fmt_ctx = nullptr;
  }
  if (bf_ctx) {
    av_bitstream_filter_close(bf_ctx);
    bf_ctx = nullptr;
  }
  if (buffer) delete buffer;

  return ret;
}

TEST(RtspServer, Streamming) { EXPECT_TRUE(TestRtspServer(GetExePath() + test_file)); }

}  // namespace cnstream
