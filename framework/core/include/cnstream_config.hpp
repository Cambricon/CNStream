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

#ifndef CNSTREAM_CONFIG_HPP_
#define CNSTREAM_CONFIG_HPP_

/**
 * @file cnstream_config.hpp
 *
 * This file contains a declaration of the CNModuleConfig class.
 */
#include <list>
#include <memory>
#include <set>
#include <string>
#include <map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"

namespace cnstream {

/*!
 * Defines an alias for std::map<std::string, std::string>.
 * ModuleParamSet now denotes an unordered map which contains the pairs of parameter name and parameter value.
 */
using ModuleParamSet = std::map<std::string, std::string>;

// Group:Framework Function
/**
 * @brief Gets the complete path of a file.
 *
 * If the path you set is an absolute path, returns the absolute path.
 * If the path you set is a relative path, retuns the path that appends the relative path
 * to the specified JSON file path.
 *
 * @param[in] path The path relative to the JSON file or an absolute path.
 * @param[in] param_set The module parameters. The JSON file path is one of the parameters.
 *
 * @return Returns the complete path of a file.
 */
std::string GetPathRelativeToTheJSONFile(const std::string &path, const ModuleParamSet &param_set);

/**
 * @struct CNConfigBase
 *
 * @brief CNConfigBase is a base structure for configurations.
 */
struct CNConfigBase {
  std::string config_root_dir = "";   ///< The directory where a configuration file is stored.
  /**
   * @brief Parses members from a JSON file.
   *
   * @param[in] jfname JSON configuration file path.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONFile(const std::string &jfname);

  /**
   * @brief Parses members from JSON string.
   *
   * @param[in] jstr JSON string of a configuration.
   *
   * @return Returns true if the JSON string has been parsed successfully. Otherwise, returns false.
   */
  virtual bool ParseByJSONStr(const std::string &jstr) = 0;

  /**
   * @brief Destructor to destruct config base.
   *
   * @return No return value.
   */
  virtual ~CNConfigBase() {}
};  // struct CNConfigBase

/**
 * @struct ProfilerConfig
 *
 * @brief ProfilerConfig is a structure for profiler configuration.
 *
 * The profiler configuration can be a JSON file.
 *
 * @code {.json}
 * {
 *   "profiler_config" : {
 *     "enable_profiling" : true,
 *     "enable_tracing" : true
 *   }
 * }
 * @endcode
 *
 * @note It will not take effect when the profiler configuration is in the subgraph configuration.
 **/
struct ProfilerConfig : public CNConfigBase {
  bool enable_profiling = false;           ///< Whether to enable profiling.
  bool enable_tracing = false;             ///< Whether to enable tracing.
  size_t trace_event_capacity = 100000;    ///< The maximum number of cached trace events.

  /**
   * @brief Parses members from JSON string.
   *
   * @param[in] jstr JSON configuration string.
   *
   * @return Returns true if the JSON string has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string &jstr) override;
};  // struct ProfilerConfig

/**
 * @struct CNModuleConfig
 *
 * CNModuleConfig is a structure for module configuration.
 * The module configuration can be a JSON file.
 *
 * @code {.json}
 * {
 *   "name": {
 *     "parallelism": 3,
 *     "max_input_queue_size": 20,
 *     "class_name": "cnstream::Inferencer",
 *     "next_modules": ["module_name/subgraph:subgraph_name",
 *                      "module_name/subgraph:subgraph_name", ...],
 *     "custom_params" : {
 *       "param_name" : "param_value",
 *       "param_name" : "param_value",
 *       ...
 *     }
 *   }
 * }
 * @endcode
 */
struct CNModuleConfig : public CNConfigBase {
  std::string name;  ///< The name of the module.
  std::map<std::string, std::string>
      parameters;   ///< The key-value pairs. The pipeline passes this value to the CNModuleConfig::name module.
  int parallelism;  ///< Module parallelism. It is equal to module thread number or the data queue of input data.
  int maxInputQueueSize;          ///< The maximum size of the input data queues.
  std::string className;          ///< The class name of the module.
  std::set<std::string> next;     ///< The name of the downstream modules/subgraphs.

