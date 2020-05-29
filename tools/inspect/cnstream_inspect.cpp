/*************************************************************************
 *  Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include <glog/logging.h>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_version.hpp"

static void Usage() {
  std::cout << "Usage:" << std::endl;
  std::cout << "\t inspect-tool [OPTION...] [MODULE-NAME]" << std::endl;
  std::cout << "Options: " << std::endl;
  std::cout << std::left << std::setw(40) << "\t -h, --help"
            << "Show usage" << std::endl;
  std::cout << std::left << std::setw(40) << "\t -a, --all"
            << "Print all modules" << std::endl;
  std::cout << std::left << std::setw(40) << "\t -m, --module-name"
            << "List the module parameters" << std::endl;
  std::cout << std::left << std::setw(40) << "\t -c, --check"
            << "Check the config file" << std::endl;
  std::cout << std::left << std::setw(40) << "\t -v, --version"
            << "Print version information\n"
            << std::endl;
}

static const struct option long_option[] = {{"help", no_argument, nullptr, 'h'},
                                            {"all", no_argument, nullptr, 'a'},
                                            {"module-name", required_argument, nullptr, 'm'},
                                            {"check", required_argument, nullptr, 'c'},
                                            {"version", no_argument, nullptr, 'v'},
                                            {nullptr, 0, nullptr, 0}};

static void PrintVersion() {
  std::cout << "CNStream: " << cnstream::VersionString() << std::endl;
  return;
}

static uint32_t GetFirstLetterPos(std::string desc, uint32_t begin, uint32_t length) {
  if (begin + length > desc.length()) {
    length = desc.length() - begin;
  }
  for (uint32_t i = 0; i < length; i++) {
    if (desc.substr(begin + i, 1) != " ") {
      return begin + i;
    }
  }
  return begin;
}

static uint32_t GetLastSpacePos(std::string desc, uint32_t end, uint32_t length) {
  if (end > desc.length()) {
    end = desc.length();
  }
  if (end - length < 0) {
    length = end;
  }
  for (uint32_t i = 0; i < length; i++) {
    if (desc.substr(end - i, 1) == " ") {
      return end - i;
    }
  }
  return end;
}

static uint32_t GetSubStrEnd(std::string desc, uint32_t begin, uint32_t sub_str_len) {
  if (begin + sub_str_len < desc.length()) {
    return GetLastSpacePos(desc, begin + sub_str_len, sub_str_len);
  } else {
    return desc.length();
  }
}

static void PrintDesc(std::string desc, uint32_t indent, uint32_t sub_len) {
  uint32_t len = desc.length();

  uint32_t sub_begin = GetFirstLetterPos(desc, 0, sub_len);
  uint32_t sub_end = GetSubStrEnd(desc, sub_begin, sub_len);

  // std::cout << std::left << std::setw(first_width) << desc.substr(sub_begin, sub_end - sub_begin) << std::endl;
  std::cout << desc.substr(sub_begin, sub_end - sub_begin) << std::endl;

  while (sub_begin + sub_len < len) {
    sub_begin = GetFirstLetterPos(desc, sub_end, sub_len);
    sub_end = GetSubStrEnd(desc, sub_begin, sub_len);
    std::cout << std::left << std::setw(indent) << "" << desc.substr(sub_begin, sub_end - sub_begin) << std::endl;
    if (sub_end != len && sub_end + sub_len >= len) {
      sub_begin = GetFirstLetterPos(desc, sub_end, len - sub_end);
      std::cout << std::left << std::setw(indent) << "" << desc.substr(sub_begin, len - sub_begin) << std::endl;
    }
  }
}

static void PrintAllModulesDesc() {
  const uint32_t width = 40;
  const uint32_t sub_str_len = 80;

  std::vector<std::string> modules = cnstream::ModuleFactory::Instance()->GetRegisted();
  cnstream::ModuleCreatorWorker creator;

  std::cout << "\033[01;32m"<< std::left << std::setw(width) << "Module Name"
            << "Description" << "\033[01;0m" << std::endl;

  for (auto& it : modules) {
    cnstream::Module* module = creator.Create(it, it);
    std::cout << "\033[01;1m" << std::left << std::setw(width) << it << "\033[0m";

    std::string desc = module->param_register_.GetModuleDesc();

    PrintDesc(desc, width, sub_str_len);

    std::cout << std::endl;

    delete module;
  }
}

static void PrintModuleCommonParameters() {
  const uint32_t width = 30;
  const uint32_t sub_str_len = 80;

  std::cout << "\033[01;32m" << "  " << std::left << std::setw(width) << "Common Parameter"
            << "Description" << "\033[0m" << std::endl;

  std::cout << "\033[01;1m" << "  " << std::left << std::setw(width) << "class_name" << "\033[0m";
  PrintDesc("Module class name.", width + 2, sub_str_len);
  std::cout << std::endl;

  std::cout << "\033[01;1m" << "  " << std::left << std::setw(width) << "parallelism" << "\033[0m";
  PrintDesc("Module parallelism.", width + 2, sub_str_len);
  std::cout << std::endl;

  std::cout << "\033[01;1m" << "  " << std::left << std::setw(width) << "max_input_queue_size" << "\033[0m";
  PrintDesc("Max size of module input queue.", width + 2, sub_str_len);
  std::cout << std::endl;

  std::cout << "\033[01;1m" << "  " << std::left << std::setw(width) << "next_modules" << "\033[0m";
  PrintDesc("Next modules.", width + 2, sub_str_len);
  std::cout << std::endl;

  std::cout << "\033[01;1m" << "  " << std::left << std::setw(width) << "show_perf_info" << "\033[0m";
  PrintDesc("Show module performance info.", width + 2, sub_str_len);
  std::cout << std::endl;
}

static void PrintModuleParameters(const std::string& module_name) {
  const uint32_t width = 30;
  const uint32_t sub_str_len = 80;

  std::string name = module_name;
  cnstream::ModuleCreatorWorker creator;
  cnstream::Module* module = creator.Create(name, name);
  if (nullptr == module) {
    name = "cnstream::" + name;
    module = creator.Create(name, name.substr(10));
    if (nullptr == module) {
      std::cout << "No such module: '" << module_name << "'." << std::endl;
      return;
    }
  }
  auto module_params = module->param_register_.GetParams();
  std::cout <<"\033[01;33m" <<  module_name << " Details:" << "\033[0m" << std::endl;

  PrintModuleCommonParameters();

  std::cout << "\033[01;32m" << "  " << std::left << std::setw(width) << "Custom Parameter"
            << "Description" << "\033[0m" << std::endl;
  for (auto& it : module_params) {
    std::cout << "\033[01;1m" << "  " << std::left << std::setw(width) << it.first << "\033[0m";

    PrintDesc(it.second, width + 2, sub_str_len);

    std::cout << std::endl;
  }
  delete module;
}

static void CheckConfigFile(const std::string& config_file) {
  std::ifstream ifs(config_file);
  if (!ifs.is_open()) {
    std::cout << "Open file filed: " << config_file << std::endl;
    return;
  }

  std::string jstr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();

  /* traversing modules */
  std::vector<cnstream::CNModuleConfig> mconfs;
  std::vector<std::string> namelist;
  rapidjson::Document doc;
  if (doc.Parse<rapidjson::kParseCommentsFlag>(jstr.c_str()).HasParseError()) {
    std::string err_str = "Check pipeline configuration failed. Error code [" + std::to_string(doc.GetParseError()) +
                          "]" + " Offset [" + std::to_string(doc.GetErrorOffset()) + "]. ";
    std::cout << err_str << std::endl;
    return;
  }

  for (rapidjson::Document::ConstMemberIterator iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter) {
    cnstream::CNModuleConfig mconf;
    mconf.name = iter->name.GetString();
    if (find(namelist.begin(), namelist.end(), mconf.name) != namelist.end()) {
      std::string err_str = "Module name should be unique in Jason file. Module name : [" + mconf.name + "]" +
                            " appeared more than one time.";
      std::cout << err_str << std::endl;
      return;
    }
    namelist.push_back(mconf.name);

    rapidjson::StringBuffer sbuf;
    rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
    iter->value.Accept(jwriter);
    if (!mconf.ParseByJSONStr(std::string(sbuf.GetString()))) {
      std::string err_str = "Check module configuration failed, Module name : [" + mconf.name + "].";
      std::cout << err_str << std::endl;
      return;
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
      std::cout
        << "Parameter [" << CNS_JSON_DIR_PARAM_NAME << "] does not take effect. It is set "
        << "up by cnstream as the directory where the configuration file is located and passed to the module.";
      return;
    }

    mconf.parameters[CNS_JSON_DIR_PARAM_NAME] = jf_dir;

    mconfs.push_back(mconf);
  }

  cnstream::ModuleCreatorWorker creator;
  // check className
  for (auto& cfg : mconfs) {
    cnstream::Module* module = creator.Create(cfg.className, cfg.name);
    if (nullptr == module) {
      std::cout << "Check module configuration failed, Module name : [" << cfg.name << "] class_name : ["
                << cfg.className << "] non-existent ." << std::endl;
      return;
    }
    if (!module->CheckParamSet(cfg.parameters)) {
      std::cout << "Check module config file failed!" << std::endl;
      delete module;
      return;
    }
    delete module;
  }
  // check next_modules
  for (auto& cfg : mconfs) {
    for (auto& name : cfg.next) {
      if (find(namelist.begin(), namelist.end(), name) == namelist.end()) {
        std::cout << "Check module configuration failed, Module name : [" << cfg.name << "] next_modules : [" << name
                  << "] non-existent ." << std::endl;
        return;
      }
    }
  }
  std::cout << "Check module config file successfully!" << std::endl;
  return;
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  int opt = 0;
  bool getopt = false;
  std::string config_file;
  std::string module_name;
  std::stringstream ss;

  if (argc == 1) {
    PrintAllModulesDesc();
    return 0;
  }

  while ((opt = getopt_long(argc, argv, "ham:c:v", long_option, nullptr)) != -1) {
    getopt = true;
    switch (opt) {
      case 'h':
        Usage();
        break;

      case 'a':
        PrintAllModulesDesc();
        break;

      case 'm':
        ss.clear();
        ss.str("");
        ss << optarg;
        module_name = ss.str();
        PrintModuleParameters(module_name);
        break;

      case 'c':
        ss.clear();
        ss.str("");
        ss << optarg;
        config_file = ss.str();
        CheckConfigFile(config_file);
        break;

      case 'v':
        PrintVersion();
        break;

      default:
        return 0;
    }
  }

  if (!getopt) {
    for (int i = 1; i < argc; i++) {
      ss.clear();
      ss.str("");
      ss << argv[i];
      module_name = ss.str();
      PrintModuleParameters(module_name);
      std::cout << std::endl;
    }
  }
  google::ShutdownGoogleLogging();
  return 0;
}
