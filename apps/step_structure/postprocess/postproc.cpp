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

#include "postproc.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::cerr;
using std::endl;
using std::pair;
using std::to_string;
using std::vector;

namespace cnstream {

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

Postproc* Postproc::Create(const std::string& proc_name) { return ReflexObjectEx<Postproc>::CreateObject(proc_name); }

void Postproc::set_threshold(const float threshold) { threshold_ = threshold; }

bool Postproc::LoadLabels(const std::string& label_path) {
  std::ifstream ifs(label_path);
  if (!ifs) {
    return false;
  }
  while (!ifs.eof()) {
    std::string label;
    std::getline(ifs, label);
    labels_.push_back(label);
  }
  ifs.close();
  return !labels_.empty();
}

bool Postproc::LoadMultiLabels(const std::vector<std::string>& label_path) {
  for (auto path : label_path) {
    try {
      path.erase(path.find_first_of(' '), path.find_first_not_of(' '));
      path.erase(path.find_last_not_of(' ') + 1);
    } catch (...) {
    }
    std::ifstream ifs(path);
    std::vector<std::string> labels;
    if (!ifs) {
      return false;
    }
    while (!ifs.eof()) {
      std::string label_item;
      std::getline(ifs, label_item);
      labels.push_back(label_item);
    }
    ifs.close();
    multi_labels_.push_back(labels);
  }
  return !multi_labels_.empty();
}
}  // namespace cnstream
