#include "src/FFmpegVideoEncoder.h"

#include <string.h>
#include <iostream>

#define INPUT_QUEUE_SIZE 0
#define OUTPUT_BUFFER_SIZE 0x200000

FFmpegVideoEncoder::FFmpegVideoFrame::FFmpegVideoFrame(FFmpegVideoEncoder *encoder) : encoder_(encoder) {
  frame_ = av_frame_alloc();
  frame_->width = encoder_->avcodec_ctx_->width;
  frame_->height = encoder_->avcodec_ctx_->height;
  frame_->format = encoder_->picture_format_;
  int align = (encoder_->picture_format_ == AV_PIX_FMT_RGB24 || encoder_->picture_format_ == AV_PIX_FMT_BGR24) ? 24 : 8;
  av_image_alloc(frame_->data, frame_->linesize, frame_->width, frame_->height, (AVPixelFormat)frame_->format, align);
}

FFmpegVideoEncoder::FFmpegVideoFrame::~FFmpegVideoFrame() {
  if (frame_) {
    av_freep(&(frame_->data[0]));
    av_frame_unref(frame_);
    av_free(frame_);
  }
}

void FFmpegVideoEncoder::FFmpegVideoFrame::Fill(uint8_t *data, int64_t timestamp) {
  if (frame_ == nullptr) return;

  frame_->pts = timestamp;

  int linesize;
  if (frame_->format == AV_PIX_FMT_RGB24 || frame_->format == AV_PIX_FMT_BGR24) {
    linesize = frame_->width * 3;
    for (int i = 0; i < frame_->height; i++) {
      memcpy(frame_->data[0] + frame_->linesize[0] * i, data + linesize * i, linesize);
    }
  } else if (frame_->format == AV_PIX_FMT_YUV420P) {
    linesize = frame_->width;
    int size = frame_->height * frame_->width;
    for (int i = 0; i < frame_->height; i++) {
      memcpy(frame_->data[0] + frame_->linesize[0] * i, data + linesize * i, linesize);
    }
    for (int i = 0; i < frame_->height / 2; i++) {
      memcpy(frame_->data[1] + frame_->linesize[1] * i, data + size + linesize / 2 * i, linesize / 2);
    }
    for (int i = 0; i < frame_->height / 2; i++) {
      memcpy(frame_->data[2] + frame_->linesize[2] * i, data + size * 5 / 4 + linesize / 2 * i, linesize / 2);
    }
  } else if (frame_->format == AV_PIX_FMT_NV21 || frame_->format == AV_PIX_FMT_NV12) {
    linesize = frame_->width;
    int size = frame_->height * frame_->width;
    for (int i = 0; i < frame_->height; i++) {
      memcpy(frame_->data[0] + frame_->linesize[0] * i, data + linesize * i, linesize);
    }
    for (int i = 0; i < frame_->height / 2; i++) {
      memcpy(frame_->data[1] + frame_->linesize[1] * i, data + size + linesize * i, linesize);
    }
  } else {
    std::cout << "unsupport pixel format: " << frame_->format << std::endl;
  }
}

