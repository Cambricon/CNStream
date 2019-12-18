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
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <sstream>

#include "easyinfer/mlu_context.h"

#ifdef CNSTK_MLU100

#include <cncodec.h>
#include "init_tools.h"

#define ALIGN(addr, boundary) (((u32_t)(addr) + (boundary)-1) & ~((boundary)-1))

#elif CNSTK_MLU270

#include <cn_video_dec.h>
#include <thread>
#endif  // CNSTK_MLU

#include <cmath>
#include <mutex>
#include <string>
#include "cxxutil/logger.h"
#include "easycodec/easy_decode.h"

using std::string;
using std::to_string;
using std::mutex;
using std::unique_lock;

namespace edk {

static std::mutex g_create_mutex;

#ifdef CNSTK_MLU100

static CN_PIXEL_FORMAT_E to_CN_PIXEL_FORMAT_E(PixelFmt format) {
  switch (format) {
    case PixelFmt::YUV420SP_NV21:
      return CN_PIXEL_FORMAT_YUV420SP;
    case PixelFmt::BGR24:
      return CN_PIXEL_FORMAT_BGR24;
    case PixelFmt::RGB24:
      return CN_PIXEL_FORMAT_RGB24;
    default:
      throw edk::EasyDecodeError("Unsupport pixel format");
  }
  return CN_PIXEL_FORMAT_YUV420SP;
}

static CN_VIDEO_CODEC_TYPE_E to_CN_VIDEO_CODEC_TYPE_E(CodecType type) {
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
      throw edk::EasyDecodeError("Unsupport video codec");
  }
  return CN_VIDEO_CODEC_H264;
}

static CN_VIDEO_MODE_E to_CN_VIDEO_MODE_E(VideoMode mode) {
  switch (mode) {
    case VideoMode::FRAME_MODE:
      return CN_VIDEO_MODE_FRAME;
    case VideoMode::STREAM_MODE:
      return CN_VIDEO_MODE_STREAM;
    default:
      throw edk::EasyDecodeError("Unsupport video mode");
  }
  return CN_VIDEO_MODE_FRAME;
}

static void PrintAttr(CN_VIDEO_CREATE_ATTR_S* p_attr) {
  printf("%-32s%s\n", "param", "value");
  printf("-------------------------------------\n");
  printf("%-32s%u\n", "u32VdecDeviceID", p_attr->u32VdecDeviceID);
  printf("%-32s%d\n", "enInputVideoCodec", p_attr->enInputVideoCodec);
  printf("%-32s%u\n", "u32MaxWidth", p_attr->u32MaxWidth);
  printf("%-32s%u\n", "u32MaxHeight", p_attr->u32MaxHeight);
  printf("%-32s%u\n", "u32TargetWidth", p_attr->u32TargetWidth);
  printf("%-32s%u\n", "u32TargetHeight", p_attr->u32TargetHeight);
  printf("%-32s%u\n", "u32TargetWidthSubstream", p_attr->u32TargetWidthSubstream);
  printf("%-32s%u\n", "u32TargetHeightSubstream", p_attr->u32TargetHeightSubstream);
  printf("%-32s%u\n", "u32MaxFrameSize", p_attr->u32MaxFrameSize);
  printf("%-32s%u\n", "u32EsBufCount", p_attr->u32EsBufCount);
  printf("%-32s%u\n", "u32ImageBufCount", p_attr->u32ImageBufCount);
  printf("%-32s%d\n", "enOutputPixelFormat", p_attr->enOutputPixelFormat);
  printf("%-32s%d\n", "enVideoCreateMode", p_attr->enVideoCreateMode);
  printf("%-32s%d\n", "stFrameRate.bEnable", p_attr->stPostProcessAttr.stFrameRate.bEnable);
  printf("%-32s%d\n", "s32SrcFrmRate", p_attr->stPostProcessAttr.stFrameRate.s32SrcFrmRate);
  printf("%-32s%d\n", "s32DstFrmRate", p_attr->stPostProcessAttr.stFrameRate.s32DstFrmRate);
}

static void FrameHandler(CN_VIDEO_IMAGE_INFO_S* frame, CN_U64 udata);

#elif CNSTK_MLU270

static CNVideoCodec to_CNVideoCodecType(CodecType type) {
  switch (type) {
    case CodecType::MPEG4:
      return CNVideoCodec_MPEG4;
    case CodecType::H264:
      return CNVideoCodec_H264;
    case CodecType::H265:
      return CNVideoCodec_HEVC;
    case CodecType::JPEG:
      return CNVideoCodec_JPEG;
    default:
      throw edk::EasyDecodeError("Unsupport video codec");
  }
  return CNVideoCodec_H264;
}

