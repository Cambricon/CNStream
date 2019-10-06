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

#ifndef MODULES_CORE_INCLUDE_CONNECTOR_HPP_
#define MODULES_CORE_INCLUDE_CONNECTOR_HPP_

#include <memory>

#include "cnstream_frame.hpp"

namespace cnstream {

using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

class ConnectorPrivate;
class Conveyor;

/****************************************************************
 * @brief Connect two modules.
 *        Transmit data between modules through conveyor(s).
 *
 * Connector could be blocked to balance the various speed of
 * different modules in the same pipeline.
 *
 * connector
 * /----------------------------------------------------------\
 * |    conveyor                                              |
 * |   /---------------------------------------------- ---\   |
 * |   |      data              data ...                  |   |
 * |   |   /--------\         /--------\                  |   |
 * |   | --|  info  |---------|  info  |----- ... -----   |   |
 * |   |   |  ...   |         |  ...   |                  |   |
 * |   |   \--------/         \--------/                  |   |
 * |   |                       data queue                 |   |
 * |   \--------------------------------------------------/   |
 * |                                                          |
 * |    conveyor  ... ...                                     |
 * \----------------------------------------------------------/
 ****************************************************************/
class Connector {
 public:
  /****************************************************************************
   * @brief Connector constructor.
   * @param
   *   [conveyor_count]: the conveyor num of this connector.
   *   [conveyor_capacity]: the maximum buffer number of a conveyor.
   ***************************************************************************/
  explicit Connector(const size_t conveyor_count, size_t conveyor_capacity = 20);
  ~Connector();

  const size_t GetConveyorCount() const;
  Conveyor* GetConveyor(int conveyor_idx) const;
  size_t GetConveyorCapacity() const;

  CNFrameInfoPtr PopDataBufferFromConveyor(int conveyor_idx);
  void PushDataBufferToConveyor(int conveyor_idx, CNFrameInfoPtr data);

  void Start();
  void Stop();
  bool IsStopped() const;
  void EmptyDataQueue();

 private:
  DECLARE_PRIVATE(d_ptr_, Connector);
  DISABLE_COPY_AND_ASSIGN(Connector);
};  // class Connector

}  // namespace cnstream

#endif  // MODULES_CORE_INCLUDE_CONNECTOR_HPP_
