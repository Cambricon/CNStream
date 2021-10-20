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

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "cnstream_logging.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnstream_config.hpp"

namespace cnstream {

static inline
bool IsProfilerItem(const std::string& item_name) {
  return kProfilerConfigName == item_name;
}

static inline
std::string GetPathDir(const std::string& path) {
  auto slash_pos = path.rfind("/");
  return slash_pos == std::string::npos ? "" : path.substr(0, slash_pos) + "/";
}

bool CNConfigBase::ParseByJSONFile(const std::string& jfile) {
  std::ifstream ifs(jfile);
  if (!ifs.is_open()) {
    LOGE(CORE) << "Config file open failed :" << jfile;
    return false;
  }
  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();
  config_root_dir = GetPathDir(jfile);
  if (!ParseByJSONStr(jstr)) {
    return false;
  }
  return true;
}

bool ProfilerConfig::ParseByJSONStr(const std::string& jstr) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOGE(CORE) << "Parse profiler configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << jstr;
    return false;
  }

  for (rapidjson::Document::ConstMemberIterator iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
    if ("enable_profiling" == iter->name) {
      if (iter->value.IsBool()) {
        this->enable_profiling = iter->value.GetBool();
      } else {
        LOGE(CORE) << "enable_profiling must be boolean type.";
        return false;
      }
    } else if ("enable_tracing"  == iter->name) {
      if (iter->value.IsBool()) {
        this->enable_tracing = iter->value.GetBool();
      } else {
        LOGE(CORE) << "enable_tracing must be boolean type.";
        return false;
      }
    } else if ("trace_event_capacity" == iter->name) {
      if (iter->value.IsUint64()) {
        this->trace_event_capacity = iter->value.GetUint64();
      } else {
        LOGE(CORE) << "trace_event_capacity must be uint64 type.";
        return false;
      }
    } else {
      LOGE(CORE) << "Unknown parameter named [" << iter->name.GetString() << "] for profiler_config.";
      return false;
    }
  }

  return true;
}

bool CNModuleConfig::ParseByJSONStr(const std::string& jstr) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOGE(CORE) << "Parse module configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << jstr;
    return false;
  }

  /* get members */
  const auto end = doc.MemberEnd();

  // className
  if (end == doc.FindMember("class_name")) {
    LOGE(CORE) << "Module has to have a class_name.";
    return false;
  } else {
    if (!doc["class_name"].IsString()) {
      LOGE(CORE) << "class_name must be string type.";
      return false;
    }
    this->className = doc["class_name"].GetString();
  }

  // parallelism
  if (end != doc.FindMember("parallelism")) {
    if (!doc["parallelism"].IsUint()) {
      LOGE(CORE) << "parallelism must be uint type.";
      return false;
    }
    this->parallelism = doc["parallelism"].GetUint();
  } else {
    this->parallelism = 1;
  }

  // maxInputQueueSize
  if (end != doc.FindMember("max_input_queue_size")) {
    if (!doc["max_input_queue_size"].IsUint()) {
      LOGE(CORE) << "max_input_queue_size must be uint type.";
      return false;
    }
    this->maxInputQueueSize = doc["max_input_queue_size"].GetUint();
  } else {
    this->maxInputQueueSize = 20;
  }

  // next
  if (end != doc.FindMember("next_modules")) {
    if (!doc["next_modules"].IsArray()) {
      LOGE(CORE) << "next_modules must be array type.";
      return false;
    }
    auto values = doc["next_modules"].GetArray();
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
      if (!iter->IsString()) {
        LOGE(CORE) << "next_modules must be an array of strings.";
        return false;
      }
      this->next.insert(iter->GetString());
    }
  } else {
    this->next = {};
  }

  // custom parameters
  if (end != doc.FindMember("custom_params")) {
    rapidjson::Value& custom_params = doc["custom_params"];
    if (!custom_params.IsObject()) {
      LOGE(CORE) << "custom_params must be an object.";
      return false;
    }
    this->parameters.clear();
    for (auto iter = custom_params.MemberBegin(); iter != custom_params.MemberEnd(); ++iter) {
      std::string value;
      if (!iter->value.IsString()) {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
        iter->value.Accept(jwriter);
        value = sbuf.GetString();
      } else {
        value = iter->value.GetString();
      }
      this->parameters.insert(std::make_pair(iter->name.GetString(), value));
    }

    if (this->parameters.end() != this->parameters.find(CNS_JSON_DIR_PARAM_NAME)) {
      LOGW(CORE) << "Parameter [" << CNS_JSON_DIR_PARAM_NAME << "] does not take effect. It is set up by "
                  << "cnstream as the directory where the configuration file is located and passed to the module.";
    }

    this->parameters[CNS_JSON_DIR_PARAM_NAME] = config_root_dir;
  } else {
    this->parameters = {};
  }
  return true;
}

