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

#ifndef MODULES_SOURCE_INCLUDE_DATA_SRC_HPP_
#define MODULES_SOURCE_INCLUDE_DATA_SRC_HPP_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "cnstream_core.hpp"
#include "cnstream_module.hpp"
#include "stream_src.hpp"

typedef enum { IMAGE, VIDEO, RTSP } SrcType;

using PostDataFunction = std::function<bool(const libstream::CnPacket&, bool)>;
using SourceHandle = int;

/****************************************************************************
 * @brief DataSrc is a module for preparing and sending data to codec.
 *
 * Three types of data are accepted, image, video and rtsp. For one DataSrc
 * module, it could not have multiple types. And for one pipeline, it should
 * only have one DataSrc module. Add source paths to pipeline when building
 * a pipeline.
 * 1, For image type data source,
 * source path is an address of a file contains image address list.
 * Read image from the list. Store the image to a packet, besides the image info
 * will be described in the packet.
 * 2, For video type data source,
 * source path is an address of a video.
 * Extract frame of the video and store the frame and its info into a packet.
 * 3, For rtsp type data source,
 * source path is the IP address of a camera. Extract frame from RTP package.
 *
 * After DataSrc is started by the pipeline it belongs to, send packets at
 * frame rate.
 ****************************************************************************/
class DataSrc {
 public:
  ~DataSrc();

  SourceHandle OpenVideoSource(const std::string& url, double src_frame_rate, const PostDataFunction& post_func,
                               SrcType type, bool loop = false);

  cv::Size GetSourceResolution(SourceHandle handle) const;

  bool CloseVideoSource(SourceHandle handle);

  bool SwitchingSource(SourceHandle handle, const std::string& url);

 private:
  std::map<SourceHandle, std::shared_ptr<StreamSrc>> sources_;
  SourceHandle max_handle_ = -1;
};  // class DataSrc

#endif  // MODULES_SOURCE_INCLUDE_DATA_SRC_HPP_
