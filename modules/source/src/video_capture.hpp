/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#ifndef CNSTREAM_VIDEO_CAPTURE_HPP_
#define CNSTREAM_VIDEO_CAPTURE_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "cnedk_vin_capture.h"
#include "video_decoder.hpp"  // for IUserPool

namespace cnstream {

class ICaptureResult {
 public:
  virtual ~ICaptureResult() = default;
  virtual void OnCaptureError(int error_code) = 0;
  virtual void OnCaptureFrame(cnedk::BufSurfWrapperPtr buf_surf) = 0;
};

class IVinCapture {
 public:
  explicit IVinCapture(const std::string &stream_id, ICaptureResult *cb, IUserPool *pool)
      : stream_id_(stream_id), result_(cb), pool_(pool) {}
  virtual ~IVinCapture() = default;
  virtual bool Create(int sensor_id) = 0;
  virtual void Destroy() = 0;
  virtual bool Process(int timeout_ms) = 0;

 protected:
  std::string stream_id_ = "";
  ICaptureResult *result_;
  IUserPool *pool_;
};

class VinCapture : public IVinCapture {
 public:
  explicit VinCapture(const std::string &stream_id, ICaptureResult *cb, IUserPool *pool);
  ~VinCapture();
  bool Create(int sensor_id) override;
  void Destroy() override;
  bool Process(int timeout_ms) override;

  static int GetBufSurface_(CnedkBufSurface **surf, int timeout_ms, void *userdata) {
    VinCapture *thiz = reinterpret_cast<VinCapture *>(userdata);
    return thiz->GetBufSurface(surf, timeout_ms);
  }
  static int OnFrame_(CnedkBufSurface *surf, void *userdata) {
    VinCapture *thiz = reinterpret_cast<VinCapture *>(userdata);
    return thiz->OnFrame(surf);
  }
  static int OnError_(int errcode, void *userdata) {
    VinCapture *thiz = reinterpret_cast<VinCapture *>(userdata);
    return thiz->OnError(errcode);
  }
  int GetBufSurface(CnedkBufSurface **surf, int timeout_ms);
  int OnFrame(CnedkBufSurface *surf);
  int OnError(int errcode);

 private:
  VinCapture(const VinCapture &) = delete;
  VinCapture(VinCapture &&) = delete;
  VinCapture &operator=(const VinCapture &) = delete;
  VinCapture &operator=(VinCapture &&) = delete;
  void *vinCapture_ = nullptr;
};

}  // namespace cnstream

#endif  // CNSTREAM_VIDEO_CAPTURE_HPP_
