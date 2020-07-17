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
#include "ffmpeg_decoder.hpp"
#include <cnrt.h>
#include <glog/logging.h>
#include <future>
#include <memory>
#include <sstream>
#include <thread>
#include <utility>

#include "cnstream_frame_va.hpp"

#define YUV420SP_STRIDE_ALIGN_FOR_SCALER 128

namespace cnstream {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// FFMPEG use AVCodecParameters instead of AVCodecContext
// since from version 3.1(libavformat/version:57.40.100)
#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

static CNDataFormat PixelFmt2CnDataFormat(cncodecPixelFormat pformat) {
  switch (pformat) {
    case CNCODEC_PIX_FMT_NV12:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
    case CNCODEC_PIX_FMT_NV21:
      return CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
    default:
      return CNDataFormat::CN_INVALID;
  }
  return CNDataFormat::CN_INVALID;
}

bool MluDecoder::Create(AVStream *st, int interval) {
  if (!handler_) {
    return false;
  }
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  AVCodecID codec_id = st->codecpar->codec_id;
  int codec_width = st->codecpar->width;
  int codec_height = st->codecpar->height;
  int field_order = st->codecpar->field_order;
  AVColorSpace color_space = st->codecpar->color_space;
#else
  AVCodecID codec_id = st->codec->codec_id;
  int codec_width = st->codec->width;
  int codec_height = st->codec->height;
  int field_order = st->codec->field_order;
  AVColorSpace color_space = st->codec->colorspace;
#endif
  int progressive;
  /*At this moment, if the demuxer does not set this value (avctx->field_order == UNKNOWN),
   *   the input stream will be assumed as progressive one.
   */
  switch (field_order) {
    case AV_FIELD_TT:
    case AV_FIELD_BB:
    case AV_FIELD_TB:
    case AV_FIELD_BT:
      progressive = 0;
      break;
    case AV_FIELD_PROGRESSIVE:  // fall through
    default:
      progressive = 1;
      break;
  }

  VideoStreamInfo info;
  info.codec_id = codec_id;
  info.codec_width = codec_width == 0 ? 1920 : codec_width;
  info.codec_height = codec_height == 0 ? 1080 : codec_height;
  info.progressive = progressive;
  info.color_space = color_space;
  return Create(&info, interval);
}

bool MluDecoder::Create(VideoStreamInfo *info, int interval) {
  // create decoder
  if (info->codec_id == AV_CODEC_ID_MJPEG) {
    if (CreateJpegDecoder(info) != true) {
      return false;
    }
  } else {
    if (CreateVideoDecoder(info) != true) {
      return false;
    }
  }
  interval_ = interval;
  frame_id_ = 0;
  frame_count_ = 0;
  return true;
}

void MluDecoder::Destroy() {
  if (instance_) {
    if (!cndec_abort_flag_.load()) {
      DestroyVideoDecoder();
    } else {
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      handler_->SendFlowEos();
    }
  }

  if (jpg_instance_) {
    if (!cndec_abort_flag_.load()) {
      DestroyJpegDecoder();
    } else {
      cnjpegDecAbort(jpg_instance_);
      jpg_instance_ = nullptr;
      handler_->SendFlowEos();
    }
  }
}

bool MluDecoder::Process(AVPacket *pkt, bool eos) {
  ESPacket epkt;
  if (pkt) {
    epkt.data = pkt->data;
    epkt.size = pkt->size;
    epkt.pts = pkt->pts;
  }
  if (eos) {
    epkt.flags |= ESPacket::FLAG_EOS;
  }
  return Process(&epkt);
}

bool MluDecoder::Process(ESPacket *pkt) {
  if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
    return false;
  }

