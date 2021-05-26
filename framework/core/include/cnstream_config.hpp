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
#include <unistd.h>
#include <string.h>

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"

namespace cnstream {

static constexpr char kPROFILER_CONFIG_NAME[] = "profiler_config";

struct ProfilerConfig {
  bool enable_profiling = false;
  bool enable_tracing = false;
  size_t trace_event_capacity = 100000;

  /**
   * Parses members from JSON string.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string &jstr);

  /**
   * Parses members from JSON file.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONFile(const std::string &jfname);
};  // struct ProfilerConfig

/// Module parameter set.
using ModuleParamSet = std::unordered_map<std::string, std::string>;

#define CNS_JSON_DIR_PARAM_NAME "json_file_dir"
/**
 * @brief The configuration parameters of a module.
 *
 * You can use ``CNModuleConfig`` to add modules in a pipeline.
 * The module configuration can be in JSON file.
 *
 * @code
 * "name(CNModuleConfig::name)": {
 *   custom_params(CNModuleConfig::parameters): {
 *     "key0": "value",
 *     "key1": "value",
 *     ...
 *   }
 *  "parallelism(CNModuleConfig::parallelism)": 3,
 *  "max_input_queue_size(CNModuleConfig::maxInputQueueSize)": 20,
 *  "class_name(CNModuleConfig::className)": "Inferencer",
 *  "next_modules": ["module0(CNModuleConfig::name)", "module1(CNModuleConfig::name)", ...],
 * }
 * @endcode
 *
 * @see Pipeline::AddModuleConfig.
 */
struct CNModuleConfig {
  std::string name;  ///< The name of the module.
  std::unordered_map<std::string, std::string>
      parameters;   ///< The key-value pairs. The pipeline passes this value to the CNModuleConfig::name module.
  int parallelism;  ///< Module parallelism. It is equal to module thread number and the data queue for input data.
  int maxInputQueueSize;          ///< The maximum size of the input data queues.
  std::string className;          ///< The class name of the module.
  std::vector<std::string> next;  ///< The name of the downstream modules.

  /**
   * Parses members from JSON string except CNModuleConfig::name.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONStr(const std::string &jstr);

  /**
   * Parses members from JSON file except CNModuleConfig::name.
   *
   * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
   */
  bool ParseByJSONFile(const std::string &jfname);
};

/**
 * Parses pipeline configs from json-config-file.
 *
 * @return Returns true if the JSON file has been parsed successfully. Otherwise, returns false.
 */
bool ConfigsFromJsonFile(const std::string &config_file,
                         std::vector<CNModuleConfig> *pmodule_configs,
                         ProfilerConfig *pprofiler_config);

/**
 * @brief ParamRegister
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
   * @param key The parameter name.
   * @param desc The description of the paramter.
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
std::string GetPathRelativeToTheJSONFile(const std::string &path, const ModuleParamSet &param_set);

/**
 * @brief Checks the module parameters.
 */
class ParametersChecker {
 public:
  /**
   * @brief Checks if the path exists.
   *
   * @param path The path relative to JSON file or an absolute path.
   * @param paramSet The module parameters. The JSON file path is one of the parameters.
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
