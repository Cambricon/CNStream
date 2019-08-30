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
  explicit Module(const std::string &name) : name_(name) { this->GetId(); }
  virtual ~Module() { this->ReturnId(); }

  /*deprecated*/
  void SetName(const std::string &name) { name_ = name; }

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
      0 : OK, but framework needs to transmit data.
      1 (>0) : OK, data has been handled by this module. (hasTransmit_ must be set)
      < 0, pipeline will post an EVENT_ERROR with return number.
   */
  virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;
  inline std::string GetName() const { return name_; }

  inline void SetContainer(Pipeline *container) { container_ = container; }

  bool PostEvent(EventType type, const std::string &msg) const;

  /**/
  size_t GetId();
  std::vector<size_t> GetParentIds() const { return parent_ids_; }
  void SetParentId(size_t id) {
    parent_ids_.push_back(id);
    mask_ = 0;
    for (auto &v : parent_ids_) mask_ |= (uint64_t)1 << v;
  }

  uint64_t GetModulesMask() const { return mask_; }

  /**/
  bool hasTranmit() const { return hasTransmit_.load(); }
 protected:
  const size_t INVALID_MODULE_ID = -1;
  Pipeline *container_ = nullptr;
  std::string name_;
  std::atomic<int> hasTransmit_{0};

 private:
  void ReturnId();
  size_t id_ = -1;
  /*support no more than 64 modules*/
  static std::mutex module_id_mutex_;
  static uint64_t module_id_mask_;

  std::vector<size_t> parent_ids_;
  uint64_t mask_ = 0;
};

class ModuleEx : public Module {
 public:
  explicit ModuleEx(const std::string &name) : Module(name) { hasTransmit_.store(1); }
};

/*ModuleCreator:
 *   refer to the ActorFactory&DynamicCreator in https://github.com/Bwar/Nebula (under Apache2.0 license)
 */
#include <cxxabi.h>
#include <functional>
#include <memory>
#include <typeinfo>

class ModuleFactory {
 public:
  static ModuleFactory *Instance() {
    if (nullptr == factory_) {
      factory_ = new ModuleFactory();
    }
    return (factory_);
  }
  virtual ~ModuleFactory(){};
  bool Regist(const std::string &strTypeName, std::function<Module *(const std::string &)> pFunc) {
    if (nullptr == pFunc) {
      return (false);
    }
    bool ret = map_.insert(std::make_pair(strTypeName, pFunc)).second;
    return ret;
  }
  Module *Create(const std::string &strTypeName, const std::string &name) {
    auto iter = map_.find(strTypeName);
    if (iter == map_.end()) {
      return (nullptr);
    } else {
      return (iter->second(name));
    }
  }

 private:
  ModuleFactory(){};
  static ModuleFactory *factory_;
  std::unordered_map<std::string, std::function<Module *(const std::string &)> > map_;
};

template <typename T>
class ModuleCreator {
 public:
  struct Register {
    Register() {
      char *szDemangleName = nullptr;
      std::string strTypeName;
#ifdef __GNUC__
      szDemangleName = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
#else
      // in this format?:     szDemangleName =  typeid(T).name();
      szDemangleName = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
#endif
      if (nullptr != szDemangleName) {
        strTypeName = szDemangleName;
        free(szDemangleName);
      }
      ModuleFactory::Instance()->Regist(strTypeName, CreateObject);
    }
    inline void do_nothing() const {};
  };
  ModuleCreator() { register_.do_nothing(); }
  virtual ~ModuleCreator() { register_.do_nothing(); };
  static T *CreateObject(const std::string &name) { return new T(name); }
  static Register register_;
};

template <typename T>
typename ModuleCreator<T>::Register ModuleCreator<T>::register_;

class ModuleCreatorWorker {
 public:
  Module *Create(const std::string &strTypeName, const std::string &name) {
    Module *p = ModuleFactory::Instance()->Create(strTypeName, name);
    return (p);
  }
};

}  // namespace cnstream

#endif  // CNSTREAM_MODULE_HPP_
