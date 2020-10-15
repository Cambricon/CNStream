/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * This source code is licensed under the Apache-2.0 license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * A part of this source code is referenced from Nebula project.
 * https://github.com/Bwar/Nebula/blob/master/src/actor/DynamicCreator.hpp
 * https://github.com/Bwar/Nebula/blob/master/src/actor/ActorFactory.hpp
 *
 * Copyright (C) Bwar.
 *
 * This source code is licensed under the Apache-2.0 license found in the
 * LICENSE file in the root directory of this source tree.
 *
 *************************************************************************/

#ifndef CNSTREAM_MODULE_HPP_
#define CNSTREAM_MODULE_HPP_

/**
 * @file cnstream_module.hpp
 *
 * This file contains a declaration of the Module class and the ModuleFactory class.
 */
#include <cxxabi.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_eventbus.hpp"
#include "cnstream_config.hpp"
#include "cnstream_frame.hpp"
#include "perf_manager.hpp"
#include "util/cnstream_queue.hpp"
#include "util/cnstream_rwlock.hpp"

namespace cnstream {
/**
 * @brief IModuleObserver virtual base class.
 *
 * IModuleObserver is an interface class. User need to implement an observer
 * based on this, and register it to one module.
 *
 */
class IModuleObserver {
 public:
  /**
   * @brief Notify "data" after processed by this module.
   */
  virtual void notify(std::shared_ptr<CNFrameInfo> data) = 0;
  virtual ~IModuleObserver() {}
};

class Pipeline;
class PerfManager;

/**
 * @brief Module virtual base class.
 *
 * Module is the parent class of all modules. A module could have configurable
 * number of upstream links and downstream links.
 * Some modules are already constructed with a framework,
 * such as source, inferencer, and so on.
 * You can also design your own modules.
 */
class Module {
 public:
  /**
   * Constructor.
   *
   * @param name The name of a module. Modules defined in a pipeline should
   *             have different names.
   */
  explicit Module(const std::string &name) : name_(name) {}
  virtual ~Module();
  /**
   * Registers an observer to the module.
   *
   * @param observer An observer you defined.
   *
   * @return Void.
   */
  void SetObserver(IModuleObserver *observer) {
    RwLockWriteGuard guard(observer_lock_);
    observer_ = observer;
  }
  /**
   * Opens resources for a module.
   *
   * @param param_set A set of parameters for this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   *
   * @note You do not need to call this function by yourself. This function is called
   *       by pipeline automatically when the pipeline is started. The pipeline calls the ``Process`` function
   *       of this module automatically after the ``Open`` function is done.
   */
  virtual bool Open(ModuleParamSet param_set) = 0;

  /**
   * Closes resources for a module.
   *
   * @return Void.
   *
   * @note You do not need to call this function by yourself. This function is called
   *       by pipeline automatically when the pipeline is stopped. The pipeline calls the ``Close`` function
   *       of this module automatically after the ``Open`` and ``Process`` functions are done.
   */
  virtual void Close() = 0;

  /**
   * Processes data.
   *
   * @param data The data to be processed by the module.
   *
   * @retval 0: The data is processed successfully. The data should be transmitted in the framework then.
   * @retval >0: The data is processed successfully. The data has been handled by this module. The ``hasTransmit_`` must
   * be set. The Pipeline::ProvideData should be called by Module to transmit data to the next modules in the pipeline.
   * @retval <0: Pipeline will post an event with the EVENT_ERROR event type and return
   *             number.
   */
  virtual int Process(std::shared_ptr<CNFrameInfo> data) = 0;

  /**
   * Gets the name of this module.
   *
   * @return Returns the name of this module.
   */
  inline std::string GetName() const { return name_; }

  /**
   * Posts an event to the pipeline.
   *
   * @param type The type of an event.
   * @param msg The event message string.
   *
   * @return Returns true if this function has run successfully. Returns false if this
   *         module has not been added to the pipeline.
   */
  bool PostEvent(EventType type, const std::string &msg);

  /**
   * Posts an event to the pipeline.
   *
   * @param Event with event type, stream_id, message, module name and thread_id.
   *
   * @return Returns true if this function has run successfully. Returns false if this
   *         module has not been added to the pipeline.
   */
  bool PostEvent(Event e);

