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

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "private/cnstream_common_pri.hpp"

#define set_thread_name(name) prctl(PR_SET_NAME, (name), 0, 0, 0)

namespace cnstream {

// Group:Framework Function
/*!
 * @brief Gets the number of modules that a pipeline is able to hold.
 *
 * @return The maximum modules of a pipeline can own.
 */
uint32_t GetMaxModuleNumber();

// Group:Framework Function
/*!
 * @brief Gets the number of streams that a pipeline can hold, regardless of the limitation of hardware resources.
 *
 * @return Returns the value of `kMaxStreamNum`.
 *
 * @note The factual stream number that a pipeline can process is always subject to hardware resources, no more than
 * `kMaxStreamNum`.
 */
uint32_t GetMaxStreamNumber();

static inline void setScheduling(std::thread *th, int priority) {
  struct sched_param sch_params;
  memset(&sch_params, 0, sizeof(sch_params));
  sch_params.sched_priority = priority;
  int ret;
  while ((ret = pthread_setschedparam(th->native_handle(), SCHED_RR, &sch_params)) < 0 && errno == EINTR) {}
  if (ret < 0) {
    std::cerr << "Failed to set Thread scheduling :" << std::strerror(errno) << std::endl;
  }
}

static inline void setThreadName(std::thread *th, const std::string &name) {
  /*
  int ret;
  ret = pthread_setname_np(th.native_handle(), name.c_str());
  if (ret < 0) {
    std::cerr <<"Failed to set Thread Name :" << std::strerror(errno) << std::endl;
  }
  */
  (void)(*th);
  set_thread_name(name.substr(0, 15).c_str());
}

inline std::vector<std::string> StringSplit(const std::string &s, char c) {
  std::stringstream ss(s);
  std::string piece;
  std::vector<std::string> result;
  while (std::getline(ss, piece, c)) {
    result.push_back(piece);
  }
  return result;
}

inline std::vector<std::string> StringSplitT(const std::string &s, char c) {
  auto strings = StringSplit(s, c);

  for (auto &str : strings) {
    if (!str.empty()) {
      str.erase(std::remove_if(str.begin(), str.end(), ::isblank), str.end());
    }
  }
  return strings;
}

/**
 * Defines an alias for std::vector<std::pair<std::string, std::string>>.
 */
using StringPairs = std::vector<std::pair<std::string, std::string>>;

inline StringPairs ParseConfigString(const std::string &s) {
  StringPairs sp;
  std::vector<std::string> params = cnstream::StringSplitT(s, ';');
  for (auto &p : params) {
    std::vector<std::string> key_value = cnstream::StringSplit(p, '=');
    sp.emplace_back(key_value[0], key_value[1]);
  }
  return sp;
}

inline bool SplitParams(const std::string &value, std::unordered_map<std::string, std::string> *params_map) {
  std::vector<std::string> params = cnstream::StringSplitT(value, ';');
  for (auto &param : params) {
    std::vector<std::string> key_value = cnstream::StringSplit(param, '=');
    (*params_map)[key_value[0]] = key_value[1];
  }
  return true;
}

}  // namespace cnstream

#ifndef ROUND_UP
#define ROUND_UP(addr, boundary) (((uint64_t)(addr) + (boundary)-1) & ~((boundary)-1))
#endif

#ifndef ROUND_DOWN
#define ROUND_DOWN(addr, boundary) ((uint64_t)(addr) & ~((boundary)-1))
#endif

#endif  // CNSTREAM_COMMON_HPP_
