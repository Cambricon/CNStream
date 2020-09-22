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

#ifndef __CNSTREAM_SERVICE__HPP__
#define __CNSTREAM_SERVICE__HPP__

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#ifdef MAKE_PYTHONAPI
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
#endif

#include "cnstream_core.hpp"
#include "cnstype.h"

class PipelineHandler;
class CNSDataObserver;
class CNSEventObserver;
class CNSMsgObserver;

class PyCNService {
 public:
  PyCNService() {}
  ~PyCNService();

  void InitService(const CNServiceInfo &info);

  bool Start(const std::string &stream_url, const std::string &config_str);
  void Stop();

  inline bool IsRegisteredData() { return cnsinfo_.register_data; }
  inline bool IsRunning() { return is_running_.load(); }

  void FrameDataCallBack(std::shared_ptr<cnstream::CNFrameInfo> data);
  void WaitStop();

#if MAKE_PYTHONAPI
  bool ReadOneFrame(CNSFrameInfo *ret_frame, py::array_t<uint8_t> *img_data);
#else
  bool ReadOneFrame(CNSFrameInfo *ret_frame, uint8_t *img_data);
#endif

 private:
  void DestoryResource();

 private:
  CNServiceInfo cnsinfo_;
  std::mutex mutex_;
  std::mutex stop_mtx_;
  std::atomic<bool> is_running_{false};

  int cache_size_ = 20;
  CNSEventObserver *observer_ = nullptr;
  CNSMsgObserver *msg_observer_ = nullptr;
  CNSDataObserver *data_observer_ = nullptr;
  PipelineHandler *ppipe_handler_ = nullptr;
  CNSQueue<CNSFrame> *cache_frameq_ = nullptr;
};

#endif
