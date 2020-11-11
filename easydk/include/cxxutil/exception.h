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

namespace edk {

/**
 * @brief edk base exception class, derived from exception.
 */
class Exception : public std::exception {
 public:
  /**
   * @brief Error code enumaration
   */
  enum Code {
    INTERNAL = 0,     ///< internal error
    UNSUPPORTED = 1,  ///< unsupported function
    INVALID_ARG = 2,  ///< invalid argument
    MEMORY = 3,       ///< memory error
    TIMEOUT = 4,      ///< timeout
    INIT_FAILED = 5,  ///< create failed or init failed
    UNAVAILABLE = 6,  ///< resoure unavailable
  };

  /**
   * @brief Get error code as string
   *
   * @return error code as string
   */
  std::string CodeString() const noexcept {
#define RETURN_CODE_STRING(code) \
  case code:                     \
    return #code
    switch (code_) {
      RETURN_CODE_STRING(INTERNAL);
      RETURN_CODE_STRING(UNSUPPORTED);
      RETURN_CODE_STRING(INVALID_ARG);
      RETURN_CODE_STRING(MEMORY);
      RETURN_CODE_STRING(TIMEOUT);
      default:
        return "UNKNOWN";
    }
#undef RETURN_CODE_STRING
  }

  /**
   * @brief Constructor.
   *
   * @param code[in] error code
   * @param file[in] source file name where the error has occurred
   * @param line[in] line number in the source file where the error has occurred
   * @param func[in] function name.
   * @param msg[in] error description
   */
  explicit Exception(Code code, const std::string& file, int line, const std::string& func, const std::string& msg)
      : code_(code) {
    msg_ = file.substr(file.find_last_of('/') + 1) + ":" + std::to_string(line) + " (" + func + ") " + CodeString() +
           "] " + msg;
  }

  /**
   * @brief Get error code
   *
   * @return error code
   */
  Code ErrorCode() const noexcept { return code_; }

  /**
   * @brief Get formatted error message
   *
   * @return formatted error message
   */
  const char* what() const noexcept override { return msg_.c_str(); }

 private:
  Code code_;
  std::string msg_;
};  // class Exception

}  // namespace edk

/**
 * @def EDK_THROW_EXCEPTION(CNAME)
 * @brief Throw exception with status code and description
 * @param code status code
 * @param exception description
 * @see edk::Exception
 */
#define THROW_EXCEPTION(code, msg)                                 \
  do {                                                             \
    throw edk::Exception(code, __FILE__, __LINE__, __func__, msg); \
  } while (0)

#endif  // CXXUTIL_EXCEPTION_H_
