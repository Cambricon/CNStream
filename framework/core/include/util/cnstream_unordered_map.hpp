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
#ifndef CNSTREAM_ThreadSafeUnorderedMap_H_
#define CNSTREAM_ThreadSafeUnorderedMap_H_

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include "cnstream_rwlock.hpp"

namespace cnstream {
template <typename _Key, typename _Tp, typename _Hash = std::hash<_Key>, typename _Pred = std::equal_to<_Key>,
          typename _Alloc = std::allocator<std::pair<const _Key, _Tp> > >
class ThreadSafeUnorderedMap {
 private:
  std::unordered_map<_Key, _Tp, _Hash, _Pred, _Alloc> map;
  mutable RwLock lock;

 public:
  using map_type = std::unordered_map<_Key, _Tp, _Hash, _Pred, _Alloc>;
  using key_type = typename map_type::key_type;
  using mapped_type = typename map_type::mapped_type;
  using value_type = typename map_type::value_type;
  using hasher = typename map_type::hasher;
  using key_equal = typename map_type::key_equal;
  using allocator_type = typename map_type::allocator_type;
  using reference = typename map_type::reference;
  using const_reference = typename map_type::const_reference;
  using pointer = typename map_type::pointer;
  using const_pointer = typename map_type::const_pointer;
  using iterator = typename map_type::iterator;
  using const_iterator = typename map_type::const_iterator;
  using local_iterator = typename map_type::local_iterator;
  using const_local_iterator = typename map_type::const_local_iterator;
  using size_type = typename map_type::size_type;
  using difference_type = typename map_type::difference_type;

