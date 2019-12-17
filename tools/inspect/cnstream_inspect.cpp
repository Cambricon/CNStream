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

static void PrintAllModulesDesc() {
  std::vector<std::string> modules = cnstream::ModuleFactory::Instance()->GetRegisted();
  cnstream::ModuleCreatorWorker creator;
  std::cout << std::left << std::setw(40) << "Module Name"
            << "Description" << std::endl;
  for (auto& it : modules) {
    cnstream::Module* module = creator.Create(it, it);
    std::cout << std::left << std::setw(40) << it << module->param_register_.GetModuleDesc() << std::endl;
    delete module;
  }
}

static void PrintModuleParameters(const std::string& module_name) {
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
  std::cout << module_name << " Details:" << std::endl;
  std::cout << "  " << std::left << std::setw(40) << "Parameter"
            << "Description" << std::endl;
  for (auto& it : module_params) {
    std::cout << "  " << std::left << std::setw(40) << it.first << it.second << std::endl;
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
    try {
      rapidjson::StringBuffer sbuf;
      rapidjson::Writer<rapidjson::StringBuffer> jwriter(sbuf);
      iter->value.Accept(jwriter);
      mconf.ParseByJSONStr(std::string(sbuf.GetString()));

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
    } catch (std::string e) {
      std::string err_str = "Check module config failed. Module name : [" + mconf.name + "]" + ". Error message: " + e;
      std::cout << err_str << std::endl;
      return;
    }
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
  return 0;
}