static CNVideoSurfaceFormat to_CNVideoSurfaceFormat(const PixelFmt& pixel_format) {
  CNVideoSurfaceFormat ret;
  switch (pixel_format) {
    case PixelFmt::YUV420SP_NV12:
      ret = CNVideoSurfaceFormat_NV12;
      break;
    case PixelFmt::YUV420SP_NV21:
      ret = CNVideoSurfaceFormat_NV21;
      break;
    default:
      throw edk::EasyDecodeError("Unsupport pixel format");
      break;
  }
  return ret;
}

static void PrintCreateAttr(CNCREATEVIDEODECODEINFO* p_attr) {
  printf("%-32s%s\n", "param", "value");
  printf("-------------------------------------\n");
  printf("%-32s%u\n", "Codectype", p_attr->Codectype);
  printf("%-32s%u\n", "Instance", p_attr->Instance);
  printf("%-32s%u\n", "CardID", p_attr->CardID);
  printf("%-32s%u\n", "MemoryAllocate", p_attr->MemoryAllocate);
  printf("%-32s%u\n", "ChromaFormat", p_attr->ChromaFormat);
  printf("%-32s%u\n", "Progressive", p_attr->Progressive);
  printf("%-32s%lu\n", "Width", p_attr->Width);
  printf("%-32s%lu\n", "Height", p_attr->Height);
  printf("%-32s%lu\n", "BitDepthMinus8", p_attr->BitDepthMinus8);
  printf("%-32s%d\n", "Output format", p_attr->OutputFormat);
  printf("%-32s%d\n", "Batch", p_attr->Batch);
  printf("%-32s%lu\n", "NumOfInputSurfaces", p_attr->NumOfInputSurfaces);
  printf("%-32s%lu\n", "NumOfOutputSurfaces", p_attr->NumOfOutputSurfaces);
}
static int FrameHandler(void* udata, CNPICTUREINFO* frame);
static int SequenceHandler(void* udata, CNVIDEOFORMAT* format);
static int EventHandler(CNVideo_EventType type, void* udata);

#endif  // CNSTK_MLU100

class DecodeHandler {
 public:
  explicit DecodeHandler(EasyDecode* decoder) : decoder_(decoder), packets_count_(0), frames_count_(0) {}

  ~DecodeHandler();

  std::pair<bool, std::string> Init(const EasyDecode::Attr& attr);

#ifdef CNSTK_MLU100

  void ReceiveFrame(CN_VIDEO_IMAGE_INFO_S* frame) {
    // 1. handle decoder status
    // 1.1 eos
    if (0 == frame->u32FrameSize) {
      if (NULL != decoder_->attr_.eos_callback) {
        decoder_->attr_.eos_callback();
      }
      // release eos buffer immediately
      decoder_->ReleaseBuffer(frame->u32BufIndex);
      unique_lock<mutex> status_lk(status_mtx_);
      status_ = EasyDecode::Status::EOS;
      return;
    }
    // 1.2 check pause status.
    if (EasyDecode::Status::PAUSED == status_) {
      unique_lock<mutex> status_lk(status_mtx_);
      status_cond_.wait(status_lk, [this]() -> bool { return EasyDecode::Status::RUNNING == status_; });
    }

    // 2. config CnFrame for callbacks
    CnFrame finfo;
    finfo.buf_id = static_cast<uint32_t>(frame->u32BufIndex);
    /******************************************
     * pts can not pass through decode when use VideoMode::STREAM_MODE.
     * use incremental pts to do an alternative solution.
     * incremental pts is maintained by DecodeHandler.
     ******************************************/
    finfo.pts = frame->u64FrameIndex;
    if (VideoMode::STREAM_MODE == decoder_->GetAttr().video_mode) {
      finfo.pts = incmt_pts_;
    }
    finfo.height = frame->u32Height;
    finfo.width = frame->u32Width;
    finfo.frame_size = frame->u32FrameSize;
    switch (frame->enPixelFormat) {
      case CN_PIXEL_FORMAT_YUV420SP:
        finfo.n_planes = 2;
        finfo.strides[0] = frame->u32Stride[0];
        finfo.strides[1] = frame->u32Stride[1];
        finfo.ptrs[0] = reinterpret_cast<void*>(frame->u64VirAddr);
        finfo.ptrs[1] = reinterpret_cast<void*>(frame->u64VirAddr + frame->u32Height * frame->u32Stride[0]);
        finfo.pformat = PixelFmt::YUV420SP_NV21;
        break;
      case CN_PIXEL_FORMAT_RGB24:
      case CN_PIXEL_FORMAT_BGR24:
        finfo.n_planes = 1;
        finfo.strides[0] = frame->u32Stride[0];
        finfo.ptrs[0] = reinterpret_cast<void*>(frame->u64VirAddr);
        if (frame->enPixelFormat == CN_PIXEL_FORMAT_RGB24)
          finfo.pformat = PixelFmt::RGB24;
        else
          finfo.pformat = PixelFmt::BGR24;
        break;
      default:
        finfo.pformat = PixelFmt::NON_FORMAT;
        break;
    }

    // 3 enter user defined callback.
    // 3.1 show decode performance infomations to user
    /**
     * always show performance informations in mainstream.
     */
    if (!substream_ && NULL != decoder_->attr_.perf_callback) {
      DecodePerfInfo perfinfo;
      perfinfo.transfer_us = frame->u64TransferUs;
      perfinfo.decode_us = frame->u64DecodeDelayUs;
      perfinfo.total_us = frame->u64InputUs;
      decoder_->attr_.perf_callback(perfinfo);
    }

    // 3.2 user defined frame callback.
    if (substream_ && NULL != decoder_->attr_.substream_callback) {
      decoder_->attr_.substream_callback(finfo);
      frames_count_++;
    } else if (!substream_ && NULL != decoder_->attr_.frame_callback) {
      decoder_->attr_.frame_callback(finfo);
      frames_count_++;
    } else {
      // no callback, release buffer right now.
      decoder_->ReleaseBuffer(finfo.buf_id);
    }

    // 4. update incremental pts in substream
    if (decoder_->SubstreamEnabled()) {
      if (substream_) {
        incmt_pts_++;
      }
    } else {
      incmt_pts_++;
    }

    // 5. update substream flag.
    if (decoder_->SubstreamEnabled()) {
      substream_ = !substream_;
    }
  }

#elif CNSTK_MLU270

