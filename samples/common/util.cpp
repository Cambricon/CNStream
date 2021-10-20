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

#include "util.hpp"
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#elif defined(__linux) || defined(__unix)
#include <dirent.h>
#include <unistd.h>
#endif
#include <gflags/gflags.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <cerrno>
#include <fstream>
#include <limits>
#include <list>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"
#include "profiler/module_profiler.hpp"

DEFINE_int32(perf_level, 0, "perf level");

std::string GetExePath() {
  char path[PATH_MAX_LENGTH];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_LENGTH);
  if (cnt < 0 || cnt >= PATH_MAX_LENGTH) {
    return NULL;
  }
  for (int i = cnt - 1; i >= 0; --i) {
    if ('/' == path[i]) {
      path[i + 1] = '\0';
      break;
    }
  }
  std::string result(path);
  return result;
}

void CheckExePath(const std::string &path) {
  extern int errno;
  if (path.size() == 0) {
    LOGF_IF(DEMO, 0 != errno) << std::string(strerror(errno));
    LOGF(DEMO) << "length of exe path is larger than " << PATH_MAX_LENGTH;
  }
}

inline bool exists_file(const std::string &name) { return (access(name.c_str(), F_OK) != -1); }

std::list<std::string> ReadFileList(const std::string &list) {
  std::ifstream ifile;
  ifile.open(list);
  std::list<std::string> files;
  if (ifile) {
    std::string path;
    while (std::getline(ifile, path)) {
      std::string file = path;
      if (file != "") {
        files.push_back(file);
        path.clear();
      }
    }
  } else {
    LOGE(DEMO) << "Open file: " << list.c_str() << " failed.";
    exit(0);
  }
  ifile.close();
  return files;
}

std::vector<std::string> LoadLabels(const std::string &filename) {
  std::vector<std::string> labels;
  std::ifstream file(filename);
  LOGF_IF(DEMO, !file.is_open()) << "file:" << filename << " open failed.";
  std::string line;
  while (std::getline(file, line)) {
    labels.push_back(std::string(line));
  }
  file.close();
  return labels;
}

bool CheckDir(const std::string &path, std::string *estr) {
  bool ret = true;
  struct stat st;
  if (::stat(path.c_str(), &st)) {
    // errmsg
    ret = false;
    *estr = std::string("Check dir '") + path + "' failed: " + std::string(strerror(errno));

  } else {
    if (!(st.st_mode & S_IFDIR)) {
      *estr = std::string("Check dir '") + path + "' failed: " + std::string("Not a directory");
      ret = false;
    } else {
      if (access(path.c_str(), W_OK)) {
        ret = false;
        *estr = std::string("Check dir '") + path + "' failed: " + std::string(strerror(errno));
      }
    }
  }
  return ret;
}

std::list<std::string> GetFileNameFromDir(const std::string &dir, const char *filter) {
  std::list<std::string> files;
#if defined(_WIN32) || defined(_WIN64)
  int64_t hFile = 0;
  struct _finddata_t fileinfo;
  std::string path;
  if ((hFile = _findfirst(path.assign(dir).append("/" + std::string(filter)).c_str(), &fileinfo)) != -1) {
    do {
      if (!(fileinfo.attrib & _A_SUBDIR)) {  // not directory
        std::string file_path = dir + "/" + fileinfo.name;
        files.push_back(file_path);
      }
    } while (_findnext(hFile, &fileinfo) == 0);
    _findclose(hFile);
  }
#elif defined(__linux) || defined(__unix)
  DIR *pDir = nullptr;
  struct dirent *pEntry;
  pDir = opendir(dir.c_str());
  if (pDir != nullptr) {
    while ((pEntry = readdir(pDir)) != nullptr) {
      if (strcmp(pEntry->d_name, ".") == 0 || strcmp(pEntry->d_name, "..") == 0
          || strstr(pEntry->d_name, strstr(filter, "*") + 1) == nullptr || pEntry->d_type != DT_REG) {  // regular file
        continue;
      }
      std::string file_path = dir + "/" + pEntry->d_name;
      files.push_back(file_path);
    }
    closedir(pDir);
  }
#endif
  return files;
}

size_t GetFileSize(const std::string &filename) {
#if defined(_WIN32) || defined(_WIN64)
  struct _stat file_stat;
  _stat(filename.c_str(), &file_stat);
  return file_stat.st_size;
#elif defined(__linux) || defined(__unix)
  struct stat file_stat;
  stat(filename.c_str(), &file_stat);
  return file_stat.st_size;
#endif
}

