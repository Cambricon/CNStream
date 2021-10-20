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
#include <string>
#include <utility>
#include <vector>

#include "obj_filter.hpp"

class CarFilter : public cnstream::ObjFilter {
 public:
  bool Filter(const cnstream::CNFrameInfoPtr& finfo, const cnstream::CNInferObjectPtr& obj) override {
    // for ssd with cnstream/data/models/label_voc.txt
    int id = atoi(obj->id.c_str());
    if (6 == id)
      return true;  // cars will be inferenced.
    else if (5 == id)
      return true;  // bus will be inferenced.
    return false;
  }

  DECLARE_REFLEX_OBJECT_EX(CarFilter, cnstream::ObjFilter)
};  // classd CarFilter

IMPLEMENT_REFLEX_OBJECT_EX(CarFilter, cnstream::ObjFilter)