  void ReceiveFrame(CNPICTUREINFO* frame) {
    // 1. handle decoder status
    if (EasyDecode::Status::PAUSED == status_) {
      unique_lock<mutex> status_lk(status_mtx_);
      status_cond_.wait(status_lk, [this]() -> bool { return EasyDecode::Status::RUNNING == status_; });
    }

    // MLU270 do not support perf information callback

    // 2. config CnFrame for user callback.
    CnFrame finfo;
    finfo.channel_id = channel_;
    finfo.buf_id = frame->Index;
    finfo.pts = frame->Pts;
    if (CodecType::JPEG == decoder_->attr_.codec_type) {
      finfo.height = frame->JpegExt.Height;
      finfo.width = frame->JpegExt.Width;
      if ((finfo.height == 0 || finfo.width == 0) && (frame->Flag & (1 << CNVideoPacket_EOS))) {
        LOG(INFO, "Got empty jpeg frame with EOS");
        return;
      }
    } else {
      finfo.height = decoder_->attr_.output_geometry.h;
      finfo.width = decoder_->attr_.output_geometry.w;
    }
    finfo.n_planes = frame->Planes;
    uint32_t plane_size_den[CN_MAXIMUM_PLANE] = {0};
    if (decoder_->attr_.pixel_format == PixelFmt::YUV420SP_NV21 ||
        decoder_->attr_.pixel_format == PixelFmt::YUV420SP_NV12) {
      if (frame->Planes != 2) {
        LOG(WARNING, "NV21/NV12 planes != 2");
        return;
      }
      plane_size_den[0] = 1;
      plane_size_den[1] = 2;
    }
    finfo.frame_size = 0;
    for (uint32_t pi = 0; pi < frame->Planes; ++pi) {
      finfo.strides[pi] = frame->Stride[pi];
      finfo.ptrs[pi] = reinterpret_cast<void*>(frame->MemoryPtr[pi]);
      if (plane_size_den[pi] != 0) finfo.frame_size += frame->Stride[pi] * finfo.height / plane_size_den[pi];
    }
    finfo.pformat = decoder_->attr_.pixel_format;

    if (NULL != decoder_->attr_.frame_callback) {
      CN_Decode_AddRef(reinterpret_cast<CNVideo_Decode>(handle_), frame->Index);
      decoder_->attr_.frame_callback(finfo);
      frames_count_++;
    }
  }