  if (instance_) {
    cnvideoDecInput input;
    memset(&input, 0, sizeof(cnvideoDecInput));
    if (pkt) {
      input.streamBuf = pkt->data;
      input.streamLength = pkt->size;
      input.pts = pkt->pts;
      input.flags |= CNVIDEODEC_FLAG_TIMESTAMP;
      if (pkt->flags & ESPacket::FLAG_EOS) {
        input.flags |= CNVIDEODEC_FLAG_EOS;
        eos_sent_.store(1);
      }
      if (input.streamLength > create_info_.suggestedLibAllocBitStrmBufSize) {
        LOG(WARNING) << "cnvideoDecFeedData- truncate " << input.streamLength
                     << " to " << create_info_.suggestedLibAllocBitStrmBufSize;
        input.streamLength = create_info_.suggestedLibAllocBitStrmBufSize;
      }
    } else {
      input.flags |= CNVIDEODEC_FLAG_EOS;
      eos_sent_.store(1);
    }
    int ret = cnvideoDecFeedData(instance_, &input, 10000);  // FIXME
    if (ret == -CNCODEC_TIMEOUT) {
      LOG(ERROR) << "cnvideoDecFeedData timeout happened";
      cndec_abort_flag_ = 1;
      return false;
    } else if (ret < 0) {
      LOG(ERROR) << "Call cnvideoDecFeedData failed, ret = " <<  ret;
      cndec_error_flag_ = 1;
      return false;
    }
    return true;
  }

  if (jpg_instance_) {
    cnjpegDecInput input;
    memset(&input, 0, sizeof(cnjpegDecInput));
    if (pkt) {
      input.streamBuffer = pkt->data;
      input.streamLength = pkt->size;
      input.pts = pkt->pts;
      input.flags |= CNJPEGDEC_FLAG_TIMESTAMP;
      if (pkt->flags & ESPacket::FLAG_EOS) {
        input.flags |= CNVIDEODEC_FLAG_EOS;
        eos_sent_.store(1);
      }
      if (input.streamLength > create_jpg_info_.suggestedLibAllocBitStrmBufSize) {
        LOG(WARNING) << "cnjpegDecFeedData- truncate " << input.streamLength
                     << " to " << create_jpg_info_.suggestedLibAllocBitStrmBufSize;
        input.streamLength = create_jpg_info_.suggestedLibAllocBitStrmBufSize;
      }
    } else {
      input.flags |= CNJPEGDEC_FLAG_EOS;
      eos_sent_.store(1);
    }
    int ret = cnjpegDecFeedData(jpg_instance_, &input, 10000);  // FIXME
    if (ret == -CNCODEC_TIMEOUT) {
      LOG(ERROR) << "cnjpegDecFeedData timeout happened";
      cndec_abort_flag_ = 1;
      return false;
    } else if (ret < 0) {
      LOG(ERROR) << "Call cnjpegDecFeedData failed, ret = " <<  ret;
      cndec_error_flag_ = 1;
      return false;
    }
    return true;
  }

  // should not come here...
  return false;
}

//
// video decoder implementation
//
static int VideoDecodeCallback(cncodecCbEventType EventType, void *pData, void *pdata1) {
  MluDecoder *pThis = reinterpret_cast<MluDecoder *>(pData);
  switch (EventType) {
    case CNCODEC_CB_EVENT_NEW_FRAME:
      pThis->VideoFrameCallback(reinterpret_cast<cnvideoDecOutput *>(pdata1));
      break;
    case CNCODEC_CB_EVENT_SEQUENCE:
      pThis->SequenceCallback(reinterpret_cast<cnvideoDecSequenceInfo *>(pdata1));
      break;
    case CNCODEC_CB_EVENT_EOS:
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      // todo....
      LOG(ERROR) << "Decode Firmware crash Event Event: " << EventType;
      pThis->VideoResetCallback();
      break;
    case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
      LOG(ERROR) << "Decode out of memory, force stop";
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_ABORT_ERROR:
      LOG(ERROR) << "Decode abort error occured, force stop";
      pThis->VideoEosCallback();
      break;
    case CNCODEC_CB_EVENT_STREAM_CORRUPT:
      LOG(WARNING) << "Stream corrupt, discard frame";
      break;
    default:
      LOG(ERROR) << "Unsupported Decode Event: " << EventType;
      break;
  }
  return 0;
}

void MluDecoder::SequenceCallback(cnvideoDecSequenceInfo *pFormat) {
  /*update decode info*/

  // FIXME, check to reset decoder ...
  create_info_.codec = pFormat->codec;
  create_info_.height = pFormat->height;
  create_info_.width = pFormat->width;

  uint32_t out_buf_num = param_.output_buf_number_;
  if (param_.reuse_cndec_buf) {
    out_buf_num += cnstream::GetFlowDepth();  // FIXME
  }
  out_buf_num += create_info_.inputBufNum;

  if (out_buf_num > pFormat->minOutputBufNum) {
    create_info_.outputBufNum = out_buf_num;
  } else {
    create_info_.outputBufNum = pFormat->minOutputBufNum + 1;
  }
  if (create_info_.outputBufNum > 32) {
    create_info_.outputBufNum = 32;
  }
  /*start decode*/
  int ret = cnvideoDecStart(instance_, &create_info_);
  if (ret < 0) {
    LOG(ERROR) << "Call cnvideoDecStart failed, ret = " << ret;
    return;
  }
  cndec_start_flag_.store(1);
}

