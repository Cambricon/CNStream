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

#ifndef MODULES_SOURCE_HANDLER_RTSP_HPP_
#define MODULES_SOURCE_HANDLER_RTSP_HPP_


#include <string>

#include "data_source.hpp"

namespace cnstream {

/*!
 * @class RtspHandler
 *
 * @brief RtspHandler is a class of source handler for rtsp stream .
 */
class RtspHandlerImpl;
class RtspHandler : public SourceHandler {
 public:
  /*!
   * @brief A constructor to construct a RtspHandler object.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] param The parameters of the handler.
   *
   * @return No return value.
   */
  explicit RtspHandler(DataSource *module, const std::string &stream_id, const RtspSourceParam &param);
  /*!
   * @brief The destructor of RtspHandler.
   *
   * @return No return value.
   */
  ~RtspHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Stops source handler.
   *
   * @return No return value
   */
  void Stop() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value
   */
  void Close() override;

 private:
  RtspHandlerImpl *impl_ = nullptr;
};  // class RtspHandler

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_RTSP_HPP_
