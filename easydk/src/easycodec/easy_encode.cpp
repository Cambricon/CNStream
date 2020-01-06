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

#include <cnrt.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "cxxutil/logger.h"
#include "easycodec/easy_encode.h"
#include "easyinfer/mlu_context.h"

#ifdef CNSTK_MLU100

#include <cncodec.h>
#include "init_tools.h"

#ifndef ALIGN
#define ALIGN(addr, boundary) (((u32_t)(addr) + (boundary)-1) & ~((boundary)-1))
#endif  // ALIGN

#elif CNSTK_MLU270

#include <cn_video_enc.h>

#endif  // CNSTK_MLU100

using std::to_string;
using std::string;

namespace edk {

#ifdef CNSTK_MLU100

static CN_PIXEL_FORMAT_E to_PF_E(PixelFmt format) {
  switch (format) {
    case PixelFmt::YUV420SP_NV21:
      return CN_PIXEL_FORMAT_YUV420SP;
    case PixelFmt::BGR24:
      return CN_PIXEL_FORMAT_BGR24;
    case PixelFmt::RGB24:
      return CN_PIXEL_FORMAT_RGB24;
    default:
      throw EasyEncodeError("Unsupport M100 pixel format");
  }
  return CN_PIXEL_FORMAT_YUV420SP;
}

static CN_VIDEO_CODEC_TYPE_E to_CT_E(CodecType type) {
  switch (type) {
    case CodecType::MPEG4:
      return CN_VIDEO_CODEC_MPEG4;
    case CodecType::H264:
      return CN_VIDEO_CODEC_H264;
    case CodecType::H265:
      return CN_VIDEO_CODEC_HEVC;
    case CodecType::JPEG:
      return CN_VIDEO_CODEC_JPEG;
    case CodecType::MJPEG:
      return CN_VIDEO_CODEC_MJPEG;
    default:
      throw EasyEncodeError("Unsupport M100 video codec");
  }
  return CN_VIDEO_CODEC_H264;
}

static CN_VENC_H264_PROFILE_E to_Profile_E(VideoProfile profile) {
  switch (profile) {
    case VideoProfile::BASELINE:
      return CN_PROFILE_BASELINE;
    case VideoProfile::MAIN:
      return CN_PROFILE_MAIN;
    case VideoProfile::HIGH:
      return CN_PROFILE_HIGH;
    default:
      throw EasyEncodeError("Unsupport M100 video profile");
  }
  return CN_PROFILE_MAIN;
}

static void VencPrintAttr(CN_VENC_CREATE_ATTR_S* p_attr) {
  printf("%-32s%s\n", "param", "value");
  printf("-------------------------------------\n");
  printf("%-32s%u\n", "u32VdecDeviceID", p_attr->u32VencDeviceID);
  printf("%-32s%d\n", "VideoCodecType", p_attr->VideoCodecType);
  printf("%-32s%d\n", "pixel_format", p_attr->pixel_format);
  printf("%-32s%u\n", "u32MaxWidth", p_attr->u32MaxWidth);
  printf("%-32s%u\n", "u32MaxHeight", p_attr->u32MaxHeight);
  printf("%-32s%u\n", "u32TargetWidth", p_attr->u32TargetWidth);
  printf("%-32s%u\n", "u32TargetHeight", p_attr->u32TargetHeight);
  printf("%-32s%u\n", "h264_profile", p_attr->h264_profile);
  printf("%-32s%u\n", "jpeg_qfactor", p_attr->jpeg_qfactor);
  printf("%-32s%s\n", "rate_control_mode", p_attr->rate_control_mode == CBR ? "CBR" : "VBR");
  printf("%-32s%s\n", "bcolor2gray", p_attr->bcolor2gray ? "true" : "false");
  printf("%-32s%s(%u,%u,%u,%u)\n", "encode_crop", p_attr->encode_crop.bEnable ? "enable" : "disable",
         p_attr->encode_crop.crop_rect.u32X, p_attr->encode_crop.crop_rect.u32Y, p_attr->encode_crop.crop_rect.u32Width,
         p_attr->encode_crop.crop_rect.u32Height);
}

#elif CNSTK_MLU270
static CNVideoSurfaceFormat to_M200_PF_E(PixelFmt format) {
  switch (format) {
    case PixelFmt::YUV420SP_NV21:
      return CNVideoSurfaceFormat_NV21;
    case PixelFmt::YUV420SP_NV12:
      return CNVideoSurfaceFormat_NV12;
    default:
      throw EasyEncodeError("Unsupport M200 pixel format");
  }
  return CNVideoSurfaceFormat_NV12;
}

static CNVideoCodec to_M200_CT_E(CodecType type) {
  switch (type) {
    case CodecType::H264:
      return CNVideoCodec_H264;
    case CodecType::H265:
      return CNVideoCodec_HEVC;
    case CodecType::JPEG:
      return CNVideoCodec_JPEG;
    default:
      throw EasyEncodeError("Unsupport M200 video codec");
  }
  return CNVideoCodec_H264;
}

static void VencPrintCreateAttr(CNVideoEncode_Create_Params* p_attr) {
  printf("%-32s%s\n", "param", "value");
  printf("-------------------------------------\n");
  printf("%-32s%u\n", "Codectype", p_attr->codecType);
  printf("%-32s%u\n", "Instance", p_attr->instance);
  printf("%-32s%u\n", "CardID", p_attr->cardid);
  printf("%-32s%u\n", "MemoryType", p_attr->memoryType);
  printf("%-32s%u\n", "Width", p_attr->Width);
  printf("%-32s%u\n", "Height", p_attr->Height);
  printf("%-32s%u\n", "FrameRateNum", p_attr->frameRateNum);
  printf("%-32s%u\n", "FrameRateDen", p_attr->frameRateDen);
  printf("%-32s%u\n", "BufferFormat", p_attr->bufferFmt);
  printf("%-32s%u\n", "RCMode", p_attr->RCParams.rcMode);
  printf("%-32s%u\n", "NumInputBuffers", p_attr->numInputBuf);
  printf("%-32s%u\n", "NumOfBitFrameBuffers", p_attr->numBitFrameBuf);
}

#endif  // CNSTK_MLU100

class EncodeHandler {
 public:
#ifdef CNSTK_MLU100
  void ReceivePacket(CN_VENC_FRAME_DATA_S* packet) {
    // performance infomation callback
    if (NULL != encoder_->attr_.perf_callback) {
      EncodePerfInfo perf_info;
      perf_info.transfer_us = packet->output_transfer_delay;
      perf_info.encode_us = packet->encode_delay;
      perf_info.input_transfer_us = packet->input_transfer_delay;
      encoder_->attr_.perf_callback(perf_info);
    }

    // eos callback
    if (0 == packet->frame_size) {
      LOG(INFO, "ReceiveEos()");
      if (NULL != encoder_->attr_.eos_callback) encoder_->attr_.eos_callback();
      return;
    }

    // packet callback
    if (NULL != encoder_->attr_.packet_callback) {
      CnPacket cn_packet;
      cn_packet.data = reinterpret_cast<void*>(packet->vir_addr);
      cn_packet.length = packet->frame_size;
      cn_packet.buf_id = packet->buf_index;
      cn_packet.pts = packet->pts;
      cn_packet.codec_type = encoder_->attr_.codec_type;
      encoder_->attr_.packet_callback(cn_packet);
    }
  }

#elif CNSTK_MLU270
  void ReceivePacket(CNVideoEncode_BitStreamInfo* packet) {
    // packet callback
    if (NULL != encoder_->attr_.packet_callback) {
      CnPacket cn_packet;
      cn_packet.data = reinterpret_cast<void*>(packet->MemoryPtr);
      cn_packet.length = packet->bytesUsed;
      cn_packet.buf_id = packet->Index;
      cn_packet.pts = packet->Pts;
      cn_packet.codec_type = encoder_->attr_.codec_type;
      encoder_->attr_.packet_callback(cn_packet);
    }
  }

