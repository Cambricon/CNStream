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

#ifndef CNSTREAM_COLLECTION_HPP_
#define CNSTREAM_COLLECTION_HPP_

#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <utility>

#include "cnstream_common.hpp"
#include "cnstream_logging.hpp"
#include "util/cnstream_any.hpp"

namespace cnstream {

/**
 * @class Collection
 *
 * @brief Collection is a class storing structured data of variable types.
 *
 * @note This class is thread safe.
 */
class Collection : public NonCopyable {
 public:
  /*!
   * @brief Constructs an instance with empty value.
   *
   * @return  No return value.
   */
  Collection() = default;
  /*!
   * @brief Destructs an instance.
   *
   * @return  No return value.
   */
  ~Collection() = default;
  /**
   * @brief Gets the reference to the object of typename ValueT if it exists, otherwise crashes.
   *
   * @param[in] tag The unique identifier of the data.
   *
   * @return Returns the reference to the object of typename ValueT which is tagged by `tag`.
   */
  template <typename ValueT>
  ValueT& Get(const std::string& tag);
  /**
   * @brief Adds data tagged by `tag`. Crashes when there is already a piece of data tagged by `tag`.
   *
   * @param[in] tag The unique identifier of the data.
   * @param[in] value Value to be add.
   *
   * @return Returns the reference to the object of typename ValueT which is tagged by `tag`.
   */
  template <typename ValueT>
  ValueT& Add(const std::string& tag, const ValueT& value);
  /**
   * @brief Adds data tagged by `tag` using move semantics. Crashes when there is already a piece of data tagged by
   * `tag`.
   *
   * @param[in] tag The unique identifier of the data.
   * @param[in] value Value to be add.
   *
   * @return Returns the reference to the object of typename ValueT which is tagged by `tag`.
   */
  template <typename ValueT>
  ValueT& Add(const std::string& tag, ValueT&& value);

  /**
   * @brief Adds data tagged by `tag`, only if there is no piece of data tagged by `tag`.
   *
   * @param[in] tag The unique identifier of the data.
   * @param[in] value Value to be add.
   *
   * @return Returns true if the data is added successfully, otherwise returns false.
   */
  template <typename ValueT>
  bool AddIfNotExists(const std::string& tag, const ValueT& value);
  /**
   * @brief Adds data tagged by `tag` using move semantics, only if there is no piece of data tagged by
   * `tag`.
   *
   * @param[in] tag The unique identifier of the data.
   * @param[in] value Value to be add.
   *
   * @return Returns true if the data is added successfully, otherwise returns false.
   */
  template <typename ValueT>
  bool AddIfNotExists(const std::string& tag, ValueT&& value);

  /**
   * @brief Checks whether there is the data tagged by `tag`.
   *
   * @param[in] tag The unique identifier of the data.
   *
   * @return Returns true if there is already a piece of data tagged by `tag`, otherwise returns false.
   */
  bool HasValue(const std::string& tag);

#if !defined(_LIBCPP_NO_RTTI)
  /**
   * @brief Gets type information for data tagged by `tag`.
   *
   * @param[in] tag The unique identifier of the data.
   *
   * @return Returns type information of the data tagged by `tag`.
   */
  const std::type_info& Type(const std::string& tag);

  /**
   * @brief Checks if the type of data tagged by `tag` is `ValueT` or not.
   *
   * @param tag The unique identifier of the data.
   *
   * @return Returns true if the type of data tagged by `tag` is ``ValueT``, otherwise returns false.
   */
  template <typename ValueT>
  bool TaggedIsOfType(const std::string& tag);
#endif

 private:
  void Add(const std::string& tag, std::unique_ptr<cnstream::any>&& value);
  bool AddIfNotExists(const std::string& tag, std::unique_ptr<cnstream::any>&& value);

 private:
  std::map<std::string, std::unique_ptr<cnstream::any>> data_;
  std::mutex data_mtx_;
};  // class Collection

template <typename ValueT>
ValueT& Collection::Get(const std::string& tag) {
  std::lock_guard<std::mutex> lk(data_mtx_);
  auto iter = data_.find(tag);
  if (data_.end() == iter) {
    LOGF(COLLECTION) << "No data tagged by [" << tag << "] has been added.";
  }
  try {
    return any_cast<ValueT&>(*iter->second);
  } catch (bad_any_cast& e) {
#if !defined(_LIBCPP_NO_RTTI)
    LOGF(COLLECTION) << "The type of data tagged by [" << tag << "]  is ["
                     << iter->second->type().name()
                     << "]. Expect type is [" << typeid(ValueT).name() << "].";
#else
    LOGF(COLLECTION) << "The type of data tagged by [" << tag << "] is not the "
                        "expected data type."
#endif
  }

  // never be here.
  return any_cast<ValueT&>(*iter->second);
}

template <typename ValueT> inline
ValueT& Collection::Add(const std::string& tag, const ValueT& value) {
  Add(tag, std::unique_ptr<cnstream::any>(new cnstream::any(value)));
  return Get<ValueT>(tag);
}

template <typename ValueT> inline
ValueT& Collection::Add(const std::string& tag, ValueT&& value) {
  Add(tag, std::unique_ptr<cnstream::any>(new cnstream::any(std::forward<ValueT>(value))));
  return Get<ValueT>(tag);
}

template <typename ValueT> inline
bool Collection::AddIfNotExists(const std::string& tag, const ValueT& value) {
  return AddIfNotExists(tag, std::unique_ptr<cnstream::any>(new cnstream::any(value)));
}

template <typename ValueT> inline
bool Collection::AddIfNotExists(const std::string& tag, ValueT&& value) {
  return AddIfNotExists(tag, std::unique_ptr<cnstream::any>(new cnstream::any(std::forward<ValueT>(value))));
}

#if !defined(_LIBCPP_NO_RTTI)
template <typename ValueT> inline
bool Collection::TaggedIsOfType(const std::string& tag) {
  if (!HasValue(tag)) return false;
  return typeid(ValueT) == Type(tag);
}
#endif

}  // namespace cnstream

#endif  // CNSTREAM_COLLECTION_HPP_
