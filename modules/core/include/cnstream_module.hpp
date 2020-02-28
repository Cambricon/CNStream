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
 * This file contains a declaration of the Module class and the ModuleFactoryclass.
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
#include "cnstream_statistic.hpp"
#include "cnstream_timer.hpp"

namespace cnstream {

/// Module parameter set
using ModuleParamSet = std::unordered_map<std::string, std::string>;

  /**
   * @brief Get the complete path.
   *
   * If the parameter path is the absolute path, the complete path is the path.
   * However, if it is a relative path, the complete path is appending the path
   * after the json file path given by user.
   * 
   * @param path The path related to the json file or the absolute path.
   *
   * @return Returns the complete path.
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
 * Module is the parent class of all modules. A module could have configurable
 * number of upstream links and downstream links.
 * Some modules are already constructed with a framework,
 * such as source, inferencer, and so on.
 * You can also design your own modules.
 */
class Module {
  /**
   * @brief ParamRegister
   *
   * Each module registers its own parameters and descriptions,
   * cnstream_inspect uses this to detect parameters of each module.
   *
   */
 public:
  class ParamRegister {
   private:
    std::vector<std::pair<std::string /*key*/, std::string /*desc*/>> module_params_;
    std::string module_desc_;

   public:
    /**
    * @brief Register the paramter and its description.
    *
    * This is for inspect tool.
    * 
    * @param key The parameter name.
    * @param desc The description of the paramter.
    *
    * @return Void
    */
    void Register(const std::string &key, const std::string &desc) {
      module_params_.push_back(std::make_pair(key, desc));
    }
    /**
    * @brief Get the registered paramters and their descriptions.
    *
    * This is for inspect tool.
    *
    * @return Returns the registered paramters and their descriptions.
    */
    std::vector<std::pair<std::string, std::string>> GetParams() { return module_params_; }
    /**
    * @brief Check if the paramter is registered.
    *
    * This is for inspect tool.
    * 
    * @param key The parameter name.
    *
    * @return Returns true if the parameter has been registered, otherwise returns false.
    */
    bool IsRegisted(const std::string &key) {
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
    * @brief Set the description of the module.
    *
    * This is for inspect tool.
    * 
    * @param desc The description of the module.
    *
    * @return Void
    */
    void SetModuleDesc(const std::string &desc) { module_desc_ = desc; }
    /**
    * @brief Get the description of the module.
    *
    * This is for inspect tool.
    *
    * @return Returns the description of the module
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
   * @param param_set Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   *
   * @note You do not need to call this function by yourself. This function will be called
   *       by pipeline when pipeline starts. The pipeline will call the Process function
   *       of this module automatically after the Open function is done.
   */
  virtual bool Open(ModuleParamSet param_set) = 0;

  /**
   * Closes resources for a module.
   *
   * @return Void.
   *
   * @note You do not need to call this function by yourself. This function will be called
   *       by pipeline when pipeline stops. The pipeline calls the Close function
   *       of this module automatically after the Open and Process functions are done.
   */
  virtual void Close() = 0;

  /**
   * Processes data.
   *
   * @param data The data that the module will process.
   *
   * @return
   * @retval 0 : OK, but framework needs to transmit data.
   * @retval >0: OK, the data has been handled by this module. The hasTransmit_ must be set.
   *            Module has to call Pipeline::ProvideData to tell pipeline to transmit data
   *            to next modules.
   * @retval <0: Pipeline will post an event with the EVENT_ERROR event type with return
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
   * @param msg Message string.
   *
   * @return Returns true if this function run successfully. Returns false if this
   *         module is not added to pipeline.
   */
  bool PostEvent(EventType type, const std::string &msg) const;

  /**
   * Show performance statistics for this module
   */
  virtual void PrintPerfInfo();

  /**
   * @brief Transmits data to next stages
   * 
   * Valid when the module has permitssion to transmit data by itself.
   * 
   * @param data The pointer to the information of the frame.
   * 
   * @return Returns true if the data is transmitted successfully, otherwise returns false.
   */
  bool TransmitData(std::shared_ptr<CNFrameInfo> data);

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  virtual bool CheckParamSet(ModuleParamSet paramSet) { return true; }

 protected:
  friend class CNDataFrame;
  friend class Pipeline;
  friend class PipelinePrivate;

#ifdef UNIT_TEST

 public:
#endif
  /**
   * Sets a container to this module and identifies which pipeline the module is added to.
   *
   * @note This function will be called automatically by the pipeline after this module
   *       is added into the pipeline. You do not need to call it by yourself.
   */
  inline void SetContainer(Pipeline *container) { container_ = container; }

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
   * @return Returns whether this module has permission to transmit data by itself.
   *
   * @see Process
   */
  bool hasTranmit() const { return hasTransmit_.load(); }

  /**
   * @brief Processes the data
   * 
   * This function will be called by pipeline.
   * 
   * @param data The pointer to the information of the frame.
   * 
   * @return
   * @retval 0 : OK, but framework needs to transmit data.
   * @retval >0: OK, the data has been handled by this module. The hasTransmit_ must be set.
   *            Module has to call Pipeline::ProvideData to tell pipeline to transmit data
   *            to next modules.
   * @retval <0: Pipeline will post an event with the EVENT_ERROR event type with return
   *             number.
   */
  int DoProcess(std::shared_ptr<CNFrameInfo> data);

  /**
   * @return Returns true if the performance info will be shown, otherwise returns false.
   */
  bool ShowPerfInfo() { return showPerfInfo_.load(); }
  /**
   * @brief Change the status of whether showing the performance info.
   * 
   * @param enable the newest status
   * 
   * @return Voids
   */
  void ShowPerfInfo(bool enable) { showPerfInfo_.store(enable); }

 protected:
  Pipeline *container_ = nullptr;         ///< The container.
  std::string name_;                      ///< The name of the module.
  std::atomic<bool> hasTransmit_{false};  ///< If it has permission to transmit data.
  std::atomic<bool> isSource_{false};     ///< If it is a source module.

 private:
  void ReturnId();
  size_t id_ = -1;
  /* supports no more than 64 modules */
  static CNSpinLock module_id_spinlock_;
  static uint64_t module_id_mask_;

  std::vector<size_t> parent_ids_;
  uint64_t mask_ = 0;

 protected:
  StreamFpsStat fps_stat_;
  std::atomic<bool> showPerfInfo_{false};
};

class ModuleEx : public Module {
 public:
  explicit ModuleEx(const std::string &name) : Module(name) { hasTransmit_.store(true); }
};

/**
 * @brief ModuleCreator/ModuleFactory/ModuleCreatorWorker:
 *   Implements reflection mechanism to create a module instance dynamically with "ModuleClassName" and
 *   the "moduleName" parameter.
 *   Refer to the ActorFactory&DynamicCreator in https://github.com/Bwar/Nebula (under Apache2.0 license)
 */

/**
 * @brief ModuleFactory
 * Provides functions to create instances with "ModuleClassName" and the "moduleName" parameter.
 */
class ModuleFactory {
 public:
   /**
   * @brief Create or get the instance of class ModuleFactory.
   *
   * @return Returns the instance of class ModuleFactory.
   */
  static ModuleFactory *Instance() {
    if (nullptr == factory_) {
      factory_ = new(std::nothrow) ModuleFactory();
      LOG_IF(FATAL, nullptr == factory_) << "ModuleFactory::Instance() new ModuleFactory failed.";
    }
    return (factory_);
  }
  virtual ~ModuleFactory() {}

