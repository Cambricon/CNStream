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

#include <memory>
#include <vector>

#include "object_filter_video.hpp"

class ObjectFilterVideoStruct : public cnstream::ObjectFilterVideoCategory {
 public:
  bool Filter(const cnstream::CNFrameInfoPtr package, const cnstream::CNInferObjectPtr object) override {
    if (object->GetExtraAttribute("SkipObject") != "") return false;
    if (object->GetExtraAttribute("Category") == "Plate") return false;
    return cnstream::ObjectFilterVideoCategory::Filter(package, object);
  }

  DECLARE_REFLEX_OBJECT_EX(ObjectFilterVideoStruct, cnstream::ObjectFilterVideo);
};  // class ObjectFilterVideoStruct

IMPLEMENT_REFLEX_OBJECT_EX(ObjectFilterVideoStruct, cnstream::ObjectFilterVideo);

class ObjectFilterLpr : public cnstream::ObjectFilterVideo {
 public:
  bool Filter(const cnstream::CNFrameInfoPtr package, const cnstream::CNInferObjectPtr object) override {
    if (object->GetExtraAttribute("Category") != "Plate") return false;
    return cnstream::ObjectFilterVideo::Filter(package, object);
  }

  DECLARE_REFLEX_OBJECT_EX(ObjectFilterLpr, cnstream::ObjectFilterVideo);
};  // class ObjectFilterLpr

IMPLEMENT_REFLEX_OBJECT_EX(ObjectFilterLpr, cnstream::ObjectFilterVideo);
