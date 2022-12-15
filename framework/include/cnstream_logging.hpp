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
#define VLOG1(tag) VLOG(1) << "[CNStream " << (#tag) << " V1] "
#define VLOG2(tag) VLOG(2) << "[CNStream " << (#tag) << " V2] "
#define VLOG3(tag) VLOG(3) << "[CNStream " << (#tag) << " V3] "
#define VLOG4(tag) VLOG(4) << "[CNStream " << (#tag) << " V4] "
#define VLOG5(tag) VLOG(5) << "[CNStream " << (#tag) << " V5] "

#define LOGF_IF(tag, condition) LOG_IF(FATAL, condition) << "[CNStream " << (#tag) << " FATAL] "
#define LOGE_IF(tag, condition) LOG_IF(ERROR, condition) << "[CNStream " << (#tag) << " ERROR] "
#define LOGW_IF(tag, condition) LOG_IF(WARNING, condition) << << "[CNStream " << (#tag) << " WARN] "
#define LOGI_IF(tag, condition) LOG_IF(INFO, condition) << "[CNStream " << (#tag) << " INFO] "
#define VLOG1_IF(tag, condition) VLOG_IF(1, condition) << "[CNStream " << (#tag) << " V1] "
#define VLOG2_IF(tag, condition) VLOG_IF(2, condition) << "[CNStream " << (#tag) << " V2] "
#define VLOG3_IF(tag, condition) VLOG_IF(3, condition) << "[CNStream " << (#tag) << " V3] "
#define VLOG4_IF(tag, condition) VLOG_IF(4, condition) << "[CNStream " << (#tag) << " V4] "
#define VLOG5_IF(tag, condition) VLOG_IF(5, condition) << "[CNStream " << (#tag) << " V5] "

#endif  // CNSTREAM_CORE_LOGGING_HPP_
