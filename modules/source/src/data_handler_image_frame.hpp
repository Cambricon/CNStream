/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_SOURCE_HANDLER_IMAGE_FRAME_HPP_
#define MODULES_SOURCE_HANDLER_IMAGE_FRAME_HPP_

#include <string>

#include "data_handler_util.hpp"
#include "data_source.hpp"

namespace cnstream {

class ImageFrameHandlerImpl;
/*!
 * @class ImageFrameHandler
 *
 * @brief ImageFrameHandler is a class of source handler for image frame in memory.
 */
class ImageFrameHandler : public SourceHandler {
 public:
  /*!
   * @brief A constructor to construct a ImageFrameHandler object.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] param The parameters of the handler.
   *
   * @return No return value.
   */
  explicit ImageFrameHandler(DataSource *module, const std::string &stream_id, const ImageFrameSourceParam &param);

  /*!
   * @brief The destructor of ImageFrameHandler.
   *
   * @return No return value.
   */
  ~ImageFrameHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;
  /*!
   * @brief Writes frame.
   *
   *        The frame will be converted to YUV420spNV12 by default.
   *        Unless if the first frame is with YUV420spNV21 format, the frame will be converted to YUV420spNV21.
   *        The supported color formats of inputs are YUV420spNV12, YUV420spNV21, RGB24, BGR24, ARGB32, ABGR32.
   *
   * @param[in] frame The image frame data.
   *
   * @return Returns 0 if this function writes data successfully.
   *         Returns -1 if failed to write data. The possible reason is the frame is nullptr,
   *         invalid data or converting data failed.
   *
   * @note Must write pkt to notify the handler it's the end of the stream, set the data of the pkt to nullptr.
   * @note Must not write a set of frames with both YUV420spNV12 and YUV420spNV21 formats.
   *       If the first frame written is not YUV420spNV21 format, do not write YUV420spNV21 afterwards.
   */
  int Write(ImageFrame* frame);

 private:
  ImageFrameHandlerImpl *impl_ = nullptr;
};  // class ImageFrameHandler

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_IMAGE_FRAME_HPP_
