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

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

#include "cnstream_version.hpp"

#define VERSION_STRING_LENGTH 10
#define MAJOR_VERSION 4
#define MINOR_VERSION 0
#define PATCH_VERSION 0

namespace cnstream {

TEST(CoreCnVersion, GetVersion) {
  EXPECT_GE(MajorVersion(), 0);
  EXPECT_GE(MinorVersion(), 0);
  EXPECT_GE(PatchVersion(), 0);
  auto version = VersionString();
  std::string ver_str = "v" + std::to_string(CNSTREAM_MAJOR_VERSION) + "." + std::to_string(CNSTREAM_MINOR_VERSION) +
                        "." + std::to_string(CNSTREAM_PATCH_VERSION);
  EXPECT_STREQ(version, ver_str.c_str());
  // EXPECT_STREQ(version, "v4.5.0");
}

}  // namespace cnstream
