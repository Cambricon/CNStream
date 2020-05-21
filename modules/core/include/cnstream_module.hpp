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

#define CNS_JSON_DIR_PARAM_NAME "json_file_dir"

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
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_eventbus.hpp"
#include "cnstream_frame.hpp"
#include "threadsafe_queue.hpp"

namespace cnstream {

class PerfManager;

/// Module parameter set.
using ModuleParamSet = std::unordered_map<std::string, std::string>;

/**
 * @brief Gets the complete path of a file.
 *
 * If the path you set is an absolute path, returns the absolute path.
 * If the path you set is a relative path, retuns the path that appends the relative path
 * to the specified JSON file path.
 *
 * @param path The path relative to the JSON file or an absolute path.
 * @param param_set The module parameters. The JSON file path is one of the parameters.
 *
 * @return Returns the complete path of a file.
 */
__attribute__((unused)) inline std::string GetPathRelativeToTheJSONFile(const std::string &path,
                                                                        const ModuleParamSet &param_set) {
  std::string jsf_dir = "./";
  if (param_set.find(CNS_JSON_DIR_PARAM_NAME) != param_set.end()) {
    jsf_dir = param_set.find(CNS_JSON_DIR_PARAM_NAME)->second;
  }

  std::string ret = "";
  if (path.size() > 0 && '/' == path[0]) {
    /*absolute path*/
    ret = path;
  } else {
    ret = jsf_dir + path;
  }
  return ret;
}

/**
 * @brief Module virtual base class.
 *
 * Module is the parent class of all modules. A module should have configurable
 * number of upstream links and downstream links.
 * Some modules are already constructed with a framework,
 * such as source, inferencer, and so on.
 * You can also design your own modules.
 */
class Module {
  /**
   * @brief ParamRegister
   *
   * Each module registers its own parameters and descriptions.
   * This is used in CNStream Inspect tool to detect parameters of each module.
   *
   */
 public:
  class ParamRegister {
   private:
    std::vector<std::pair<std::string /*key*/, std::string /*desc*/>> module_params_;
    std::string module_desc_;

   public:
    /**
     * @brief Registers a parameter and its description.
     *
     * This is used in CNStream Inspect tool.
     *
     * @param key The parameter name.
     * @param desc The description of the parameter.
     *
     * @return Void.
     */
    void Register(const std::string &key, const std::string &desc) {
      module_params_.push_back(std::make_pair(key, desc));
    }
    /**
     * @brief Gets the registered parameters and the parameter description.
     *
     * This is used in CNStream Inspect tool.
     *
     * @return Returns the registered parameters and the parameter description.
     */
    std::vector<std::pair<std::string, std::string>> GetParams() { return module_params_; }
    /**
     * @brief Checks if the parameter is registered.
     *
     * This is used in CNStream Inspect tool.
     *
     * @param key The parameter name.
     *
     * @return Returns true if the parameter has been registered. Otherwise, returns false.
     */
    bool IsRegisted(const std::string &key) const {
      if (strcmp(key.c_str(), CNS_JSON_DIR_PARAM_NAME) == 0) {
        return true;
      }
      for (auto &it : module_params_) {
        if (key == it.first) {
          return true;
        }
      }
      return false;
    }
    /**
     * @brief Sets the description of the module.
     *
     * This is used in CNStream Inspect tool.
     *
     * @param desc The description of the module.
     *
     * @return Void.
     */
    void SetModuleDesc(const std::string &desc) { module_desc_ = desc; }
    /**
     * @brief Gets the description of the module.
     *
     * This is used in CNStream Inspect tool.
     *
     * @return Returns the description of the module.
     */
    std::string GetModuleDesc() { return module_desc_; }
  };

  ParamRegister param_register_;

  /**
   * Constructor.
   *
   * @param name The name of a module. Modules defined in a pipeline should
   *             have different names.
   */
  explicit Module(const std::string &name) : name_(name) { this->GetId(); }
  virtual ~Module() { this->ReturnId(); }

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
   * @retval >0: The data is processed successfully. The data has been handled by this module. The ``hasTransmit_`` must be set.
   *             The Pipeline::ProvideData should be called by Module to transmit data
   *             to the next modules in the pipeline.
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
   *         module is not added to the pipeline.
   */
  bool PostEvent(EventType type, const std::string &msg) const;

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

 protected:
  friend class Pipeline;
  friend class PipelinePrivate;

#ifdef UNIT_TEST

 public:
#endif
  /**
   * Sets a container to this module and identifies which pipeline the module is added to.
   *
   * @param container A pipeline pointer to the container of this module.
   * 
   * @note This function is called automatically by the pipeline after this module
   *       is added into the pipeline. You do not need to call this function by yourself.
   */
  inline void SetContainer(Pipeline *container) { container_ = container; }