  int ReceiveSequence(CNVIDEOFORMAT* format) {
    LOG(INFO, "Receive sequence");
    const EasyDecode::Attr& attr = decoder_->GetAttr();
    params_.Codectype = format->CodecType;
    params_.ChromaFormat = format->ChromaFormat;
    params_.MemoryAllocate = CNVideoMemory_Allocate;
    params_.Width = format->Width;
    params_.Height = format->Height;
    params_.NumOfOutputSurfaces =
        format->MinOutBufNum > params_.NumOfOutputSurfaces ? format->MinOutBufNum : params_.NumOfOutputSurfaces;
    params_.OutputFormat = to_CNVideoSurfaceFormat(attr.pixel_format);
    params_.pUserPtr = reinterpret_cast<void*>(this);
    channel_ = static_cast<int>(format->CnrtChannel);
    int ecode = CN_Decode_Start(reinterpret_cast<CNVideo_Decode>(handle_), &params_);
    if (ecode < 0) {
      LOG(ERROR, "Start Decoder failed.");
      return -1;
    }
    return 0;
  }

  void ReceiveEos() {
    std::ostringstream ss;
    ss << "Thread id: " << std::this_thread::get_id() << ",Received EOS from cncodec";
    LOG(INFO, ss.str());

    if (NULL != decoder_->attr_.eos_callback) {
      decoder_->attr_.eos_callback();
    }
    unique_lock<mutex> status_lk(status_mtx_);
    status_ = EasyDecode::Status::EOS;

    unique_lock<mutex> eos_lk(eos_mtx_);
    got_eos_ = true;
    eos_cond_.notify_one();
  }

  void ReceiveEvent(CNVideo_EventType EventType) {
    // event callback
    std::ostringstream ss;
    ss << "Thread id: " << std::this_thread::get_id() << ",ReceiveEvent(type:" << EventType << ")";
    LOG(INFO, ss.str());
  }

#endif  // CNSTK_MLU100

  friend class EasyDecode;
  EasyDecode* decoder_ = nullptr;
  // cncodec handle
  int64_t handle_ = -1;

  uint32_t packets_count_ = 0;
  uint32_t frames_count_ = 0;

  EasyDecode::Status status_ = EasyDecode::Status::RUNNING;
  std::mutex status_mtx_;
  std::condition_variable status_cond_;

  /// eos workarround
  std::mutex eos_mtx_;
  std::condition_variable eos_cond_;
  bool send_eos_ = false;
  bool got_eos_ = false;

#ifdef CNSTK_MLU100

  /**
   * lu buffers, alloced in EasyDecode::Create and free in ~EasyDecode.
   */
  void* mlu_ptr_ = nullptr;
  /**
   * the first one is mainstream frame, the second one is substream, loop
   */
  bool substream_ = false;
  /**
   * incremental pts
   */
  uint64_t incmt_pts_ = 0;

#elif CNSTK_MLU270

  CNCREATEVIDEODECODEINFO params_;
  int channel_;

#endif  // CNSTR_MLU100
};      // class DecodeHandler

