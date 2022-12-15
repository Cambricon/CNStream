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

#ifndef MODULES_SOURCE_HANDLER_MEM_HPP_
#define MODULES_SOURCE_HANDLER_MEM_HPP_

#include <string>

#include "data_handler_util.hpp"
#include "data_source.hpp"

namespace cnstream {

class ESMemHandlerImpl;
/*!
 * @class ESMemHandler
 *
 * @brief ESMemHandler is a class of source handler for H264/H265 bitstreams in memory.
 */
class ESMemHandler : public SourceHandler {
 public:
  /*!
   * @brief A constructor to construct a ESMemHandler object.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] param The parameters of the handler.
   *
   * @return No return value.
   */
  explicit ESMemHandler(DataSource *module, const std::string &stream_id, const ESMemSourceParam &param);

  /*!
   * @brief The destructor of ESMemHandler.
   *
   * @return No return value.
   */
  ~ESMemHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Stops source handler. The Close() function should be called afterwards.
   *
   * @return No return value.
   */
  void Stop() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;

  /*!
   * @brief Sends data in frame mode.
   *
   * @param[in] pkt The data packet.
   *
   * @return Returns 0 if this function writes data successfully.
   *         Returns -1 if it fails to writes data. The possible reason is the handler is closed,
   *         the pkt is nullptr or parsing failed.
   *
   * @note If the data does not end normally, must write pkt to notify the parser its the last packet,
   *       set the data of the pkt to nullptr or the size of the pkt to 0.
   *
   * @note Must write pkt to notify the parser it's the end of the stream,
   *       set FLAG_EOS to the flags of the pkt and set the data of the pkt to nullptr or the size to 0.
   */
  int Write(ESPacket *pkt);

 private:
  ESMemHandlerImpl *impl_ = nullptr;
};  // class ESMemHandler

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_MEM_HPP_
