/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include "cnstream_collection.hpp"

#include <memory>
#include <string>
#include <utility>

namespace cnstream {

void Collection::Add(const std::string& tag, std::unique_ptr<cnstream::any>&& value) {
  std::lock_guard<std::mutex> lk(data_mtx_);
  if (data_.end() != data_.find(tag)) {
#if !defined(_LIBCPP_NO_RTTI)
    LOGF(COLLECTION) << "Data tagged by [" << tag << "] had been added, "
                        "and value type is [" << data_[tag]->type().name()
                     << "]. Current type is [" << value->type().name() << "].";
#else
    LOGF(COLLECTION) << "Data tagged by [" << tag << "] had been added.";
#endif
  }
  data_[tag] = std::forward<std::unique_ptr<cnstream::any>>(value);
}

bool Collection::AddIfNotExists(const std::string& tag, std::unique_ptr<cnstream::any>&& value) {
  std::lock_guard<std::mutex> lk(data_mtx_);
  if (data_.end() != data_.find(tag)) {
    LOGD(COLLECTION) << "Data tagged by [" << tag << "] had been added. Current data will not be added.";
    return false;
  }
  data_[tag] = std::forward<std::unique_ptr<cnstream::any>>(value);
  return true;
}

bool Collection::HasValue(const std::string& tag) {
  std::lock_guard<std::mutex> lk(data_mtx_);
  return data_.end() != data_.find(tag);
}

#if !defined(_LIBCPP_NO_RTTI)
const std::type_info& Collection::Type(const std::string& tag) {
  std::lock_guard<std::mutex> lk(data_mtx_);
  auto iter = data_.find(tag);
  if (data_.end() == iter) {
    LOGF(COLLECTION) << "No data tagged by [" << tag << "] was been added.";
  }
  return iter->second->type();
}
#endif

}  // namespace cnstream