std::pair<bool, std::string> DecodeHandler::Init(const EasyDecode::Attr& attr) {
  status_ = EasyDecode::Status::RUNNING;
#ifdef CNSTK_MLU100
  // 2. init cncodec
  CncodecInitTool* init_tools = CncodecInitTool::instance();
  init_tools->init();

  // 3. prepare decoder create parameters
  // 3.1 common params.
  CN_VIDEO_CREATE_ATTR_S params;
  memset(&params, 0, sizeof(CN_VIDEO_CREATE_ATTR_S));
  params.enInputVideoCodec = to_CN_VIDEO_CODEC_TYPE_E(attr.codec_type);
  params.enVideoMode = to_CN_VIDEO_MODE_E(attr.video_mode);
  params.u32MaxWidth = attr.maximum_geometry.w;
  params.u32MaxHeight = attr.maximum_geometry.h;
  params.u32TargetWidth = attr.output_geometry.w;
  params.u32TargetHeight = attr.output_geometry.h;
  params.u32TargetWidthSubstream = attr.substream_geometry.w;
  params.u32TargetHeightSubstream = attr.substream_geometry.h;
  params.enOutputPixelFormat = to_CN_PIXEL_FORMAT_E(attr.pixel_format);
  params.stPostProcessAttr.stFrameRate.bEnable = CN_TRUE;
  const int src_fr = 30;
  params.stPostProcessAttr.stFrameRate.s32SrcFrmRate = src_fr;
  params.stPostProcessAttr.stFrameRate.s32DstFrmRate = std::ceil(src_fr * (1 - attr.drop_rate));
  if (params.u32TargetWidth * params.u32TargetHeight <
      params.u32TargetWidthSubstream * params.u32TargetHeightSubstream) {
    return std::make_pair(false,
                          "mainstream's resulotion should "
                          "not be lower than substream");
  }
  // 3.2 params about mlu buffers.
  int frame_num = attr.frame_buffer_num;
  int type = CNRT_MALLOC_EX_PARALLEL_FRAMEBUFFER;
  int dp = 1;
  int frame_size = attr.output_geometry.w * attr.output_geometry.h;
  switch (attr.pixel_format) {
    case PixelFmt::YUV420SP_NV21:
      frame_size *= 1.5;
      break;
    case PixelFmt::RGB24:
    case PixelFmt::BGR24:
      frame_size *= 3;
      break;
    default:
      return std::make_pair(false, "unsupported pixel format");
  }
  frame_size = ALIGN(frame_size, 64 * 1024);
  params.mluP2pAttr.buffer_num = frame_num;
  params.mluP2pAttr.p_buffers = new CN_MLU_P2P_BUFFER_S[frame_num];
  void* cnrt_param = nullptr;
  cnrtRet_t ecode = cnrtAllocParam(&cnrt_param);
  if (nullptr == cnrt_param) {
    return std::make_pair(false, "Alloc p2p param failed. Error code: " + std::to_string(ecode));
  }
  string param_name = "type";
  cnrtAddParam(cnrt_param, const_cast<char*>(param_name.c_str()), sizeof(type), &type);
  param_name = "data_parallelism";
  cnrtAddParam(cnrt_param, const_cast<char*>(param_name.c_str()), sizeof(dp), &dp);
  param_name = "frame_num";
  cnrtAddParam(cnrt_param, const_cast<char*>(param_name.c_str()), sizeof(frame_num), &frame_num);
  param_name = "frame_size";
  cnrtAddParam(cnrt_param, const_cast<char*>(param_name.c_str()), sizeof(frame_size), &frame_size);
  ecode = cnrtMallocBufferEx(&mlu_ptr_, cnrt_param);
  if (CNRT_RET_SUCCESS != ecode) {
    delete[] params.mluP2pAttr.p_buffers;
    return std::make_pair(false, "Malloc p2p buffer failed. Error code: " + std::to_string(ecode));
  }
  cnrtDestoryParam(cnrt_param);
  for (CN_U32 buf_id = 0; buf_id < params.mluP2pAttr.buffer_num; ++buf_id) {
    CN_U64 ptr = reinterpret_cast<CN_U64>(mlu_ptr_) + frame_size * buf_id;
    params.mluP2pAttr.p_buffers[buf_id].addr = ptr;
    params.mluP2pAttr.p_buffers[buf_id].len = frame_size;
  }

  // 3.3 callbacks
  params.u64UserData = reinterpret_cast<CN_U64>(this);
  params.pImageCallBack = &FrameHandler;

  // 3.4 codec device
  params.u32VdecDeviceID = init_tools->CncodecDeviceId(attr.dev_id);
  if (!attr.silent) {
    PrintAttr(&params);
  }
  CNResult cncodec_ecode = CN_MPI_VDEC_Create(reinterpret_cast<CN_HANDLE_VDEC*>(&handle_), &params);
  if (CN_SUCCESS != cncodec_ecode) {
    handle_ = -1;
    delete[] params.mluP2pAttr.p_buffers;
    return std::make_pair(false,
                          "Decoder Worker initialize failed, "
                          "can't create decoder, may be there is no "
                          "enough device to support such number of channels, "
                          "Error Code: " +
                              std::to_string(cncodec_ecode));
  }

  // 4. free resources
  delete[] params.mluP2pAttr.p_buffers;

#elif CNSTK_MLU270

  // 1. decoder create parameters.
  memset(&params_, 0, sizeof(CNCREATEVIDEODECODEINFO));
#ifdef INSTANCE_TABLE
  std::unique_lock<std::mutex> lk(g_create_mutex);
  static int _create_cnt = 0;
#ifdef _VPU_SOLUTION_A
  static CNVideoDecode_Instance _instances[] = {
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_0, CNVideoDecode_Instance_0, CNVideoDecode_Instance_0,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_0, CNVideoDecode_Instance_0, CNVideoDecode_Instance_0,
      CNVideoDecode_Instance_1, CNVideoDecode_Instance_1, CNVideoDecode_Instance_1, CNVideoDecode_Instance_1,
      CNVideoDecode_Instance_1, CNVideoDecode_Instance_1, CNVideoDecode_Instance_1, CNVideoDecode_Instance_1,
      CNVideoDecode_Instance_1, CNVideoDecode_Instance_1, CNVideoDecode_Instance_1, CNVideoDecode_Instance_1,
      CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_3, CNVideoDecode_Instance_3,
      CNVideoDecode_Instance_3, CNVideoDecode_Instance_3, CNVideoDecode_Instance_3, CNVideoDecode_Instance_3,
      CNVideoDecode_Instance_3, CNVideoDecode_Instance_3, CNVideoDecode_Instance_3, CNVideoDecode_Instance_3,
      CNVideoDecode_Instance_3, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_5, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_5, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_5, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5};
#else
  static CNVideoDecode_Instance _instances[] = {
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_0, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5, CNVideoDecode_Instance_1,
      CNVideoDecode_Instance_3, CNVideoDecode_Instance_5, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3,
      CNVideoDecode_Instance_5, CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5,
      CNVideoDecode_Instance_1, CNVideoDecode_Instance_3, CNVideoDecode_Instance_5, CNVideoDecode_Instance_5};
#endif
  params_.Instance = _instances[_create_cnt++ % 48];
  lk.unlock();
#else
  params_.Instance = CNVideoDecode_Instance_Auto;
#endif
  params_.Codectype = to_CNVideoCodecType(attr.codec_type);
  params_.ChromaFormat = CNVideoChromaFormat_420;
  params_.MemoryAllocate = CNVideoMemory_Allocate;
  params_.Width = attr.output_geometry.w;
  params_.Height = attr.output_geometry.h;
  params_.NumOfInputSurfaces = attr.input_buffer_num;
  params_.NumOfOutputSurfaces = attr.frame_buffer_num;
  params_.CardID = attr.dev_id;
  params_.Progressive = attr.interlaced ? 0 : 1;
  params_.OutputFormat = to_CNVideoSurfaceFormat(attr.pixel_format);
  params_.BitDepthMinus8 = 0;
  params_.pUserPtr = reinterpret_cast<void*>(this);
  if (CNVideoCodec_JPEG == params_.Codectype) {
    // params_.NumOfOutputSurfaces = 6;
    params_.Batch = 1;
  }

  if (!attr.silent) {
    PrintCreateAttr(&params_);
  }

  CNCodecVersion version;
  CN_Decode_GetVersion(&version);
  printf("%-32s%d.%d\n", "CNCodec Version is", version.Major, version.Minor);
  // 2. create decoder.
  int ecode = CN_Decode_Create(reinterpret_cast<CNVideo_Decode*>(&handle_), &SequenceHandler, &FrameHandler,
                               &EventHandler, &params_);
  if (0 != ecode) {
    return std::make_pair(false, "Create decode failed: " + to_string(ecode));
  }

  /**
   * jpeg has no sequence informations
   * start decode directly.
   */
  if (CNVideoCodec_JPEG == params_.Codectype) {
    ecode = CN_Decode_Start(reinterpret_cast<CNVideo_Decode>(handle_), &params_);
    if (ecode < 0) {
      return std::make_pair(false, "Start JPEG decode failed: " + to_string(ecode));
    }
  }
#endif  // #if CNSTK_MLU

  return std::make_pair(true, "Init succeed");
}