void MluDecoder::VideoFrameCallback(cnvideoDecOutput *output) {
  if (output->frame.width == 0 || output->frame.height == 0) {
    LOG(WARNING) << "Skip frame! " << (int64_t)this << " width x height:" << output->frame.width << " x "
                 << output->frame.height << " timestamp:" << output->pts << std::endl;
    return;
  }
  bool reused = false;
  if (frame_count_++ % interval_ == 0) {
    cnvideoDecAddReference(instance_, &output->frame);
    ProcessFrame(output, &reused);
    if (!reused) {
      cnvideoDecReleaseReference(instance_, &output->frame);
    }
  }
}

int MluDecoder::ProcessFrame(cnvideoDecOutput *output, bool *reused) {
  *reused = false;

  // FIXME, remove infinite-loop
  std::shared_ptr<CNFrameInfo> data;
  while (1) {
    data = handler_->CreateFrameInfo();
    if (data != nullptr) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
      return -1;
    }
  }
  std::shared_ptr<CNDataFrame> dataframe(new (std::nothrow) CNDataFrame());
  if (!dataframe) {
    return -1;
  }
  dataframe->frame_id = frame_id_++;
  data->timestamp = output->pts;
  /*fill source data info*/
  dataframe->width = output->frame.width;
  dataframe->height = output->frame.height;
  dataframe->fmt = PixelFmt2CnDataFormat(output->frame.pixelFmt);
  if (OUTPUT_MLU == param_.output_type_) {
    dataframe->ctx.dev_type = DevContext::MLU;
    dataframe->ctx.dev_id = param_.device_id_;
    dataframe->ctx.ddr_channel = output->frame.channel;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      dataframe->stride[i] = output->frame.stride[i];
      dataframe->ptr_mlu[i] = reinterpret_cast<void *>(output->frame.plane[i].addr);
    }
    if (param_.reuse_cndec_buf) {
      dataframe->deAllocator_ = std::make_shared<CNDeallocator>(this, &output->frame);
      if (dataframe->deAllocator_) {
        *reused = true;
      }
    }
    dataframe->CopyToSyncMem();
  } else if (OUTPUT_CPU == param_.output_type_) {
    dataframe->ctx.dev_type = DevContext::CPU;
    dataframe->ctx.dev_id = -1;
    dataframe->ctx.ddr_channel = 0;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      dataframe->stride[i] = output->frame.stride[i];
    }
    size_t bytes = dataframe->GetBytes();
    bytes = ROUND_UP(bytes, 64 * 1024);
    CNStreamMallocHost(&dataframe->cpu_data, bytes);
    if (nullptr == dataframe->cpu_data) {
      LOG(FATAL) << "MluDecoder: failed to alloc cpu memory";
    }
    void* dst = dataframe->cpu_data;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      void *src = reinterpret_cast<void *>(output->frame.plane[i].addr);
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, src, plane_size, CNRT_MEM_TRANS_DIR_DEV2HOST), param_.device_id_,
                           output->frame.channel);
      dataframe->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size));
      dataframe->data[i]->SetCpuData(dst);
      dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
    }
  } else {
    LOG(FATAL) << "MluDecoder:output type not supported";
  }
  data->datas[CNDataFramePtrKey] = dataframe;
  handler_->SendFrameInfo(data);
  return 0;
}

void MluDecoder::VideoEosCallback() {
  handler_->SendFlowEos();
  eos_got_.store(1);
}

void MluDecoder::VideoResetCallback() {
  cndec_abort_flag_.store(1);
}

