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

#include "test_base.hpp"

#include <unistd.h>
#include <string.h>

#include <cerrno>
#include <string>
#include <utility>

#include "cnstream_logging.hpp"

extern int errno;

std::string GetExePath() {
  char path[PATH_MAX_LENGTH];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_LENGTH);
  if (cnt < 0 || cnt >= PATH_MAX_LENGTH) {
    return "";
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

void CheckExePath(const std::string& path) {
  if (path.size() == 0) {
    LOGF_IF(COREUNITEST, 0 != errno) << std::string(strerror(errno));
    LOGF(COREUNITEST) << "length of exe path is larger than " << PATH_MAX_LENGTH;
  }
}

std::pair<int, std::string> CreateTempFile(const std::string& filename_prefix) {
  char filename[1024];
  if (filename_prefix.size() > 1024 - 7) {
    LOGF(COREUNITEST) << "filename_prefix is too long, must be less than " << 1024 - 7 << std::endl;
  }
  strncpy(filename, filename_prefix.c_str(), filename_prefix.size());
  strncpy(filename + filename_prefix.size(), "XXXXXX", 6);
  filename[filename_prefix.size() + 6] = '\0';
  int fd = mkstemp(filename);
  LOGF_IF(COREUNITEST, -1 == fd) << "Create temporary file for BuildPipelineByJSONFile test case failed! "
      << strerror(errno);
  return std::make_pair(fd, std::string(filename));
}
