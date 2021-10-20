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

#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "private/cnstream_common_pri.hpp"

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
 * @return Returns the value of `MAX_STREAM_NUM`.
 *
 * @note The factual stream number that a pipeline can process is always subject to hardware resources, no more than
 * `MAX_STREAM_NUM`.
 */
uint32_t GetMaxStreamNumber();

}  // namespace cnstream

#ifndef ROUND_UP
#define ROUND_UP(addr, boundary) (((uint64_t)(addr) + (boundary)-1) & ~((boundary)-1))
#endif

#ifndef ROUND_DOWN
#define ROUND_DOWN(addr, boundary) ((uint64_t)(addr) & ~((boundary)-1))
#endif

#endif  // CNSTREAM_COMMON_HPP_