bool MluDecoder::CreateVideoDecoder(VideoStreamInfo *info) {
  if (instance_) {
    return false;
  }
  memset(&create_info_, 0, sizeof(cnvideoDecCreateInfo));
  create_info_.deviceId = param_.device_id_;
  create_info_.instance = CNVIDEODEC_INSTANCE_AUTO;
  switch (info->codec_id) {
    case AV_CODEC_ID_H264:
      create_info_.codec = CNCODEC_H264;
      break;
    case AV_CODEC_ID_HEVC:
      create_info_.codec = CNCODEC_HEVC;
      break;
    default: {
      LOG(ERROR) << "codec type not supported yet, codec_id = " << info->codec_id;
      return false;
    }
  }
  create_info_.pixelFmt = CNCODEC_PIX_FMT_NV12;
  // FIXME
  switch (info->color_space) {
    case AVCOL_SPC_BT709:
      create_info_.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
      break;
    case AVCOL_SPC_BT2020_CL:
    case AVCOL_SPC_BT2020_NCL:
      create_info_.colorSpace = CNCODEC_COLOR_SPACE_BT_2020;
      break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
      create_info_.colorSpace = CNCODEC_COLOR_SPACE_BT_601;
      break;
    default:
      create_info_.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
      break;
  }
  create_info_.width = info->codec_width;
  create_info_.height = info->codec_height;
  create_info_.bitDepthMinus8 = 0;  // FIXME
  create_info_.progressive = info->progressive;
  create_info_.inputBufNum = param_.input_buf_number_;
  create_info_.outputBufNum = param_.output_buf_number_;  // must be non-zero, although it is not used.
  create_info_.allocType = CNCODEC_BUF_ALLOC_LIB;
  create_info_.suggestedLibAllocBitStrmBufSize = info->codec_width * info->codec_height * 3 / 2 / 2
                                               + YUV420SP_STRIDE_ALIGN_FOR_SCALER;
  create_info_.userContext = reinterpret_cast<void*>(this);
  eos_got_.store(0);
  eos_sent_.store(0);
  cndec_abort_flag_.store(0);
  cndec_error_flag_.store(0);
  cndec_start_flag_.store(0);
  int ret = cnvideoDecCreate(&this->instance_, VideoDecodeCallback, &create_info_);
  if (ret < 0) {
    LOG(ERROR) << "Call cnvideoDecCreate failed, ret = " << ret;
    return false;
  }
  int stride_align = 1;
  if (param_.apply_stride_align_for_scaler_)
    stride_align = YUV420SP_STRIDE_ALIGN_FOR_SCALER;

  ret = cnvideoDecSetAttributes(this->instance_, CNVIDEO_DEC_ATTR_OUT_BUF_ALIGNMENT, &stride_align);
  if (0 != ret) {
    LOG(ERROR) << "Failed to set output buffer stride alignment,error code: " << ret;
    return false;
  }
  return true;
}

