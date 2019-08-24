/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#include "data_src.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "cnstream_eventbus.hpp"
#include "image_src.hpp"
#include "video_src.hpp"

DataSrc::~DataSrc() {
  for (auto& it : sources_) {
    it.second->Close();
  }
}

SourceHandle DataSrc::OpenVideoSource(const std::string& url, double src_frame_rate, const PostDataFunction& post_func,
                                      SrcType type, bool loop) {
  std::shared_ptr<StreamSrc> ssrc = NULL;
  switch (type) {
    case IMAGE:
      ssrc = std::make_shared<ImageSrc>(url);
      break;
    case VIDEO:
      ssrc = std::make_shared<VideoSrc>(url);
      break;
    case RTSP:
      ssrc = std::make_shared<VideoSrc>(url);
      break;
  }

  ssrc->SetCallback(post_func);
  ssrc->SetFrameRate(src_frame_rate);
  ssrc->SetLoop(loop);
  if (!ssrc->Open()) return -1;

  sources_.insert(std::make_pair(++max_handle_, ssrc));

  return max_handle_;
}

cv::Size DataSrc::GetSourceResolution(SourceHandle handle) const {
  auto iter = sources_.find(handle);
  if (sources_.end() == iter) return cv::Size(0, 0);

  return iter->second->GetResolution();
}

bool DataSrc::CloseVideoSource(SourceHandle handle) {
  auto iter = sources_.find(handle);

  if (sources_.end() == iter) return false;

  iter->second->Close();
  sources_.erase(iter);

  return true;
}

bool DataSrc::SwitchingSource(SourceHandle handle, const std::string& url) {
  auto iter = sources_.find(handle);

  if (sources_.end() == iter) return false;

  return iter->second->SwitchingUrl(url);
}
