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

#ifndef MTCNN_PROCESS_HPP
#define MTCNN_PROCESS_HPP

#include <gflags/gflags.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_frame.hpp"

DECLARE_double(nms_threshold);
DECLARE_double(pNet_socre_threshold);
DECLARE_double(rNet_socre_threshold);
DECLARE_double(oNet_socre_threshold);
DECLARE_bool(detect_face_points);

namespace mtcnn {
// nms compare function object
struct CmpBoundingBox {
  bool operator()(const std::shared_ptr<cnstream::CNInferObject> b1,
                  const std::shared_ptr<cnstream::CNInferObject> b2) {
    return b1->score < b2->score;
  }
};

// nms mode
enum NMS_MODE { UNION, MIN };

// Non-maximum suppression to reduce overlapping boundary boxes that may be the same face
void nms(cnstream::ThreadSafeVector<std::shared_ptr<cnstream::CNInferObject>> *ptrInBBoxes,
         std::vector<std::shared_ptr<cnstream::CNInferObject>> *ptrOutBBoxs, float threshold, NMS_MODE mode);

// Convert mtcnn::pNet's output feature map to bounding box structure
void generateBoundingBox(const std::vector<float *> nn_outputs, const std::pair<int, int> &shape,
                         const std::pair<float, float> &scale, const float threshold,
                         cnstream::ThreadSafeVector<std::shared_ptr<cnstream::CNInferObject>> *pOutBBoxs);

// Convert bouding box to a standard square
void convertToSquare(std::vector<std::shared_ptr<cnstream::CNInferObject>> *ptrBBox);
}  // namespace mtcnn
#endif
