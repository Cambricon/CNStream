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

#ifndef MODULES_SOURCE_HANDLER_FILE_HPP_
#define MODULES_SOURCE_HANDLER_FILE_HPP_

#include <string>

#include "data_source.hpp"

namespace cnstream {
/*!
 * @class FileHandler
 *
 * @brief FileHandler is a class of source handler for video with format mp4, flv, matroska and USBCamera
 * ("/dev/videoxxx") .
 */
class FileHandlerImpl;
class FileHandler : public SourceHandler {
 public:
  /*!
   * @brief A constructor to construct a FileHandler object.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] param The parameters of the handler.
   *
   * @return No return value.
   */
  explicit FileHandler(DataSource *module, const std::string &stream_id, const FileSourceParam &param);
  /*!
   * @brief The destructor of FileHandler.
   *
   * @return No return value.
   */
  ~FileHandler();
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
  FileHandlerImpl *impl_ = nullptr;
};  // class FileHandler

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_FILE_HPP_
