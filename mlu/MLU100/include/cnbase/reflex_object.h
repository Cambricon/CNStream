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
/*****************************************************************************
 * @file   reflex_object.hpp
 * @brief  reflex mechanism
 * @author liumingxuan
 * @email  liumingxuan@cambricon.com
 * @date   2019/05/08
******************************************************************************/
#ifndef _REFLEX_OBJECT_H_
#define _REFLEX_OBJECT_H_

#include <functional>
#include <map>
#include <memory>
#include <string>

#define DECLARE_REFLEX_OBJECT(Class)                                       \
 public:                                                                   \
  static libstream::ClassInfo<libstream::ReflexObject> sclass_info;        \
 protected:                                                                \
  const libstream::ClassInfo<libstream::ReflexObject>& class_info() const;

#define IMPLEMENT_REFLEX_OBJECT(Class)                                           \
libstream::ClassInfo<ReflexObject> Class::sclass_info(std::string(#Class),       \
    libstream::ObjectConstructor<libstream::ReflexObject>(                       \
      [] () {                                                                    \
        return reinterpret_cast<libstream::ReflexObject*>(new Class);            \
      }), true);                                                                 \
const libstream::ClassInfo<libstream::ReflexObject>& Class::class_info() const { \
  return sclass_info;                                                            \
}

#define DECLARE_REFLEX_OBJECT_EX(Class, BaseType)                          \
 public:                                                                   \
  static libstream::ClassInfo< BaseType > sclass_info_##BaseType;          \
 protected:                                                                \
  const libstream::ClassInfo< BaseType >& class_info_##BaseType() const;

#define IMPLEMENT_REFLEX_OBJECT_EX(Class, BaseType)                                 \
libstream::ClassInfo< BaseType > Class::sclass_info_##BaseType(std::string(#Class), \
    libstream::ObjectConstructor< BaseType >(                                       \
      [] () {                                                                       \
        return reinterpret_cast< BaseType* >(new Class);                            \
      }), true);                                                                    \
const libstream::ClassInfo< BaseType >& Class::class_info_##BaseType() const {      \
  return sclass_info_##BaseType;                                                    \
}

namespace libstream {

/*****************************************
 * [T]: The return type for reflection object.
 *****************************************/

template<typename T>
using ObjectConstructor = std::function<T*()>;

template<typename T>
class ClassInfo {
 public:
  ClassInfo(const std::string& name, const ObjectConstructor<T>& constructor,
      bool regist = false);

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
};  // class reflexobject<void>

template<typename T>
class ReflexObjectEx : public ReflexObject {
 public:
  static T* CreateObject(const std::string& name);

  static bool Register(const ClassInfo<T>& info);

  virtual ~ReflexObjectEx() = 0;
};  // class reflectobject

template<typename T>
ClassInfo<T>::ClassInfo(const std::string& name,
    const ObjectConstructor<T>& constructor, bool regist)
    : name_(name), constructor_(constructor) {
  if (regist) {
    Register();
  }
}

template<typename T>
inline std::string ClassInfo<T>::name() const {
  return name_;
}

template<typename T>
inline const ObjectConstructor<T>& ClassInfo<T>::constructor() const {
  return constructor_;
}

template<typename T>
inline bool ClassInfo<T>::Register() const {
  return ReflexObjectEx<T>::Register(*this);
}

template<typename T>
T* ClassInfo<T>::CreateObject() const {
  if (NULL != constructor_) {
    return constructor_();
  }
  return nullptr;
}

template<typename T>
T* ReflexObjectEx<T>::CreateObject(const std::string& name) {
  return reinterpret_cast<T*>(ReflexObject::CreateObject(name));
}

template<typename T>
bool ReflexObjectEx<T>::Register(const ClassInfo<T>& info) {
  // build base ClassInfo(ClassInfo<ReflexObjectEx>)
  ObjectConstructor<ReflexObject> base_constructor = NULL;
  if (info.constructor() != NULL) {
    base_constructor = [info] () {
      return reinterpret_cast<ReflexObject*>(info.constructor()());
    };
  }
  ClassInfo<ReflexObject> base_info(info.name(), base_constructor);

  return ReflexObject::Register(base_info);
}

template<typename T>
ReflexObjectEx<T>::~ReflexObjectEx() {}

}  // namespace libstream

#endif  // _REFLEX_OBJECT_HPP_

