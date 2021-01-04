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

#ifndef SAMPLES_COMMON_UTIL_HPP_
#define SAMPLES_COMMON_UTIL_HPP_

#define PATH_MAX_LENGTH 1024

#include <profiler/profile.hpp>

#include <iostream>
#include <list>
#include <string>
#include <vector>

std::vector<std::string> LoadLabels(const std::string &filename);
bool CheckDir(const std::string &path, std::string *estr);
std::string GetExePath();
void CheckExePath(const std::string &path);
std::list<std::string> ReadFileList(const std::string &list);
std::list<std::string> GetFileNameFromDir(const std::string &dir, const char *filter);
size_t GetFileSize(const std::string &filename);
void PrintPipelinePerformance(const std::string& prefix_str, const cnstream::PipelineProfile& profile);

#endif  // SAMPLES_COMMON_UTIL_HPP_
