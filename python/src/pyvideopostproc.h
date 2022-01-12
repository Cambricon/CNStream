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

#include <video_postproc.hpp>
#include <pybind11/pybind11.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cnstream {

class __attribute__((visibility("default"))) PyVideoPostproc : public VideoPostproc {
 public:
  ~PyVideoPostproc();
  bool Init(const std::unordered_map<std::string, std::string> &params) override;
  bool Execute(infer_server::InferData *output_data, const infer_server::ModelIO &model_output,
               const infer_server::ModelInfo *model_info) override;
 private:
  std::string pyclass_name_;
  pybind11::object pyinstance_;
  pybind11::object pyinit_;
  pybind11::object pyexecute_;
  DECLARE_REFLEX_OBJECT_EX(PyVideoPostproc, VideoPostproc);
};  // class PyVideoPostproc

}  // namespace cnstream

