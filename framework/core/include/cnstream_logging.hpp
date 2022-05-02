/*************************************************************************
* Copyright (C) [2019-2022] by Cambricon, Inc. All rights reserved
*
* This source code is licensed under the Apache-2.0 license found in the
* LICENSE file in the root directory of this source tree.
*
* A part of this source code is referenced from glog project.
* https://github.com/google/glog/blob/master/src/logging.cc
*
* Copyright (c) 1999, Google Inc.
*
* This source code is licensed under the BSD 3-Clause license found in the
* LICENSE file in the root directory of this source tree.
*
*************************************************************************/

#ifndef CNSTREAM_CORE_LOGGING_HPP_
#define CNSTREAM_CORE_LOGGING_HPP_
#include <glog/logging.h>

#define LOGF(tag) LOG(FATAL) << "[CNStream " << (#tag) << " FATAL] "
#define LOGE(tag) LOG(ERROR) << "[CNStream " << (#tag) << " ERROR] "
#define LOGW(tag) LOG(WARNING) << "[CNStream " << (#tag) << " WARN] "
#define LOGI(tag) LOG(INFO) << "[CNStream " << (#tag) << " INFO] "
#define LOGD(tag) VLOG(1) << "[CNStream " << (#tag) << " DEBUG] "
#define LOGT(tag) VLOG(2) << "[CNStream " << (#tag) << " TRACE] "

#define LOGF_IF(tag, condition) LOG_IF(FATAL, condition) << "[CNStream " << (#tag) << " FATAL] "
#define LOGE_IF(tag, condition) LOG_IF(ERROR, condition) << "[CNStream " << (#tag) << " ERROR] "
#define LOGW_IF(tag, condition) LOG_IF(WARNING, condition) << << "[CNStream " << (#tag) << " WARN] "
#define LOGI_IF(tag, condition) LOG_IF(INFO, condition) << "[CNStream " << (#tag) << " INFO] "
#define LOGD_IF(tag, condition) VLOG_IF(1, condition) << "[CNStream " << (#tag) << " DEBUG] "
#define LOGT_IF(tag, condition) VLOG_IF(2, condition) << "[CNStream " << (#tag) << " TRACE] "

#endif  // CNSTREAM_CORE_LOGGING_HPP_
