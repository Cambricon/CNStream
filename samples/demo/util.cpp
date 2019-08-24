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

#include "util.hpp"
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>
#include <list>
#include <string>
#include <vector>

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
    LOG_IF(FATAL, 0 != errno) << std::string(strerror(errno));
    LOG(FATAL) << "length of exe path is larger than " << PATH_MAX_LENGTH;
  }
}

std::list<std::string> ReadFileList(const std::string &list) {
  std::ifstream ifile;
  ifile.open(list);
  std::list<std::string> files;
  if (ifile) {
    std::string path;
    while (std::getline(ifile, path)) {
      std::string file = path;
      files.push_back(file);
      path.clear();
    }
  } else {
    LOG(ERROR) << "Open file: " << list.c_str() << " failed.";
    exit(0);
  }
  ifile.close();
  return files;
}

std::vector<std::string> LoadLabels(const std::string &filename) {
  std::vector<std::string> labels;
  std::ifstream file(filename);
  LOG_IF(FATAL, !file.is_open()) << "file:" << filename << " open failed.";
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
