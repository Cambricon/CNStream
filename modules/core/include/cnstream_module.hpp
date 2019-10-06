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

/**
 * \file cnstream_module.hpp
 *
 * This file contains a declaration of class Module and class ModuleFactory.
 */

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"

namespace cnstream {

using ModuleParamSet = std::unordered_map<std::string, std::string>;

/**
 * @brief Module virtual base class.
 *
 * Module is the parent class of all modules. A module could have configurable
 * number of upstream links as well as downstream.
 * Some modules have been constructed along with framework
 * e.g. decoder, inferencer, etc.
 * Also, users can design their own module.
 */
class Module {
 public:
  /**
   * Constructor.
   *
   * @param name Module name. Modules in pipeline should have different name.
   */
  explicit Module(const std::string &name) : name_(name) { this->GetId(); }
  virtual ~Module() { this->ReturnId(); }

  /**
   * @deprecated
   *
   * Set module name.
   *
   * @param name Module name. Modules in pipeline should have different name.
   *
   * @return void.
   */
  void SetName(const std::string &name) { name_ = name; }

  /**
   * Open resources for module.
   *
   * @param param_set Parameters for this module.
   *
   * @return Return true for success, otherwise, false will be returned.
   *
   * @note Do not call this function by yourself. This function will be called
   *       by pipeline when pipeline starts. Pipeline guarantees that the Process function
   *       of this module will be called after the Open function.
   */
  virtual bool Open(ModuleParamSet param_set) = 0;

  /**
   * Close resources for module.
   *
   * @return void.
   *
   * @note Do not call this function by yourself. This function will be called
   *       by pipeline when pipeline stops. Pipeline guarantees that the Close function
   *       of this module will be called after the Open and Process function.
   */
  virtual void Close() = 0;

  /**
   * Processing data.
   *
   * @param data The data to be processed by this module.
   *
   * @return
   * @retval 0 : OK, but framework needs to transmit data.
   * @retval 1: OK, data has been handled by this module. (hasTransmit_ must be set). Module has to
   *            call Pipeline::ProvideData to tell pipeline to transmit data to next modules.
   * @retval >1: OK, data has been handled by this module, and pipeline will transmit data to next modules.
   * @retval <0: Pipeline will post an event with type is EVENT_ERROR with return number.
   */
  virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;

  /**
   * Get name of this module.
   *
   * @return Return name of this module.
   */
  inline std::string GetName() const { return name_; }

  /**
   * Set container to this module, identify this module is added to which pipeline.
   *
   * @note This function will be called by pipeline when this module added into pipeline,
   *       do not call it by yourself.
   */
  inline void SetContainer(Pipeline *container) { container_ = container; }

  /**
   * Post event to pipeline.
   *
   * @param type Event type.
   * @param msg Message string.
   *
   * @return Return true for success. When this module is not added to pipeline, false will be returned.
   */
  bool PostEvent(EventType type, const std::string &msg) const;

  /* useless for users */
  size_t GetId();
  /* useless for users */
  std::vector<size_t> GetParentIds() const { return parent_ids_; }
  /* useless for users, set upstream node id to this module */
  void SetParentId(size_t id) {
    parent_ids_.push_back(id);
    mask_ = 0;
    for (auto &v : parent_ids_) mask_ |= (uint64_t)1 << v;
  }

  /* useless for users */
  uint64_t GetModulesMask() const { return mask_; }

  /**
   * @return Return whether this module has permission to transmit data by itself.
   *
   * @see Process
   */
  bool hasTranmit() const { return hasTransmit_.load(); }

 protected:
  const size_t INVALID_MODULE_ID = -1;
  Pipeline *container_ = nullptr;    ///> Container.
  std::string name_;                 ///> Module name.
  std::atomic<int> hasTransmit_{0};  ///> Has permission to transmit data.

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

/**
 * ModuleCreator:
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