  /**
   * @brief Transmits data to the following stages.
   *
   * Valid when the module has permission to transmit data by itself.
   *
   * @param data A pointer to the information of the frame.
   *
   * @return Returns true if the data has been transmitted successfully. Otherwise, returns false.
   */
  bool TransmitData(std::shared_ptr<CNFrameInfo> data);

  /**
   * @brief Checks parameters for a module, including parameter name, type, value, validity, and so on.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  virtual bool CheckParamSet(const ModuleParamSet &paramSet) const { return true; }

  /**
   * @brief Records the start time and the end time of the module
   *
   * @param data A pointer to the information of the frame.
   * @param is_finished If it is false, records start time, otherwise records end time.
   *
   * @return void
   */
  virtual void RecordTime(std::shared_ptr<CNFrameInfo> data, bool is_finished);
  /**
   * @brief Gets PerfManager by stream ID.
   *
   * @param stream_id The stream ID.
   *
   * @return Returns the shared_ptr object of PerfManager.
   */
  std::shared_ptr<PerfManager> GetPerfManager(const std::string &stream_id);

 public:
  /**
   * @brief ParamRegister
   *
   * Each module registers its own parameters and descriptions.
   * CNStream Inspect tool uses this class to detect parameters of each module.
   *
   */
  ParamRegister param_register_;

  /* useless for users */
  size_t GetId();

 protected:
#ifdef UNIT_TEST

 public:
#endif

  friend class Pipeline;
  /**
   * Sets a container to this module and identifies which pipeline the module is added to.
   *
   * @param container A pipeline pointer to the container of this module.
   *
   * @note This function is called automatically by the pipeline after this module
   *       is added into the pipeline. You do not need to call this function by yourself.
   */
  void SetContainer(Pipeline *container);

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
   * @brief Checks if this module has permission to transmit data by itself.
   *
   * @return Returns true if this module has permission to transmit data by itself. Otherwise, returns false.
   *
   * @see Process
   */
  bool HasTransmit() const { return hasTransmit_.load(); }

  /**
   * @brief Processes the data.
   *
   * This function is called by a pipeline.
   *
   * @param data A pointer to the information of the frame.
   *
   * @return
   * @retval 0: The process has been run successfully. The data should be transmitted by framework then.
   * @retval >0: The process has been run successfully. The data has been handled by this module. The ``hasTransmit_``
   * must be set. The Pipeline::ProvideData should be called by Module to transmit data to the next modules in the
   * pipeline.
   * @retval <0: Pipeline posts an event with the EVENT_ERROR event type and return
   *             number.
   */
  int DoProcess(std::shared_ptr<CNFrameInfo> data);

  /**
   * @brief Checks if the display of performance information is enabled.
   *
   * @return Returns true if the performance information is displayed. Otherwise, returns false.
   */
  bool ShowPerfInfo() { return showPerfInfo_.load(); }

  /**
   * @brief Enables or disables to display performance information.
   *
   * @param enable If this parameter is set to true, the performance information is enabled to display.  
   *               Otherwise, the performance information is disabled to display.
   *
   * @return Void.
   */
  void ShowPerfInfo(bool enable) { showPerfInfo_.store(enable); }

 protected:
  Pipeline *container_ = nullptr;  ///< The container.
  RwLock container_lock_;

  std::string name_;                      ///< The name of the module.
  std::atomic<bool> hasTransmit_{false};  ///< Whether it has permission to transmit data.

 private:
  size_t id_ = INVALID_MODULE_ID;
  /* supports no more than 64 modules */
  static SpinLock module_id_spinlock_;
  static uint64_t module_id_mask_;

  std::vector<size_t> parent_ids_;
  uint64_t mask_ = 0;

  IModuleObserver *observer_ = nullptr;
  RwLock observer_lock_;
  void NotifyObserver(std::shared_ptr<CNFrameInfo> data) {
    RwLockReadGuard guard(observer_lock_);
    if (observer_) {
      observer_->notify(data);
    }
  }

