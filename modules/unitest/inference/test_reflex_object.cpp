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
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "preproc.hpp"
#include "reflex_object.h"

namespace cnstream {

class ReflexObjectTest : public ReflexObject {
 public:
  DECLARE_REFLEX_OBJECT_EX(ReflexObjectTest, ReflexObject);
};
IMPLEMENT_REFLEX_OBJECT_EX(ReflexObjectTest, ReflexObject);

class A : public ReflexObjectEx<A> {
 public:
  virtual ~A() {}
};
class AChild : public A {
  DECLARE_REFLEX_OBJECT_EX(AChild, A);
};
IMPLEMENT_REFLEX_OBJECT_EX(AChild, A);
class B : public ReflexObjectEx<B> {
 public:
  virtual ~B() {}
};
class Bchild : public B {
  DECLARE_REFLEX_OBJECT_EX(Bchild, B);
};
IMPLEMENT_REFLEX_OBJECT_EX(Bchild, B);

TEST(Inferencer, ReflexObject_CreateObject) {
  // already registered
  EXPECT_NE(ReflexObject::CreateObject("ReflexObjectTest"), nullptr);

  EXPECT_EQ(ReflexObject::CreateObject("ReflexObject"), nullptr);
  ClassInfo<ReflexObject> info(std::string("ReflexObject"), ObjectConstructor<ReflexObject>([]() {
                                 return reinterpret_cast<ReflexObject*>(new ReflexObjectTest);
                               }));
  ObjectConstructor<ReflexObject> base_constructor;
  base_constructor = [info]() { return reinterpret_cast<ReflexObject*>(info.constructor()()); };
  ClassInfo<ReflexObject> base_info(info.name(), base_constructor);
  EXPECT_EQ(ReflexObject::Register(base_info), true);
  EXPECT_NE(ReflexObject::CreateObject("ReflexObject"), nullptr);
  ReflexObject::Remove("ReflexObject");
}

TEST(Inferencer, ReflexObject_Register) {
  // reference to static ClassInfo
  ClassInfo<ReflexObject>& info = ReflexObjectTest::sclass_info;
  EXPECT_EQ(ReflexObject::Register(info), false);

  ClassInfo<ReflexObject> info1(std::string("ReflexObject_test"), ObjectConstructor<ReflexObject>([]() {
                                  return reinterpret_cast<ReflexObject*>(new ReflexObjectTest);
                                }));
  ObjectConstructor<ReflexObject> base_constructor;
  base_constructor = [info1]() { return reinterpret_cast<ReflexObject*>(info1.constructor()()); };
  ClassInfo<ReflexObject> base_info(info1.name(), base_constructor);
  EXPECT_EQ(ReflexObject::Register(base_info), true);
  ReflexObject::Remove("ReflexObject_test");
}

TEST(Inferencer, ReflexObjectEx_CreateObject) {
  EXPECT_EQ(ReflexObjectEx<A>::CreateObject("Bchild"), nullptr);

  EXPECT_NE(nullptr, ReflexObjectEx<A>::CreateObject("AChild"));
}

}  // namespace cnstream
