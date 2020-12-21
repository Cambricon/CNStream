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
#include "discard_frame.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace cnstream {
DiscardFrame::DiscardFrame(const std::string& name) : Module(name) {
  hasTransmit_.store(1);
  param_register_.SetModuleDesc("DiscardFrame is a module for discard frame every n frames.");
  param_register_.Register("discard_interval",
                           "How many frames will be discarded between two frames"
                           " which will be sent to next modeule.");
}

bool DiscardFrame::Open(ModuleParamSet paramSet) {
  if (paramSet.find("discard_interval") == paramSet.end()) {
    // set the default value
    frame_Mod = 0;
    std::cout << "warning! use the default diacard_frame value." << std::endl;
  } else {
    frame_Mod = std::stoi(paramSet["discard_interval"]);
    if (frame_Mod < 0) {
      std::cout << "cannot give the negative value!" << std::endl;
      return false;
    }
  }
  return true;
}
void DiscardFrame::Close() { /*empty*/
}

int DiscardFrame::Process(std::shared_ptr<CNFrameInfo> data) {
  if (0 == frame_Mod) {
    hasTransmit_.store(0);
    return 0;
  } else {
    if (data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS) {
      container_->ProvideData(this, data);
    }
    if ((data->frame.frame_id + 1) % frame_Mod == 0) {
      container_->ProvideData(this, data);
    }
  }
  return 1;
}

DiscardFrame::~DiscardFrame() {}

bool DiscardFrame::CheckParamSet(const ModuleParamSet& paramSet) const {
  ParametersChecker checker;
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOGW(MODULESCONTRIB) << "[DiscardFrame] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("discard_interval") != paramSet.end()) {
    std::string err_msg;
    if (!checker.IsNum({"discard_interval"}, paramSet, err_msg)) {
      LOGE(MODULESCONTRIB) << "[DiscardFrame] " << err_msg;
      return false;
    }
  }
  return true;
}

}  // namespace cnstream
