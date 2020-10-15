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

#ifndef __CNSTREAM_PIPELINE_HANDLER__HPP__
#define __CNSTREAM_PIPELINE_HANDLER__HPP__

#include <mutex>
#include <string>

#include "cnstream_core.hpp"
#include "cnstype.h"

class PipelineHandler {
 public:
  PipelineHandler() {}
  ~PipelineHandler();

  bool CreatePipeline(const std::string& config_fname, const std::string perf_dir);

  void SetMsgObserver(cnstream::StreamMsgObserver*);
  void SetDataObserver(cnstream::IModuleObserver*);

  bool Start();
  void Stop();

  bool AddStream(const std::string& stream_url, const std::string& stream_id, int fps = 25, bool loop = false);
  bool RemoveStream(const std::string& stream_id);

 private:
  std::mutex stop_mtx_;
  std::string stream_id_;
  std::string perf_dir_ = "perf_cache";
  cnstream::Pipeline* ppipeline_ = nullptr;
};

#endif
