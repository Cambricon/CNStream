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

#ifndef MODULES_SOURCE_INCLUDE_STREAM_SRC_HPP_
#define MODULES_SOURCE_INCLUDE_STREAM_SRC_HPP_

#define MAX_INPUT_DATA_SIZE (25 << 20)

#include <functional>
#include <future>
#include <string>

#include "opencv2/opencv.hpp"

#include "cnstream_core.hpp"
#include "decoder.hpp"

/*****************************************************************************
 * @brief StreamSrc is a parent class for ImageSrc, VideoSrc and RtspSrc.
 *        It is responsible for extracting data from the specified url.
 * Each extracted data will be sent at frame rate to coedc by calling callback.
 *****************************************************************************/
class StreamSrc {
 public:
  typedef std::function<bool(const libstream::CnPacket &, bool)> CallBack;
  StreamSrc();
  explicit StreamSrc(const std::string &url);
  virtual ~StreamSrc() {}
  virtual bool CheckUrl() { return true; }
  /**********************************************************
   * @brief Open url and start extracting data.
   *        Data can be sent to codec by calling the callback.
   **********************************************************/
  virtual bool Open() = 0;
  /******************************
   * @brief Stop extracting data.
   ******************************/
  virtual void Close() {}
  /******************************************
   * @brief Switching url without call Close.
   ******************************************/
  bool SwitchingUrl(const std::string &url);

  virtual cv::Size GetResolution();

  inline std::string GetUrl() const { return url_; }
  inline void SetUrl(const std::string &url) { url_ = url; }
  inline CallBack GetCallback() const { return callback_; }
  inline void SetCallback(const CallBack &callback) { callback_ = callback; }
  inline uint32_t GetFrameRate() const { return frame_rate_; }
  inline void SetFrameRate(uint32_t frame_rate) { frame_rate_ = frame_rate; }
  inline uint64_t GetFrameIndex() const { return frame_index_; }
  inline void SetFrameIndex(uint64_t frame_idx) { frame_index_ = frame_idx; }
  inline void SetLoop(const bool loop) { loop_ = loop; }
  inline bool IsLoop() { return loop_; }

 protected:
  cv::Size resolution_ = cv::Size(0, 0);
  std::unique_ptr<std::promise<cv::Size>> resolution_promise_ = nullptr;
  uint64 frame_index_ = 0;

 private:
  std::string url_;
  // Each time the data is fetched, it is invoked.
  CallBack callback_;
  // Frame rate. Vaild when the url is not a network path.
  uint32_t frame_rate_ = 0;

  bool loop_ = false;
};  // class StreamSrc

#endif  // MODULES_SOURCE_INCLUDE_STREAM_SRC_HPP_
