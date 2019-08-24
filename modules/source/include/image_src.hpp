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

#ifndef MODULES_SOURCE_INCLUDE_IMAGE_SRC_HPP_
#define MODULES_SOURCE_INCLUDE_IMAGE_SRC_HPP_

#include <list>
#include <string>
#include <thread>

#include "stream_src.hpp"

/*****************************************************************************
 * @brief ImageSrc is a child class of StreamSrc.
 *        It is used to read images from file list stored in a file,
 *        whose storage path is added by pipeline.
 * Each image will be sent at frame rate to coedc by calling callback.
 *****************************************************************************/
class ImageSrc : public StreamSrc {
 public:
  ImageSrc() {}
  explicit ImageSrc(const std::string& url) : StreamSrc(url) {}
  bool Open() override;
  void Close() override;
  virtual ~ImageSrc() {}
  cv::Size GetResolution() override { return cv::Size(0, 0); }

 protected:
  virtual bool PrepareResources();
  virtual void ClearResources();
  virtual bool Extract(libstream::CnPacket* pdata);
  virtual void ReleaseData(libstream::CnPacket* pdata);

 private:
  void ExtractingLoop();

 private:
  std::thread thread_;
  bool running_ = false;
  std::list<std::string> img_paths_;
  uint8_t img_buffer_[MAX_INPUT_DATA_SIZE];
};  // class ImageSrc

#endif  // MODULES_SOURCE_INCLUDE_IMAGE_SRC_HPP_
