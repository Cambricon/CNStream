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

#ifndef CNSTREAM_PARAM_HPP_
#define CNSTREAM_PARAM_HPP_

/**
 * \file cnstream_param.hpp
 *
 * This file is for module parameter registration , parsing and checking.
 */
#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <map>
#include <vector>

#include "cnstream_config.hpp"
#include "cnstream_logging.hpp"

#define OFFSET(S, M) (size_t) & (((S*)0)->M)  // NOLINT
#define PARAM_OPTIONAL 0
#define PARAM_REQUIRED 1
#define PARAM_DEPRECATED 2

namespace cnstream {

/**
 * @struct ModuleParamDesc
 *
 * @brief The ModuleParamDesc is a structure describing a parameter.
 */
typedef struct ModuleParamDesc {
  std::string name;          /*!< The name of this parameter. */
  std::string default_value; /*!< The default value of this parameter. */
  std::string str_desc;      /*!< The description of this parameter. */
  int optional;              /*!< Does the user have to set this parameter. */
  int offset;                /*!< This Parameter offset relative to structure. */
  std::function<bool(const ModuleParamSet&, const std::string&, const std::string&, void*)>
      parser;                                                                /*!< How to parse this parameter. */
  std::string type;                                                          /*!< This Parameter`s type . */
} ModuleParamDesc;

/**
 * @class ModuleParamParser
 *
 * @brief Some built-in parameter parse functions.
 *
 * @param[in] T Parameter type。
 */
template <typename T>
class ModuleParamParser {
 public:
  static bool Parser(const ModuleParamSet& param_set, const std::string& param_name, const std::string& str,
                     void* result) {
    try {
      std::istringstream ss(str);
      ss >> *static_cast<T*>(result);
      return true;
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
    }
    return false;
  }