static
std::string FindTheSlowestOne(const cnstream::PipelineProfile& profile) {
  std::string slowest_module_name = "";
  double minimum_fps = std::numeric_limits<double>::max();
  for (const auto& module_profile : profile.module_profiles) {
    for (const auto& process_profile : module_profile.process_profiles) {
      if (process_profile.process_name == cnstream::kPROCESS_PROFILER_NAME) {
        if (minimum_fps > process_profile.fps) {
          minimum_fps = process_profile.fps;
          slowest_module_name = module_profile.module_name;
        }
      }
    }
  }
  return slowest_module_name;
}

static
std::string FillStr(std::string str, uint32_t length, char charactor) {
  int filled_length = (length - str.length()) / 2;
  filled_length = filled_length > 0 ? filled_length : 0;
  int remainder = 0;
  if (filled_length && (length - str.length()) % 2) remainder = 1;
  return std::string(filled_length + remainder, charactor) + str + std::string(filled_length, charactor);
}

static
void PrintProcessPerformance(std::ostream& os, const cnstream::ProcessProfile& profile) {
  if (FLAGS_perf_level <= 1) {
    if (FLAGS_perf_level == 1) {
      os << "[Latency]: (Avg): " << profile.latency << "ms";
      os << ", (Min): " << profile.minimum_latency << "ms";
      os << ", (Max): " << profile.maximum_latency << "ms" << std::endl;
    }
    os << "[Counter]: " << profile.counter;
    os << ", [Throughput]: " << profile.fps << "fps" << std::endl;
  } else if (FLAGS_perf_level >=2) {
    os << "[Counter]: " << profile.counter;
    os << ", [Completed]: " << profile.completed;
    os << ", [Dropped]: " << profile.dropped;
    os << ", [Ongoing]: " << profile.ongoing << std::endl;
    os << "[Latency]: (Avg): " << profile.latency << "ms";
    os << ", (Min): " << profile.minimum_latency << "ms";
    os << ", (Max): " << profile.maximum_latency << "ms" << std::endl;
    os << "[Throughput]: " << profile.fps << "fps" << std::endl;
  }

  if (FLAGS_perf_level >= 3) {
    uint32_t stream_name_max_length = 15;
    if (profile.stream_profiles.size()) {
      os << "\n------ Stream ------\n";
    }
    for (const auto& stream_profile : profile.stream_profiles) {
      std::string stream_name = "[" + stream_profile.stream_name + "]";
      os << stream_name << std::string(stream_name_max_length - stream_name.length(), ' ');
      os << "[Counter]: " << stream_profile.counter;
      os << ", [Completed]: " << stream_profile.completed;
      os << ", [Dropped]: " << stream_profile.dropped << std::endl;
      os << std::string(stream_name_max_length, ' ');
      os << "[Latency]: (Avg): " << stream_profile.latency << "ms";
      os << ", (Min): " << stream_profile.minimum_latency << "ms";
      os << ", (Max): " << stream_profile.maximum_latency << "ms" << std::endl;
      os << std::string(stream_name_max_length, ' ');
      os << "[Throughput]: " << stream_profile.fps << "fps" << std::endl;
    }
  }
}

void PrintPipelinePerformance(const std::string& prefix_str, const cnstream::PipelineProfile& profile) {
  auto slowest_module_name = FindTheSlowestOne(profile);
  std::stringstream ss;
  int length = 80;
  ss << "\033[1m\033[36m" << FillStr("  Performance Print Start  (" + prefix_str + ")  ", length, '*') << "\033[0m\n";
  ss << "\033[1m" << FillStr("  Pipeline: [" + profile.pipeline_name + "]  ", length, '=') << "\033[0m\n";

  for (const auto& module_profile : profile.module_profiles) {
    ss << "\033[1m\033[32m" << FillStr(" Module: [" + module_profile.module_name + "] ", length, '-');
    if (slowest_module_name == module_profile.module_name) {
      ss << "\033[0m\033[41m" << " (slowest) ";
    }
    ss << "\033[0m\n";

    for (const auto& process_profile : module_profile.process_profiles) {
      ss << "\033[1m\033[33m" << std::string(length / 8, '-');
      ss << "Process Name: [" << process_profile.process_name << "\033[0m" << "]\n";
      PrintProcessPerformance(ss, process_profile);
    }
  }
  ss << "\n\033[1m\033[32m" << FillStr("  Overall  ", length, '-') << "\033[0m\n";
  PrintProcessPerformance(ss, profile.overall_profile);
  ss << "\033[1m\033[36m" << FillStr("  Performance Print End  (" + prefix_str + ")  ", length, '*') << "\033[0m\n";
  std::cout << ss.str() << std::endl;
}

