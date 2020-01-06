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
 * @file exception.h
 *
 * This file contains a declaration of the Exception class and helper register macro
 */

#ifndef CXXUTIL_EXCEPTION_H_
#define CXXUTIL_EXCEPTION_H_

#include <stdexcept>
#include <string>

/**
 * @brief Register exception class derived from Exception
 * @param CNAME Corresponding class name
 * @see Exception
 */
#define TOOLKIT_REGISTER_EXCEPTION(CNAME)                      \
  class CNAME##Error : public edk::Exception {                 \
   public:                                                     \
    explicit CNAME##Error(std::string msg) : Exception(msg) {} \
  };

namespace edk {

/**
 * @brief Toolkit base exception class, derived from runtime_error.
 */
class Exception : public std::runtime_error {
 public:
  explicit Exception(std::string msg) : std::runtime_error(msg) {}
};  // class Exception

}  // namespace edk

#endif  // CXXUTIL_EXCEPTION_H_