  /* useless for users */
  size_t GetId();
  /* useless for users */
  std::vector<size_t> GetParentIds() const { return parent_ids_; }
  /* useless for users, set upstream node ID to this module */
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
   * @retval >0: The process has been run successfully. The data has been handled by this module. The ``hasTransmit_`` must be set.
   *             The Pipeline::ProvideData should be called by Module to transmit data
   *             to the next modules in the pipeline.
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
   * @param enable If it is true, enable to display performance information, otherwise, disable to display the performance information.
   *
   * @return Void.
   */
  void ShowPerfInfo(bool enable) { showPerfInfo_.store(enable); }

  /**
   * @brief Sets PerfManagers. The module can then use PerfManagers.
   *
   * @param perf_managers A container instance in the unordered_map type. The key is the stream ID.
   * The value is a shared_ptr object of PerfManager.
   *
   * @return Void.
   */
  void SetPerfManagers(const std::unordered_map<std::string, std::shared_ptr<PerfManager>>& perf_managers);
  /**
   * @brief Gets PerfManager by stream ID.
   *
   * @param stream_id The stream ID.
   *
   * @return Returns the shared_ptr object of PerfManager.
   */
  std::shared_ptr<PerfManager> GetPerfManager(const std::string& stream_id);
  /**
   * @brief Clears PerfManagers.
   *
   * @return Void.
   */
  void ClearPerfManagers() {
    perf_managers_.clear();
  }

  std::shared_ptr<CNFrameInfo> GetOutputFrame();

 protected:
  Pipeline *container_ = nullptr;         ///< The container.
  std::string name_;                      ///< The name of the module.
  std::atomic<bool> hasTransmit_{false};  ///< Whether it has permission to transmit data.

 private:
  void ReturnId();
  size_t id_ = -1;
  /* supports no more than 64 modules */
  static CNSpinLock module_id_spinlock_;
  static uint64_t module_id_mask_;

  std::vector<size_t> parent_ids_;
  uint64_t mask_ = 0;
  ThreadSafeQueue<std::shared_ptr<CNFrameInfo>> output_frame_queue_;

 protected:
  std::atomic<bool> showPerfInfo_{false};
  std::unordered_map<std::string, std::shared_ptr<PerfManager>> perf_managers_;
};

class ModuleEx : public Module {
 public:
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
   * Gets all registed modules.
   *
   * @return All registed module class names.
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
 *   ModuleCreator provides ``CreateFunction``, and registers ``ModuleClassName`` and ``CreateFunction`` to ModuleFactory().
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
   * @brief Creates an instance of template (T) with sepcified instance name.
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

/**
 *@brief Checks the module parameters.
 */
class ParametersChecker {
 public:
  /**
   * @brief Checks if the path exists.
   *
   * @param path The path relative to JSON file or an absolute path.
   * @param paramSet The module parameters. The path of the JSON file is one of the parameters.
   *
   * @return Returns true if exists. Otherwise, returns false.
   */
  bool CheckPath(const std::string &path, const ModuleParamSet &paramSet) {
    std::string relative_path = GetPathRelativeToTheJSONFile(path, paramSet);
    if ((access(relative_path.c_str(), R_OK)) == -1) {
      return false;
    }
    return true;
  }
  /**
   * @brief Checks if the parameters are number, and the value is specified in the correct range.
   *
   * @param check_list A list of parameter names.
   * @param paramSet The module parameters.
   * @param err_msg The error message.
   * @param greater_than_zero If this parameter is set to ``true``, the parameter set should be
   * greater than or equal to zero. If this parameter is set to ``false``, the parameter set should be less than zero.
   *
   * @return Returns true if the parameters are number and the value is in the correct range. Otherwise, returns false.
   */
  bool IsNum(const std::list<std::string> &check_list, const ModuleParamSet &paramSet, std::string &err_msg,  // NOLINT
             bool greater_than_zero = false) {
    for (auto &it : check_list) {
      if (paramSet.find(it) != paramSet.end()) {
        std::stringstream sin(paramSet.find(it)->second);
        double d;
        char c;
        if (!(sin >> d)) {
          err_msg = "[" + it + "] : " + paramSet.find(it)->second + " is not a number.";
          return false;
        }
        if (sin >> c) {
          err_msg = "[" + it + "] : " + paramSet.find(it)->second + " is not a number.";
          return false;
        }
        if (greater_than_zero) {
          if (d < 0) {
            err_msg = "[" + it + "] : " + paramSet.find(it)->second + " must be greater than zero.";
            return false;
          }
        }
      }
    }
    return true;
  }
};  // class ParametersChecker

}  // namespace cnstream

#endif  // CNSTREAM_MODULE_HPP_
