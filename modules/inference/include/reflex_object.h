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

#ifndef MODULES_INFERENCE_INCLUDE_REFLEX_OBJECT_H_
#define MODULES_INFERENCE_INCLUDE_REFLEX_OBJECT_H_

#include <functional>
#include <map>
#include <memory>
#include <string>

#define DECLARE_REFLEX_OBJECT(Class)                              \
 public:                                                          \
  static cnstream::ClassInfo<cnstream::ReflexObject> sclass_info; \
                                                                  \
 protected:                                                       \
  const cnstream::ClassInfo<cnstream::ReflexObject>& class_info() const;

#define IMPLEMENT_REFLEX_OBJECT(Class)                                                                                \
  cnstream::ClassInfo<ReflexObject> Class::sclass_info(std::string(#Class),                                           \
                                                       cnstream::ObjectConstructor<cnstream::ReflexObject>([]() {     \
                                                         return reinterpret_cast<cnstream::ReflexObject*>(new Class); \
                                                       }),                                                            \
                                                       true);                                                         \
  const cnstream::ClassInfo<cnstream::ReflexObject>& Class::class_info() const { return sclass_info; }

#define DECLARE_REFLEX_OBJECT_EX(Class, BaseType)   \
 public:                                            \
  static cnstream::ClassInfo<BaseType> sclass_info; \
                                                    \
 protected:                                         \
  const cnstream::ClassInfo<BaseType>& class_info() const;

#define IMPLEMENT_REFLEX_OBJECT_EX(Class, BaseType)                                                          \
  cnstream::ClassInfo<BaseType> Class::sclass_info(                                                          \
      std::string(#Class),                                                                                   \
      cnstream::ObjectConstructor<BaseType>([]() { return reinterpret_cast<BaseType*>(new Class); }), true); \
  const cnstream::ClassInfo<BaseType>& Class::class_info() const { return sclass_info; }

namespace cnstream {

/*****************************************
 * [T]: The return type for reflection object.
 *****************************************/

template <typename T>
using ObjectConstructor = std::function<T*()>;

template <typename T>
class ClassInfo {
 public:
  ClassInfo(const std::string& name, const ObjectConstructor<T>& constructor, bool regist = false);

  T* CreateObject() const;

  std::string name() const;

  bool Register() const;

  const ObjectConstructor<T>& constructor() const;

 private:
  std::string name_;
  ObjectConstructor<T> constructor_;
};  // class classinfo

class ReflexObject {
 public:
  static ReflexObject* CreateObject(const std::string& name);

  static bool Register(const ClassInfo<ReflexObject>& info);

  virtual ~ReflexObject() = 0;
#ifdef UNIT_TEST
  static void Remove(const std::string& name);
#endif
};  // class reflexobject<void>

template <typename T>
class ReflexObjectEx : public ReflexObject {
 public:
  static T* CreateObject(const std::string& name);

  static bool Register(const ClassInfo<T>& info);

  virtual ~ReflexObjectEx() = 0;
};  // class reflectobject

template <typename T>
ClassInfo<T>::ClassInfo(const std::string& name, const ObjectConstructor<T>& constructor, bool regist)
    : name_(name), constructor_(constructor) {
  if (regist) {
    Register();
  }
}

template <typename T>
inline std::string ClassInfo<T>::name() const {
  return name_;
}

template <typename T>
inline const ObjectConstructor<T>& ClassInfo<T>::constructor() const {
  return constructor_;
}

template <typename T>
inline bool ClassInfo<T>::Register() const {
  return ReflexObjectEx<T>::Register(*this);
}

template <typename T>
T* ClassInfo<T>::CreateObject() const {
  if (NULL != constructor_) {
    return constructor_();
  }
  return nullptr;
}

template <typename T>
T* ReflexObjectEx<T>::CreateObject(const std::string& name) {
  auto ptr = ReflexObject::CreateObject(name);
  if (nullptr == ptr) return nullptr;
  T* ret = dynamic_cast<T*>(ptr);
  if (nullptr == ret) {
    delete ptr;
    return nullptr;
  }
  return ret;
}

template <typename T>
bool ReflexObjectEx<T>::Register(const ClassInfo<T>& info) {
  // build base ClassInfo(ClassInfo<ReflexObjectEx>)
  ObjectConstructor<ReflexObject> base_constructor = NULL;
  if (info.constructor() != NULL) {
    base_constructor = [info]() { return static_cast<ReflexObject*>(info.constructor()()); };
  }
  ClassInfo<ReflexObject> base_info(info.name(), base_constructor);

  return ReflexObject::Register(base_info);
}

template <typename T>
ReflexObjectEx<T>::~ReflexObjectEx() {}

}  // namespace cnstream

#endif  // MODULES_INFERENCE_INCLUDE_REFLEX_OBJECT_HPP_