  /**
   * @brief Parses members except ``CNModuleConfig::name`` from the JSON file.
   *
   * @param[in] jstr JSON string of a configuration.
   *
   * @return Returns true if the JSON string has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string &jstr) override;
};

/**
 * @struct CNSubgraphConfig
 *
 * @brief CNSubgraphConfig is a structure for subgraph configuration.
 *
 * The subgraph configuration can be a JSON file.
 *
 * @code {.json}
 * {
 *   "subgraphs:name" : {
 *     "config_path" : "/your/path/to/config_file.json",
 *     "next_modules": ["module_name/subgraph:subgraph_name",
 *                      "module_name/subgraph:subgraph_name", ...]
 *   }
 * }
 * @endcode
 */
struct CNSubgraphConfig : public CNConfigBase {
  std::string name;              ///< The name of the subgraph.
  std::string config_path;       ///< The path of configuration file.
  std::set<std::string> next;    ///< The name of the downstream modules/subgraphs.

  /**
   * @brief Parses members except ``CNSubgraphConfig::name`` from the JSON file.
   *
   * @param[in] jstr JSON string of a configuration.
   *
   * @return Returns true if the JSON string has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string &jstr) override;
};

/**
 * @struct CNGraphConfig
 *
 * @brief CNGraphConfig is a structure for graph configuration.
 *
 * You can use ``CNGraphConfig`` to initialize a CNGraph instance.
 * The graph configuration can be a JSON file.
 *
 * @code {.json}
 * {
 *   "profiler_config" : {
 *     "enable_profiling" : true,
 *     "enable_tracing" : true
 *   },
 *   "module1": {
 *     "parallelism": 3,
 *     "max_input_queue_size": 20,
 *     "class_name": "cnstream::DataSource",
 *     "next_modules": ["subgraph:subgraph1"],
 *     "custom_params" : {
 *       "param_name" : "param_value",
 *       "param_name" : "param_value",
 *       ...
 *     }
 *   },
 *   "subgraph:subgraph1" : {
 *     "config_path" : "/your/path/to/subgraph_config_file.json"
 *   }
 * }
 * @endcode
 */
struct CNGraphConfig : public CNConfigBase {
  std::string name = "";                            ///< Graph name.
  ProfilerConfig profiler_config;                   ///< Configuration of profiler.
  std::vector<CNModuleConfig> module_configs;       ///< Configurations of modules.
  std::vector<CNSubgraphConfig> subgraph_configs;   ///< Configurations of subgraphs.

  /**
   * @brief Parses members except ``CNGraphConfig::name`` from the JSON file.
   *
   * @param[in] jstr: Json configuration string.
   *
   * @return Returns true if the JSON string has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string &jstr) override;
};  // struct GraphConfig

/**
 * @class ParamRegister
 *
 * @brief ParamRegister is a class for module parameter registration.
 *
 * Each module registers its own parameters and descriptions.
 * This is used in CNStream Inspect tool to detect parameters of each module.
 *
 */
class ParamRegister {
 private:
  std::vector<std::pair<std::string /*key*/, std::string /*desc*/>> module_params_;
  std::string module_desc_;

 public:
  /**
   * @brief Registers a paramter and its description.
   *
   * This is used in CNStream Inspect tool.
   *
   * @param[in] key The parameter name.
   * @param[in] desc The description of the paramter.
   *
   * @return Void.
   */
  void Register(const std::string &key, const std::string &desc) {
    module_params_.push_back(std::make_pair(key, desc));
  }
  /**
   * @brief Gets the registered paramters and the parameter descriptions.
   *
   * This is used in CNStream Inspect tool.
   *
   * @return Returns the registered paramters and the parameter descriptions.
   */
  std::vector<std::pair<std::string, std::string>> GetParams() { return module_params_; }
  /**
   * @brief Checks if the paramter is registered.
   *
   * This is used in CNStream Inspect tool.
   *
   * @param[in] key The parameter name.
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
   * @param[in] desc The description of the module.
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

/**
 * @class ParametersChecker
 *
 * @brief ParameterChecker is a class used to check module parameters.
 */
class ParametersChecker {
 public:
  /**
   * @brief Checks if a path exists.
   *
   * @param[in] path The path relative to JSON file or an absolute path.
   * @param[in] paramSet The module parameters. The JSON file path is one of the parameters.
   *
   * @return Returns true if the path exists. Otherwise, returns false.
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
   * @param[in] check_list A list of parameter names.
   * @param[in] paramSet The module parameters.
   * @param[out] err_msg The error message.
   * @param[in] greater_than_zero If this parameter is set to ``true``, the parameter set should be
   * greater than or equal to zero. If this parameter is set to ``false``, the parameter set is less than zero.
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

#endif  // CNSTREAM_CONFIG_HPP_
