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
#ifndef MODULES_ENCODE_MLU_HANDLER_HPP_
#define MODULES_ENCODE_MLU_HANDLER_HPP_

#include <map>
#include <memory>
#include <mutex>
#include <future>
#include <string>

#include "cnedk_encode.h"
#include "cnstream_frame.hpp"

#include "encode_handler.hpp"

namespace cnstream {

class VencMluHandler : public VencHandler {
 public:
  explicit VencMluHandler(int dev_id_);
  ~VencMluHandler();

 public:
  int SendFrame(std::shared_ptr<CNFrameInfo> data) override;
  int SendFrame(Scaler::Buffer* data) override;

  static int OnFrameBits_(CnedkVEncFrameBits *framebits, void *userdata) {
    VencMluHandler *thiz = reinterpret_cast<VencMluHandler *>(userdata);
    return thiz->OnFrameBits(framebits);
  }
  static int OnEos_(void *userdata) {
    VencMluHandler *thiz = reinterpret_cast<VencMluHandler *>(userdata);
    return thiz->OnEos();
  }
  static int OnError_(int errcode, void *userdata) {
    VencMluHandler *thiz = reinterpret_cast<VencMluHandler *>(userdata);
    return thiz->OnError(errcode);
  }

 private:
  int OnEos();
  int OnError(int errcode);
  int InitEncode(int width, int height, CnedkBufSurfaceColorFormat color_format);

 private:
  std::unique_ptr<std::promise<void>> eos_promise_;
  void *venc_handle_ = nullptr;
  uint32_t session_id_ = (uint32_t)-1;
  std::mutex mutex_;
  int dev_id_;
  std::string platform_;
};

}  // namespace cnstream

#endif