DecodeHandler::~DecodeHandler() {
  /**
   * Decode destroied. status set to STOP.
   */
  unique_lock<mutex> status_lk(status_mtx_);
  status_ = EasyDecode::Status::STOP;
  status_lk.unlock();
/**
 * Release resources.
 */
#ifdef CNSTK_MLU100
  // 1. destroy decoder
  if (-1 != handle_) {
    CNResult ecode = CN_SUCCESS;
    if ((ecode = CN_MPI_VDEC_Destroy(static_cast<CN_HANDLE_VDEC>(handle_))) != CN_SUCCESS) {
      LOG(ERROR, "Decoder destroy failed Error Code: %d", ecode);
    }
  }
  // 2. free mlu buffers
  if (nullptr != mlu_ptr_) {
    cnrtRet_t ecode = cnrtFree(mlu_ptr_);
    if (CNRT_RET_SUCCESS != ecode) {
      LOG(ERROR, "cnrtFree failed");
    }
  }

#elif CNSTK_MLU270
  eos_mtx_.lock();
  if (!send_eos_ && -1 != handle_) {
    eos_mtx_.unlock();
    LOG(INFO, "Send EOS in destruct");
    CnPacket packet;
    memset(&packet, 0, sizeof(CnPacket));
    decoder_->SendData(packet, true);
  } else {
    got_eos_ = true;
    eos_mtx_.unlock();
  }

  unique_lock<mutex> eos_lk(eos_mtx_);
  if (!got_eos_) {
    eos_cond_.wait(eos_lk, [this]() -> bool { return got_eos_; });
    LOG(INFO, "Received EOS in destruct");
  }

  if (-1 != handle_) {
    int ecode = CN_Decode_Stop(reinterpret_cast<CNVideo_Decode>(handle_));
    if (0 != ecode) {
      LOG(ERROR, "Decoder stop failed Error Code: %d", ecode);
    }
    ecode = CN_Decode_Destroy(reinterpret_cast<CNVideo_Decode>(handle_));
    if (0 != ecode) {
      LOG(ERROR, "Decoder destroy failed Error Code: %d", ecode);
    }
  }
#endif  // CNSTK_MLU100
}

