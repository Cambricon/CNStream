/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
#ifndef LIBSTREAM_INCLUDE_BASE_STREAMLIBS_ERROR_H_
#define LIBSTREAM_INCLUDE_BASE_STREAMLIBS_ERROR_H_

#include <stdexcept>
#include <string>

#define STREAMLIBS_REGISTER_EXCEPTION(CNAME)         \
class CNAME##Error : public libstream::StreamlibsError {        \
 public:                                         \
  explicit CNAME##Error(std::string msg) :       \
    StreamlibsError(msg) {}                          \
};


namespace libstream {

class StreamlibsError : public std::runtime_error {
 public:
  explicit StreamlibsError(std::string msg) :
      std::runtime_error(msg) {}
};  // class StreamlibError

}  // namespace libstream

#endif  // LIBSTREAM_INCLUDE_BASE_STREAMLIBS_ERROR_H_