void MluDecoder::DestroyVideoDecoder() {
  if (this->instance_) {
    if (!cndec_start_flag_.load()) {
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      handler_->SendFlowEos();
      return;
    }
    if (handler_ && !eos_sent_.load()) {
      this->Process(nullptr, true);
    }
    /*make sure all cndec buffers released before destorying cndecoder
     */
    while (cndec_buf_ref_count_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    if (cndec_abort_flag_.load()) {
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      handler_->SendFlowEos();
      return;
    }
    int ret = cnvideoDecStop(instance_);
    if (ret == -CNCODEC_TIMEOUT) {
      LOG(ERROR) << "cnvideoDecStop timeout happened";
      cnvideoDecAbort(instance_);
      instance_ = nullptr;
      handler_->SendFlowEos();
      return;
    } else if (ret < 0) {
      LOG(ERROR) << "Call cnvideoDecStop failed, ret = " << ret;
    }
    ret = cnvideoDecDestroy(instance_);
    if (ret < 0) {
      LOG(ERROR) << "Call cnvideoDecDestroy failed, ret = " << ret;
    }
    instance_ = nullptr;
  }
}

//
// cnjpeg decoder
//
void MluDecoder::JpegEosCallback(void) {
  handler_->SendFlowEos();
  eos_got_.store(1);
}

void MluDecoder::JpegResetCallback() {
  cndec_abort_flag_.store(1);
}

void MluDecoder::JpegFrameCallback(cnjpegDecOutput *output) {
  if (output->result != 0) {
    LOG(ERROR) << "jpeg decode return error " << output->result;
    return;
  }
  bool reused = false;
  if (frame_count_++ % interval_ == 0) {
    cnjpegDecAddReference(jpg_instance_, &output->frame);
    ProcessJpegFrame(output, &reused);
    if (!reused) {
      cnjpegDecReleaseReference(jpg_instance_, &output->frame);
    }
  }
}

int MluDecoder::ProcessJpegFrame(cnjpegDecOutput *output, bool *reused) {
  *reused = false;

  // FIXME, remove infinite-loop
  std::shared_ptr<CNFrameInfo> data;
  while (1) {
    data = handler_->CreateFrameInfo();
    if (data != nullptr) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (cndec_abort_flag_.load() || cndec_error_flag_.load()) {
      return -1;
    }
  }
  std::shared_ptr<CNDataFrame> dataframe(new (std::nothrow) CNDataFrame());
  if (!dataframe) {
    return -1;
  }
  dataframe->frame_id = frame_id_++;
  data->timestamp = output->pts;
  /*fill source data info*/
  dataframe->width = output->frame.width;
  dataframe->height = output->frame.height;
  dataframe->fmt = PixelFmt2CnDataFormat(output->frame.pixelFmt);
  if (OUTPUT_MLU == param_.output_type_) {
    dataframe->ctx.dev_type = DevContext::MLU;
    dataframe->ctx.dev_id = param_.device_id_;
    dataframe->ctx.ddr_channel = output->frame.channel;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      dataframe->stride[i] = output->frame.stride[i];
      dataframe->ptr_mlu[i] = reinterpret_cast<void *>(output->frame.plane[i].addr);
    }
    if (param_.reuse_cndec_buf) {
      dataframe->deAllocator_ = std::make_shared<CNDeallocatorJpg>(this, &output->frame);
      if (dataframe->deAllocator_) {
        *reused = true;
      }
    }
    dataframe->CopyToSyncMem();
  } else if (OUTPUT_CPU == param_.output_type_) {
    dataframe->ctx.dev_type = DevContext::CPU;
    dataframe->ctx.dev_id = -1;
    dataframe->ctx.ddr_channel = 0;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      dataframe->stride[i] = output->frame.stride[i];
    }
    size_t bytes = dataframe->GetBytes();
    bytes = ROUND_UP(bytes, 64 * 1024);
    CNStreamMallocHost(&dataframe->cpu_data, bytes);
    if (nullptr == dataframe->cpu_data) {
      LOG(FATAL) << "MluDecoder: failed to alloc cpu memory";
    }
    void* dst = dataframe->cpu_data;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      void *src = reinterpret_cast<void *>(output->frame.plane[i].addr);
      CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dst, src, plane_size, CNRT_MEM_TRANS_DIR_DEV2HOST), param_.device_id_,
                           output->frame.channel);
      dataframe->data[i].reset(new (std::nothrow) CNSyncedMemory(plane_size));
      dataframe->data[i]->SetCpuData(dst);
      dst = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(dst) + plane_size);
    }
  } else {
    LOG(FATAL) << "MluDecoder:output type not supported";
  }
  data->datas[CNDataFramePtrKey] = dataframe;
  handler_->SendFrameInfo(data);
  return 0;
}

static int JpegEventCallback(cncodecCbEventType event, void *context, void *data) {
  MluDecoder *pThis = reinterpret_cast<MluDecoder *>(context);
  switch (event) {
    case CNCODEC_CB_EVENT_EOS:
      pThis->JpegEosCallback();
      break;

    case CNCODEC_CB_EVENT_SW_RESET:
    case CNCODEC_CB_EVENT_HW_RESET:
      LOG(ERROR) << "RESET Event received type = " << event;
      pThis->JpegResetCallback();
      break;

    case CNCODEC_CB_EVENT_NEW_FRAME:
      if (data) {
        pThis->JpegFrameCallback(reinterpret_cast<cnjpegDecOutput *>(data));
      }
      break;

    default:
      LOG(ERROR) << "unexpected Event received = " << event;
      return -1;
  }
  return 0;
}

