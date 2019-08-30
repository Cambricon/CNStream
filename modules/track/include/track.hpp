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

#ifndef MODULES_TRACK_INCLUDE_TRACK_H_
#define MODULES_TRACK_INCLUDE_TRACK_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "cnstream_core.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cntrack/cntrack.h"
#include "deepsort.h"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<cnstream::CNFrameInfo>;

struct TrackerContext {
  libstream::DeepSortTrack *mlu_processer_ = nullptr;
  DS_Tracker cpu_tracker_ = nullptr;
  bool initialized_ = false;
};

class Tracker : public Module, public ModuleCreator<Tracker> {
 public:
  explicit Tracker(const std::string &name);
  ~Tracker();
  /*
   * @brief Called by pipeline when pipeline start.
   * @paramSet
   *   label_path: label path
   */
  bool Open(cnstream::ModuleParamSet paramSet) override;

  /*
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;

  /*
   * @brief do for each frame
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

 private:
  TrackerContext *GetTrackerContext(CNFrameInfoPtr data);
  std::unordered_map<int, TrackerContext *> tracker_ctxs_;
  std::shared_ptr<libstream::ModelLoader> ploader_;
};  // class Tracker

}  // namespace cnstream

#endif  // MODULES_TRACK_INCLUDE_TRACK_H_
