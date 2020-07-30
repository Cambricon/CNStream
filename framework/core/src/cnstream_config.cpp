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

bool CNModuleConfig::ParseByJSONStr(const std::string& jstr) {
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOG(ERROR) << "Parse module configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. JSON:" << jstr;
    return false;
  }

  /* get members */
  const auto end = doc.MemberEnd();

  // className
  if (end == doc.FindMember("class_name")) {
    LOG(ERROR) << "Module has to have a class_name.";
    return false;
  } else {
    if (!doc["class_name"].IsString()) {
      LOG(ERROR) << "class_name must be string type.";
      return false;
    }
    this->className = doc["class_name"].GetString();
  }

  // parallelism
  if (end != doc.FindMember("parallelism")) {
    if (!doc["parallelism"].IsUint()) {
      LOG(ERROR) << "parallelism must be uint type.";
      return false;
    }
    this->parallelism = doc["parallelism"].GetUint();
    if (this->className != "cnstream::DataSource" && this->className != "cnstream::TestDataSource" &&
        this->className != "cnstream::ModuleIPC" && this->parallelism < 1) {
      LOG(ERROR) << "parallelism must be larger than 0, when class name is " << this->className;
      return false;
    }
  } else {
    this->parallelism = 1;
  }

  // maxInputQueueSize
  if (end != doc.FindMember("max_input_queue_size")) {
    if (!doc["max_input_queue_size"].IsUint()) {
      LOG(ERROR) << "max_input_queue_size must be uint type.";
      return false;
    }
    this->maxInputQueueSize = doc["max_input_queue_size"].GetUint();
  } else {
    this->maxInputQueueSize = 20;
  }

  // enablePerfInfo
  if (end != doc.FindMember("show_perf_info")) {
    if (!doc["show_perf_info"].IsBool()) {
      LOG(ERROR) << "show_perf_info must be Boolean type.";
      return false;
    }
    this->showPerfInfo = doc["show_perf_info"].GetBool();
  } else {
    this->showPerfInfo = false;
  }

  // next
  if (end != doc.FindMember("next_modules")) {
    if (!doc["next_modules"].IsArray()) {
      LOG(ERROR) << "next_modules must be array type.";
      return false;
    }
    auto values = doc["next_modules"].GetArray();
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
      if (!iter->IsString()) {
        LOG(ERROR) << "next_modules must be an array of strings.";
        return false;
      }
      this->next.push_back(iter->GetString());
    }
  } else {
    this->next = {};
  }

  // custom parameters
  if (end != doc.FindMember("custom_params")) {
    rapidjson::Value& custom_params = doc["custom_params"];
    if (!custom_params.IsObject()) {
      LOG(ERROR) << "custom_params must be an object.";
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
  } else {
    this->parameters = {};
  }
  return true;
}

bool CNModuleConfig::ParseByJSONFile(const std::string& jfname) {
  std::ifstream ifs(jfname);

  if (!ifs.is_open()) {
    LOG(ERROR) << "File open failed :" << jfname;
    return false;
  }

  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  if (!ParseByJSONStr(jstr)) {
    return false;
  }

  /***************************************************
   * add config file path to custom parameters
   ***************************************************/

  std::string jf_dir = "";
  auto slash_pos = jfname.rfind("/");
  if (slash_pos == std::string::npos) {
    jf_dir = ".";
  } else {
    jf_dir = jfname.substr(0, slash_pos);
  }
  jf_dir += '/';

  if (this->parameters.end() != this->parameters.find(CNS_JSON_DIR_PARAM_NAME)) {
    LOG(WARNING) << "Parameter [" << CNS_JSON_DIR_PARAM_NAME << "] does not take effect. It is set "
                 << "up by cnstream as the directory where the configuration file is located and passed to the module.";
  }

  this->parameters[CNS_JSON_DIR_PARAM_NAME] = jf_dir;
  return true;
}

bool ConfigsFromJsonFile(const std::string& config_file, std::vector<CNModuleConfig>& configs) {  // NOLINT
  std::ifstream ifs(config_file);
  if (!ifs.is_open()) {
    LOG(ERROR) << "Failed to open file: " << config_file;
    return false;
  }

  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  /* traversing modules */
  std::vector<std::string> namelist;
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    LOG(ERROR) << "Parse pipeline configuration failed. Error code [" << std::to_string(doc.GetParseError()) << "]"
               << " Offset [" << std::to_string(doc.GetErrorOffset()) << "]. ";
    return false;
  }

  for (rapidjson::Document::ConstMemberIterator iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
    CNModuleConfig mconf;
    mconf.name = iter->name.GetString();
    if (find(namelist.begin(), namelist.end(), mconf.name) != namelist.end()) {
      LOG(ERROR) << "Module name should be unique in Jason file. Module name : [" << mconf.name + "]"
                 << " appeared more than one time.";
      return false;
    }
    namelist.push_back(mconf.name);

    rapidjson::StringBuffer sbuf;
    rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
    iter->value.Accept(jwriter);

    if (!mconf.ParseByJSONStr(std::string(sbuf.GetString()))) {
      LOG(ERROR) << "Parse module config failed. Module name : [" << mconf.name << "]";
      return false;
    }

    /***************************************************
     * add config file path to custom parameters
     ***************************************************/

    std::string jf_dir = "";
    auto slash_pos = config_file.rfind("/");
    if (slash_pos == std::string::npos) {
      jf_dir = ".";
    } else {
      jf_dir = config_file.substr(0, slash_pos);
    }
    jf_dir += '/';

    if (mconf.parameters.end() != mconf.parameters.find(CNS_JSON_DIR_PARAM_NAME)) {
      LOG(WARNING)
          << "Parameter [" << CNS_JSON_DIR_PARAM_NAME << "] does not take effect. It is set "
          << "up by cnstream as the directory where the configuration file is located and passed to the module.";
    }

    mconf.parameters[CNS_JSON_DIR_PARAM_NAME] = jf_dir;
    configs.push_back(mconf);
  }
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
