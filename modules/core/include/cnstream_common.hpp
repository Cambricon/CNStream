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

#ifndef CNSTREAM_COMMON_HPP_
#define CNSTREAM_COMMON_HPP_

#include "glog/logging.h"

#define DISABLE_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName &) = delete;    \
  const TypeName &operator=(const TypeName &) = delete;

#define DECLARE_PRIVATE(d_ptr, Class) \
  friend class Class##Private;        \
  Class##Private *d_ptr = nullptr;

#define DECLARE_PUBLIC(q_ptr, Class) \
  friend class Class;                \
  Class *q_ptr = nullptr;

#define UNSUPPORTED LOG(FATAL) << "Not supported";

#define DEFAULT_ABORT LOG(FATAL) << "Default abort"

#define CNS_CNRT_CHECK(__EXPRESSION__)                                                                        \
  do {                                                                                                        \
    cnrtRet_t ret = (__EXPRESSION__);                                                                         \
    LOG_IF(FATAL, CNRT_RET_SUCCESS != ret) << "Call [" << #__EXPRESSION__ << "] failed, error code: " << ret; \
  } while (0)

#define CALL_CNRT_BY_CONTEXT(__EXPRESSION__, __DEV_ID__, __DDR_CHN__)          \
  do {                                                                         \
    int dev_id = (__DEV_ID__);                                                 \
    cnrtDev_t dev;                                                             \
    cnrtChannelType_t ddr_chn = static_cast<cnrtChannelType_t>((__DDR_CHN__)); \
    CNS_CNRT_CHECK(cnrtGetDeviceHandle(&dev, dev_id));                         \
    CNS_CNRT_CHECK(cnrtSetCurrentDevice(dev));                                 \
    CNS_CNRT_CHECK(cnrtSetCurrentChannel(ddr_chn));                            \
    CNS_CNRT_CHECK(__EXPRESSION__);                                            \
  } while (0)

#endif  // CNSTREAM_COMMON_HPP_
