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
#include "video_capture.hpp"

#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include "cnedk_vin_capture.h"
#include "cnstream_logging.hpp"

namespace cnstream {

VinCapture::VinCapture(const std::string &stream_id, ICaptureResult *cb, IUserPool *pool)
    : IVinCapture(stream_id, cb, pool) {}

VinCapture::~VinCapture() { Destroy(); }

bool VinCapture::Create(int sensor_id) {
  if (vinCapture_) {
    LOGW(SOURCE) << "[" << stream_id_ << "]: VinCapture create duplicated.";
    return false;
  }

  CnedkVinCaptureCreateParams create_params;
  memset(&create_params, 0, sizeof(create_params));
  create_params.sensor_id = sensor_id;
  create_params.surf_timeout_ms = 5000;
  create_params.userdata = this;
  create_params.GetBufSurf = VinCapture::GetBufSurface_;
  create_params.OnFrame = VinCapture::OnFrame_;
  create_params.OnError = VinCapture::OnError_;

  int ret = CnedkVinCaptureCreate(&vinCapture_, &create_params);
  if (ret) {
    LOGE(SOURCE) << "[" << stream_id_ << "]: Create capture failed";
    return false;
  }
  LOGI(SOURCE) << "[" << stream_id_ << "]: Finish create capture";
  return true;
}

void VinCapture::Destroy() {
  if (vinCapture_) {
    CnedkVinCaptureDestroy(vinCapture_);
    vinCapture_ = nullptr;
  }
}

bool VinCapture::Process(int timeout_ms) {
  if (vinCapture_) {
    if (CnedkVinCapture(vinCapture_, timeout_ms) < 0) {
      return false;
    }
    return true;
  }
  return false;
}

int VinCapture::GetBufSurface(CnedkBufSurface **surf, int timeout_ms) {
  cnedk::BufSurfWrapperPtr wrapper = pool_->GetBufSurface(timeout_ms);
  if (wrapper) {
    *surf = wrapper->BufSurfaceChown();
    return 0;
  }
  return -1;
}

int VinCapture::OnFrame(CnedkBufSurface *surf) {
  cnedk::BufSurfWrapperPtr wrapper = std::make_shared<cnedk::BufSurfaceWrapper>(surf);
  if (result_) {
    result_->OnCaptureFrame(wrapper);
    return 0;
  }
  return -1;
}

int VinCapture::OnError(int errcode) {
  if (result_) {
    // FIXME
    result_->OnCaptureError(errcode);
    return 0;
  }
  return -1;
}

}  // namespace cnstream