 protected:
  std::atomic<bool> showPerfInfo_{false};
};

/**
 * @brief ModuleEx class.
 *
 * Module has permission to transmit data by itself.
 */
class ModuleEx : public Module {
 public:
  /**
   * Constructor.
   *
   * @param name The name of a module. Modules defined in a pipeline should
   *             have different names.
   */
  explicit ModuleEx(const std::string &name) : Module(name) { hasTransmit_.store(true); }
};

/**
 * @brief ModuleCreator, ModuleFactory, and ModuleCreatorWorker:
 *   Implements reflection mechanism to create a module instance dynamically with the ``ModuleClassName`` and
 *    ``moduleName`` parameters.
 *   See ActorFactory&DynamicCreator in https://github.com/Bwar/Nebula (under Apache2.0 license)
 */

/**
 * @brief ModuleFactory
 * Provides functions to create instances with the ``ModuleClassName``and ``moduleName`` parameters.
 */
class ModuleFactory {
 public:
  /**
   * @brief Creates or gets the instance of the ModuleFactory class.
   *
   * @return Returns the instance of the ModuleFactory class.
   */
  static ModuleFactory *Instance() {
    if (nullptr == factory_) {
      factory_ = new (std::nothrow) ModuleFactory();
      LOG_IF(FATAL, nullptr == factory_) << "ModuleFactory::Instance() new ModuleFactory failed.";
    }
    return (factory_);
  }
  virtual ~ModuleFactory() {}

  /**
   * Registers ``ModuleClassName`` and ``CreateFunction``.
   *
   * @param strTypeName The module class name.
   * @param pFunc The ``CreateFunction`` of a Module object that has a parameter ``moduleName``.
   *
   * @return Returns true if this function has run successfully.
   */
  bool Regist(const std::string &strTypeName, std::function<Module *(const std::string &)> pFunc) {
    if (nullptr == pFunc) {
      return (false);
    }
    bool ret = map_.insert(std::make_pair(strTypeName, pFunc)).second;
    return ret;
  }

  /**
   * Creates a module instance with ``ModuleClassName`` and ``moduleName``.
   *
   * @param strTypeName The module class name.
   * @param name The ``CreateFunction`` of a Module object that has a parameter ``moduleName``.
   *
   * @return Returns the module instance if this function has run successfully. Otherwise, returns nullptr if failed.
   */
  Module *Create(const std::string &strTypeName, const std::string &name) {
    auto iter = map_.find(strTypeName);
    if (iter == map_.end()) {
      return (nullptr);
    } else {
      return (iter->second(name));
    }
  }

  /**
   * Gets all registered modules.
   *
   * @return All registered module class names.
   */
  std::vector<std::string> GetRegisted() {
    std::vector<std::string> registed_modules;
    for (auto &it : map_) {
      registed_modules.push_back(it.first);
    }
    return registed_modules;
  }

 private:
  ModuleFactory() {}
  static ModuleFactory *factory_;
  std::unordered_map<std::string, std::function<Module *(const std::string &)>> map_;
};

/**
 * @brief ModuleCreator
 *   A concrete ModuleClass needs to inherit ModuleCreator to enable reflection mechanism.
 *   ModuleCreator provides ``CreateFunction``, and registers ``ModuleClassName`` and ``CreateFunction`` to
 * ModuleFactory().
 */
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
    inline void do_nothing() const {}
  };
  ModuleCreator() { register_.do_nothing(); }
  virtual ~ModuleCreator() { register_.do_nothing(); }
  /**
   * @brief Creates an instance of template (T) with specified instance name.
   *
   * This is a template function.
   *
   * @param name The name of the instance.
   *
   * @return Returns the instance of template (T).
   */
  static T *CreateObject(const std::string &name) { return new (std::nothrow) T(name); }
  static Register register_;
};

template <typename T>
typename ModuleCreator<T>::Register ModuleCreator<T>::register_;

/**
 * @brief ModuleCreatorWorker, a dynamic-creator helper.
 */
class ModuleCreatorWorker {
 public:
  /**
   * @brief Creates a module instance with ``ModuleClassName`` and ``moduleName``.
   *
   * @param strTypeName The module class name.
   * @param name The module name.
   *
   * @return Returns the module instance if the module instance is created successfully. Returns nullptr if failed.
   * @see ModuleFactory::Create
   */
  Module *Create(const std::string &strTypeName, const std::string &name) {
    Module *p = ModuleFactory::Instance()->Create(strTypeName, name);
    return (p);
  }
};

}  // namespace cnstream

#endif  // CNSTREAM_MODULE_HPP_
