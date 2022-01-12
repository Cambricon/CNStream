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

#include "reflex_object.h"

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "cnstream_logging.hpp"

namespace cnstream {

static
std::map<std::string, ClassInfo<ReflexObject>>& GlobalObjMap() {
  static std::map<std::string, ClassInfo<ReflexObject>> sg_obj_map;
  return sg_obj_map;
}

ReflexObject* ReflexObject::CreateObject(const std::string& name) {
  const auto& obj_map = GlobalObjMap();
  auto info_iter = obj_map.find(name);

  if (obj_map.end() == info_iter) return nullptr;

  const auto& info = info_iter->second;
  return info.CreateObject();
}

bool ReflexObject::Register(const ClassInfo<ReflexObject>& info) {
  auto& obj_map = GlobalObjMap();
  if (obj_map.find(info.name()) != obj_map.end()) {
    LOGI(REFLEX_OBJECT) << "Register object named [" << info.name() << "] failed!!!"
              << "Object name has been registered.";
    return false;
  }

  obj_map.insert(std::pair<std::string, ClassInfo<ReflexObject>>(info.name(), info));

  LOGI(REFLEX_OBJECT) << "Register object named [" << info.name() << "]";
  return true;
}

ReflexObject::~ReflexObject() {}

#ifdef UNIT_TEST
void ReflexObject::Remove(const std::string& name) {
  auto& obj_map = GlobalObjMap();
  auto info_iter = obj_map.find(name);

  if (obj_map.end() != info_iter) {
    obj_map.erase(name);
  }
}
#endif

}  // namespace cnstream