  /**
   * Registers "ModuleClassName" and CreateFunction.
   *
   * @param strTypeName, ModuleClassName (TypeName).
   * @param pFunc, CreateFunction which has a parameter "moduleName".
   *
   * @return Returns true for success.
   */
  bool Regist(const std::string &strTypeName, std::function<Module *(const std::string &)> pFunc) {
    if (nullptr == pFunc) {
      return (false);
    }
    bool ret = map_.insert(std::make_pair(strTypeName, pFunc)).second;
    return ret;
  }

  /**
   * Creates a module instance with  "ModuleClassName" and "moduleName".
   *
   * @param strTypeName, ModuleClassName (TypeName).
   * @param name, The moduleName that is the parameter of CreateFunction.
   *
   * @return The module instance if run successfully. Returns nullptr if failed.
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
   * Get all registed module.
   *
   * @return All registed module ModuleClassName.
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
 *   A concrete ModuleClass needs inherit ModuleCreator to enable reflection mechanism.
 *   ModuleCreator provides CreateFunction and registers ModuleClassName & CreateFunction to ModuleFactory.
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
   * @brief Create an instance of T with "name".
   *
   * This is a template function.
   * 
   * @param name The name.
   * 
   * @return Returns the instance of T.
   */
  static T *CreateObject(const std::string &name) { return new(std::nothrow) T(name); }
  static Register register_;
};

template <typename T>
typename ModuleCreator<T>::Register ModuleCreator<T>::register_;

/**
 * @brief ModuleCreatorWorker, dynamic-creator helper.
 */
class ModuleCreatorWorker {
 public:
   /**
   * @brief Creates a module instance with  "ModuleClassName" and "moduleName".
   *
   * @param strTypeName, The module class name (TypeName).
   * @param name, The module name.
   *
   * @return Returns the module instance if create successfully. Returns nullptr if failed.
   * @see ModuleFactory::Create
   */
  Module *Create(const std::string &strTypeName, const std::string &name) {
    Module *p = ModuleFactory::Instance()->Create(strTypeName, name);
    return (p);
  }
};

/**
 *@brief ParametersChecker, check the module parameters.
 */
class ParametersChecker {
 public:
  /**
   * @brief Check if the path exists.
   *
   * @param path, The path relative to json file or the absolute path.
   * @param paramSet, The module parameters. The json file path is one of the parameters.
   *
   * @return Returns true if exists, otherwise false.
   */
  bool CheckPath(const std::string &path, const ModuleParamSet &paramSet) {
    std::string relative_path = GetPathRelativeToTheJSONFile(path, paramSet);
    if ((access(relative_path.c_str(), R_OK)) == -1) {
      return false;
    }
    return true;
  }
  /**
   * @brief Check if the parameters are number and the value is in the correct range.
   *
   * @param check_list, A list of parameter name
   * @param paramSet, The module parameters
   * @param err_msg, The error message
   * @param greater_than_zero, If it is "true" presents the parameter should be
   * greater or equal than zero, otherwise less than zero.
   *
   * @return Returns true if the parameters are number and the value is in the correct range, otherwise false.
   */
  bool IsNum(const std::list<std::string> &check_list, const ModuleParamSet &paramSet, std::string &err_msg, // NOLINT
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