FFmpegVideoEncoder::FFmpegVideoEncoder(uint32_t width, uint32_t height, PictureFormat format, CodecType type,
                                       float frame_rate, uint32_t gop_size, uint32_t bit_rate)
    : VideoEncoder(INPUT_QUEUE_SIZE, OUTPUT_BUFFER_SIZE) {
  int ret = 0;

  picture_width_ = width;
  picture_height_ = height;
  switch (format) {
    case YUV420P:
      picture_format_ = AV_PIX_FMT_YUV420P;
      break;
    case RGB24:
      picture_format_ = AV_PIX_FMT_RGB24;
      break;
    case BGR24:
      picture_format_ = AV_PIX_FMT_BGR24;
      break;
    case NV21:
      picture_format_ = AV_PIX_FMT_NV21;
      break;
    case NV12:
      picture_format_ = AV_PIX_FMT_NV12;
      break;
    default:
      picture_format_ = AV_PIX_FMT_YUV420P;
      break;
  }
  switch (type) {
    case H264:
      avcodec_id_ = AV_CODEC_ID_H264;
      break;
    case HEVC:
      avcodec_id_ = AV_CODEC_ID_HEVC;
      break;
    case MPEG4:
      avcodec_id_ = AV_CODEC_ID_MPEG4;
      break;
    default:
      avcodec_id_ = AV_CODEC_ID_H264;
      break;
  }
  if (frame_rate > 0) {
    frame_rate_ = av_d2q(frame_rate, 60000);
  } else {
    frame_rate_.num = 25;
    frame_rate_.den = 1;
  }
  gop_size_ = gop_size;
  bit_rate_ = bit_rate;

  avcodec_register_all();
  av_register_all();

  avcodec_ = avcodec_find_encoder(avcodec_id_);
  if (!avcodec_) {  // plan to add qsv or other codec
    std::cout << "cannot find encoder,use 'libx264'" << std::endl;
    avcodec_ = avcodec_find_encoder_by_name("libx264");
    if (avcodec_ == nullptr) {
      Destroy();
      std::cout << "Can't find encoder with libx264" << std::endl;
      return;
    }
  }

  avcodec_ctx_ = avcodec_alloc_context3(avcodec_);
  avcodec_ctx_->codec_id = avcodec_id_;
  avcodec_ctx_->bit_rate = bit_rate_;
  avcodec_ctx_->width = picture_width_;
  avcodec_ctx_->height = picture_height_;
  avcodec_ctx_->time_base.num = frame_rate_.den;
  avcodec_ctx_->time_base.den = frame_rate_.num;
  avcodec_ctx_->framerate.num = frame_rate_.num;
  avcodec_ctx_->framerate.den = frame_rate_.den;
  avcodec_ctx_->gop_size = gop_size_;
  avcodec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
  avcodec_ctx_->max_b_frames = 1;
  // avcodec_ctx_->thread_count = 1;

  av_dict_set(&avcodec_opts_, "preset", "veryfast", 0);
  av_dict_set(&avcodec_opts_, "tune", "zerolatency", 0);
  av_dict_set(&avcodec_opts_, "level", "4.2", 0);
  av_dict_set(&avcodec_opts_, "profile", "high", 0);
  ret = avcodec_open2(avcodec_ctx_, avcodec_, &avcodec_opts_);
  if (ret < 0) {
    std::cout << "avcodec_open2() failed, ret=" << ret << std::endl;
    Destroy();
    return;
  }

  if (picture_format_ != AV_PIX_FMT_YUV420P) {
    avframe_ = av_frame_alloc();
    avframe_->format = AV_PIX_FMT_YUV420P;
    avframe_->data[0] = nullptr;
    avframe_->linesize[0] = -1;
    avframe_->pts = 0;
    avframe_->width = avcodec_ctx_->width;
    avframe_->height = avcodec_ctx_->height;
    ret = av_image_alloc(avframe_->data, avframe_->linesize, avcodec_ctx_->width, avcodec_ctx_->height,
                         AV_PIX_FMT_YUV420P, 8);
    if (ret < 0) {
      std::cout << "av_image_alloc() failed, ret=" << ret << std::endl;
      Destroy();
      return;
    }

    sws_ctx_ = sws_getContext(avcodec_ctx_->width, avcodec_ctx_->height, picture_format_, avcodec_ctx_->width,
                              avcodec_ctx_->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
      std::cout << "sws_getContext() failed, ret=" << ret << std::endl;
      Destroy();
      return;
    }
  }

#if LIBAVCODEC_VERSION_MAJOR < 59
  avpacket_ = reinterpret_cast<AVPacket *>(av_mallocz(sizeof(AVPacket)));
#else
  avpacket_ = av_packet_alloc();
#endif
  av_init_packet(avpacket_);
}

FFmpegVideoEncoder::~FFmpegVideoEncoder() {
  Stop();
  Destroy();
}

void FFmpegVideoEncoder::Destroy() {
  if (avcodec_ctx_) {
    avcodec_close(avcodec_ctx_);
    avcodec_ctx_ = nullptr;
  }
  if (avcodec_opts_) {
    av_dict_free(&avcodec_opts_);
    avcodec_opts_ = nullptr;
  }
  if (avframe_) {
    av_freep(&(avframe_->data[0]));
    av_frame_unref(avframe_);
    av_free(avframe_);
    avframe_ = nullptr;
  }
  if (sws_ctx_) {
    sws_freeContext(sws_ctx_);
    sws_ctx_ = nullptr;
  }
  if (avpacket_) {
    av_packet_unref(avpacket_);
    av_free(avpacket_);
    avpacket_ = nullptr;
  }
}

VideoEncoder::VideoFrame *FFmpegVideoEncoder::NewFrame() { return new FFmpegVideoFrame(this); }

uint32_t FFmpegVideoEncoder::GetOffSet(const uint8_t *data) {
  uint32_t offset = 0;
  const uint8_t *p = data;
  if (p[0] == 0x00 && p[1] == 0x00) {
    if (p[2] == 0x01) {
      offset = 3;
    } else if ((p[2] == 0x00) && (p[3] == 0x01)) {
      offset = 4;
    }
  }
  return offset;
}

void FFmpegVideoEncoder::EncodeFrame(VideoFrame *frame) {
  FFmpegVideoFrame *ffpic = dynamic_cast<FFmpegVideoFrame *>(frame);
  AVFrame *picture = ffpic->Get();

  if (sws_ctx_) {
    sws_scale(sws_ctx_, picture->data, picture->linesize, 0, picture->height, avframe_->data, avframe_->linesize);
    avframe_->pts = picture->pts;
    picture = avframe_;
  }

  int ret = 0, got_packet;
  ret = avcodec_encode_video2(avcodec_ctx_, avpacket_, picture, &got_packet);
  if (ret < 0) {
    std::cout << "avcodec_encode_video2() failed, ret=" << ret << std::endl;
    return;
  }

  if (!ret && got_packet && avpacket_->size) {
    // std::cout << "===got packet: size=" << avpacket_->size << ", pts=" << avpacket_->pts << std::endl;
    int offset = 0;
    uint8_t *packet_data = nullptr;
    packet_data = reinterpret_cast<uint8_t *>(avpacket_->data);
    offset = GetOffSet(packet_data);
    size_t length = avpacket_->size - offset;
    uint8_t *data = avpacket_->data + offset;
    PushOutputBuffer(data, length, frame_count_, avpacket_->pts);
    frame_count_++;
    Callback(NEW_FRAME);
  }
  av_packet_unref(avpacket_);
}