bool MluDecoder::CreateJpegDecoder(VideoStreamInfo *info) {
  if (instance_) {
    return false;
  }
  // maximum resolution 8K
  info->codec_width = 7680;
  info->codec_height = 4320;
  memset(&create_jpg_info_, 0, sizeof(cnjpegDecCreateInfo));
  create_jpg_info_.deviceId = param_.device_id_;
  create_jpg_info_.instance = CNVIDEODEC_INSTANCE_AUTO;
  create_jpg_info_.pixelFmt = CNCODEC_PIX_FMT_NV12;          // FIXME
  create_jpg_info_.colorSpace = CNCODEC_COLOR_SPACE_BT_709;  // FIXME
  create_jpg_info_.width = info->codec_width;
  create_jpg_info_.height = info->codec_height;
  create_jpg_info_.enablePreparse = 0;
  create_jpg_info_.userContext = reinterpret_cast<void *>(this);
  create_jpg_info_.allocType = CNCODEC_BUF_ALLOC_LIB;
  create_jpg_info_.inputBufNum = param_.input_buf_number_;
  create_jpg_info_.outputBufNum = param_.output_buf_number_;
  create_jpg_info_.suggestedLibAllocBitStrmBufSize = info->codec_width * info->codec_height * 3 / 2 / 2
                                                   + YUV420SP_STRIDE_ALIGN_FOR_SCALER;
  eos_got_.store(0);
  eos_sent_.store(0);
  cndec_abort_flag_.store(0);
  cndec_error_flag_.store(0);
  int ret = cnjpegDecCreate(&this->jpg_instance_, CNJPEGDEC_RUN_MODE_ASYNC, JpegEventCallback, &create_jpg_info_);
  if (ret < 0) {
    LOG(ERROR) << "Call cnjpegDecCreate failed, ret = " << ret;
    return false;
  }
  return true;
}

void MluDecoder::DestroyJpegDecoder() {
  if (this->jpg_instance_) {
    if (handler_ && !eos_sent_.load()) {
      this->Process(nullptr, true);
    }
    /*make sure all cndec buffers released before destorying cndecoder
     */
    while (cndec_buf_ref_count_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
      if (cndec_abort_flag_.load()) {
        break;
      }
    }
    if (cndec_abort_flag_.load()) {
      cnjpegDecAbort(jpg_instance_);
      jpg_instance_ = nullptr;
      handler_->SendFlowEos();
      return;
    }
    int ret = cnjpegDecDestroy(jpg_instance_);
    if (ret < 0) {
      LOG(ERROR) << "Call cnjpegDecDestroy failed, ret = " << ret;
    }
    jpg_instance_ = nullptr;
  }
}

//----------------------------------------------------------------------------
// CPU decoder
bool FFmpegCpuDecoder::Create(VideoStreamInfo *info, int interval) {
  if (!handler_) {
    return false;
  }

  stream_ = new (std::nothrow) AVStream();
  if (!stream_) {
    LOG(ERROR) << "Create AVStream failed!";
    return false;
  }

#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  auto codec_param = new (std::nothrow) AVCodecParameters();
  stream_->codecpar = codec_param;
  stream_->codecpar->codec_id = info->codec_id;
  stream_->codecpar->width = info->codec_width;
  stream_->codecpar->height = info->codec_height;
#else
  auto codec_ctx = new (std::nothrow) AVCodecContext();
  stream_->codec = codec_ctx;
  stream_->codec->codec_id = info->codec_id;
  stream_->codec->width = info->codec_width;
  stream_->codec->height = info->codec_height;
#endif
  return Create(stream_, interval);
}

bool FFmpegCpuDecoder::Create(AVStream *st, int interval) {
  if (!handler_) {
    return false;
  }
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  AVCodecID codec_id = st->codecpar->codec_id;
#else
  AVCodecID codec_id = st->codec->codec_id;
#endif
  // create decoder
  AVCodec *dec = avcodec_find_decoder(codec_id);
  if (!dec) {
    LOG(ERROR) << "avcodec_find_decoder failed";
    return false;
  }
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 14, 0)) || \
    ((LIBAVCODEC_VERSION_MICRO >= 100) && (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 33, 100)))
  instance_ = st->codec;
#else
  instance_ = avcodec_alloc_context3(dec);
  if (!instance_) {
    LOG(ERROR) << "Failed to do avcodec_alloc_context3";
    return false;
  }
  if (avcodec_parameters_to_context(instance_, st->codecpar) < 0) {
    LOG(ERROR) << "Failed to copy codec parameters to decoder context";
    return false;
  }
  av_codec_set_pkt_timebase(instance_, st->time_base);
#endif
  if (avcodec_open2(instance_, dec, NULL) < 0) {
    LOG(ERROR) << "Failed to open codec";
    return false;
  }
  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    LOG(ERROR) << "Could not alloc frame";
    return false;
  }
  interval_ = interval;
  frame_id_ = 0;
  frame_count_ = 0;
  eos_got_.store(0);
  eos_sent_.store(0);
  return true;
}

