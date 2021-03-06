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

#include <limits.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <atomic>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace cnstream {

/**
 * @brief Flag to specify how bus watcher handle a single event.
 */
enum EventType {
  EVENT_INVALID,  ///< An invalid event type.
  EVENT_ERROR,    ///< An error event.
  EVENT_WARNING,  ///< A warning event.
  EVENT_EOS,      ///< An EOS event.
  EVENT_STOP,     ///< Stops an event that is called by application layer usually.
  EVENT_STREAM_ERROR,  ///< A stream error event.
  EVENT_TYPE_END  ///< Reserved for your custom events.
};


class NonCopyable {
 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

 private:
  NonCopyable(const NonCopyable& ) = delete;
  NonCopyable(NonCopyable&& ) = delete;
  NonCopyable& operator=(const NonCopyable& ) = delete;
  NonCopyable& operator=(NonCopyable&& ) = delete;
};

/*helper functions
 */
static const pthread_t invalid_pthread_tid = static_cast<pthread_t>(-1);

inline void SetThreadName(const std::string& name, pthread_t thread = invalid_pthread_tid) {
  /*name length should be less than 16 bytes */
  if (name.empty() || name.size() >= 16) {
    return;
  }
  if (thread == invalid_pthread_tid) {
    prctl(PR_SET_NAME, name.c_str());
    return;
  }
  pthread_setname_np(thread, name.c_str());
}

inline std::string GetThreadName(pthread_t thread = invalid_pthread_tid) {
  char name[80];
  if (thread == invalid_pthread_tid) {
    prctl(PR_GET_NAME, name);
    return name;
  }
  pthread_getname_np(thread, name, 80);
  return name;
}

/*pipeline capacities*/
constexpr size_t INVALID_MODULE_ID = (size_t)(-1);
uint32_t GetMaxModuleNumber();

constexpr uint32_t INVALID_STREAM_IDX = (uint32_t)(-1);
uint32_t GetMaxStreamNumber();

/**
 * Limit the resource for each stream,
 * there will be no more than "flow_depth" frames simultaneously.
 * Disabled by default.
 */
void SetFlowDepth(int flow_depth);
int GetFlowDepth();

/*for force-remove-source*/
bool CheckStreamEosReached(const std::string &stream_id, bool sync = true);
void SetStreamRemoved(const std::string &stream_id, bool value = true);
bool IsStreamRemoved(const std::string &stream_id);

/**
 * @brief Converts number to string
 *
 * @param number the number
 * @param width Padding with zero
 * @return Returns string
 */
template <typename T>
std::string NumToFormatStr(const T &number, uint32_t width) {
    std::stringstream ss;
    ss << std::setw(width) << std::setfill('0') << number;
    return ss.str();
}
}  // namespace cnstream

#endif  // CNSTREAM_COMMON_HPP_
