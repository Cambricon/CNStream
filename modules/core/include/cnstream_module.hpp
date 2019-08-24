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

#ifndef CNSTREAM_MODULE_HPP_
#define CNSTREAM_MODULE_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"

namespace cnstream {

/*************************************************************************
 * @brief Module is the parent class of all modules. A module could
 *        have configurable number of upstream links as well as downstream
 *
 * Some modules have been constucted along with framework
 * e.g. decoder, inferencer, etc.
 * Also, users can design their own module.
 ************************************************************************/

using ModuleParamSet = std::unordered_map<std::string, std::string>;

class Module {
 public:
  explicit Module(const std::string &name) { SetName(name); }
  virtual ~Module() {}

  /*
  @brief Called before Process()
   */
  virtual bool Open(ModuleParamSet paramSet) = 0;
  /*
    @brief Called when Process() not invoked.
   */
  virtual void Close() = 0;

  /*
    @brief Called by pipeline when data is comming for this module.
    @param
      data[in]: Data that should be processed by this module.
    @return
      Return 0 for good.
      otherwise, pipeline will post an EVENT_ERROR with return number.
   */
  virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;

  inline void SetName(const std::string &name) { name_ = name; }

  inline std::string GetName() const { return name_; }

  inline void SetContainer(Pipeline *container) { container_ = container; }

  bool PostEvent(EventType type, const std::string &msg) const;

  /**/
  unsigned int GetId() {
    if (!id_.load()) {
      id_ = module_id_.fetch_add(1);
    }
    return id_.load();
  }
  std::vector<unsigned int> GetParentIds() const { return parent_ids_; }
  void SetParentId(unsigned int id) {
    parent_ids_.push_back(id);
    mask_ = 0;
    for (auto &v : parent_ids_) mask_ |= (unsigned long)1 << v;
  }
  unsigned long GetModulesMask() const { return mask_; }
  static void ResetIdBase() { module_id_.store(0); }

 protected:
  Pipeline *container_ = nullptr;
  std::string name_;
  std::atomic<unsigned int> id_{0};
  std::vector<unsigned int> parent_ids_;
  unsigned long mask_ = 0;
  static std::atomic<unsigned int> module_id_;
};

}  // namespace cnstream

#endif  // CNSTREAM_MODULE_HPP_
