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
#ifndef _CNTRACK_HPP_
#define _CNTRACK_HPP_

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <vector>

#include "cnbase/cntypes.h"
#include "cnbase/reflex_object.h"
#include "cnbase/streamlibs_error.h"
#include "cnvformat/cnvformat.h"
#include "cninfer/model_loader.h"

using CnObjects = std::vector<CnDetectObject>;

namespace libstream {

STREAMLIBS_REGISTER_EXCEPTION(CnTrack);

class CnTrack {
 public:
  static CnTrack* Create(const std::string &name);
  virtual ~CnTrack() {}

  virtual void SetModel(std::shared_ptr<ModelLoader> model,
                      int dev_id = 0,
                      uint32_t batch_size = 1) {}
  virtual void SetParams(float max_cosine_distance,
                      int nn_budget,
                      float max_iou_distance,
                      int max_age,
                      int n_init) {}
  virtual void UpdateCpuFrame(cv::Mat image,
                      const CnObjects &detects,
                      CnObjects *tracks) {}

  struct MluFrame {
    void *data;
    int device_id;
    CnGeometry size;
    CnPixelFormat format;
    int64_t frame_id;
  };
  virtual void UpdateMluFrame(const MluFrame &frame,
                      const CnObjects &detects,
                      CnObjects *tracks) noexcept(false) {}
};  // class CnTrack
}  // namespace libstream

#endif  // _CNTRACK_HPP_