  void ReceiveEos() {
    // eos callback
    LOG(INFO, "ReceiveEos()");
    if (NULL != encoder_->attr_.eos_callback) {
      encoder_->attr_.eos_callback();
    }
  }

  void ReceiveEvent(CNVideo_EventType EventType) {
    // event callback
    LOG(INFO, "ReceiveEvent(type:" + std::to_string(EventType) + ")");
  }

#endif  // CNSTK_MLU100

  EasyEncode* encoder_ = nullptr;

  int64_t handle_ = -1;

#ifdef CNSTK_MLU100
  void* packet_ptr_ = nullptr;
#endif  // CNSTK_MLU100
};      // class EncodeHandler

#ifdef CNSTK_MLU100

static void ReceivePacket(CN_VENC_FRAME_DATA_S* packet, void* udata) {
  auto handler = reinterpret_cast<EncodeHandler*>(udata);
  handler->ReceivePacket(packet);
}

#elif CNSTK_MLU270

static int PacketHandler(void* udata, CNVideoEncode_BitStreamInfo* packet) {
  auto handler = reinterpret_cast<EncodeHandler*>(udata);
  handler->ReceivePacket(packet);
  return 0;
}

static int32_t EventHandler(CNVideo_EventType type, void* udata) {
  auto handler = reinterpret_cast<EncodeHandler*>(udata);
  if (type == CNVideo_Event_EOS) {
    handler->ReceiveEos();
  } else {
    handler->ReceiveEvent(type);
  }

  return 0;
}

#endif  // CNSTK_MLU100

EasyEncode* EasyEncode::Create(const Attr& attr) {
#ifdef CNSTK_MLU100
  // init cncodec
  CncodecInitTool* init_tools = CncodecInitTool::instance();
  try {
    init_tools->init();
  } catch (CncodecInitToolError& err) {
    throw EasyEncodeError(err.what());
  }

  // 1. encoder create parameters
  CN_VENC_CREATE_ATTR_S create_params;
  memset(&create_params, 0, sizeof(CN_VENC_CREATE_ATTR_S));

  // 1.1 codec type. only support h264 and JPEG
  if (attr.codec_type != CodecType::H264 && attr.codec_type != CodecType::JPEG) {
    throw EasyEncodeError("Encoder only support codec type h264 and jpeg");
  }
  create_params.VideoCodecType = to_CT_E(attr.codec_type);

  // 1.2 rate control parameters for h264
  const RateControl& rate_params = attr.rate_control;
  if (CodecType::H264 == attr.codec_type) {
    if (rate_params.gop == 0) throw EasyEncodeError("Encoder gop must > 0");
    create_params.H264CBR.u32Gop = rate_params.gop;
    create_params.H264CBR.u32StatTime = rate_params.stat_time;
    if (!rate_params.vbr) {
      create_params.rate_control_mode = CBR;
      if (rate_params.bit_rate == 0) throw EasyEncodeError("Encoder stream bitrate number must > 0");
      create_params.H264CBR.u32BitRate = rate_params.bit_rate;
      if (rate_params.src_frame_rate_num == 0) throw EasyEncodeError("Encoder src frame rate numerator must > 0");
      if (rate_params.src_frame_rate_den == 0) throw EasyEncodeError("Encoder src frame rate denominator must > 0");
      create_params.H264CBR.u32SrcFrmRate = rate_params.src_frame_rate_num / rate_params.src_frame_rate_den;
      if (rate_params.dst_frame_rate_num == 0) throw EasyEncodeError("Encoder dst frame rate numerator must > 0");
      if (rate_params.dst_frame_rate_den == 0) throw EasyEncodeError("Encoder dst frame rate denominator must > 0");
      create_params.H264CBR.fr32DstFrmRate = rate_params.dst_frame_rate_num / rate_params.dst_frame_rate_den;
      create_params.H264CBR.u32FluctuateLevel = rate_params.fluctuate_level;
    } else {
      create_params.rate_control_mode = VBR;
      create_params.H264VBR.u32SrcFrmRate = rate_params.src_frame_rate_num / rate_params.src_frame_rate_den;
      create_params.H264VBR.fr32DstFrmRate = rate_params.dst_frame_rate_num / rate_params.dst_frame_rate_den;
      create_params.H264VBR.u32MaxBitRate = rate_params.max_bit_rate;
      create_params.H264VBR.u32MaxQp = rate_params.max_qp;
      create_params.H264VBR.u32MinQp = rate_params.min_qp;
    }
  }

  // 1.3 geometrys
  create_params.u32MaxWidth = attr.maximum_geometry.w;
  create_params.u32MaxHeight = attr.maximum_geometry.h;

  create_params.u32TargetWidth = attr.output_geometry.w;
  create_params.u32TargetHeight = attr.output_geometry.h;
  if (CodecType::H264 == attr.codec_type) {
    if (create_params.u32MaxWidth != create_params.u32TargetWidth ||
        create_params.u32MaxHeight != create_params.u32TargetHeight)
      throw EasyEncodeError("H264 max w/h must equal output w/h");
  }

  // 1.4 input pixel format and profile/qulity
  create_params.pixel_format = to_PF_E(attr.pixel_format);
  if (CodecType::H264 == attr.codec_type) {
    create_params.h264_profile = to_Profile_E(attr.profile);
  } else {
    if (attr.jpeg_qfactor < 1 || attr.jpeg_qfactor > 99) throw EasyEncodeError("Invalid jpeg_qfactor");
    create_params.jpeg_qfactor = attr.jpeg_qfactor;
  }
  create_params.bcolor2gray = static_cast<CN_BOOL>(attr.color2gray);

  // 1.5 crop params
  create_params.encode_crop.bEnable = static_cast<CN_BOOL>(attr.crop_config.enable);
  create_params.encode_crop.crop_rect.u32X = attr.crop_config.x;
  create_params.encode_crop.crop_rect.u32Y = attr.crop_config.y;
  create_params.encode_crop.crop_rect.u32Width = attr.crop_config.w;
  create_params.encode_crop.crop_rect.u32Height = attr.crop_config.h;

  // 1.6 output buffers
  if (attr.packet_buffer_num == 0) throw EasyEncodeError("Encoder output buffers number must > 0");
  create_params.mluP2pAttr.p_buffers = new CN_MLU_P2P_BUFFER_S[attr.packet_buffer_num];
  create_params.mluP2pAttr.buffer_num = attr.packet_buffer_num;

  int frame_size = attr.output_geometry.w * attr.output_geometry.h * 3;
  void* packet_ptr = nullptr;

  // 1.6.1 alloc contiguous memory
  if (attr.output_on_cpu) {
    // output on cpu
    create_params.mluP2pAttr.buffer_type = CN_CPU_BUFFER;
    packet_ptr = static_cast<void*>(new uint8_t[frame_size * attr.packet_buffer_num]);
  } else {
    // output on mlu
    create_params.mluP2pAttr.buffer_type = CN_MLU_BUFFER;
    frame_size = ALIGN(frame_size, 64 * 1024);

    int type = CNRT_MALLOC_EX_PARALLEL_FRAMEBUFFER;
    int dp = 1;
    int frame_num = attr.packet_buffer_num;
    void* cnrt_param = nullptr;
    cnrtRet_t ecode = cnrtAllocParam(&cnrt_param);
    if (nullptr == cnrt_param) {
      delete[] create_params.mluP2pAttr.p_buffers;
      throw EasyEncodeError("Alloc p2p param failed. Error code: " + to_string(ecode));
    }
    string attr_name = "type";
    cnrtAddParam(cnrt_param, const_cast<char*>(attr_name.c_str()), sizeof(type), &type);
    attr_name = "data_parallelism";
    cnrtAddParam(cnrt_param, const_cast<char*>(attr_name.c_str()), sizeof(dp), &dp);
    attr_name = "frame_num";
    cnrtAddParam(cnrt_param, const_cast<char*>(attr_name.c_str()), sizeof(frame_num), &frame_num);
    attr_name = "frame_size";
    cnrtAddParam(cnrt_param, const_cast<char*>(attr_name.c_str()), sizeof(frame_size), &frame_size);
    ecode = cnrtMallocBufferEx(&packet_ptr, cnrt_param);
    if (CNRT_RET_SUCCESS != ecode) {
      delete[] create_params.mluP2pAttr.p_buffers;
      throw EasyEncodeError("Malloc MLU p2p buffer failed. Error code: " + to_string(ecode));
    }
    cnrtDestoryParam(cnrt_param);
  }

  /******************
   * dangerous when there is no enough memory on cpu
   ******************/
  assert(packet_ptr != nullptr);

  // 1.6.2 divide contiguous memory into chunks.
  for (uint32_t bi = 0; bi < attr.packet_buffer_num; ++bi) {
    create_params.mluP2pAttr.p_buffers[bi].addr = reinterpret_cast<CN_U64>(packet_ptr) + bi * frame_size;
    create_params.mluP2pAttr.p_buffers[bi].len = frame_size;
  }

  // 1.7 callback
  auto handler = new EncodeHandler;
  create_params.pu64UserData = reinterpret_cast<void*>(handler);
  create_params.pEncodeCallBack = &ReceivePacket;

  // 1.8 choose device
  create_params.u32VencDeviceID = init_tools->CncodecDeviceId(attr.dev_id);

  // 2. create
  if (!attr.silent) {
    VencPrintAttr(&create_params);
  }

  CNResult ecode = CN_MPI_VENC_Create(reinterpret_cast<CN_U64*>(&handler->handle_), &create_params);
  if (CN_SUCCESS != ecode) {
    delete handler;
    delete[] create_params.mluP2pAttr.p_buffers;
    if (attr.output_on_cpu) {
      auto ptr = static_cast<uint8_t*>(packet_ptr);
      delete[] ptr;
    } else {
      cnrtRet_t ecode = cnrtFree(packet_ptr);
      if (CNRT_RET_SUCCESS != ecode) {
        LOG(ERROR, "cnrtFree failed. Error code: %d", ecode);
      }
    }
    throw EasyEncodeError("Create Encoder failed, Error code: " + to_string(ecode));
  }

  // 3. free useless resources
  delete[] create_params.mluP2pAttr.p_buffers;

  // 4. create EasyEncode
  auto encoder = new EasyEncode(attr, handler);
  handler->packet_ptr_ = packet_ptr;
  encoder->handler_->encoder_ = encoder;

  return encoder;

#elif CNSTK_MLU270
  // 1. create params
  CNVideoEncode_Create_Params create_params;
  memset(&create_params, 0, sizeof(CNVideoEncode_Create_Params));

  // 1.1 codec type. support H264/H265/JPEG
  create_params.codecType = to_M200_CT_E(attr.codec_type);
  if (CodecType::H264 != attr.codec_type && CodecType::H265 != attr.codec_type && CodecType::JPEG != attr.codec_type) {
    throw EasyEncodeError("Encoder only support format H264/H265/JPEG");
  }

  // 1.2 rate control params
  if (CodecType::H264 == attr.codec_type || CodecType::H265 == attr.codec_type) {
    create_params.RCParams.gopLength = attr.rate_control.gop;
    if (!attr.rate_control.vbr) {
      create_params.RCParams.rcMode = CNVideoEncode_RC_Mode_CBR;
    } else {
      create_params.RCParams.rcMode = CNVideoEncode_RC_Mode_VBR;
    }
    create_params.RCParams.targetBitrate = attr.rate_control.bit_rate;
    create_params.RCParams.peakBitrate = attr.rate_control.max_bit_rate;
  }

  create_params.frameRateNum = attr.rate_control.dst_frame_rate_num;
  create_params.frameRateDen = attr.rate_control.dst_frame_rate_den;

  // 1.3 geometrys
  create_params.Width = attr.maximum_geometry.w;
  create_params.Height = attr.maximum_geometry.h;

  // 1.4 pixel format
  create_params.bufferFmt = to_M200_PF_E(attr.pixel_format);

  // 1.5 buffers
  create_params.memoryType = CNVideoMemory_Allocate;
  create_params.numInputBuf = 5;
  create_params.numBitFrameBuf = 5;

  // 2. new handler
  auto handler = new EncodeHandler;

  // 3. create
  if (!attr.silent) {
    VencPrintCreateAttr(&create_params);
  }

  CNVideoCodec codec_type = to_M200_CT_E(attr.codec_type);
  int ecode = CN_Encode_Create(reinterpret_cast<void**>(&handler->handle_), codec_type, &PacketHandler, &EventHandler,
                               reinterpret_cast<void*>(handler));
  if (CNVideoCodec_Success != ecode) {
    delete handler;
    throw EasyEncodeError("Create encoder failed. Error code: " + to_string(ecode));
  }
  ecode = CN_Encode_Initialize(reinterpret_cast<CNVideo_Encode>(handler->handle_), &create_params);
  if (CNVideoCodec_Success != ecode) {
    delete handler;
    throw EasyEncodeError("Initialize encoder failed. Error code: " + to_string(ecode));
  }

  // 4. create EasyEncode
  auto encoder = new EasyEncode(attr, handler);
  encoder->handler_->encoder_ = encoder;

  return encoder;
#endif  // CNSTK_MLU100
}

EasyEncode::EasyEncode(const Attr& attr, EncodeHandler* handler) {
  attr_ = attr;
  handler_ = handler;
}
EasyEncode::~EasyEncode() {
#ifdef CNSTK_MLU100
  // 1. destroy encoder
  if (-1 != handler_->handle_) {
    CNResult ecode = CN_MPI_VENC_Destroy(handler_->handle_);
    if (CN_SUCCESS != ecode) {
      LOG(ERROR, "Encoder destroy failed. Error code: %d", ecode);
    }
  }

  // 2. free packet buffers
  if (nullptr != handler_->packet_ptr_) {
    if (attr_.output_on_cpu) {
      auto ptr = static_cast<uint8_t*>(handler_->packet_ptr_);
      delete[] ptr;
    } else {
      cnrtRet_t ecode = cnrtFree(handler_->packet_ptr_);
      if (CNRT_RET_SUCCESS != ecode) {
        LOG(ERROR, "cnrtFree failed. Error code: %d", ecode);
      }
    }
    handler_->packet_ptr_ = nullptr;
  }
#elif CNSTK_MLU270
  // 1. destroy encoder
  if (handler_->handle_ != -1) {
    int ecode = CN_Encode_Destroy(reinterpret_cast<CNVideo_Encode>(handler_->handle_));
    if (CNVideoCodec_Success != ecode) {
      LOG(ERROR, "Destroy encoder failed. Error code: %d", ecode);
    }
  }
#endif  // CNSTK_MLU100

  // 2. free members
  delete handler_;
}

bool EasyEncode::SendData(const CnFrame& frame, bool eos) {
#ifdef CNSTK_MLU100
  CN_VIDEO_PIC_PARAM_S params;
  memset(&params, 0, sizeof(CN_VIDEO_PIC_PARAM_S));
  if (frame.frame_size > 0) {
    params.pBitstreamData = reinterpret_cast<CN_U64>(frame.ptrs[0]);
    params.nBitstreamDataLen = frame.frame_size;
    params.u64Pts = frame.pts;
    params.u32Width = frame.width;
    params.u32Height = frame.height;

    CNResult ecode = CN_MPI_VENC_Send(handler_->handle_, &params);
    if (CN_SUCCESS != ecode) {
      throw EasyEncodeError("Send packet failed, Error code: " + to_string(ecode));
    }
  }
  if (eos) {
    memset(&params, 0, sizeof(CN_VIDEO_PIC_PARAM_S));
    CNResult ecode = CN_MPI_VENC_Send(handler_->handle_, &params);
    if (CN_SUCCESS != ecode) {
      throw EasyEncodeError("Send packet failed, Error code: " + to_string(ecode));
    }
  }
#elif CNSTK_MLU270
  // 1. alloc input buffer
  CNVideoEncode_Input_Buffer input_buffer;
  int buf_id;
  int ecode =
      CN_Encode_Available_InputBuffer(reinterpret_cast<CNVideo_Encode>(handler_->handle_), &buf_id, &input_buffer);
  if (CNVideoCodec_Success != ecode) {
    throw EasyEncodeError("Avaliable input buffer failed. Error code: " + to_string(ecode));
  }

  // 2. copy data to codec
  uint32_t frame_size = frame.width * frame.height;
  if (frame.frame_size > 0) {
    cnrtRet_t cnrt_ecode = CNRT_RET_SUCCESS;
    switch (attr_.pixel_format) {
      case PixelFmt::YUV420SP_NV12:
      case PixelFmt::YUV420SP_NV21:
        cnrt_ecode = cnrtMemcpy(reinterpret_cast<void*>(input_buffer.inputBuffer[0]), frame.ptrs[0],
                                frame.width * frame.height, CNRT_MEM_TRANS_DIR_HOST2DEV);
        if (CNRT_RET_SUCCESS != cnrt_ecode) {
          throw EasyEncodeError("copy luminance failed. Error code: " + to_string(cnrt_ecode));
        }
        cnrt_ecode = cnrtMemcpy(reinterpret_cast<void*>(input_buffer.inputBuffer[1]),
                                reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(frame.ptrs[0]) + frame_size),
                                frame_size / 2, CNRT_MEM_TRANS_DIR_HOST2DEV);
        if (CNRT_RET_SUCCESS != cnrt_ecode) {
          throw EasyEncodeError("copy chroma failed. Error code: " + to_string(cnrt_ecode));
        }
        break;
      /*
            case PixelFmt::YUV420SP_I420:
              cnrt_ecode = cnrtMemcpy(
                  reinterpret_cast<void *>(input_buffer.inputBuffer[0]),
                  frame.ptrs[0],
                  frame.width * frame.height,
                  CNRT_MEM_TRANS_DIR_HOST2DEV);
              if (CNRT_RET_SUCCESS != cnrt_ecode) {
                throw EasyEncodeError("copy luminance failed. Error code: "
                    + to_string(cnrt_ecode));
              }
              cnrt_ecode = cnrtMemcpy(
                  reinterpret_cast<void *>(input_buffer.inputBuffer[1]),
                  reinterpret_cast<void *>(reinterpret_cast<uint8_t*>(frame.ptrs[0]) + frame_size),
                  frame_size / 4, CNRT_MEM_TRANS_DIR_HOST2DEV);
              if (CNRT_RET_SUCCESS != cnrt_ecode) {
                throw EasyEncodeError("copy chroma 0 failed. Error code: "
                    + to_string(ecode));
              }
              cnrt_ecode = cnrtMemcpy(
                  reinterpret_cast<void *>(input_buffer.inputBuffer[2]),
                  reinterpret_cast<void *>(reinterpret_cast<uint8_t*>(frame.ptrs[0]) + frame_size * 5 / 4),
                  frame_size / 4, CNRT_MEM_TRANS_DIR_HOST2DEV);
              if (CNRT_RET_SUCCESS != cnrt_ecode) {
                throw EasyEncodeError("copy chroma 1 failed. Error code: "
                    + to_string(cnrt_ecode));
              }
              break;
      */
      default:
        throw EasyEncodeError("Unsupported pixel format");
        break;
    }
  }