void FFmpegCpuDecoder::Destroy() {
  if (instance_ != nullptr) {
    if (handler_ && !eos_sent_.load()) {
      while (this->Process(nullptr, true)) {
      }
    }
    while (!eos_got_.load()) {
      std::this_thread::yield();
    }
#if !((LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 14, 0)) || \
      ((LIBAVCODEC_VERSION_MICRO >= 100) && (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 33, 100))))
    avcodec_free_context(&instance_);
#endif
    instance_ = nullptr;
  }

  if (stream_) {
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    delete stream_->codecpar;
#else
    delete stream_->codec;
#endif
    delete stream_;
    stream_ = nullptr;
  }

  if (av_frame_) {
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }
}

bool FFmpegCpuDecoder::Process(ESPacket *pkt) {
  if (!pkt) {
    return false;
  }
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = pkt->data;
  packet.size = pkt->size;
  packet.pts = pkt->pts;
  if (pkt->flags & ESPacket::FLAG_EOS) {
    return Process(&packet, true);
  }
  return Process(&packet, false);
}

bool FFmpegCpuDecoder::Process(AVPacket *pkt, bool eos) {
  LOG_IF(INFO, eos) << "[FFmpegCpuDecoder]  " << (int64_t)this << " send eos.";
  if (eos) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    packet.data = NULL;

    eos_sent_.store(1);
    // flush all frames ...
    int got_frame = 0;
    do {
      avcodec_decode_video2(instance_, av_frame_, &got_frame, &packet);
      if (got_frame) ProcessFrame(av_frame_);
    } while (got_frame);

    handler_->SendFlowEos();
    eos_got_.store(1);
    return false;
  }
  int got_frame = 0;
  int ret = avcodec_decode_video2(instance_, av_frame_, &got_frame, pkt);
  if (ret < 0) {
    LOG(ERROR) << "avcodec_decode_video2 failed";
    return false;
  }
  if (got_frame) {
    ProcessFrame(av_frame_);
  }
  return true;
}

bool FFmpegCpuDecoder::FrameCvt2Yuv420sp(AVFrame *frame, uint8_t *sp, int dst_stride, bool nv21) {
  if (frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVJ420P) {
    LOG(ERROR) << "FFmpegCpuDecoder only supports AV_PIX_FMT_YUV420P at this moment";
    return false;
  }

  int height = frame->height;
  int src_stride = frame->linesize[0];

  uint8_t *py = frame->data[0];
  uint8_t *pu = frame->data[1];
  uint8_t *pv = frame->data[2];

  uint8_t *pdst_y = sp;
  uint8_t *pdst_uv = sp + dst_stride * height;

  if (dst_stride == src_stride) {
    memcpy(pdst_y, py, src_stride * height);
  } else {
    for (int row = 0; row < height; ++row) {
      uint8_t *psrc_yt = py + row * src_stride;
      uint8_t *pdst_yt = pdst_y + row * dst_stride;
      memcpy(pdst_yt, psrc_yt, src_stride);
    }
  }

  for (int row = 0; row < height / 2; ++row) {
    uint8_t *psrc_u = pu + frame->linesize[1] * row;
    uint8_t *psrc_v = pv + frame->linesize[2] * row;
    if (nv21) std::swap(psrc_u, psrc_v);
    uint8_t *pdst_uvt = pdst_uv + dst_stride * row;
    for (int col = 0; col < frame->linesize[1]; ++col) {
      pdst_uvt[col * 2] = psrc_u[col];
      pdst_uvt[col * 2 + 1] = psrc_v[col];
    }
  }

  return true;
}

