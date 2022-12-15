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
bool CheckDir(const std::string &path, std::string *estr = nullptr);
std::string GetExePath();
void CheckExePath(const std::string &path);
bool ExistsFile(const std::string& name);
std::list<std::string> ReadFileList(const std::string &list);
std::list<std::string> GetFileNameFromDir(const std::string &dir, const char *filter);
size_t GetFileSize(const std::string &filename);
void PrintPipelinePerformance(const std::string &prefix_str, const cnstream::PipelineProfile &profile);
int GetSensorNumber(const std::list<std::string> &urls);

struct SensorParam {
  int id = 0;  // optional
  int type = 0;
  int mipi_dev = 0;
  int bus_id = 0;
  int sns_clk_id = 0;
};

bool GetSensorParam(const std::list<std::string> &urls, std::vector<SensorParam>* sensor_param_vec);
bool GetSensorId(const std::list<std::string> &urls, std::vector<int>* sensor_id_vec);

#endif  // SAMPLES_COMMON_UTIL_HPP_