  // 3. params
  CNVideoEncode_PIC_Params params;
  memset(&params, 0, sizeof(CNVideoEncode_PIC_Params));
  if (eos) {
    params.Flags |= CNVideoEncode_PIC_Flag_EOS;
  }
  params.frameIdx = buf_id;
  params.TimeStamp = frame.pts;
  params.Duration = 33;
  params.inputPtr = input_buffer;

  // 4. send
  ecode = CN_Encode_Feed_Frame(reinterpret_cast<CNVideo_Encode>(handler_->handle_), &params);
  if (CNVideoCodec_Success != ecode) {
    throw EasyEncodeError("CN_Encode_Feed_Frame failed. Error code: " + to_string(ecode));
  }
#endif  // CNSTK_MLU100
  return true;
}

void EasyEncode::ReleaseBuffer(uint32_t buf_id) {
#ifdef CNSTK_MLU100
  CNResult ecode = CN_MPI_MLU_P2P_ReleaseBuffer(handler_->handle_, buf_id);
  if (CN_SUCCESS != ecode) {
    throw EasyEncodeError("Release buffer failed. Error code: " + to_string(ecode));
  }
#elif CNSTK_MLU270
#endif  // CNSTK_MLU100
}

bool EasyEncode::CopyPacket(void* dst, const CnPacket& packet) {
  if (attr_.output_on_cpu) {
    memcpy(dst, packet.data, packet.length);
    return true;
  }

  // output on mlu
  cnrtRet_t ecode = cnrtMemcpy(dst, packet.data, packet.length, CNRT_MEM_TRANS_DIR_DEV2HOST);
  if (CNRT_RET_SUCCESS != ecode) {
    throw EasyEncodeError("Copy output packet failed. Error code: " + to_string(ecode));
  }
  return true;
}

}  // namespace edk
