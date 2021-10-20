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

#include <gtest/gtest.h>
#include <string>
#include <utility>

#include "cnstream_collection.hpp"

namespace collection_test {

struct TestStructA {
  std::string member_a;
  float member_b;
  TestStructA() = default;
  TestStructA(const std::string ma, float mb) : member_a(ma), member_b(mb) {}
  TestStructA(TestStructA&& other) {
    member_a = std::move(other.member_a);
    std::swap(member_b, other.member_b);
  }
  TestStructA(const TestStructA& other) {
    member_a = other.member_a;
    member_b = other.member_b;
  }
  TestStructA& operator=(const TestStructA& other) {
    member_a = other.member_a;
    member_b = other.member_b;
    return *this;
  }
  TestStructA& operator=(TestStructA&& other) {
    member_a = std::move(other.member_a);
    std::swap(member_b, other.member_b);
    return *this;
  }
};  // struct TestStructA

struct TestStructB {
  std::string member_a;
  int member_b;
  TestStructB() = default;
  TestStructB(const std::string ma, int mb) : member_a(ma), member_b(mb) {}
  TestStructB(TestStructB&& other) {
    member_a = std::move(other.member_a);
    std::swap(member_b, other.member_b);
  }
  TestStructB(const TestStructB& other) {
    member_a = other.member_a;
    member_b = other.member_b;
  }
  TestStructB& operator=(const TestStructB& other) {
    member_a = other.member_a;
    member_b = other.member_b;
    return *this;
  }
  TestStructB& operator=(TestStructB&& other) {
    member_a = std::move(other.member_a);
    std::swap(member_b, other.member_b);
    return *this;
  }
};  // struct TestStructB

bool operator==(const TestStructA& a, const TestStructA& b) {
  return a.member_a == b.member_a && a.member_b == b.member_b;
}

bool operator==(const TestStructB& a, const TestStructB& b) {
  return a.member_a == b.member_a && a.member_b == b.member_b;
}

}  // namespace collection_test

static const char test_tag0[] = "test_tag0";
static const char test_tag1[] = "test_tag1";
static const collection_test::TestStructA value_a { "structa_member_a", 1.2 };
static const collection_test::TestStructB value_b { "structb_member_b", 1 };

TEST(CoreCollection, Add) {
  {
    // const lvalue version
    cnstream::Collection collection;
    collection_test::TestStructA& ret = collection.Add(test_tag0, value_a);
    EXPECT_EQ(ret, value_a);
  }
  {
    // rvalue version
    cnstream::Collection collection;
    collection_test::TestStructA value_moved = value_a;
    collection_test::TestStructA& ret = collection.Add(test_tag0, std::move(value_moved));
    EXPECT_EQ(ret, value_a);
  }
  {
    // add two tag
    cnstream::Collection collection;
    collection_test::TestStructA& ret_a = collection.Add(test_tag0, value_a);
    collection_test::TestStructB& ret_b = collection.Add(test_tag1, value_b);
    EXPECT_EQ(ret_a, value_a);
    EXPECT_EQ(ret_b, value_b);
  }
  {
    // add the same tag twice
    cnstream::Collection collection;
    collection.Add(test_tag0, value_a);
    EXPECT_DEATH(collection.Add(test_tag0, value_a), "");
  }
}

TEST(CoreCollection, AddIfNotExist) {
  {
    // const lvalue version
    cnstream::Collection collection;
    bool ret = collection.AddIfNotExists(test_tag0, value_a);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value_a, collection.Get<collection_test::TestStructA>(test_tag0));

    ret = collection.AddIfNotExists(test_tag0, value_b);
    EXPECT_FALSE(ret);
    EXPECT_EQ(value_a, collection.Get<collection_test::TestStructA>(test_tag0));

    ret = collection.AddIfNotExists(test_tag1, value_b);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value_b, collection.Get<collection_test::TestStructB>(test_tag1));
  }
  {
    // rvalue version
    cnstream::Collection collection;
    collection_test::TestStructA value_moved = value_a;
    bool ret = collection.AddIfNotExists(test_tag0, std::move(value_moved));
    EXPECT_TRUE(ret);
    EXPECT_EQ(value_a, collection.Get<collection_test::TestStructA>(test_tag0));

    collection_test::TestStructB value_b_moved = value_b;
    ret = collection.AddIfNotExists(test_tag0, std::move(value_b));
    EXPECT_FALSE(ret);
    EXPECT_EQ(value_a, collection.Get<collection_test::TestStructA>(test_tag0));

    ret = collection.AddIfNotExists(test_tag1, std::move(value_b));
    EXPECT_TRUE(ret);
    EXPECT_EQ(value_b, collection.Get<collection_test::TestStructB>(test_tag1));
  }
}

TEST(CoreCollection, Get) {
  {
    // normal
    cnstream::Collection collection;
    collection.Add(test_tag0, value_a);
    collection_test::TestStructA& ret = collection.Get<collection_test::TestStructA>(test_tag0);
    EXPECT_EQ(ret, value_a);
    // modify value
    ret.member_a = "modified_member_a";
    EXPECT_EQ(collection.Get<collection_test::TestStructA>(test_tag0), ret);
  }
  {
    // never added
    cnstream::Collection collection;
    EXPECT_DEATH(collection.Get<collection_test::TestStructB>(test_tag0), "");
  }
  {
    // not match type
    cnstream::Collection collection;
    collection.Add(test_tag0, value_a);
    EXPECT_DEATH(collection.Get<collection_test::TestStructB>(test_tag0), "");
  }
  {
    // two tag
    cnstream::Collection collection;
    collection_test::TestStructA& ret = collection.Add(test_tag0, value_a);
    collection.Add(test_tag1, value_b);
    EXPECT_EQ(ret, value_a);
    EXPECT_EQ(ret, collection.Get<collection_test::TestStructA>(test_tag0));
  }
}

TEST(CoreCollection, MapRealloc) {
  // test map realloc
  cnstream::Collection collection;
  collection_test::TestStructA& ret = collection.Add(test_tag0, value_a);
  ret.member_a = "modified_member_a";
  for (int i = 0; i < 2048; ++i)
    collection.Add(std::to_string(i), value_a);
  EXPECT_EQ(collection.Get<collection_test::TestStructA>(test_tag0), ret);
}

TEST(CoreCollection, HasValue) {
  cnstream::Collection collection;
  collection.Add(test_tag0, value_a);
  EXPECT_TRUE(collection.HasValue(test_tag0));
  EXPECT_FALSE(collection.HasValue(test_tag1));
}

#if !defined(_LIBCPP_NO_RTTI)
TEST(CoreCollection, Type) {
  cnstream::Collection collection;
  collection.Add(test_tag0, value_a);
  EXPECT_EQ(typeid(collection_test::TestStructA), collection.Type(test_tag0));
  EXPECT_NE(typeid(collection_test::TestStructB), collection.Type(test_tag0));
  // tag not added
  EXPECT_DEATH(collection.Get<collection_test::TestStructB>(test_tag1), "");
}

TEST(CoreCollection, TaggedIsOfType) {
  cnstream::Collection collection;
  collection.Add(test_tag0, value_a);
  EXPECT_TRUE(collection.TaggedIsOfType<collection_test::TestStructA>(test_tag0));
  EXPECT_FALSE(collection.TaggedIsOfType<collection_test::TestStructB>(test_tag0));
}
#endif

