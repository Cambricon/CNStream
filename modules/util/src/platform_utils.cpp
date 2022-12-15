/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#include "platform_utils.hpp"

#include <string>

#include "cnstream_logging.hpp"

#include "cnedk_platform.h"

namespace cnstream {

bool IsEdgePlatform(int device_id) {
  CnedkPlatformInfo platform_info;
  if (CnedkPlatformGetInfo(device_id, &platform_info) != 0) {
    LOG(ERROR) << "[EasyDK] IsEdgePlatform(): CnedkPlatformGetInfo failed";
    return false;
  }

  std::string platform_name(platform_info.name);
  if (platform_name.rfind("CE", 0) == 0) {
    return true;
  }
  return false;
}

bool IsEdgePlatform(const std::string& platform_name) {
  if (platform_name.rfind("CE", 0) == 0) {
    return true;
  }
  return false;
}

bool IsCloudPlatform(int device_id) {
  CnedkPlatformInfo platform_info;
  if (CnedkPlatformGetInfo(device_id, &platform_info) != 0) {
    LOG(ERROR) << "[EasyDK] IsCloudPlatform(): CnedkPlatformGetInfo failed";
    return false;
  }

  std::string platform_name(platform_info.name);
  if (platform_name.rfind("MLU", 0) == 0) {
    return true;
  }
  return false;
}

bool IsCloudPlatform(const std::string& platform_name) {
  if (platform_name.rfind("MLU", 0) == 0) {
    return true;
  }
  return false;
}

}  // namespace cnstream
