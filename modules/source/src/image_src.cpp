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

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "fr_controller.hpp"
#include "image_src.hpp"

static std::list<std::string> ReadFileList(const std::string& list) {
  std::ifstream ifile;
  ifile.open(list);
  std::list<std::string> files;
  if (ifile) {
    std::string path;
    while (std::getline(ifile, path)) {
      std::string file = path;
      files.push_back(file);
      path.clear();
    }
  } else {
    LOG(ERROR) << "Open file: " << list.c_str() << " failed.";
    exit(0);
  }
  ifile.close();
  return files;
}

bool ImageSrc::Open() {
  running_ = true;
  auto ret = PrepareResources();
  if (!ret) return false;
  thread_ = std::move(std::thread(&ImageSrc::ExtractingLoop, this));
  return true;
}

void ImageSrc::Close() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
  ClearResources();
}

bool ImageSrc::PrepareResources() {
  img_paths_ = ::ReadFileList(GetUrl());
  return true;
}

void ImageSrc::ClearResources() { img_paths_.clear(); }

bool ImageSrc::Extract(libstream::CnPacket* pdata) {
  if (img_paths_.empty()) {
    return false;
  }
  auto fname = img_paths_.front();
  img_paths_.pop_front();
  FILE* fid;
  fid = fopen(fname.c_str(), "rb");
  if (NULL == fid) {
    LOG(ERROR) << std::string(strerror(errno)) << " (Filename:" + fname + ")";
    return false;
  }
  fseek(fid, 0, SEEK_END);
  int64_t file_len = ftell(fid);
  rewind(fid);
  if ((file_len == 0) || (file_len > MAX_INPUT_DATA_SIZE)) {
    LOG(ERROR) << "The resolution of this image is too large to decoder."
               << " (File name: " << fname << ")";
    fclose(fid);
    return false;
  }
  pdata->length = fread(img_buffer_, 1, MAX_INPUT_DATA_SIZE, fid);
  pdata->data = reinterpret_cast<void*>(img_buffer_);
  pdata->pts = GetFrameIndex();
  SetFrameIndex(GetFrameIndex() + 1);
  fclose(fid);
  return true;
}

void ImageSrc::ReleaseData(libstream::CnPacket* pdata) {}

void ImageSrc::ExtractingLoop() {
  FrController controller(GetFrameRate());
  libstream::CnPacket pic;
  controller.Start();
  bool bEOS = false;
  while (running_ && !bEOS) {
    if (!Extract(&pic)) {
      bEOS = true;
    }
    if (GetCallback()) {
      if (!GetCallback()(pic, bEOS)) break;
    }
    ReleaseData(&pic);
    controller.Control();
  }
}
