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
#include "postproc.hpp"

class PostprocMtcnnPnet : public cnstream::Postproc {
 public:
  int Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
              const cnstream::CNFrameInfoPtr& package) override;

  DECLARE_REFLEX_OBJECT_EX(PostprocMtcnnPnet, cnstream::Postproc)
};  // classd PostprocMtcnnPnet

IMPLEMENT_REFLEX_OBJECT_EX(PostprocMtcnnPnet, cnstream::Postproc)

int PostprocMtcnnPnet::Execute(const std::vector<float*>& net_outputs, const std::shared_ptr<edk::ModelLoader>& model,
                               const cnstream::CNFrameInfoPtr& package) {
  std::vector<std::shared_ptr<cnstream::CNInferObject>> getBox;
  std::vector<std::shared_ptr<cnstream::CNInferObject>> outBbox;

  // generate box based on pNet output
  float xScale = static_cast<float>((model->InputShapes())[0].w) / package->frame.width;
  float yScale = static_cast<float>((model->InputShapes())[0].h) / package->frame.height;
  std::pair<float, float> piarScale(xScale, yScale);
  int netOutputWeight = (model->OutputShapes())[0].w;
  int netOutputHeight = (model->OutputShapes())[0].h;
  std::pair<int, int> pairNetOutputShape(netOutputWeight, netOutputHeight);
  mtcnn::generateBoundingBox(net_outputs, pairNetOutputShape, piarScale, FLAGS_pNet_socre_threshold, &getBox);

  // nms
  mtcnn::nms(&getBox, &outBbox, FLAGS_nms_threshold, mtcnn::UNION);

  // save outBbox to frame CNInferObject
  package->objs.insert(package->objs.end(), outBbox.begin(), outBbox.end());
  return 0;
}
