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

#ifndef MODULES_INFERENCE_INCLUDE_EXCEPTION_HPP_
#define MODULES_INFERENCE_INCLUDE_EXCEPTION_HPP_

/**
 *  @file exception.hpp
 *
 *  This file contains a declaration of class CnstreamError.
 */

#include <stdexcept>
#include <string>

/**
 * Registor exception class derived from CnstreamError
 *
 * @c CNAME Class name.
 */
#define CNSTREAM_REGISTER_EXCEPTION(CNAME)                                \
  class CNAME##Error : public cnstream::CnstreamError {                   \
   public:                                                                \
    explicit CNAME##Error(const std::string &msg) : CnstreamError(msg) {} \
  };

namespace cnstream {

/**
 * Cnstream base exception class, derived from runtime_error.
 */
class CnstreamError : public std::runtime_error {
 public:
  explicit CnstreamError(const std::string &msg) : std::runtime_error(msg) {}
};  // class CnstreamError

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_EXCEPTION_HPP_