  ThreadSafeUnorderedMap() = default;
  ThreadSafeUnorderedMap(const ThreadSafeUnorderedMap&) = delete;
  ThreadSafeUnorderedMap(ThreadSafeUnorderedMap&&) = default;
  ThreadSafeUnorderedMap& operator=(const ThreadSafeUnorderedMap&) = delete;
  ThreadSafeUnorderedMap& operator=(ThreadSafeUnorderedMap&&) = delete;
  explicit ThreadSafeUnorderedMap(size_type __n, const hasher& __hf = hasher(), const key_equal& __eql = key_equal(),
                                  const allocator_type& __a = allocator_type())
      : map(__n, __hf, __eql, __a) {}
  template <typename _InputIterator>
  ThreadSafeUnorderedMap(_InputIterator __first, _InputIterator __last, size_type __n = 0,
                         const hasher& __hf = hasher(), const key_equal& __eql = key_equal(),
                         const allocator_type& __a = allocator_type())
      : map(__first, __last, __n, __hf, __eql, __a) {}
  explicit ThreadSafeUnorderedMap(const map_type& v) : map(v) {}
  explicit ThreadSafeUnorderedMap(map_type&& rv) : map(std::move(rv)) {}
  explicit ThreadSafeUnorderedMap(const allocator_type& __a) : map(__a) {}
  ThreadSafeUnorderedMap(const map_type& __umap, const allocator_type& __a) : map(__umap, __a) {}
  ThreadSafeUnorderedMap(map_type&& __umap, const allocator_type& __a) : map(std::move(__umap), __a) {}
  ThreadSafeUnorderedMap(std::initializer_list<value_type> __l, size_type __n = 0, const hasher& __hf = hasher(),
                         const key_equal& __eql = key_equal(), const allocator_type& __a = allocator_type())
      : map(__l, __n, __hf, __eql, __a) {}
  ThreadSafeUnorderedMap(size_type __n, const allocator_type& __a)
      : ThreadSafeUnorderedMap(__n, hasher(), key_equal(), __a) {}
  ThreadSafeUnorderedMap(size_type __n, const hasher& __hf, const allocator_type& __a)
      : ThreadSafeUnorderedMap(__n, __hf, key_equal(), __a) {}
  template <typename _InputIterator>
  ThreadSafeUnorderedMap(_InputIterator __first, _InputIterator __last, size_type __n, const allocator_type& __a)
      : map(__first, __last, __n, __a) {}
  template <typename _InputIterator>
  ThreadSafeUnorderedMap(_InputIterator __first, _InputIterator __last, size_type __n, const hasher& __hf,
                         const allocator_type& __a)
      : ThreadSafeUnorderedMap(__first, __last, __n, __hf, key_equal(), __a) {}
  ThreadSafeUnorderedMap(std::initializer_list<value_type> __l, size_type __n, const allocator_type& __a)
      : ThreadSafeUnorderedMap(__l, __n, hasher(), key_equal(), __a) {}
  ThreadSafeUnorderedMap(std::initializer_list<value_type> __l, size_type __n, const hasher& __hf,
                         const allocator_type& __a)
      : ThreadSafeUnorderedMap(__l, __n, __hf, key_equal(), __a) {}
  bool empty() const noexcept {
    RwLockReadGuard guard(lock);
    return map.empty();
  }
  size_type size() const noexcept {
    RwLockReadGuard guard(lock);
    return map.size();
  }
  size_type max_size() const noexcept {
    RwLockReadGuard guard(lock);
    return map.max_size();
  }
  iterator begin() noexcept {
    RwLockWriteGuard guard(lock);
    return map.begin();
  }
  const_iterator begin() const noexcept {
    RwLockReadGuard guard(lock);
    return map.begin();
  }
  const_iterator cbegin() const noexcept {
    RwLockReadGuard guard(lock);
    return map.cbegin();
  }
  iterator end() noexcept {
    RwLockWriteGuard guard(lock);
    return map.end();
  }
  const_iterator end() const noexcept {
    RwLockReadGuard guard(lock);
    return map.end();
  }
  const_iterator cend() const noexcept {
    RwLockReadGuard guard(lock);
    return map.cend();
  }
  template <typename... _Args>
  std::pair<iterator, bool> emplace(_Args&&... __args) {
    RwLockWriteGuard guard(lock);
    return map.emplace(std::forward<_Args>(__args)...);
  }
  template <typename... _Args>
  iterator emplace_hint(const_iterator __pos, _Args&&... __args) {
    RwLockWriteGuard guard(lock);
    return map.emplace_hint(__pos, std::forward<_Args>(__args)...);
  }
  std::pair<iterator, bool> insert(const value_type& __x) {
    RwLockWriteGuard guard(lock);
    return map.insert(__x);
  }
  template <typename _Pair, typename = typename std::enable_if<std::is_constructible<value_type, _Pair&&>::value>::type>
  std::pair<iterator, bool> insert(_Pair&& __x) {
    RwLockWriteGuard guard(lock);
    return map.insert(std::forward<_Pair>(__x));
  }
  iterator insert(const_iterator __hint, const value_type& __x) {
    RwLockWriteGuard guard(lock);
    return map.insert(__hint, __x);
  }
  template <typename _Pair, typename = typename std::enable_if<std::is_constructible<value_type, _Pair&&>::value>::type>
  iterator insert(const_iterator __hint, _Pair&& __x) {
    RwLockWriteGuard guard(lock);
    return map.insert(__hint, std::forward<_Pair>(__x));
  }
  template <typename _InputIterator>
  void insert(_InputIterator __first, _InputIterator __last) {
    RwLockWriteGuard guard(lock);
    map.insert(__first, __last);
  }
  void insert(std::initializer_list<value_type> __l) {
    RwLockWriteGuard guard(lock);
    map.insert(__l);
  }
  iterator erase(const_iterator __position) {
    RwLockWriteGuard guard(lock);
    return map.erase(__position);
  }
  iterator erase(iterator __position) {
    RwLockWriteGuard guard(lock);
    return map.erase(__position);
  }
  size_type erase(const key_type& __x) {
    RwLockWriteGuard guard(lock);
    return map.erase(__x);
  }
  iterator erase(const_iterator __first, const_iterator __last) {
    RwLockWriteGuard guard(lock);
    return map.erase(__first, __last);
  }
  void clear() noexcept {
    RwLockWriteGuard guard(lock);
    map.clear();
  }
  void swap(map_type& __x) noexcept(noexcept(map.swap(__x._M_h))) {
    RwLockWriteGuard guard(lock);
    map.swap(__x._M_h);
  }
  hasher hash_function() const {
    RwLockReadGuard guard(lock);
    return map.hash_function();
  }
  key_equal key_eq() const {
    RwLockReadGuard guard(lock);
    return map.key_eq();
  }
  iterator find(const key_type& __x) {
    RwLockWriteGuard guard(lock);
    return map.find(__x);
  }
  const_iterator find(const key_type& __x) const {
    RwLockReadGuard guard(lock);
    return map.find(__x);
  }
  size_type count(const key_type& __x) const {
    RwLockReadGuard guard(lock);
    return map.count(__x);
  }
  std::pair<iterator, iterator> equal_range(const key_type& __x) {
    RwLockWriteGuard guard(lock);
    return map.equal_range(__x);
  }
  std::pair<const_iterator, const_iterator> equal_range(const key_type& __x) const {
    RwLockWriteGuard guard(lock);
    return map.equal_range(__x);
  }
  mapped_type& operator[](const key_type& __k) {
    RwLockWriteGuard guard(lock);
    return map[__k];
  }
  mapped_type& operator[](key_type&& __k) {
    RwLockWriteGuard guard(lock);
    return map[std::move(__k)];
  }
  mapped_type& at(const key_type& __k) {
    RwLockWriteGuard guard(lock);
    return map.at(__k);
  }
  const mapped_type& at(const key_type& __k) const {
    RwLockReadGuard guard(lock);
    return map.at(__k);
  }
  size_type bucket_count() const noexcept {
    RwLockReadGuard guard(lock);
    return map.bucket_count();
  }

