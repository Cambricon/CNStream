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

/**
 * @file mlu_context.h
 *
 * This file contains a declaration of the MluContext class.
 */

#ifndef EASYINFER_MLU_CONTEXT_H_
#define EASYINFER_MLU_CONTEXT_H_

#include "cxxutil/exception.h"

namespace edk {

TOOLKIT_REGISTER_EXCEPTION(MluContext);

/**
 * @brief MLU environment helper class
 */
class MluContext {
 public:
  /**
   * @brief Get the device id
   *
   * @return Device id
   */
  int DeviceId() const;

  /**
   * @brief Set the device id
   *
   * @param id[in] Device id
   */
  void SetDeviceId(int id);

  /**
   * @brief Get the MLU channel id
   *
   * @return MLU Channel id
   */
  int ChannelId() const;

  /**
   * @brief Set the MLU channel id in range [0, 3]
   *
   * @param id[in] MLU channel id
   */
  void SetChannelId(int id);

  /**
   * @brief Bind MLU environment for this thread
   * @note Each thread processing MLU memory or task need to set MLU environment
   */
  void ConfigureForThisThread() const;

 private:
  int dev_id_ = 0;
  int channel_id_ = -1;
};  // class MluContext

}  // namespace edk

#endif  // EASYINFER_MLU_CONTEXT_H_
