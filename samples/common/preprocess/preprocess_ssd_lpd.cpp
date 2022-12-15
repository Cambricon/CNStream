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

#include <vector>

#include "cnstream_logging.hpp"
#include "cnstream_preproc.hpp"
#include "preprocess_common.hpp"

// #define LOCAL_DEBUG_DUMP_IMAGE
#ifdef LOCAL_DEBUG_DUMP_IMAGE
#include <atomic>
#endif

class PreprocSSDLpd : public cnstream::Preproc {
 public:
  int OnTensorParams(const infer_server::CnPreprocTensorParams *params) override {
    std::unique_lock<std::mutex> lk(mutex_);
    if (GetNetworkInfo(params, &info_) < 0) {
      LOGE(PERPROC) << "[PreprocSSDLpd] get network information failed.";
      return -1;
    }

    if (info_.c != 3) {
      LOGE(PERPROC) << "[PreprocSSDLpd] input c is not 3, not suppoted yet";
      return -1;
    }

    VLOG1(PERPROC) << "[PreprocSSDLpd] Model input : w = " << info_.w << ", h = " << info_.h << ", c = " << info_.c
                   << ", dtype = " << static_cast<int>(info_.dtype)
                   << ", pixel_format = " << static_cast<int>(info_.format);

    if (info_.dtype == infer_server::DataType::FLOAT32) {
      mean_ = {127.5, 127.5, 127.5};
      std_ = {127.5, 127.5, 127.5};
    }

    return 0;
  }

  int Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
              const std::vector<CnedkTransformRect> &src_rects) {
    bool keep_aspect_ratio = false;

    bool mean_std = false;
    if (info_.dtype == infer_server::DataType::FLOAT32) {
      mean_std = true;
    }

    infer_server::NetworkInputFormat fmt;

    // If the data type of the model is float, we need to set the input format to tensor.
    // If model input pixel format is tensor, set real model input pixel format here, e.g., for this network it is BGR.
    if (info_.format == infer_server::NetworkInputFormat::TENSOR) {
      fmt = infer_server::NetworkInputFormat::BGR;
    } else {
      fmt = info_.format;
    }

    if (hw_accel_) {
      if (PreprocessTransform(src, dst, src_rects, info_, fmt, keep_aspect_ratio, 0, mean_std, mean_, std_) != 0) {
        LOGE(PERPROC) << "[PreprocSSDLpd] preprocess on mlu failed.";
        return -1;
      }
    } else {
      if (PreprocessCpu(src, dst, src_rects, info_, fmt, keep_aspect_ratio, 0, mean_std, mean_, std_) != 0) {
        LOGE(PERPROC) << "[PreprocSSDLpd] preprocess on cpu failed.";
        return -1;
      }
    }

#ifdef LOCAL_DEBUG_DUMP_IMAGE
    static std::atomic<unsigned int> count{0};
    std::unique_lock<std::mutex> lk(mutex_);
    SaveResult("preproc_ssd_lpd", count.load(), src->GetNumFilled(), dst, info_);
    ++count;
    lk.unlock();
#endif
    return 0;
  }

  ~PreprocSSDLpd() = default;

 private:
  std::mutex mutex_;
  cnstream::CnPreprocNetworkInfo info_;
  std::vector<float> mean_;
  std::vector<float> std_;

 private:
  DECLARE_REFLEX_OBJECT_EX(PreprocSSDLpd, cnstream::Preproc);
};  // class PreprocSSDLpd

IMPLEMENT_REFLEX_OBJECT_EX(PreprocSSDLpd, cnstream::Preproc);
