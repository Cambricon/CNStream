/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_INFER_HANDLER_HPP_
#define MODULES_INFER_HANDLER_HPP_

#include <memory>
#include <string>

#include "inferencer2.hpp"

namespace cnstream {

class InferDataObserver;

/**
 * @brief for inference handler used to do inference based on infer_server.
 */
class InferHandlerImpl : public InferHandler {
 public:
  explicit InferHandlerImpl(Inferencer2* module, Infer2Param infer_params,
                            std::shared_ptr<VideoPostproc> post_processor, std::shared_ptr<VideoPreproc> pre_processor,
                            std::shared_ptr<FrameFilter> frame_filter, std::shared_ptr<ObjFilter> obj_filter)
      : InferHandler(module, infer_params, post_processor, pre_processor, frame_filter, obj_filter) {}

  virtual ~InferHandlerImpl();

  bool Open() override;
  void Close() override;

  int Process(CNFrameInfoPtr data, bool with_objs = false) override;

  void WaitTaskDone(const std::string& stream_id) override;

  void PostEvent(EventType e, const std::string& msg) { module_->PostEvent(e, msg); }

 private:
  bool LinkInferServer();

 private:
  std::unique_ptr<InferEngine> infer_server_ = nullptr;
  std::shared_ptr<InferDataObserver> data_observer_ = nullptr;
  InferEngineSession session_ = nullptr;
  InferPreprocessType scale_platform_ = InferPreprocessType::UNKNOWN;
};

inline void InferHandler::TransmitData(const CNFrameInfoPtr& data) {
  if (module_) module_->TransmitData(data);
}

}  // namespace cnstream

#endif  // MODULES_INFER_HANDLER_HPP_
