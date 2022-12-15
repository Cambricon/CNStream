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

class PreprocYolov5 : public cnstream::Preproc {
 public:
  int OnTensorParams(const infer_server::CnPreprocTensorParams *params) override {
    std::unique_lock<std::mutex> lk(mutex_);
    if (GetNetworkInfo(params, &info_) < 0) {
      LOGE(PERPROC) << "[PreprocYolov5] get network information failed.";
      return -1;
    }

    if (info_.c != 3) {
      LOGE(PERPROC) << "[PreprocYolov5] input c is not 3, not suppoted yet";
      return -1;
    }

    VLOG1(PERPROC) << "[PreprocYolov5] Model input : w = " << info_.w << ", h = " << info_.h << ", c = " << info_.c
                   << ", dtype = " << static_cast<int>(info_.dtype)
                   << ", pixel_format = " << static_cast<int>(info_.format);
    return 0;
  }

  int Execute(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
              const std::vector<CnedkTransformRect> &src_rects) {
    bool keep_aspect_ratio = true;
    int pad_val = 114;
    infer_server::NetworkInputFormat fmt = info_.format;
    if (hw_accel_) {
      if (PreprocessTransform(src, dst, src_rects, info_, fmt, keep_aspect_ratio, pad_val) != 0) {
        LOGE(PERPROC) << "[PreprocYolov5] preprocess on mlu failed.";
        return -1;
      }
    } else {
      if (PreprocessCpu(src, dst, src_rects, info_, fmt, keep_aspect_ratio, pad_val) != 0) {
        LOGE(PERPROC) << "[PreprocYolov5] preprocess on cpu failed.";
        return -1;
      }
    }

#ifdef LOCAL_DEBUG_DUMP_IMAGE
    static std::atomic<unsigned int> count{0};
    std::unique_lock<std::mutex> lk(mutex_);
    SaveResult("preproc_yolov5", count.load(), src->GetNumFilled(), dst, info_);
    ++count;
    lk.unlock();
#endif
    return 0;
  }

  ~PreprocYolov5() = default;

 private:
  std::mutex mutex_;
  cnstream::CnPreprocNetworkInfo info_;

 private:
  DECLARE_REFLEX_OBJECT_EX(PreprocYolov5, cnstream::Preproc);
};  // class PreprocYolov5

IMPLEMENT_REFLEX_OBJECT_EX(PreprocYolov5, cnstream::Preproc);