bool FFmpegCpuDecoder::ProcessFrame(AVFrame *frame) {
  if (frame_count_++ % interval_ != 0) {
    return true;  // discard frames
  }

  // FIXME, remove infinite-loop
  std::shared_ptr<CNFrameInfo> data;
  while (1) {
    data = handler_->CreateFrameInfo();
    if (data != nullptr) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(5));
  }

  if (instance_->pix_fmt != AV_PIX_FMT_YUV420P && instance_->pix_fmt != AV_PIX_FMT_YUVJ420P) {
    LOG(ERROR) << "FFmpegCpuDecoder only supports AV_PIX_FMT_YUV420P at this moment";
    return false;
  }
  std::shared_ptr<CNDataFrame> dataframe(new (std::nothrow) CNDataFrame());
  if (!dataframe) {
    return false;
  }

  if (param_.output_type_ == OUTPUT_MLU) {
    dataframe->ctx.dev_type = DevContext::MLU;
    dataframe->ctx.dev_id = param_.device_id_;
    dataframe->ctx.ddr_channel = 0;  // FIXME
  } else {
    dataframe->ctx.dev_type = DevContext::CPU;
    dataframe->ctx.dev_id = -1;
    dataframe->ctx.ddr_channel = 0;
  }
  int dst_stride = frame->linesize[0];

  if (param_.apply_stride_align_for_scaler_)
    dst_stride = std::ceil(1.0 * dst_stride / YUV420SP_STRIDE_ALIGN_FOR_SCALER) * YUV420SP_STRIDE_ALIGN_FOR_SCALER;

  size_t frame_size = dst_stride * frame->height * 3 / 2;
  uint8_t *sp_data = new (std::nothrow) uint8_t[frame_size];
  if (!sp_data) {
    LOG(ERROR) << "Malloc failed, size:" << frame_size;
    return false;
  }
  if (!FrameCvt2Yuv420sp(frame, sp_data, dst_stride, false)) {
    LOG(ERROR) << "Yuv420p cvt yuv420sp failed.";
    return false;
  }

  dataframe->fmt = CN_PIXEL_FORMAT_YUV420_NV12;
  dataframe->width = frame->width;
  dataframe->height = frame->height;
  dataframe->stride[0] = dst_stride;
  dataframe->stride[1] = dst_stride;

  if (param_.output_type_ == OUTPUT_MLU) {
    CALL_CNRT_BY_CONTEXT(cnrtMalloc(&dataframe->mlu_data, frame_size), dataframe->ctx.dev_id,
                         dataframe->ctx.ddr_channel);
    if (nullptr == dataframe->mlu_data) {
      LOG(ERROR) << "FFmpegCpuDecoder: Failed to alloc mlu memory";
      return false;
    }
    CALL_CNRT_BY_CONTEXT(cnrtMemcpy(dataframe->mlu_data, sp_data, frame_size, CNRT_MEM_TRANS_DIR_HOST2DEV),
                         dataframe->ctx.dev_id, dataframe->ctx.ddr_channel);

    auto t = reinterpret_cast<uint8_t *>(dataframe->mlu_data);
    for (int i = 0; i < dataframe->GetPlanes(); ++i) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      CNSyncedMemory *CNSyncedMemory_ptr =
          new (std::nothrow) CNSyncedMemory(plane_size, dataframe->ctx.dev_id, dataframe->ctx.ddr_channel);
      LOG_IF(FATAL, nullptr == CNSyncedMemory_ptr) << "FFmpegCpuDecoder::ProcessFrame() new CNSyncedMemory failed";
      dataframe->data[i].reset(CNSyncedMemory_ptr);
      dataframe->data[i]->SetMluData(t);
      t += plane_size;
    }
  } else if (param_.output_type_ == OUTPUT_CPU) {
    dataframe->cpu_data = sp_data;
    sp_data = nullptr;
    auto t = reinterpret_cast<uint8_t *>(dataframe->cpu_data);
    for (int i = 0; i < dataframe->GetPlanes(); ++i) {
      size_t plane_size = dataframe->GetPlaneBytes(i);
      CNSyncedMemory *CNSyncedMemory_ptr = new (std::nothrow) CNSyncedMemory(plane_size);
      LOG_IF(FATAL, nullptr == CNSyncedMemory_ptr) << "FFmpegCpuDecoder::ProcessFrame() new CNSyncedMemory failed";
      dataframe->data[i].reset(CNSyncedMemory_ptr);
      dataframe->data[i]->SetCpuData(t);
      t += plane_size;
    }
  } else {
    LOG(ERROR) << "DevContex::INVALID";
    return false;
  }

  dataframe->frame_id = frame_id_++;
  data->timestamp = frame->pts;
  data->datas[CNDataFramePtrKey] = dataframe;
  if (sp_data) delete[] sp_data;
  handler_->SendFrameInfo(data);
  return true;
}

}  // namespace cnstream
