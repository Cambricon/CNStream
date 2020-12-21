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
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <cerrno>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"

std::string GetExePath() {
  char path[PATH_MAX_LENGTH];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_LENGTH);
  if (cnt < 0 || cnt >= PATH_MAX_LENGTH) {
    return NULL;
  }
  for (int i = cnt; i >= 0; --i) {
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
