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

//===------------------------------ any -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LINB_ANY_HPP
#define LINB_ANY_HPP

#include <memory>
#include <new>
#include <typeinfo>
#include <type_traits>
#include <iostream>
#include <cstdlib>
#include <utility>

template< class T >
using decay_t = typename std::decay<T>::type;

template< bool B, class T = void >
using enable_if_t = typename std::enable_if<B,T>::type;

template< class T >
using add_const_t = typename std::add_const<T>::type;

template< bool B, class T, class F >
using conditional_t = typename std::conditional<B,T,F>::type;

template< std::size_t Len, std::size_t Align >
using aligned_storage_t = typename std::aligned_storage<Len, Align>::type;

template< class T >
using add_pointer_t = typename std::add_pointer<T>::type;

template <class _Tp>
struct in_place_type_t {
  explicit in_place_type_t() = default;
};

class bad_any_cast : public std::bad_cast {
 public:
  const char* what() const noexcept override { return "bad any cast"; }
};


template <class _Alloc>
class __allocator_destructor {
  typedef std::allocator_traits<_Alloc> __alloc_traits;
 public:
  typedef typename __alloc_traits::pointer pointer;
  typedef typename __alloc_traits::size_type size_type;
 private:
  _Alloc& __alloc_;
  size_type __s_;
 public:
  __allocator_destructor(_Alloc& __a, size_type __s) : __alloc_(__a), __s_(__s) {}
  void operator()(pointer __p) {__alloc_traits::deallocate(__alloc_, __p, __s_);}
};


template <class _Tp>
struct __uncvref  {
  typedef typename std::remove_cv<typename std::remove_reference<_Tp>::type>::type type;
};

template <class _Tp>
using __uncvref_t = typename __uncvref<_Tp>::type;

template <class _Tp> struct __is_inplace_type_imp : std::false_type {};
// template <class _Tp> struct __is_inplace_type_imp<in_place_type_t<_Tp>> : true_type {};

template <class _Tp>
using __is_inplace_type = __is_inplace_type_imp<__uncvref_t<_Tp>>;

namespace cnstream {


namespace __any_imp {
  using _Buffer = aligned_storage_t<3*sizeof(void*), std::alignment_of<void*>::value>;

  template <class _Tp>
  using _IsSmallObject = std::integral_constant<bool,
                                          sizeof(_Tp) <= sizeof(_Buffer) &&
                                          std::alignment_of<_Buffer>::value % std::alignment_of<_Tp>::value == 0 &&
                                          std::is_nothrow_move_constructible<_Tp>::value>;

  enum class _Action {
    _Destroy,
    _Copy,
    _Move,
    _Get,
    _TypeInfo
  };

  template <class _Tp> struct _SmallHandler;
  template <class _Tp> struct _LargeHandler;

  template <class _Tp>
  struct  __unique_typeinfo { static constexpr int __id = 0; };
  template <class _Tp> constexpr int __unique_typeinfo<_Tp>::__id;

  template <class _Tp>
  constexpr const void* __get_fallback_typeid() {
      return &__unique_typeinfo<decay_t<_Tp>>::__id;
  }

  template <class _Tp>
  bool __compare_typeid(std::type_info const* __id, const void* __fallback_id) {
#if !defined(_LIBCPP_NO_RTTI)
    if (__id && *__id == typeid(_Tp))
      return true;
#endif
    if (!__id && __fallback_id == __any_imp::__get_fallback_typeid<_Tp>())
      return true;
    return false;
  }

  template <class _Tp>
  using _Handler = conditional_t<
    _IsSmallObject<_Tp>::value, _SmallHandler<_Tp>, _LargeHandler<_Tp>>;

} // namespace __any_imp

class any final {

 public:
  // construct/destruct
  constexpr any() : __h(nullptr) {}

  any(any const & __other) : __h(nullptr) {
    if (__other.__h) __other.__call(_Action::_Copy, this);
  }

  any(any && __other) : __h(nullptr) {
    if (__other.__h) __other.__call(_Action::_Move, this);
  }

  template <class _ValueType, class _Tp = decay_t<_ValueType>, 
            class = enable_if_t< !std::is_same<_Tp, any>::value && 
                                 !__is_inplace_type<_ValueType>::value && 
                                 std::is_copy_constructible<_Tp>::value>>
  any(_ValueType && __value);

  
  template <class _ValueType, class ..._Args,
            class _Tp = decay_t<_ValueType>,
            class = enable_if_t<std::is_constructible<_Tp, _Args...>::value &&
                                std::is_copy_constructible<_Tp>::value>>
  explicit any(in_place_type_t<_ValueType>, _Args&&... __args);

