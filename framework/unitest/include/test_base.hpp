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

#ifndef MODULES_TEST_INCLUDE_TEST_BASE_HPP_
#define MODULES_TEST_INCLUDE_TEST_BASE_HPP_

#include <string>
#include <utility>

#define PATH_MAX_LENGTH 1024

std::string GetExePath();
void CheckExePath(const std::string& path);

/**
 * @brief Creates a temp file.
 * @return Returns temp file informations.
 * Return value is a std::pair object, the first value stored temp file fd,
 * and the second value stored temp file name.
 * @note close(fd), unlink(filename) must be called when the temp file is uesd up.
 **/
std::pair<int, std::string> CreateTempFile(const std::string& filename_prefix);

#endif  // MODULES_TEST_INCLUDE_TEST_BASE_HPP_
