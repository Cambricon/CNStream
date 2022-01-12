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

#ifndef MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_HPP_
#define MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_HPP_

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <map>

#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_frame_va.hpp"
#include "private/cnstream_param.hpp"


#include "video/video_stream/video_stream.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

struct RtspSinkContext;
struct RtspSinkParam;

/**
 * @brief RtspSink is a module to deliver stream by RTSP protocol
 */
class RtspSink : public Module, public ModuleCreator<RtspSink> {
 public:
  /**
   * @brief RtspSink constructor
   *
   * @param  name : module name
   */
  explicit RtspSink(const std::string& name);
  /**
   * @brief RtspSink destructor
   */
  ~RtspSink();

  /**
   * @brief Called by pipeline when pipeline start.
   *
   * @param paramSet : parameter set
   *
   * @return ture if module open succeed, otherwise false.
   */
  bool Open(ModuleParamSet paramSet) override;

  /**
   * @brief  Called by pipeline when pipeline stop
   */
  void Close() override;

  /**
   * @brief Encode each frame
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   * @retval <0: failed
   */
  int Process(CNFrameInfoPtr data) override;

  void OnEos(const std::string &stream_id) override;

 private:
  RtspSinkContext * GetContext(CNFrameInfoPtr data);
  RtspSinkContext * CreateContext(CNFrameInfoPtr data, const std::string &stream_id);

  ModuleParamsHelper<RtspSinkParam>* param_helper_ = nullptr;
  int stream_index_ = 0;
  std::mutex ctx_lock_;
  std::map<std::string, RtspSinkContext *> contexts_;
  std::set<std::string> tile_streams_;
};  // class RtspSink

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_INCLUDE_RTSP_SINK_HPP_