  template <class _ValueType, class _Up, class ..._Args,
    class _Tp = decay_t<_ValueType>,
    class = enable_if_t<
        std::is_constructible<_Tp, std::initializer_list<_Up>&, _Args...>::value &&
        std::is_copy_constructible<_Tp>::value>>
  explicit any(in_place_type_t<_ValueType>, std::initializer_list<_Up>, _Args&&... __args);

  ~any() { this->reset(); }

  // assignments
  any & operator=(any const & __rhs) {
    any(__rhs).swap(*this);
    return *this;
  }

  any & operator=(any && __rhs) {
    any(std::move(__rhs)).swap(*this);
    return *this;
  }

  template <class _ValueType, 
            class _Tp = decay_t<_ValueType>, 
            class = enable_if_t< !std::is_same<_Tp, any>::value && 
                                 std::is_copy_constructible<_Tp>::value>>
  any & operator=(_ValueType && __rhs);

  template <class _ValueType,
            class ..._Args,
            class _Tp = decay_t<_ValueType>,
            class = enable_if_t< std::is_constructible<_Tp, _Args...>::value &&
                                 std::is_copy_constructible<_Tp>::value>>
  _Tp& emplace(_Args&&... args);

  template <class _ValueType,
            class _Up,
            class ..._Args,
            class _Tp = decay_t<_ValueType>,
            class = enable_if_t<std::is_constructible<_Tp, std::initializer_list<_Up>&, _Args...>::value &&
                    std::is_copy_constructible<_Tp>::value>>
  _Tp& emplace(std::initializer_list<_Up>, _Args&&...);

  // 6.3.3 any modifiers
  void reset() { if (__h) this->__call(_Action::_Destroy); }

  void swap(any & __rhs) {
    if (this == &__rhs)
      return;
    if (__h && __rhs.__h) {
      any __tmp;
      __rhs.__call(_Action::_Move, &__tmp);
      this->__call(_Action::_Move, &__rhs);
      __tmp.__call(_Action::_Move, this);
    }
    else if (__h) {
      this->__call(_Action::_Move, &__rhs);
    }
    else if (__rhs.__h) {
      __rhs.__call(_Action::_Move, this);
    }
  }

  // 6.3.4 any observers
  bool has_value() const { return __h != nullptr; }

#if !defined(_LIBCPP_NO_RTTI)
  const std::type_info & type() const {
    if (__h) {
        return *static_cast<std::type_info const *>(this->__call(_Action::_TypeInfo));
    } else {
        return typeid(void);
    }
  }
#endif

 private:
  typedef __any_imp::_Action _Action;
  using _HandleFuncPtr =  void* (*)(_Action, any const *, any *, const std::type_info *,
    const void* __fallback_info);

  union _Storage {
    constexpr _Storage() : __ptr(nullptr) {}
    void*  __ptr;
    __any_imp::_Buffer __buf;
  };

  void* __call(_Action __a, any * __other = nullptr,
                std::type_info const * __info = nullptr,
                 const void* __fallback_info = nullptr) const {
    return __h(__a, this, __other, __info, __fallback_info);
  }

  void* __call(_Action __a, any * __other = nullptr,
                std::type_info const * __info = nullptr,
                const void* __fallback_info = nullptr) {
    return __h(__a, this, __other, __info, __fallback_info);
  }

  template <class>
  friend struct __any_imp::_SmallHandler;
  template <class>
  friend struct __any_imp::_LargeHandler;

  template <class _ValueType>
  friend add_pointer_t<add_const_t<_ValueType>> any_cast(any const *);

  template <class _ValueType>
  friend add_pointer_t<_ValueType> any_cast(any *);

  _HandleFuncPtr __h = nullptr;
  _Storage __s;
};

namespace __any_imp {
  template <class _Tp>
  struct _SmallHandler {
    static void* __handle(_Action __act, any const * __this, any * __other,
                          std::type_info const * __info, const void* __fallback_info) {
       switch (__act) {
       case _Action::_Destroy:
         __destroy(const_cast<any &>(*__this));
         return nullptr;
       case _Action::_Copy:
           __copy(*__this, *__other);
         return nullptr;
       case _Action::_Move:
         __move(const_cast<any &>(*__this), *__other);
         return nullptr;
       case _Action::_Get:
         return __get(const_cast<any &>(*__this), __info, __fallback_info);
       case _Action::_TypeInfo:
         return __type_info();
       }
       return nullptr;
   }