  static bool VectorParser(const ModuleParamSet& param_set, const std::string& param_name, const std::string& str,
                           void* result) {
    std::vector<std::string> strs = cnstream::StringSplit(str, ',');
    std::vector<T> values;
    try {
      for (auto& s : strs) {
        T value;
        std::istringstream ss(s);
        ss >> *static_cast<T*>(&value);
        values.push_back(value);
      }
      *static_cast<std::vector<T>*>(result) = values;
      return true;
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
    }
    return false;
  }
};

template <>
class ModuleParamParser<int> {
 public:
  static bool Parser(const ModuleParamSet& param_set, const std::string& param_name, const std::string& str,
                     void* result) {
    std::string not_const_str = str;
    std::remove_if(not_const_str.begin(), not_const_str.end(), ::isblank);
    try {
      *static_cast<int*>(result) = std::stoi(not_const_str);
      return true;
    } catch (const std::exception& e) {
      LOGE(CORE) << "[ModuleParamParser] : Int Parser wrong param : " << param_name << ": " << not_const_str;
    }
    return false;
  }
};
template <>
class ModuleParamParser<float> {
 public:
  static bool Parser(const ModuleParamSet& param_set, const std::string& param_name, const std::string& str,
                     void* result) {
    std::string not_const_str = str;
    std::remove_if(not_const_str.begin(), not_const_str.end(), ::isblank);
    try {
      *static_cast<float*>(result) = std::stof(not_const_str);
      return true;
    } catch (const std::exception& e) {
      LOGE(CORE) << "[ModuleParamParser] : Float Parser wrong param : " << param_name << ": " << not_const_str;
    }
    return false;
  }
};
template <>
class ModuleParamParser<double> {
 public:
  static bool Parser(const ModuleParamSet& param_set, const std::string& param_name, const std::string& str,
                     void* result) {
    std::string not_const_str = str;
    std::remove_if(not_const_str.begin(), not_const_str.end(), ::isblank);
    try {
      *static_cast<double*>(result) = std::stod(not_const_str);
      return true;
    } catch (const std::exception& e) {
      LOGE(CORE) << "[ModuleParamParser] : Double Parser wrong param : " << param_name << ": " << not_const_str;
    }
    return false;
  }
};
template <>
class ModuleParamParser<bool> {
 public:
  static bool Parser(const ModuleParamSet& param_set, const std::string& param_name, const std::string& str,
                     void* result) {
    static std::vector<std::string> true_vec = {"True", "TRUE", "true", "1"};
    static std::vector<std::string> false_vec = {"False", "FALSE", "false", "0"};
    std::string not_const_str = str;
    std::remove_if(not_const_str.begin(), not_const_str.end(), ::isblank);
    for (auto& it : true_vec) {
      if (it == not_const_str) {
        *static_cast<bool*>(result) = true;
        return true;
      }
    }
    for (auto& it : false_vec) {
      if (it == not_const_str) {
        *static_cast<bool*>(result) = false;
        return true;
      }
    }
    LOGE(CORE) << "[ModuleParamParser] : Bool Parser wrong param : " << param_name << ": " << not_const_str;
    return false;
  }
};

/**
 * @class ModuleParamsHelper
 *
 * @brief ModuleParamsHelper used to manage module parameters.
 *
 * @param[in] T Structure that module`s parameters.
 */
template <class T>
class ModuleParamsHelper {
 public:
  /**
   * @brief A constructor to construct one module parameters helper.
   *
   * @param[in] name The name of the module parameters helper.
   *
   * @return None.
   */
  explicit ModuleParamsHelper(const std::string& name) : module_name_(name) {}
  ModuleParamsHelper(const ModuleParamsHelper&) = delete;
  ModuleParamsHelper& operator=(const ModuleParamsHelper&) = delete;
  /**
   * @brief Gets the module's parameters.
   *
   * @param None.
   *
   * @return Returns the module's parameters.
   */
  const T& GetParams() const noexcept {
    if (!init_) {
      LOGW(CORE) << "module param not init.";
    }
    return params_;
  }
  /**
   * @brief Register a series of parameters.
   *
   * @param[in] params_desc A series description of parameters.
   *
   * @return Returns true if parameters have register successfully. Returns false if parameters
   *         register failed.
   */
  bool Register(const std::vector<ModuleParamDesc>& params_desc, ParamRegister* param_register = nullptr) {
    if (param_register) {
      param_register_ = param_register;
    }
    for (auto& it : params_desc) {
      if (!Register(it)) {
        LOGE(CORE) << "Parameter [ " << it.name << " ] Register failed. ";
        return false;
      }
    }
    return true;
  }
  /**
   * @brief Register a parameter.
   *
   * @param[in] params_desc A  description of parameter.
   *
   * @return Returns true if parameter has register successfully. Returns false if parameter
   *         register failed.
   */
  bool Register(const ModuleParamDesc& param_desc, ParamRegister* param_register = nullptr) {
    if (param_register) {
      param_register_ = param_register;
    }

    std::shared_ptr<ModuleParamDesc> p_desc = std::make_shared<ModuleParamDesc>();
    std::string name = param_desc.name;
    std::remove_if(name.begin(), name.end(), ::isblank);
    p_desc->name = name;
    if ("" == p_desc->name) {
      LOGE(CORE) << "[ModuleParam] : empty parameter name, register failed.";
      return false;
    }
    p_desc->default_value = param_desc.default_value;
    p_desc->optional = param_desc.optional;
    if (p_desc->optional != PARAM_DEPRECATED) {
      p_desc->offset = param_desc.offset;
      p_desc->type = param_desc.type;
      if (nullptr == param_desc.parser) {
        LOGE(CORE) << "[ModuleParam] : register " << param_desc.name << " failed, "
                   << " you should set default parser or custom parser";
        return false;
      } else {
        p_desc->parser = param_desc.parser;
      }
    }
    p_desc->str_desc = param_desc.str_desc;
    params_desc_[name] = p_desc;
    if (param_register_ && p_desc->optional != PARAM_DEPRECATED) {
      IRegisterParam(*p_desc.get());
    }
    registered_ = true;
    return true;
  }
  /**
   * @brief Parse parameters.
   *
   * @param[in] params A map contains parameter names and user set parameter values.
   *
   * @return Returns true if parameters have parse successfully. Returns false if parameters
   *         parse failed.
   */
  bool ParseParams(const std::map<std::string, std::string>& params) {
    if (!registered_) {
      LOGE(CORE) << "[ModuleParam] : register failed, ";
      return false;
    }

    std::map<std::string, std::string> map = params;
    for (auto& it : params_desc_) {
      if (it.second->optional == PARAM_DEPRECATED) {
        continue;
      }
      auto iter = map.find(it.first);
      if (map.end() == iter && it.second->optional == PARAM_REQUIRED) {
        LOGE(CORE) << "[ModuleParam]:not find " << it.first << " , parser failed, "
                   << "you must set this parameter!";
        return false;
      }
      std::string str_value;
      std::string str_param = it.first;
      if (it.second->optional == PARAM_OPTIONAL && map.end() == iter) {
        str_value = it.second->default_value;
      } else {
        str_value = iter->second;
      }
      if (!it.second->parser(params, str_param, str_value, ((char*)&params_ + it.second->offset))) {  // NOLINT
        LOGE(CORE) << "[MOduleParam]: parse parameter failed. param: " << str_param << " val: " <<  str_value;
        return false;
      }
    }
    for (auto& it : params_desc_) {
      auto iter = map.find(it.first);
      if (iter != map.end()) {
        if (it.second->optional == PARAM_DEPRECATED) {
          LOGW(CORE) << "[ModuleParam]: " << it.first << " is a deprecated parameter. " << it.second->str_desc;
        }
        map.erase(it.first);
      }
    }
    bool flag = true;
    for (auto& it : map) {
      if (CNS_JSON_DIR_PARAM_NAME != it.first) {
        LOGE(CORE) << "[ModuleParam]: not registed this param:[" << it.first << "]:[" << it.second << "]";
        flag = false;
      }
    }
    init_ = true;
    return flag;
  }

void SetRegister(ParamRegister *param_register) {
  if (!param_register) {
    LOGE(CORE) << "[ModuleParam] set param register failed";
    return;
  }
  param_register_ = param_register;
}

bool IRegisterParam(const ModuleParamDesc &param_desc) {
  if (param_register_) {
      std::string desc = param_desc.str_desc + " --- "
                   + "type : [" + param_desc.type  + "] --- "
                   + "default value : [" + param_desc.default_value + "]";
      param_register_->Register(param_desc.name, desc);
      return true;
  }
  LOGE(CORE) << "you should call SetRegister before registing parameters.";
  return false;
}

 private:
  std::atomic<bool> init_{false};
  std::atomic<bool> registered_{false};
  std::string module_name_;
  using ParamsDesc = std::map<std::string, std::shared_ptr<ModuleParamDesc>>;
  ParamsDesc params_desc_;
  T params_;
  ParamRegister *param_register_ = nullptr;
};  // class ModuleParamHelper

}  // namespace cnstream

#endif  // !CNSTREAM_PARAM_HPP_
