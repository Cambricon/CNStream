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

#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "mtcnn_process.hpp"

using std::cout;
using std::endl;

namespace mtcnn {
void nms(cnstream::ThreadSafeVector<std::shared_ptr<cnstream::CNInferObject>> *ptrInBBoxes,
         std::vector<std::shared_ptr<cnstream::CNInferObject>> *ptrOutBBoxs, float threshold, NMS_MODE mode) {
  // sort the boundingBox
  if (!ptrInBBoxes->empty()) {
    sort(ptrInBBoxes->begin(), ptrInBBoxes->end(), CmpBoundingBox());
  }

  // pick the box
  uint32_t lastBoxIdex;
  float overLop;
  float area1, area2, inter_area;
  float infer_x1, infer_y1, infer_x2, infer_y2;
  float w, h = 0;
  ptrOutBBoxs->clear();
  while (!ptrInBBoxes->empty()) {
    lastBoxIdex = ptrInBBoxes->size() - 1;
    auto tempBox = (*ptrInBBoxes)[lastBoxIdex];
    ptrOutBBoxs->push_back((*ptrInBBoxes)[lastBoxIdex]);
    ptrInBBoxes->erase(ptrInBBoxes->begin() + lastBoxIdex);
    area1 = tempBox->bbox.w * tempBox->bbox.h;
    auto boxIter = ptrInBBoxes->begin();
    while (boxIter != ptrInBBoxes->end()) {
      infer_x1 = std::max((*boxIter)->bbox.x, tempBox->bbox.x);
      infer_y1 = std::max((*boxIter)->bbox.y, tempBox->bbox.y);
      infer_x2 = std::min((*boxIter)->bbox.x + (*boxIter)->bbox.w, tempBox->bbox.x + tempBox->bbox.w);
      infer_y2 = std::min((*boxIter)->bbox.y + (*boxIter)->bbox.h, tempBox->bbox.y + tempBox->bbox.h);
      w = std::max(infer_x2 - infer_x1 + 1, 0.0f);
      h = std::max(infer_y2 - infer_y1 + 1, 0.0f);

      // compute intersection
      inter_area = w * h;
      area2 = (*boxIter)->bbox.w * (*boxIter)->bbox.h;

      // compute overlop ratio
      if (mode == UNION) {
        overLop = inter_area / (area1 + area2 - inter_area);
      } else {
        overLop = inter_area / std::min(area1, area2);
      }
      if (overLop > threshold) {
        boxIter = ptrInBBoxes->erase(boxIter);
      } else {
        boxIter++;
      }
    }
  }
#ifdef MTCNN_DEBUG
  std::cout << "nms output box size :" << ptrOutBBoxs->size() << std::endl;
#endif
}

void generateBoundingBox(const std::vector<float *> nn_outputs, const std::pair<int, int> &shape,
                         const std::pair<float, float> &scale, const float threshold,
                         cnstream::ThreadSafeVector<std::shared_ptr<cnstream::CNInferObject>> *ptrOutBoxs) {
  auto regsOutput = nn_outputs[0];
  auto scoresOutput = nn_outputs[1];
  const int stride = 2;
  const int cellSize = 12;
  int w = shape.first;
  int h = shape.second;
  float *regs = reinterpret_cast<float *>(regsOutput);
  float *scores = reinterpret_cast<float *>(scoresOutput);
  float dx1, dy1, dx2, dy2;
  float boxWidth, boxHeight;
  float x_scale = scale.first;
  float y_scale = scale.second;

  // traver feature map
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      float tempScore = scores[(y * w + x) * 2 + 1];

      // convert each feature map's pixel to bounding box
      if (tempScore > threshold) {
        auto tempBox = std::make_shared<cnstream::CNInferObject>();
        tempBox->bbox.x = (stride * x + 1) / x_scale;
        tempBox->bbox.y = (stride * y + 1) / y_scale;
        tempBox->bbox.w = (cellSize + 1) / x_scale;
        tempBox->bbox.h = (cellSize + 1) / y_scale;
        boxWidth = tempBox->bbox.w;
        boxHeight = tempBox->bbox.h;
        dx1 = regs[(y * w + x) * 4 + 0];
        dy1 = regs[(y * w + x) * 4 + 1];
        dx2 = regs[(y * w + x) * 4 + 2];
        dy2 = regs[(y * w + x) * 4 + 3];

        // bounding box adjust
        tempBox->bbox.x += floor(boxWidth * dx1);
        tempBox->bbox.y += floor(boxHeight * dy1);
        tempBox->bbox.w += floor(boxWidth * (dx2 - dx1));
        tempBox->bbox.h += floor(boxHeight * (dy2 - dy1));
        tempBox->score = tempScore;
        ptrOutBoxs->push_back(tempBox);
      }
    }
}

void convertToSquare(std::vector<std::shared_ptr<cnstream::CNInferObject>> *ptrBBox) {
  float w, h, l;
  for (auto obj : (*ptrBBox)) {
    w = obj->bbox.w;
    h = obj->bbox.h;
    l = std::max(w, h);
    obj->bbox.x += (w - l) * 0.5;
    obj->bbox.y += (h - l) * 0.5;
    obj->bbox.w = l;
    obj->bbox.h = l;
  }
}

}  // namespace mtcnn