  template <class ..._Args>
  static _Tp& __create(any & __dest, _Args&&... __args) {
    _Tp* __ret = ::new (static_cast<void*>(&__dest.__s.__buf)) _Tp(std::forward<_Args>(__args)...);
    __dest.__h = &_SmallHandler::__handle;
    return *__ret;
  }

  private:
    static void __destroy(any & __this) {
      _Tp & __value = *static_cast<_Tp *>(static_cast<void*>(&__this.__s.__buf));
      __value.~_Tp();
      __this.__h = nullptr;
    }

    static void __copy(any const & __this, any & __dest) {
      _SmallHandler::__create(__dest, *static_cast<_Tp const *>(
            static_cast<void const *>(&__this.__s.__buf)));
    }

    static void __move(any & __this, any & __dest) {
      _SmallHandler::__create(__dest, std::move(
          *static_cast<_Tp*>(static_cast<void*>(&__this.__s.__buf))));
      __destroy(__this);
    }

    static void* __get(any & __this,
                       std::type_info const * __info,
                       const void* __fallback_id) {
      if (__any_imp::__compare_typeid<_Tp>(__info, __fallback_id))
        return static_cast<void*>(&__this.__s.__buf);
      return nullptr;
    }

    static void* __type_info() {
#if !defined(_LIBCPP_NO_RTTI)
      return const_cast<void*>(static_cast<void const *>(&typeid(_Tp)));
#else
      return nullptr;
#endif
    }
  };

  template <class _Tp>
  struct _LargeHandler {
    static void* __handle(_Action __act, any const * __this,
                          any * __other, std::type_info const * __info,
                          void const* __fallback_info) {
      switch (__act) {
      case _Action::_Destroy:
        __destroy(const_cast<any &>(*__this));
        return nullptr;
      case _Action::_Copy:
        __copy(*__this, *__other);
        return nullptr;
      case _Action::_Move:
        __move(const_cast<any &>(*__this), *__other);
        return nullptr;
      case _Action::_Get:
          return __get(const_cast<any &>(*__this), __info, __fallback_info);
      case _Action::_TypeInfo:
        return __type_info();
      }
      return nullptr;
    }

    template <class ..._Args>
    static _Tp& __create(any & __dest, _Args&&... __args) {
      typedef std::allocator<_Tp> _Alloc;
      typedef __allocator_destructor<_Alloc> _Dp;
      _Alloc __a;
      std::unique_ptr<_Tp, _Dp> __hold(__a.allocate(1), _Dp(__a, 1));
      _Tp* __ret = ::new ((void*)__hold.get()) _Tp(std::forward<_Args>(__args)...);
      __dest.__s.__ptr = __hold.release();
      __dest.__h = &_LargeHandler::__handle;
      return *__ret;
    }

  private:
    static void __destroy(any & __this) {
      delete static_cast<_Tp*>(__this.__s.__ptr);
      __this.__h = nullptr;
    }

    static void __copy(any const & __this, any & __dest) {
      _LargeHandler::__create(__dest, *static_cast<_Tp const *>(__this.__s.__ptr));
    }

    static void __move(any & __this, any & __dest) {
      __dest.__s.__ptr = __this.__s.__ptr;
      __dest.__h = &_LargeHandler::__handle;
      __this.__h = nullptr;
    }

    static void* __get(any & __this, std::type_info const * __info,
                       void const* __fallback_info) {
      if (__any_imp::__compare_typeid<_Tp>(__info, __fallback_info))
        return static_cast<void*>(__this.__s.__ptr);
      return nullptr;
    }