#ifdef CNSTK_MLU100

static void FrameHandler(CN_VIDEO_IMAGE_INFO_S* frame, CN_U64 udata) {
  auto decode_handler = reinterpret_cast<DecodeHandler*>(udata);
  decode_handler->ReceiveFrame(frame);
}

#elif CNSTK_MLU270

static int FrameHandler(void* udata, CNPICTUREINFO* frame) {
  auto handler = reinterpret_cast<DecodeHandler*>(udata);
  handler->ReceiveFrame(frame);
  return 0;
}

static int SequenceHandler(void* udata, CNVIDEOFORMAT* format) {
  auto handler = reinterpret_cast<DecodeHandler*>(udata);
  return handler->ReceiveSequence(format);
}

static int EventHandler(CNVideo_EventType type, void* udata) {
  auto handler = reinterpret_cast<DecodeHandler*>(udata);
  if (type == CNVideo_Event_EOS) {
    handler->ReceiveEos();
  } else {
    handler->ReceiveEvent(type);
  }

  return 0;
}

#endif  // CNSTK_MLU100

EasyDecode::Status EasyDecode::GetStatus() const {
  unique_lock<mutex> lock(handler_->status_mtx_);
  return handler_->status_;
}

EasyDecode* EasyDecode::Create(const Attr& attr) {
  auto decoder = new EasyDecode(attr);
  // init members
  decoder->handler_ = new DecodeHandler(decoder);
  std::pair<bool, std::string> ret = decoder->handler_->Init(attr);
  if (!ret.first) {
    delete decoder->handler_;
    decoder->handler_ = nullptr;
    delete decoder;
    throw EasyDecodeError(ret.second);
  }

  return decoder;
}  // EasyDecode::Create

bool EasyDecode::Pause() {
  unique_lock<mutex> lock(handler_->status_mtx_);
  if (Status::RUNNING == handler_->status_) {
    handler_->status_ = Status::PAUSED;
    return true;
  }
  return false;
}

bool EasyDecode::Resume() {
  unique_lock<mutex> lock(handler_->status_mtx_);
  if (Status::PAUSED == handler_->status_) {
    handler_->status_ = Status::RUNNING;
    handler_->status_cond_.notify_all();
    return true;
  }
  return false;
}

