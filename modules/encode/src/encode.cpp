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

#include "encode.hpp"

#include <memory>
#include <sstream>
#include <string>

#include "cnencode.hpp"
#include "cnstream_frame_va.hpp"
#include "common.hpp"
#include "image_preproc.hpp"

namespace cnstream {

struct EncodeParam {
  int frame_rate = 25;               // Target fps
  int dst_width = 0;                 // Target width, prefered size same with input
  int dst_height = 0;                // Target height, prefered size same with input
  int gop = 30;                      // Target gop, default is 30
  int bit_rate = 0x100000;           // Target bit rate, default is 1Mbps
  bool use_ffmpeg = false;           // Whether use ffmpeg to do image preprocessing, default is false
  CNCodecType codec_type = H264;     // Video codec type
  std::string encoder_type = "cpu";  // Encoding type, cpu or mlu encoding, default is cpu encoding
  std::string preproc_type = "cpu";  // Preproc type, do image preprocessing on cpu ot mlu, default is cpu
  std::string output_dir = "";       // Output directory
  int device_id = -1;                // mlu device id, -1 :disable mlu
};

/**
 * @brief Encode context structer
 */
struct EncodeContext {
  std::unique_ptr<ImagePreproc> preproc = nullptr;
  std::unique_ptr<CNEncode> cnencode = nullptr;
  CNPixelFormat src_pix_fmt = NV21;
  uint8_t *data_yuv = nullptr;
  cv::Mat dst_image;
};

Encode::Encode(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("Encode is a module to encode videos or images.");
  param_register_.Register("encoder_type", "Use cpu encoding or mlu encoding. It could be cpu or mlu.");
  param_register_.Register("codec_type", "encoder type, it could be h264, h265 or jpeg.");
  param_register_.Register("preproc_type",
                           "Preprocessing data on cpu or mlu(mlu is not supported yet). "
                           "Normally, preprocessing includes resizing and color space converting.");
  param_register_.Register("use_ffmpeg", "Do resize and color space convert using ffmpeg. It could be true or false.");
  param_register_.Register("dst_width", "The width of the output.");
  param_register_.Register("dst_height", "The height of the output.");
  param_register_.Register("frame_rate", "Frame rate of the encoded video.");
  param_register_.Register("kbit_rate",
                           "The amount data encoded for a unit of time. Only valid when encode on mlu."
                           "A higher bitrate means a higher quality video, but lower encoding speed.");
  param_register_.Register("gop_size",
                           "Group of pictures is known as GOP. Only valid when encode on mlu."
                           "gop_size is the number of frames between two I-frames.");
  param_register_.Register("output_dir", "Where to store the encoded video. Default dir is {CURRENT_DIR}/output.");
  param_register_.Register("device_id", "Which device will be used. If there is only one device, it might be 0.");

  hasTransmit_.store(1);  // for receive eos
}

EncodeContext *Encode::GetEncodeContext(CNFrameInfoPtr data) {
  EncodeContext *ctx = nullptr;
  if (!data) {
    LOG(ERROR) << "[Encode] data is nullptr.";
    return ctx;
  }
  {
    RwLockReadGuard lg(ctx_lock_);
    if (ctxs_.find(data->stream_id) != ctxs_.end()) {
      ctx = ctxs_[data->stream_id];
      return ctx;
    }
  }
  if (data->IsEos()) {
    LOG(WARNING) << "[Encode] data is eos, get EncodeContext failed.";
    return ctx;
  }

  // Create encode context
  RwLockWriteGuard lg(ctx_lock_);

  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  ctx = new EncodeContext();
  CNPixelFormat src_pix_fmt;
  CNPixelFormat dst_pix_fmt;
  CNPixelFormat frame_pix_fmt;
  bool has_bgr_img = frame->HasBGRImage();
  switch (frame->fmt) {
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_BGR24:
      frame_pix_fmt = BGR24;
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      frame_pix_fmt = RGB24;
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
      frame_pix_fmt = NV12;
      break;
    case cnstream::CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      frame_pix_fmt = NV21;
      break;
    default:
      LOG(ERROR) << "[Encode] unsuport pixel format.";
      if (ctx) {
        delete ctx;
      }
      return nullptr;
  }

  if (has_bgr_img || frame_pix_fmt == BGR24 || frame_pix_fmt == RGB24 || param_->encoder_type == "cpu") {
    src_pix_fmt = BGR24;
  } else {
    src_pix_fmt = frame_pix_fmt;
  }
  if (param_->encoder_type == "mlu") {
    if (frame_pix_fmt != NV12 && frame_pix_fmt != NV21) {
      dst_pix_fmt = NV12;
    } else {
      dst_pix_fmt = frame_pix_fmt;
    }
  } else {
    dst_pix_fmt = BGR24;
  }
  ctx->src_pix_fmt = src_pix_fmt;
  if (param_->dst_height <= 0) param_->dst_height = frame->height / 2 * 2;
  if (param_->dst_width <= 0) param_->dst_width = frame->width / 2 * 2;
  if (param_->codec_type == JPEG && param_->encoder_type == "mlu") {
    dst_stride_ = ALIGN(param_->dst_width, JEPG_ENC_ALIGNMENT);
  } else {
    dst_stride_ = param_->dst_width;
  }

  // build preproc
  ImagePreproc::ImagePreprocParam preproc_param;
  preproc_param.src_width = frame->width;
  preproc_param.src_height = frame->height;
  if (!has_bgr_img) {
    preproc_param.src_stride = frame->stride[0];
  }
  preproc_param.dst_width = param_->dst_width;
  preproc_param.dst_height = param_->dst_height;
  preproc_param.dst_stride = dst_stride_;
  preproc_param.src_pix_fmt = src_pix_fmt;
  preproc_param.dst_pix_fmt = dst_pix_fmt;
  preproc_param.preproc_type = param_->preproc_type;
  preproc_param.use_ffmpeg = param_->use_ffmpeg;
  if (param_->preproc_type == "mlu") {
    preproc_param.device_id = param_->device_id;
  }
  ctx->preproc.reset(new ImagePreproc(preproc_param));
  if (!ctx->preproc->Init()) {
    LOG(ERROR) << "[Encode] encoder preproc init failed.";
    if (ctx) {
      delete ctx;
    }
    return nullptr;
  }

  // build cnencode
  CNEncode::CNEncodeParam cnencode_param;
  cnencode_param.dst_width = param_->dst_width;
  cnencode_param.dst_height = param_->dst_height;
  cnencode_param.dst_stride = dst_stride_;
  cnencode_param.dst_pix_fmt = dst_pix_fmt;
  cnencode_param.encoder_type = param_->encoder_type;
  cnencode_param.codec_type = param_->codec_type;
  cnencode_param.frame_rate = param_->frame_rate;
  cnencode_param.bit_rate = param_->bit_rate;
  cnencode_param.gop = param_->gop;
  cnencode_param.stream_id = data->stream_id;
  cnencode_param.output_dir = param_->output_dir;
  if (param_->encoder_type == "mlu") {
    cnencode_param.device_id = param_->device_id;
  }

  ctx->cnencode.reset(new CNEncode(cnencode_param));
  if (!ctx->cnencode->Init()) {
    LOG(ERROR) << "[Encode] CNEncode type object initialized failed.";
    if (ctx) {
      delete ctx;
    }
    return nullptr;
  }
  if (param_->encoder_type == "mlu" && (param_->preproc_type == "cpu" || ctx->src_pix_fmt == BGR24)) {
    ctx->data_yuv = new uint8_t[dst_stride_ * param_->dst_height * 3 / 2];
    memset(ctx->data_yuv, 0, sizeof(uint8_t) * dst_stride_ * param_->dst_height * 3 / 2);
  }
  if (param_->encoder_type == "cpu") {
    ctx->dst_image = cv::Mat(param_->dst_height, param_->dst_width, CV_8UC3);
  }
  std::shared_ptr<PerfManager> manager = GetPerfManager(data->stream_id);
  ctx->cnencode->SetPerfManager(manager);
  ctx->cnencode->SetModuleName(GetName());
  ctxs_[data->stream_id] = ctx;
  return ctx;
}

Encode::~Encode() { Close(); }

bool Encode::Open(ModuleParamSet paramSet) {
  if (param_) {
    LOG(WARNING) << "[Encode] encode param is existed. Please Close before Open.";
    return false;
  }
  param_ = new EncodeParam();
  if (paramSet.find("dump_dir") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``dump_dir`` is deprecated. Please use ``output_dir`` instead.";
    return false;
  }
  if (paramSet.find("dump_type") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``dump_type`` is deprecated. Please use ``codec_type`` instead. "
               << "Supported options are jpeg, h264 and hevc.";
    return false;
  }
  if (paramSet.find("bit_rate") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``bit_rate`` is deprecated. Please use ``kbit_rate`` instead.";
    return false;
  }
  if (paramSet.find("pre_type") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``pre_type`` is deprecated. Please use ``preproc_type`` instead.";
    return false;
  }
  if (paramSet.find("enc_type") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``enc_type`` is deprecated. Please use ``codec_type`` instead. "
               << "Supported options are jpeg, h264 and hevc.";
    return false;
  }
  if (paramSet.find("frame_rate") != paramSet.end()) {
    param_->frame_rate = std::stoi(paramSet["frame_rate"]);
  }
  if (paramSet.find("kbit_rate") != paramSet.end()) {
    param_->bit_rate = std::stoi(paramSet["kbit_rate"]) * 1024;
  }
  if (paramSet.find("gop_size") != paramSet.end()) {
    param_->gop = std::stoi(paramSet["gop_size"]);
  }
  if (paramSet.find("dst_width") != paramSet.end()) {
    param_->dst_width = std::stoi(paramSet["dst_width"]);
  }
  if (paramSet.find("dst_height") != paramSet.end()) {
    param_->dst_height = std::stoi(paramSet["dst_height"]);
  }
  if (paramSet.find("output_dir") != paramSet.end()) {
    param_->output_dir = paramSet["output_dir"];
  } else {
    char *path;
    path = getcwd(NULL, 0);
    if (path) {
      param_->output_dir = path;
      param_->output_dir += "/output";
      free(path);
    } else {
      LOG(ERROR) << "[Encode] Can not get current path.";
      return false;
    }
  }

  if (paramSet.find("use_ffmpeg") != paramSet.end() && paramSet["use_ffmpeg"] == "true") {
    param_->use_ffmpeg = true;
  }
  if (paramSet.find("encoder_type") != paramSet.end()) {
    if (paramSet["encoder_type"] == "mlu") {
      param_->encoder_type = "mlu";
    } else if (paramSet["encoder_type"] == "cpu") {
      param_->encoder_type = "cpu";
    } else {
      LOG(WARNING) << "[Encode] encoder type should be choosen from mlu and cpu. "
                   << "It is invalid, cpu will be selected as default.";
    }
  }
  if (paramSet.find("preproc_type") != paramSet.end()) {
    if (paramSet["preproc_type"] == "mlu") {
      LOG(ERROR) << "[Encode] Preproc on MLU is not supported now.";
      return false;
    } else if (paramSet["preproc_type"] == "cpu") {
      param_->preproc_type = "cpu";
    } else {
      LOG(WARNING) << "[Encode] preprocess type should be choosen from mlu and cpu. "
                   << "It is invalid, cpu will be selected as default.";
    }
  }
  if (paramSet.find("codec_type") != paramSet.end()) {
    std::string codec_type = paramSet["codec_type"];
    if ("h264" == codec_type) {
      param_->codec_type = H264;
    } else if ("hevc" == codec_type) {
      param_->codec_type = HEVC;
    } else if ("jpeg" == codec_type) {
      param_->codec_type = JPEG;
    } else {
      LOG(WARNING) << "[Encode] codec type should be choosen from h264, h265 and jpeg. "
                   << "It is invalid, h264 will be selected as default.";
    }
  }
  if (paramSet.find("device_id") != paramSet.end()) {
    param_->device_id = std::stoi(paramSet["device_id"]);
  }
  if ((param_->preproc_type == "mlu" || param_->encoder_type == "mlu") && param_->device_id < 0) {
    LOG(ERROR) << "[Encode] Please set device id if use mlu to encode or preprococess.";
    return false;
  }
  if (param_->preproc_type == "mlu" && param_->encoder_type == "cpu") {
    LOG(ERROR) << "[Encode] Not supported cpu encoding after mlu preprocessing";
    return false;
  }
  if (param_->encoder_type == "mlu" && (param_->dst_height % 2 || param_->dst_width % 2)) {
    LOG(ERROR) << "[Encode] Not supported mlu encoding image the height or the width of which is odd.";
    return false;
  }
  return true;
}

void Encode::Close() {
  if (param_) {
    delete param_;
    param_ = nullptr;
  }
  if (ctxs_.empty()) {
    return;
  }
  for (auto &pair : ctxs_) {
    if (pair.second->data_yuv) {
      delete[] pair.second->data_yuv;
      pair.second->data_yuv = nullptr;
    }
    if (pair.second) {
      delete pair.second;
      pair.second = nullptr;
    }
  }
  ctxs_.clear();
}

int Encode::Process(CNFrameInfoPtr data) {
  if (!data) {
    return -1;
  }
  bool eos = data->IsEos();
  EncodeContext *ctx = GetEncodeContext(data);
  if (!ctx) {
    LOG(ERROR) << "[Encode] Get encode context failed.";
    return -1;
  }
  if (ctx->data_yuv) {
    memset(ctx->data_yuv, 0, sizeof(uint8_t) * dst_stride_ * param_->dst_height * 3 / 2);
  }
  cv::Mat *image = nullptr;
  uint8_t *dst_y = nullptr, *dst_uv = nullptr;

  if (eos) {
    if (param_->encoder_type == "mlu") {
      if (!ctx->cnencode->Update(dst_y, dst_uv, data->timestamp, eos)) {
        LOG(ERROR) << "[Encode] Encode frame on Mlu failed.";
        return -1;
      }
    }
    TransmitData(data);
    return 1;
  }

  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(data->datas[CNDataFramePtrKey]);
  if (frame->width * frame->height == 0) {
    LOG(ERROR) << "[Encode] The height or the width of the data frame is invalid.";
    return -1;
  }

  if (param_->encoder_type == "mlu") {
    if (frame->HasBGRImage()) {
      ctx->preproc->SetSrcWidthHeight(frame->width, frame->height);
    } else {
      ctx->preproc->SetSrcWidthHeight(frame->width, frame->height, frame->stride[0]);
    }
    if (ctx->src_pix_fmt == BGR24) {
      image = frame->ImageBGR();
    }
    if (param_->preproc_type == "mlu") {
      // Mlu Preproc
      // if (!image) {
      //   const uint8_t *src_y = reinterpret_cast<const uint8_t*>(frame->data[0]->GetMluData());
      //   const uint8_t *src_uv = reinterpret_cast<const uint8_t*>(frame->data[1]->GetMluData());
      //   // TODO: Get encoder mlu address
      //   if (!ctx->preproc->Yuv2Yuv(src_y, src_uv, dst_y, dst_uv)) {
      //     LOG(ERROR) << "[Encode] mlu yuv2yuv reisze failed.";
      //     return -1;
      //   }
      // } else {
      //   // bgr 2 yuv (mlu is not supported, use cpu instead opencv/ffmpeg)
      //   if (!ctx->preproc->Bgr2Yuv(*image, ctx->data_yuv)) {
      //     LOG(ERROR) << "[Encode] cpu bgr2yuv reisze failed. (mlu is not supported yet)";
      //     return -1;
      //   }
      // }
      LOG(ERROR) << "[Encode] mlu preproc is not supported yet.";
      return -1;
    } else {
      // Cpu Preproc
      if (!image) {
        const uint8_t *src_y = reinterpret_cast<const uint8_t *>(frame->data[0]->GetCpuData());
        const uint8_t *src_uv = reinterpret_cast<const uint8_t *>(frame->data[1]->GetCpuData());
        if (!ctx->preproc->Yuv2Yuv(src_y, src_uv, ctx->data_yuv)) {
          LOG(ERROR) << "[Encode] cpu yuv reisze failed.";
          return -1;
        }
      } else {
        if (!ctx->preproc->Bgr2Yuv(*image, ctx->data_yuv)) {
          LOG(ERROR) << "[Encode] cpu bgr2yuv and reisze failed.";
          return -1;
        }
      }
      if (ctx->data_yuv) {
        dst_y = ctx->data_yuv;
        dst_uv = ctx->data_yuv + param_->dst_height * dst_stride_;
      }
    }

    // Mlu Encode
    if (!ctx->cnencode->Update(dst_y, dst_uv, data->timestamp, eos)) {
      LOG(ERROR) << "[Encode] Encode frame on Mlu failed.";
      return -1;
    }
  } else if (param_->encoder_type == "cpu") {
    ctx->preproc->SetSrcWidthHeight(frame->width, frame->height);
    image = frame->ImageBGR();
    // Cpu Preproc
    if (!ctx->preproc->Bgr2Bgr(*image, ctx->dst_image)) {
      LOG(ERROR) << "[Encode] cpu bgr reisze failed.";
      return -1;
    }
    // Cpu Encode
    if (!ctx->cnencode->Update(ctx->dst_image, data->timestamp)) {
      LOG(ERROR) << "[Encode] Encode frame on Cpu failed.";
      return -1;
    }
  }
  TransmitData(data);
  return 1;
}

bool Encode::CheckParamSet(const ModuleParamSet &paramSet) const {
  bool ret = true;
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Encode] Unknown param: " << it.first;
    }
  }
  if (paramSet.find("dump_dir") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``dump_dir`` is deprecated. Please use ``output_dir`` instead.";
    ret = false;
  }
  if (paramSet.find("dump_type") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``dump_type`` is deprecated. Please use ``codec_type`` instead. "
               << "Supported options are jpeg, h264 and hevc.";
    ret = false;
  }
  if (paramSet.find("bit_rate") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``bit_rate`` is deprecated. Please use ``kbit_rate`` instead.";
    ret = false;
  }
  if (paramSet.find("pre_type") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``pre_type`` is deprecated. Please use ``preproc_type`` instead.";
    ret = false;
  }
  if (paramSet.find("enc_type") != paramSet.end()) {
    LOG(ERROR) << "[Encode] parameter ``enc_type`` is deprecated. Please use ``codec_type`` instead. "
               << "Supported options are jpeg, h264 and hevc.";
    ret = false;
  }
  std::string preproc_type = "cpu";
  std::string encoder_type = "cpu";
  if (paramSet.find("preproc_type") != paramSet.end()) {
    preproc_type = paramSet.at("preproc_type");
    if (preproc_type != "cpu") {
      LOG(ERROR) << "[Encode] preproc_type is invalid, ``" << paramSet.at("preproc_type")
                 << "``. Choose ``cpu``. (mlu preproc is not supported yet.)";
      ret = false;
    }
  }
  if (paramSet.find("use_ffmpeg") != paramSet.end() && paramSet.at("use_ffmpeg") != "true" &&
      paramSet.at("use_ffmpeg") != "false") {
    LOG(ERROR) << "[Encode] use_ffmpeg is invalid, ``" << paramSet.at("use_ffmpeg")
               << "``. Choose from ``true`` and ``false``.";
    ret = false;
  }
  if (paramSet.find("encoder_type") != paramSet.end()) {
    encoder_type = paramSet.at("encoder_type");
    if (encoder_type != "mlu" && encoder_type != "cpu") {
      LOG(ERROR) << "[Encode] encoder_type is invalid, ``" << paramSet.at("encoder_type")
                 << "``. Choose from ``mlu`` and ``cpu``.";
      ret = false;
    }
  }

  if (paramSet.find("codec_type") != paramSet.end() && paramSet.at("codec_type") != "jpeg" &&
      paramSet.at("codec_type") != "h264" && paramSet.at("codec_type") != "hevc") {
    LOG(ERROR) << "[Encode] codec_type is invalid, ``" << paramSet.at("codec_type")
               << "``. Choose from ``jpeg``, ``h264`` and ``hevc``.";
    ret = false;
  }

  std::string err_msg;
  if (!checker.IsNum({"dst_width", "dst_height", "frame_rate", "kbit_rate", "gop_size", "device_id"}, paramSet, err_msg,
                     true)) {
    LOG(ERROR) << "[Encode] " << err_msg;
    return false;
  }

  if ((preproc_type == "mlu" || encoder_type == "mlu") && paramSet.find("device_id") == paramSet.end()) {
    LOG(ERROR) << "[Encode] Must set device id when encoder type is ``mlu``";
    ret = false;
  }
  if (preproc_type == "mlu" && encoder_type == "cpu") {
    LOG(ERROR) << "[Encode] mlu preproc and cpu encoding is not supported.";
    ret = false;
  }
  if (encoder_type == "mlu" &&
      ((paramSet.find("dst_width") != paramSet.end() && stoi(paramSet.at("dst_width")) % 2 != 0) ||
       (paramSet.find("dst_height") != paramSet.end() && stoi(paramSet.at("dst_height")) % 2 != 0))) {
    LOG(ERROR) << "[Encode] Not supported mlu encoding image the height or the width of which is odd."
               << " width: " << paramSet.at("dst_width") << ", height: " << paramSet.at("dst_height");
    ret = false;
  }
  return ret;
}

void Encode::RecordTime(std::shared_ptr<CNFrameInfo> data, bool is_finished) {
  std::shared_ptr<PerfManager> manager = GetPerfManager(data->stream_id);
  if (!data->IsEos() && manager && !is_finished) {
    manager->Record(is_finished, PerfManager::GetDefaultType(), this->GetName(), data->timestamp);
    manager->Record(PerfManager::GetDefaultType(), PerfManager::GetPrimaryKey(), std::to_string(data->timestamp),
                    this->GetName() + "_th", "'" + GetThreadName(pthread_self()) + "'");
  }
}

}  // namespace cnstream