    static void* __type_info() {
#if !defined(_LIBCPP_NO_RTTI)
      return const_cast<void*>(static_cast<void const *>(&typeid(_Tp)));
#else
      return nullptr;
#endif
    }
  };

} // namespace __any_imp


template <class _ValueType, class _Tp, class>
any::any(_ValueType && __v) : __h(nullptr) {
  __any_imp::_Handler<_Tp>::__create(*this, std::forward<_ValueType>(__v));
}

template <class _ValueType, class ..._Args, class _Tp, class>
any::any(in_place_type_t<_ValueType>, _Args&&... __args) {
  __any_imp::_Handler<_Tp>::__create(*this, std::forward<_Args>(__args)...);
}

template <class _ValueType, class _Up, class ..._Args, class _Tp, class>
any::any(in_place_type_t<_ValueType>, std::initializer_list<_Up> __il, _Args&&... __args) {
  __any_imp::_Handler<_Tp>::__create(*this, __il, std::forward<_Args>(__args)...);
}

template <class _ValueType, class, class>
any& any::operator=(_ValueType && __v) {
  any(std::forward<_ValueType>(__v)).swap(*this);
  return *this;
}

template <class _ValueType, class ..._Args, class _Tp, class>
_Tp& any::emplace(_Args&&... __args) {
  reset();
  return __any_imp::_Handler<_Tp>::__create(*this, std::forward<_Args>(__args)...);
}

template <class _ValueType, class _Up, class ..._Args, class _Tp, class>
_Tp& any::emplace(std::initializer_list<_Up> __il, _Args&&... __args) {
  reset();
  return __any_imp::_Handler<_Tp>::__create(*this, __il, std::forward<_Args>(__args)...);
}

// 6.4 Non-member functions
/*
void swap(any & __lhs, any & __rhs) {
    __lhs.swap(__rhs);
}

template <class _Tp, class ..._Args>
any make_any(_Args&&... __args) {
  return any(in_place_type<_Tp>, std::forward<_Args>(__args)...);
}

template <class _Tp, class _Up, class ..._Args>
any make_any(initializer_list<_Up> __il, _Args&&... __args) {
  return any(in_place_type<_Tp>, __il, std::forward<_Args>(__args)...);
}
*/
template <class _ValueType>
_ValueType any_cast(any const & __v) {
  using _RawValueType = __uncvref_t<_ValueType>;
  static_assert(std::is_constructible<_ValueType, _RawValueType const &>::value,
                "ValueType is required to be a const lvalue reference "
                "or a CopyConstructible type");
  auto __tmp = any_cast<add_const_t<_RawValueType>>(&__v);
  if (__tmp == nullptr)
    throw bad_any_cast();
  return static_cast<_ValueType>(*__tmp);
}

template <class _ValueType>
_ValueType any_cast(any & __v) {
  using _RawValueType = __uncvref_t<_ValueType>;
  static_assert(std::is_constructible<_ValueType, _RawValueType &>::value,
                "ValueType is required to be an lvalue reference "
                "or a CopyConstructible type");
  auto __tmp = any_cast<_RawValueType>(&__v);
  if (__tmp == nullptr)
    throw bad_any_cast();
  return static_cast<_ValueType>(*__tmp);
}

template <class _ValueType>
_ValueType any_cast(any && __v) {
  using _RawValueType = __uncvref_t<_ValueType>;
  static_assert(std::is_constructible<_ValueType, _RawValueType>::value,
                "ValueType is required to be an rvalue reference "
                "or a CopyConstructible type");
  auto __tmp = any_cast<_RawValueType>(&__v);
  if (__tmp == nullptr)
    throw bad_any_cast();
  return static_cast<_ValueType>(std::move(*__tmp));
}

template <class _ValueType>
add_pointer_t<add_const_t<_ValueType>> any_cast(any const * __any) {
  static_assert(!std::is_reference<_ValueType>::value,
                "_ValueType may not be a reference.");
  return any_cast<_ValueType>(const_cast<any *>(__any));
}

template <class _RetType>
_RetType __pointer_or_func_cast(void* __p, /*IsFunction*/std::false_type) noexcept {
  return static_cast<_RetType>(__p);
}

template <class _RetType>
_RetType __pointer_or_func_cast(void*, /*IsFunction*/std::true_type) noexcept {
  return nullptr;
}

template <class _ValueType>
add_pointer_t<_ValueType> any_cast(any * __any) {
  using __any_imp::_Action;
  static_assert(!std::is_reference<_ValueType>::value,
                "_ValueType may not be a reference.");
  typedef typename std::add_pointer<_ValueType>::type _ReturnType;
  if (__any && __any->__h) {
    void *__p = __any->__call(_Action::_Get, nullptr,
#if !defined(_LIBCPP_NO_RTTI)
                        &typeid(_ValueType),
#else
                        nullptr,
#endif
                        __any_imp::__get_fallback_typeid<_ValueType>());
      return __pointer_or_func_cast<_ReturnType>(
            __p, std::is_function<_ValueType>{});
  }
  return nullptr;
}

}  // namespace cnstream

#endif
