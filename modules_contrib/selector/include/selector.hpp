/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_SELECTOR_HPP_
#define MODULES_SELECTOR_HPP_

#include <map>
#include <memory>
#include <string>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "private/cnstream_param.hpp"
#include "strategy.hpp"

namespace cnstream {

struct SelectorParams;
struct SelectorContext;

class Selector : public ModuleEx, public ModuleCreator<Selector> {
 public:
  explicit Selector(const std::string &name);
  virtual ~Selector();

  bool Open(ModuleParamSet param_set) override;
  void Close() override;
  int Process(CNFrameInfoPtr data) override;

 private:
  void Select(CNFrameInfoPtr current, CNFrameInfoPtr provide, SelectorContext *ctx);
  SelectorContext *GetContext(CNFrameInfoPtr data);

  std::unique_ptr<ModuleParamsHelper<SelectorParams>> param_helper_ = nullptr;
  std::map<std::string, SelectorContext *> contexts_;
  std::mutex mutex_;
};

}  // namespace cnstream

#endif  // MODULES_SELECTOR_HPP_