bool CNSubgraphConfig::ParseByJSONStr(const std::string& jstr) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOGE(CORE) << "Parse subgraph configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << jstr;
    return false;
  }

  /* get members */
  const auto end = doc.MemberEnd();

  // config_path
  if (end == doc.FindMember("config_path")) {
    LOGE(CORE) << "Subgraph has to have a config_path.";
    return false;
  } else {
    if (!doc["config_path"].IsString()) {
      LOGE(CORE) << "config_path must be string type.";
      return false;
    }
    this->config_path = config_root_dir + doc["config_path"].GetString();
  }

  // next
  if (end != doc.FindMember("next_modules")) {
    if (!doc["next_modules"].IsArray()) {
      LOGE(CORE) << "next_modules must be array type.";
      return false;
    }
    auto values = doc["next_modules"].GetArray();
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
      if (!iter->IsString()) {
        LOGE(CORE) << "next_modules must be an array of strings.";
        return false;
      }
      this->next.insert(iter->GetString());  // De-duplication
    }
  } else {
    this->next = {};
  }

  return true;
}

bool CNGraphConfig::ParseByJSONStr(const std::string& json_str) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(json_str.c_str()).HasParseError()) {
    LOGE(CORE) << "Parse graph configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. ";
    return false;
  }

  // traversing config items
  for (rapidjson::Document::ConstMemberIterator iter = doc.MemberBegin();
      iter != doc.MemberEnd(); ++iter) {
    rapidjson::StringBuffer sbuf;
    rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
    iter->value.Accept(jwriter);

    std::string item_name = iter->name.GetString();
    std::string item_value = sbuf.GetString();

    if (IsProfilerItem(item_name)) {
      // parse if profiler config
      if (!profiler_config.ParseByJSONStr(item_value)) {
        LOGE(CORE) << "Parse profiler config failed.";
        return false;
      }
    } else if (IsSubgraphItem(item_name)) {
      // parse if subgraph config
      CNSubgraphConfig subgraph_config;
      subgraph_config.name = item_name;
      if (!subgraph_config.ParseByJSONStr(item_value)) {
        LOGE(CORE) << "Parse subgraph config failed. Subgraph name : [" + item_name + "].";
        return false;
      }
      // correct the relative path of subgraph configuration file.
      subgraph_config.config_path = config_root_dir + subgraph_config.config_path;
      subgraph_configs.push_back(std::move(subgraph_config));
    } else {
      // parse module config and insert graph nodes
      CNModuleConfig mconf;
      mconf.config_root_dir = config_root_dir;
      mconf.name = item_name;
      if (!mconf.ParseByJSONStr(item_value)) {
        LOGE(CORE) << "Parse module config failed. Module name : [" << mconf.name << "]";
        return false;
      }

      module_configs.push_back(std::move(mconf));
    }
  }  // for json items
  return true;
}

std::string GetPathRelativeToTheJSONFile(const std::string& path, const ModuleParamSet& param_set) {
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

}  // namespace cnstream