  size_type max_bucket_count() const noexcept {
    RwLockReadGuard guard(lock);
    return map.max_bucket_count();
  }
  size_type bucket_size(size_type __n) const {
    RwLockReadGuard guard(lock);
    return map.bucket_size(__n);
  }
  size_type bucket(const key_type& __key) const {
    RwLockReadGuard guard(lock);
    return map.bucket(__key);
  }
  local_iterator begin(size_type __n) {
    RwLockWriteGuard guard(lock);
    return map.begin(__n);
  }
  const_local_iterator begin(size_type __n) const {
    RwLockReadGuard guard(lock);
    return map.begin(__n);
  }
  const_local_iterator cbegin(size_type __n) const {
    RwLockReadGuard guard(lock);
    return map.cbegin(__n);
  }
  local_iterator end(size_type __n) {
    RwLockWriteGuard guard(lock);
    return map.end(__n);
  }
  const_local_iterator end(size_type __n) const {
    RwLockReadGuard guard(lock);
    return map.end(__n);
  }
  const_local_iterator cend(size_type __n) const {
    RwLockReadGuard guard(lock);
    return map.cend(__n);
  }
  float load_factor() const noexcept {
    RwLockReadGuard guard(lock);
    return map.load_factor();
  }
  float max_load_factor() const noexcept {
    RwLockReadGuard guard(lock);
    return map.max_load_factor();
  }
  void max_load_factor(float __z) {
    RwLockWriteGuard guard(lock);
    map.max_load_factor(__z);
  }
  void rehash(size_type __n) {
    RwLockWriteGuard guard(lock);
    map.rehash(__n);
  }
  void reserve(size_type __n) {
    RwLockWriteGuard guard(lock);
    map.reserve(__n);
  }

  template <typename _Key1, typename _Tp1, typename _Hash1, typename _Pred1, typename _Alloc1>
  friend bool operator==(const ThreadSafeUnorderedMap<_Key1, _Tp1, _Hash1, _Pred1, _Alloc1>&,
                         const ThreadSafeUnorderedMap<_Key1, _Tp1, _Hash1, _Pred1, _Alloc1>&);
};
template <class _Key, class _Tp, class _Hash, class _Pred, class _Alloc>
inline bool operator==(const ThreadSafeUnorderedMap<_Key, _Tp, _Hash, _Pred, _Alloc>& __x,
                       const ThreadSafeUnorderedMap<_Key, _Tp, _Hash, _Pred, _Alloc>& __y) {
  RwLockReadGuard guardx(__x.lock);
  RwLockReadGuard guardy(__y.lock);
  return __x.map._M_equal(__y.map);
}
template <class _Key, class _Tp, class _Hash, class _Pred, class _Alloc>
inline bool operator!=(const ThreadSafeUnorderedMap<_Key, _Tp, _Hash, _Pred, _Alloc>& __x,
                       const ThreadSafeUnorderedMap<_Key, _Tp, _Hash, _Pred, _Alloc>& __y) {
  RwLockReadGuard guardx(__x.lock);
  RwLockReadGuard guardy(__y.lock);
  return !(__x == __y);
}
} /* namespace cnstream */

#endif /* CNSTREAM_ThreadSafeUnorderedMap_H_ */
