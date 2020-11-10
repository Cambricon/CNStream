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
#include "cnencode.hpp"

#include <glog/logging.h>
#include <sys/stat.h>

#include <memory>
#include <string>
#include <utility>
#include "device/mlu_context.h"
#include "easycodec/easy_encode.h"
#include "easycodec/vformat.h"

#define SAVE_PACKET 1
#define IGNORE_HEAD 1

namespace cnstream {

CNEncode::CNEncode(const CNEncodeParam &param) {
  cnencode_param_ = param;
}

bool CNEncode::Init() {
  if (is_init_) {
    LOG(ERROR) << "[CNEncode] Init function should be called only once.";
    return false;
  }
  if (cnencode_param_.encoder_type == "cpu" &&
      cnencode_param_.dst_pix_fmt != BGR24 &&
      cnencode_param_.dst_pix_fmt != RGB24) {
    LOG(ERROR) << "[CNEncode] cpu encoding only support bgr24/rgb24 format.";
    return false;
  }
  if (cnencode_param_.encoder_type == "mlu" &&
      cnencode_param_.dst_pix_fmt != NV12 &&
      cnencode_param_.dst_pix_fmt != NV21) {
    LOG(ERROR) << "[CNEncode] mlu encoding only support nv12/nv2 format.";
    return false;
  }
  if (cnencode_param_.output_dir.empty()) {
    cnencode_param_.output_dir = "./output";
  }
  if (!CreateDir(cnencode_param_.output_dir + "/")) {
    LOG(ERROR) << "[CNEncode] Create output dir " << cnencode_param_.output_dir << " failed.";
    return false;
  }
  if (cnencode_param_.encoder_type == "mlu") {
    if (cnencode_param_.device_id >= 0) {
      edk::MluContext context;
      try {
        context.SetDeviceId(cnencode_param_.device_id);
        context.BindDevice();
      } catch (edk::Exception &err) {
        LOG(ERROR) << "CNEncode: set mlu env failed";
        return false;
      }
    } else {
      return false;
    }
    switch (cnencode_param_.dst_pix_fmt) {
      case NV12:
        picture_format_ = edk::PixelFmt::NV12;
        output_frame_size_ = cnencode_param_.dst_width * cnencode_param_.dst_height * 3 / 2;
        break;
      case NV21:
        picture_format_ = edk::PixelFmt::NV21;
        output_frame_size_ = cnencode_param_.dst_width * cnencode_param_.dst_height * 3 / 2;
        break;
      default:
        picture_format_ = edk::PixelFmt::NV21;
        break;
    }
    if (!CreateMluEncoder()) {
      LOG(ERROR) << "[CNEncode] Create Mlu encoder instance failed.";
      return false;
    }
  } else {
    if (!CreateCpuEncoder()) {
      LOG(ERROR) << "[CNEncode] Create Cpu encoder instance failed.";
      return false;
    }
  }
  is_init_ = true;
  return true;
}

bool CNEncode::CreateMluEncoder() {
  edk::CodecType codec_type = edk::CodecType::H264;
  switch (cnencode_param_.codec_type) {
    case H264:
      LOG(INFO) << "[CNEncode] CodecType::H264";
      codec_type = edk::CodecType::H264;
      break;
    case HEVC:
      LOG(INFO) << "[CNEncode] CodecType::HEVC";
      codec_type = edk::CodecType::H265;
      break;
    case MPEG4:
      LOG(INFO) << "[CNEncode] CodecType::MPEG4";
      codec_type = edk::CodecType::MPEG4;
      break;
    case JPEG:
      LOG(INFO) << "[CNEncode] CodecType::JPEG";
      codec_type = edk::CodecType::JPEG;
      break;
    default:
      codec_type = edk::CodecType::H264;
      break;
  }

  uint32_t frame_rate_den = 1;

  edk::EasyEncode::Attr attr;
  attr.dev_id = cnencode_param_.device_id;
  attr.frame_geometry.w = cnencode_param_.dst_width;
  attr.frame_geometry.h = cnencode_param_.dst_height;
  attr.pixel_format = picture_format_;
  attr.codec_type = codec_type;
  attr.b_frame_num = 0;
  attr.input_buffer_num = 6;
  attr.output_buffer_num = 6;
  attr.gop_type = edk::GopType::BIDIRECTIONAL;
  if (cnencode_param_.codec_type == H264) {
    attr.insertSpsPpsWhenIDR = 1;
    attr.level = edk::VideoLevel::H264_41;
    attr.profile = edk::VideoProfile::H264_MAIN;
  } else if (cnencode_param_.codec_type == HEVC) {
    attr.level = edk::VideoLevel::H265_MAIN_41;
    attr.profile = edk::VideoProfile::H265_MAIN;
  }
  memset(&attr.rate_control, 0, sizeof(edk::RateControl));
  attr.rate_control.vbr = false;
  attr.rate_control.gop = cnencode_param_.gop;
  attr.rate_control.frame_rate_num = cnencode_param_.frame_rate;
  attr.rate_control.frame_rate_den = frame_rate_den;
  attr.rate_control.bit_rate = cnencode_param_.bit_rate;
  attr.rate_control.max_bit_rate = cnencode_param_.bit_rate;
  memset(&attr.crop_config, 0, sizeof(edk::CropConfig));
  attr.crop_config.enable = false;
  attr.silent = false;
  attr.jpeg_qfactor = 50;
  attr.eos_callback = std::bind(&CNEncode::EosCallback, this);
  attr.packet_callback = std::bind(&CNEncode::PacketCallback, this, std::placeholders::_1);

  try {
    mlu_encoder_ = edk::EasyEncode::Create(attr);
  } catch (edk::Exception &err) {
    LOG(ERROR) << "[CNEncode] create mlu encode failed. error message:" << err.what();
    if (mlu_encoder_) {
      delete mlu_encoder_;
      mlu_encoder_ = nullptr;
    }
    return false;
  }
  if (!mlu_encoder_) {
    LOG(ERROR) << "[CNEncode] create mlu encoder failed.";
    return false;
  }
  return true;
}

bool CNEncode::CreateCpuEncoder() {
  if (cnencode_param_.dst_width == 0 || cnencode_param_.dst_height == 0) {
    LOG(ERROR) << "[CNEncode]  Create cpu encoder failed, dst_width or dst_height is 0";
    return false;
  }
  size_ = cv::Size(cnencode_param_.dst_width, cnencode_param_.dst_height);
  std::string filename = cnencode_param_.output_dir + "/encode_stream_" + cnencode_param_.stream_id;
  int fourcc = -1;
#if ((CV_MAJOR_VERSION < 3) || CNS_MLU220_SOC)
  LOG(WARNING) << "[CNEncode] H264 or HEVC encoder is not supported. MJPG encoder will be used instead.";
  if (cnencode_param_.codec_type == H264 || cnencode_param_.codec_type == HEVC) {
    fourcc = CV_FOURCC('M', 'J', 'P', 'G');
    filename += ".avi";
  }
#else
  if (cnencode_param_.codec_type == H264) {
    fourcc = CV_FOURCC('a', 'v', 'c', '1');
    filename += ".mp4";
  } else if (cnencode_param_.codec_type == HEVC) {
    fourcc = CV_FOURCC('h', 'e', 'v', '1');
    filename += ".mov";
  }
#endif
  if (fourcc >= 0) {
    writer_ = std::move(cv::VideoWriter(filename, fourcc, cnencode_param_.frame_rate, size_));
    if (!writer_.isOpened()) {
      LOG(ERROR) << "[CNEncode] Create cpu encoder failed";
      return false;
    }
  }
  return true;
}

bool CNEncode::CreateDir(std::string dir) {
  std::string path = "";
  int success = 0;
  if (!access(dir.c_str(), 0)) {
    return true;
  }
  for (uint32_t i = 0; i < dir.size(); i++) {
    path.push_back(dir[i]);
    if (dir[i] == '/' && access(path.c_str(), 0) != success && mkdir(path.c_str(), 00700) != success) {
      LOG(ERROR) << "[CNEncode] Failed at create directory";
      return false;
    }
  }
  return true;
}

CNEncode::~CNEncode() {
  if (cnencode_param_.encoder_type == "mlu" && cnencode_param_.device_id >= 0) {
    edk::MluContext context;
    try {
      context.SetDeviceId(cnencode_param_.device_id);
      context.BindDevice();
    } catch (edk::Exception &err) {
      LOG(ERROR) << "[CNEncode][Close] set mlu env failed";
    }
    if (mlu_encoder_) {
      delete mlu_encoder_;
      mlu_encoder_ = nullptr;
    }
  }
  if (p_file_) {
    fclose(p_file_);
  }
  if (writer_.isOpened()) {
    writer_.release();
  }
}

bool CNEncode::Update(const cv::Mat src, int64_t timestamp) {
  if (cnencode_param_.codec_type == JPEG) {
    cv::imwrite(cnencode_param_.output_dir + "/stream_" + cnencode_param_.stream_id + "_frame_" +
                std::to_string(++frame_count_) + ".jpg", src);
  } else {
    if (!writer_.isOpened()) {
      LOG(ERROR) << "[CNEncode] cpu encoder is not existed.";
      return false;
    }
    writer_.write(src);
  }
  RecordEndTime(timestamp);
  return true;
}

bool CNEncode::Update(uint8_t* src_y, uint8_t* src_uv, int64_t timestamp, bool eos) {
  if (!mlu_encoder_) {
    LOG(ERROR) << "[CNEncode] mlu encoder is not existed.";
    return false;
  }
  edk::CnFrame *cnframe = new edk::CnFrame;
  memset(cnframe, 0, sizeof(edk::CnFrame));
  cnframe->pts = timestamp;
  if (!eos) {
    if (!src_y || !src_uv) {
      LOG(ERROR) << "[CNEncode] src y or src uv pointer is nullptr.";
      delete cnframe;
      return false;
    }
    cnframe->width = cnencode_param_.dst_width;
    cnframe->height = cnencode_param_.dst_height;
    cnframe->pformat = picture_format_;
    cnframe->frame_size = output_frame_size_;
    cnframe->n_planes = 2;
    cnframe->strides[0] = cnencode_param_.dst_stride;
    cnframe->strides[1] = cnencode_param_.dst_stride;
    cnframe->ptrs[0] = reinterpret_cast<void *>(src_y);
    cnframe->ptrs[1] = reinterpret_cast<void *>(src_uv);
  }
  try {
    if (!mlu_encoder_->SendDataCPU(*cnframe, eos)) {
      LOG(ERROR) << "[CNEncode] send data to mlu encoder failed.";
      delete cnframe;
      return false;
    }
  } catch (edk::Exception &err) {
    LOG(ERROR) << "EncoderError: send data to mlu encoder" << err.what();
    delete cnframe;
    return false;
  }
  delete cnframe;
  return true;
}

void CNEncode::PacketCallback(const edk::CnPacket &packet) {
  if (packet.length == 0 || packet.data == 0) return;
  if (cnencode_param_.device_id < 0) return;
  if (packet.codec_type == edk::CodecType::JPEG) {
    RecordEndTime(packet.pts);
  } else {
#if IGNORE_HEAD
  static bool head_frame = true;
  if (!head_frame) {
    RecordEndTime(packet.pts);
  }
  head_frame = false;
#else
    RecordEndTime(packet.pts);
#endif
  }

  try {
    edk::MluContext context;
    context.SetDeviceId(cnencode_param_.device_id);
    context.BindDevice();
  } catch (edk::Exception &err) {
    LOG(ERROR) << "[CNEncode][PacketCallback] set mlu env faild";
    mlu_encoder_->ReleaseBuffer(packet.buf_id);
    return;
  }

#if SAVE_PACKET
  if (packet.codec_type == edk::CodecType::H264) {
    output_file_name_ = cnencode_param_.output_dir + "/encode_stream_" + cnencode_param_.stream_id + ".h264";
  } else if (packet.codec_type == edk::CodecType::H265) {
    output_file_name_ = cnencode_param_.output_dir + "/encode_stream_" + cnencode_param_.stream_id + ".h265";
  } else if (packet.codec_type == edk::CodecType::JPEG) {
    output_file_name_ = cnencode_param_.output_dir + "/encode_stream_" + cnencode_param_.stream_id + "_" +
                        std::to_string(++frame_count_) + ".jpg";
  } else {
    LOG(ERROR) << "[CNEncode][PacketCallback] unknown output codec type: " << static_cast<int>(packet.codec_type);
  }

  if (packet.codec_type == edk::CodecType::JPEG) {
    p_file_ = fopen(output_file_name_.c_str(), "wb");
  } else if (p_file_ == nullptr) {
    p_file_ = fopen(output_file_name_.c_str(), "wb");
  }
  if (p_file_ == nullptr) {
    LOG(ERROR) << "[CNEncode][PacketCallback] open output file failed";
  } else {
    uint32_t length = packet.length;
    written_ = fwrite(packet.data, 1, length, p_file_);
    if (written_ != length) {
      LOG(ERROR) << "[CNEncode][PacketCallback] written size: " << written_
                 << " is not equal to data length: " << length;
    }
    if (packet.codec_type == edk::CodecType::JPEG) {
      fclose(p_file_);
      p_file_ = nullptr;
    }
  }
#endif
  mlu_encoder_->ReleaseBuffer(packet.buf_id);
}

void CNEncode::EosCallback() {
  LOG(INFO) << "[CNEncode] EosCallback ... ";
}

void CNEncode::RecordEndTime(int64_t pts) {
  if (perf_manager_ != nullptr) {
    perf_manager_->Record(true, cnstream::PerfManager::GetDefaultType(), module_name_, pts);
  }
}

}  // namespace cnstream
