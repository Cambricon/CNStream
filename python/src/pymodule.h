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

#include <pybind11/pybind11.h>

#include <cnstream_module.hpp>

#include <memory>
#include <string>

namespace cnstream {

class __attribute__((visibility("default"))) PyModule : public ModuleEx, public ModuleCreator<PyModule> {
 public:
  explicit PyModule(const std::string& name);
  ~PyModule();
  bool CheckParamSet(const ModuleParamSet &params) const override;
  bool Open(ModuleParamSet params) override;
  void Close() override;
  int Process(std::shared_ptr<CNFrameInfo> data) override;

 private:
  pybind11::object pyinstance_;
  pybind11::object pyopen_;
  pybind11::object pyclose_;
  pybind11::object pyprocess_;
  pybind11::object pyon_eos_;
  bool instance_has_transmit_ = false;
};  // class PyModule

}  // namespace cnstream