bool EasyDecode::SendData(const CnPacket& packet, bool eos) {
  if (-1 == handler_->handle_) return false;
  // check status
  unique_lock<mutex> lock(handler_->status_mtx_);

  if (Status::PAUSED == handler_->status_) {
    handler_->status_cond_.wait(lock, [this]() -> bool { return Status::RUNNING == handler_->status_; });
  }

#ifdef CNSTK_MLU100

  CN_VIDEO_PIC_PARAM_S params;
  if (packet.data != NULL && packet.length > 0) {
    memset(&params, 0, sizeof(CN_VIDEO_PIC_PARAM_S));
    params.pBitstreamData = reinterpret_cast<CN_U64>(packet.data);
    params.nBitstreamDataLen = packet.length;
    params.u64FrameIndex = packet.pts;

    CNResult ecode = CN_MPI_VDEC_Send(handler_->handle_, &params);
    if (CN_SUCCESS != ecode) {
      throw EasyDecodeError("Send data failed: " + to_string(ecode));
    }
  }

  if (eos) {
    memset(&params, 0, sizeof(CN_VIDEO_PIC_PARAM_S));
    CNResult ecode = CN_MPI_VDEC_Send(handler_->handle_, &params);
    if (CN_SUCCESS != ecode) {
      throw EasyDecodeError("Send eos failed: " + to_string(ecode));
    }
  }

#elif CNSTK_MLU270

  if (packet.length == 0 && !eos) {
    LOG(ERROR, "Packet length is equal to 0. The packet will not be sent.");
    return true;
  }

  CNSTREAMINFO streaminfo;
  if (packet.data != NULL && packet.length > 0) {
    memset(&streaminfo, 0, sizeof(CNSTREAMINFO));
    streaminfo.Data = packet.data;
    streaminfo.BufLength = packet.length;
    streaminfo.BitSize = packet.length;
    streaminfo.Pts = packet.pts;
    streaminfo.Flag = (1 << CNVideoPacket_Timestamp);
    std::ostringstream ss;
    ss << "Thread id: " << std::this_thread::get_id();
    LOG(TRACE, ss.str());
    LOG(TRACE, "Flag: %d", streaminfo.Flag);
    LOG(TRACE, "Stream info: ");
    LOG(TRACE, "Data: %p", streaminfo.Data);
    LOG(TRACE, "Length: %lu", streaminfo.BufLength);
    LOG(TRACE, "PTS: %lu", streaminfo.Pts);

    auto ecode = CN_Decode_FeedData(reinterpret_cast<CNVideo_Decode>(handler_->handle_), &streaminfo, -1);
    if (0 != ecode) {
      throw EasyDecodeError("Send data failed. Error code: " + to_string(ecode));
    }

    handler_->packets_count_++;
  }

  if (eos) {
    unique_lock<mutex> eos_lk(handler_->eos_mtx_);
    if (!handler_->send_eos_) {
      memset(&streaminfo, 0, sizeof(CNSTREAMINFO));
      streaminfo.Flag = (1 << CNVideoPacket_EOS);
    } else {
      return false;
    }
    std::ostringstream ss;
    ss << "Thread id: " << std::this_thread::get_id() << ",Feed EOS data";
    LOG(INFO, ss.str());
    auto ecode = CN_Decode_FeedData(reinterpret_cast<CNVideo_Decode>(handler_->handle_), &streaminfo, -1);
    if (0 != ecode) {
      throw EasyDecodeError("Send EOS failed. Error code: " + to_string(ecode));
    }

    handler_->send_eos_ = true;
  }

#endif

  return true;
}

EasyDecode::EasyDecode(const Attr& attr) { attr_ = attr; }

EasyDecode::~EasyDecode() {
  if (handler_) {
    delete handler_;
    handler_ = nullptr;
  }
}

void EasyDecode::ReleaseBuffer(uint32_t buf_id) {
#ifdef CNSTK_MLU100

  CNResult ecode = CN_MPI_MLU_P2P_ReleaseBuffer(handler_->handle_, buf_id);
  if (CN_SUCCESS != ecode) {
    throw EasyDecodeError("Release buffer failed: " + to_string(ecode));
  }

#elif CNSTK_MLU270

  CN_Decode_ReleaseRef(reinterpret_cast<CNVideo_Decode>(handler_->handle_), buf_id);

#endif
}

bool EasyDecode::CopyFrame(void* dst, const CnFrame& frame) {
#ifdef CNSTK_MLU100

  cnrtRet_t ret = cnrtMemcpy(dst, frame.ptrs[0], frame.frame_size, CNRT_MEM_TRANS_DIR_DEV2HOST);
  if (CNRT_RET_SUCCESS != ret) {
    throw EasyDecodeError("copy frame failed. Error code: " + to_string(ret));
  }

#elif CNSTK_MLU270
  auto odata = reinterpret_cast<uint8_t*>(dst);
  const CNCREATEVIDEODECODEINFO& params = handler_->params_;

  switch (params.OutputFormat) {
    case CNVideoSurfaceFormat_NV21:
    case CNVideoSurfaceFormat_NV12: {
      size_t len_y = frame.strides[0] * frame.height;
      size_t len_uv = frame.strides[1] * frame.height / 2;
      cnrtRet_t ecode = cnrtMemcpy(reinterpret_cast<void*>(odata), frame.ptrs[0], len_y, CNRT_MEM_TRANS_DIR_DEV2HOST);
      if (CNRT_RET_SUCCESS != ecode) {
        throw EasyDecodeError("copy frame failed. Error code: " + to_string(ecode));
      }
      ecode = cnrtMemcpy(reinterpret_cast<void*>(odata + len_y), frame.ptrs[1], len_uv, CNRT_MEM_TRANS_DIR_DEV2HOST);
      if (CNRT_RET_SUCCESS != ecode) {
        throw EasyDecodeError("copy frame failed. Error code: " + to_string(ecode));
      }
    } break;
    default:
      LOG(ERROR, "don't support format: %d", params.OutputFormat);
      break;
  }

#endif

  return true;
}

}  // namespace edk
