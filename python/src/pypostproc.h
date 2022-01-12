/*************************************************************************
 * copyright (c) [2021] by cambricon, inc. all rights reserved
 *
 *  licensed under the apache license, version 2.0 (the "license");
 *  you may not use this file except in compliance with the license.
 *  you may obtain a copy of the license at
 *
 *     http://www.apache.org/licenses/license-2.0
 *
 * the above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the software.
 * the software is provided "as is", without warranty of any kind, express
 * or implied, including but not limited to the warranties of merchantability,
 * fitness for a particular purpose and noninfringement. in no event shall
 * the authors or copyright holders be liable for any claim, damages or other
 * liability, whether in an action of contract, tort or otherwise, arising from,
 * out of or in connection with the software or the use or other dealings in
 * the software.
 *************************************************************************/

#include <postproc.hpp>
#include <pybind11/pybind11.h>

#include <memory>
#include <string>
#include <map>
#include <utility>
#include <vector>

namespace cnstream {

struct __attribute__((visibility("default"))) PostprocPyObjects {
  bool Init(const std::map<std::string, std::string> &params);
  ~PostprocPyObjects();
  std::string pyclass_name;
  pybind11::object pyinstance;
  pybind11::object pyinit;
  pybind11::object pyexecute;
  std::map<std::string, std::string> params;
};  // struct PostprocPyObjects

class __attribute__((visibility("default"))) PyPostproc : public Postproc {
 public:
  bool Init(const std::map<std::string, std::string> &params) override {
    return pyobjs_.Init(params);
  }
  int Execute(const std::vector<float*> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const cnstream::CNFrameInfoPtr &finfo) override;
  int Execute(const std::vector<void*> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const std::vector<CNFrameInfoPtr> &finfos) override;

 private:
  PostprocPyObjects pyobjs_;
  DECLARE_REFLEX_OBJECT_EX(PyPostproc, Postproc);
};  // class PyPostproc

class __attribute__((visibility("default"))) PyObjPostproc : public ObjPostproc {
 public:
  bool Init(const std::map<std::string, std::string> &params) override {
    return pyobjs_.Init(params);
  }
  int Execute(const std::vector<float*> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const CNFrameInfoPtr &finfo, const std::shared_ptr<CNInferObject> &obj) override;
  int Execute(const std::vector<void*> &net_outputs, const std::shared_ptr<edk::ModelLoader> &model,
              const std::vector<std::pair<CNFrameInfoPtr, std::shared_ptr<CNInferObject>>> &obj_infos) override;

 private:
  PostprocPyObjects pyobjs_;
  DECLARE_REFLEX_OBJECT_EX(PyObjPostproc, ObjPostproc);
};  // class PyObjPostproc

}  // namespace cnstream

