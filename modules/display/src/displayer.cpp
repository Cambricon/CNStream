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

#include "displayer.hpp"

#include <glog/logging.h>

#include "cnstream_eventbus.hpp"
#include "display_stream.hpp"

namespace cnstream {

Displayer::Displayer(const std::string &name) : Module(name) { stream_ = new DisplayStream; }

Displayer::~Displayer() { delete stream_; }

bool Displayer::Open(ModuleParamSet paramSet) {
  if (paramSet.find("window-width") == paramSet.end() || paramSet.find("window-height") == paramSet.end() ||
      paramSet.find("cols") == paramSet.end() || paramSet.find("rows") == paramSet.end() ||
      paramSet.find("refresh-rate") == paramSet.end()) {
    LOG(ERROR) << "[Displayer] Parameter not set";
    return false;
  }
  int window_w = std::stoi(paramSet["window-width"]);
  int window_h = std::stoi(paramSet["window-height"]);
  int rows = std::stoi(paramSet["rows"]);
  int cols = std::stoi(paramSet["cols"]);
  float display_rate = std::stof(paramSet["refresh-rate"]);

  if (!stream_->Open(window_w, window_h, cols, rows, display_rate)) {
    LOG(ERROR) << "[Displayer] Invalid parameter";
    return false;
  }

  return true;
}

void Displayer::Close() { stream_->Close(); }

int Displayer::Process(CNFrameInfoPtr data) {
  stream_->Update(*data->frame.ImageBGR(), data->channel_idx);
  return 0;
}

}  // namespace cnstream
