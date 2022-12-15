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

#ifndef CNSTREAM_VIDEO_DECODER_HPP_
#define CNSTREAM_VIDEO_DECODER_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "cnedk_decode.h"
#include "video_parser.hpp"

namespace cnstream {

static constexpr int MAX_PLANE_NUM = 3;

struct ExtraDecoderInfo {
  int32_t device_id = 0;
  int32_t max_width = 0;
  int32_t max_height = 0;
};

// FIXME
enum class DecodeErrorCode { ERROR_FAILED_TO_START, ERROR_CORRUPT_DATA, ERROR_RESET, ERROR_ABORT, ERROR_UNKNOWN };

class IDecodeResult {
 public:
  virtual ~IDecodeResult() = default;
  virtual void OnDecodeError(DecodeErrorCode error_code) {}
  virtual void OnDecodeFrame(cnedk::BufSurfWrapperPtr buf_surf) = 0;
  virtual void OnDecodeEos() = 0;
};

class IUserPool {
 public:
  virtual ~IUserPool() {}
  virtual void OnBufInfo(int width, int height, CnedkBufSurfaceColorFormat fmt) = 0;
  virtual int CreatePool(CnedkBufSurfaceCreateParams *params, uint32_t block_count) = 0;
  virtual void DestroyPool() = 0;
  virtual cnedk::BufSurfWrapperPtr GetBufSurface(int timeout_ms) = 0;
};

class Decoder {
 public:
  explicit Decoder(const std::string &stream_id, IDecodeResult *cb, IUserPool *pool)
      : stream_id_(stream_id), result_(cb), pool_(pool) {}
  virtual ~Decoder() = default;
  virtual bool Create(VideoInfo *info, ExtraDecoderInfo *extra = nullptr) = 0;
  virtual bool Process(VideoEsPacket *pkt) = 0;
  virtual void Destroy() = 0;
  void SetPlatformName(std::string name) { platform_name_ = name; }

 protected:
  std::string stream_id_ = "";
  IDecodeResult *result_;
  IUserPool *pool_;
  std::string platform_name_ = "";
};

class MluDecoder : public Decoder {
 public:
  explicit MluDecoder(const std::string &stream_id, IDecodeResult *cb, IUserPool *pool);
  ~MluDecoder();
  bool Create(VideoInfo *info, ExtraDecoderInfo *extra = nullptr) override;
  void Destroy() override;
  bool Process(VideoEsPacket *pkt) override;

  static int GetBufSurface_(CnedkBufSurface **surf, int width, int height, CnedkBufSurfaceColorFormat fmt,
                            int timeout_ms, void *userdata) {
    MluDecoder *thiz = reinterpret_cast<MluDecoder *>(userdata);
    return thiz->GetBufSurface(surf, width, height, fmt, timeout_ms);
  }
  static int OnFrame_(CnedkBufSurface *surf, void *userdata) {
    MluDecoder *thiz = reinterpret_cast<MluDecoder *>(userdata);
    return thiz->OnFrame(surf);
  }
  static int OnEos_(void *userdata) {
    MluDecoder *thiz = reinterpret_cast<MluDecoder *>(userdata);
    return thiz->OnEos();
  }
  static int OnError_(int errcode, void *userdata) {
    MluDecoder *thiz = reinterpret_cast<MluDecoder *>(userdata);
    return thiz->OnError(errcode);
  }
  int GetBufSurface(CnedkBufSurface **surf, int width, int height, CnedkBufSurfaceColorFormat fmt, int timeout_ms);
  int OnFrame(CnedkBufSurface *surf);
  int OnEos();
  int OnError(int errcode);

 private:
  MluDecoder(const MluDecoder &) = delete;
  MluDecoder(MluDecoder &&) = delete;
  MluDecoder &operator=(const MluDecoder &) = delete;
  MluDecoder &operator=(MluDecoder &&) = delete;
  void *vdec_ = nullptr;
};

}  // namespace cnstream

#endif  // CNSTREAM_VIDEO_DECODER_HPP_
