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
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#else
#error OpenCV required
#endif
#include "../mtcnn_process.hpp"
#include "cnstream_frame.hpp"
#include "postproc.hpp"

class PostprocMtcnnRnet : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package, const std::shared_ptr<cnstream::CNInferObject>& obj) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocMtcnnRnet, cnstream::Postproc)
};  // classd PostprocMtcnnRnet

IMPLEMENT_REFLEX_OBJECT_EX(PostprocMtcnnRnet, cnstream::Postproc)

int PostprocMtcnnRnet::Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                               const cnstream::CNFrameInfoPtr& package,
                               const std::shared_ptr<cnstream::CNInferObject>& obj) {
  auto regsOutput = net_outputs[0];
  auto scoresOutput = net_outputs[1];
  float tempScore = scoresOutput[1];
  float dx1, dx2, dy1, dy2;
#ifdef MTCNN_DEBUG
  std::cout << "rNet score :" << tempScore << std::endl;
#endif
  const float scoreThreshold = FLAGS_rNet_socre_threshold;
  if (tempScore > scoreThreshold) {
    // save rNet output to CNInferObject
    float w = obj->bbox.w;
    float h = obj->bbox.h;
    dx1 = regsOutput[0] * w;
    dy1 = regsOutput[1] * h;
    dx2 = regsOutput[2] * w;
    dy2 = regsOutput[3] * h;
    obj->bbox.x += dx1;
    obj->bbox.y += dy1;
    obj->bbox.w += dx2 - dx1;
    obj->bbox.h += dy2 - dy1;
    obj->score = tempScore;
    package->objs.push_back(obj);
  }
  return 0;
}
