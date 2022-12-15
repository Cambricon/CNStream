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
#include "video_decoder.hpp"

#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include "cnedk_decode.h"
#include "cnstream_logging.hpp"
#include "platform_utils.hpp"

namespace cnstream {

MluDecoder::MluDecoder(const std::string &stream_id, IDecodeResult *cb, IUserPool *pool)
    : Decoder(stream_id, cb, pool) {}

MluDecoder::~MluDecoder() { Destroy(); }

bool MluDecoder::Create(VideoInfo *info, ExtraDecoderInfo *extra) {
  if (vdec_) {
    LOGW(SOURCE) << "[" << stream_id_ << "]: Decoder create duplicated.";
    return false;
  }

  CnedkVdecCreateParams create_params;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = extra->device_id;
  switch (info->codec_id) {
    case AV_CODEC_ID_H264:
      create_params.type = CNEDK_VDEC_TYPE_H264;
      break;
    case AV_CODEC_ID_HEVC:
      create_params.type = CNEDK_VDEC_TYPE_H265;
      break;
    case AV_CODEC_ID_MJPEG:
      create_params.type = CNEDK_VDEC_TYPE_JPEG;
      break;
    default:
      LOGE(SOURCE) << "[" << stream_id_ << "]: "
                   << "Codec type not supported yet, codec_id = " << info->codec_id;
      return false;
  }

  if (IsCloudPlatform(platform_name_)) {
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV12;
  } else if (IsEdgePlatform(platform_name_)) {
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV21;
  } else {
    create_params.color_format = CNEDK_BUF_COLOR_FORMAT_NV21;
  }

  if (create_params.type == CNEDK_VDEC_TYPE_JPEG) {
    create_params.max_width = (extra && extra->max_width > 0) ? extra->max_width : 8192;
    create_params.max_height = (extra && extra->max_height > 0) ? extra->max_height : 4320;
  } else {
    if (extra && extra->max_width > 0 && extra->max_height > 0) {
      create_params.max_width = extra->max_width;
      create_params.max_height = extra->max_height;
    } else {
      if (IsCloudPlatform(platform_name_)) {
        create_params.max_width = 0;
        create_params.max_height = 0;
      } else if (IsEdgePlatform(platform_name_)) {
        create_params.max_width = 1920;
        create_params.max_height = 1080;
      } else {
        create_params.max_width = 1920;
        create_params.max_height = 1080;
      }
    }
  }
  create_params.frame_buf_num = 34;  // for CE3226, input buffer
  create_params.surf_timeout_ms = 5000;
  create_params.userdata = this;
  create_params.GetBufSurf = MluDecoder::GetBufSurface_;
  create_params.OnFrame = MluDecoder::OnFrame_;
  create_params.OnEos = MluDecoder::OnEos_;
  create_params.OnError = MluDecoder::OnError_;

  int ret = CnedkVdecCreate(&vdec_, &create_params);
  if (ret) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: Create decoder failed";
    return false;
  }
  LOGI(SOURCE) << "[" << stream_id_ << "]: Finish create decoder";
  return true;
}

void MluDecoder::Destroy() {
  if (vdec_) {
    CnedkVdecDestroy(vdec_);
    vdec_ = nullptr;
  }
}

bool MluDecoder::Process(VideoEsPacket *pkt) {
  if (vdec_) {
    CnedkVdecStream stream;
    memset(&stream, 0, sizeof(stream));
    if (pkt) {
      stream.bits = pkt->data;
      stream.len = pkt->len;
      stream.pts = pkt->pts;
    }
    int max_try_send_time = 30;
    while (max_try_send_time--) {
      int ret = CnedkVdecSendStream(vdec_, &stream, 1000);
      if (ret < 0) {
        if (ret == -3) {
          if (result_) {
            cnedk::BufSurfWrapperPtr wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(nullptr, false);
            wrapper->SetPts(pkt->pts);
            result_->OnDecodeFrame(wrapper);
            return true;
          }
          return false;
        }
        if (max_try_send_time) {
          continue;
        }
        LOGE(SOURCE) << "[MluDecoder] Process(): Send package failed. Maximum number of attempts reached";
        return false;
      } else {
        break;
      }
    }
    return true;
  }
  return false;
}

int MluDecoder::GetBufSurface(CnedkBufSurface **surf, int width, int height, CnedkBufSurfaceColorFormat fmt,
                              int timeout_ms) {
  pool_->OnBufInfo(width, height, fmt);
  // FIXME, alloc surface according to width&height&fmt
  //   on CE3226, we have to use VB pool.
  cnedk::BufSurfWrapperPtr wrapper = pool_->GetBufSurface(timeout_ms);
  if (wrapper) {
    *surf = wrapper->BufSurfaceChown();
    return 0;
  }
  return -1;
}

int MluDecoder::OnFrame(CnedkBufSurface *surf) {
  surf->surface_list[0].width -= surf->surface_list[0].width & 1;
  surf->surface_list[0].height -= surf->surface_list[0].height & 1;
  surf->surface_list[0].plane_params.width[0] -= surf->surface_list[0].plane_params.width[0] & 1;
  surf->surface_list[0].plane_params.height[0] -= surf->surface_list[0].plane_params.height[0] & 1;
  surf->surface_list[0].plane_params.width[1] -= surf->surface_list[0].plane_params.width[1] & 1;
  surf->surface_list[0].plane_params.height[1] -= surf->surface_list[0].plane_params.height[1] & 1;
  cnedk::BufSurfWrapperPtr wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  if (result_) {
    result_->OnDecodeFrame(wrapper);
    return 0;
  }
  return -1;
}

int MluDecoder::OnEos() {
  if (result_) {
    result_->OnDecodeEos();
    return 0;
  }
  return -1;
}

int MluDecoder::OnError(int errcode) {
  if (result_) {
    // FIXME, TODO(liujian)
    (void)errcode;
    result_->OnDecodeError(DecodeErrorCode::ERROR_UNKNOWN);
    return 0;
  }
  return -1;
}

}  // namespace cnstream
