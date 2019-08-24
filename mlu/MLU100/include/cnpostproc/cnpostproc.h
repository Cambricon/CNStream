/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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
#ifndef _POSTPROC_H_
#define _POSTPROC_H_

#include <string>
#include <utility>
#include <vector>
#include "cnbase/cnshape.h"
#include "cnbase/cntypes.h"
#include "cnbase/streamlibs_error.h"
#include "cnbase/reflex_object.h"

namespace libstream {

STREAMLIBS_REGISTER_EXCEPTION(CnPostproc);

class CnPostproc {
 public:
  virtual ~CnPostproc() {}
  static CnPostproc* Create(const std::string &proc_name);

  void set_batch_index(const int batch_index);
  void set_threshold(const float threshold);
  /*********************************************************
   * @brief post proc
   * @param
   *   net_outputs[in]: net_outputs[index].first is the neuron
   *   network's output. net_outputs[index].second is length.
   *********************************************************/
  std::vector<CnDetectObject> Execute(
      const std::vector<std::pair<float*, uint64_t>>& net_outputs);

 protected:
  /*********************************************************
   * @brief called by Execute
   *********************************************************/
  virtual std::vector<CnDetectObject> Postproc(
      const std::vector<std::pair<float*, uint64_t>>& net_outputs) = 0;

  /***************************************
   * @brief called by Execute.
   * if false is reutrned, the corresponding object is
   * discarded by Execute.
   ***************************************/
  virtual bool CheckInvalidObject(const CnDetectObject& obj);

  uint32_t batch_index_ = 0;
  float threshold_ = 0;
};  // class CnPostproc

class ClassificationPostproc : public CnPostproc,
  virtual public ReflexObjectEx<CnPostproc> {
  DECLARE_REFLEX_OBJECT_EX(ClassificationPostproc, CnPostproc)
 protected:
  std::vector<CnDetectObject> Postproc(
      const std::vector<std::pair<float*, uint64_t>>& net_outputs);
};  // class ClassificationPostproc

class SsdPostproc : public CnPostproc,
  virtual public ReflexObjectEx<CnPostproc> {
  DECLARE_REFLEX_OBJECT_EX(SsdPostproc, CnPostproc)
 protected:
  std::vector<CnDetectObject> Postproc(
      const std::vector<std::pair<float*, uint64_t>>& net_outputs);
};  // class SsdPostproc

class FasterrcnnPostproc : public CnPostproc,
  virtual public ReflexObjectEx<CnPostproc> {
  DECLARE_REFLEX_OBJECT_EX(FasterrcnnPostproc, CnPostproc)
 protected:
  std::vector<CnDetectObject> Postproc(
      const std::vector<std::pair<float*, uint64_t>>& net_outputs);
};  // class FasterrcnnPostproc

class Yolov3Postproc : public CnPostproc,
  virtual public ReflexObjectEx<CnPostproc> {
  DECLARE_REFLEX_OBJECT_EX(Yolov3Postproc, CnPostproc)
 public:
  inline void set_padl_ratio(float ratio) {
    padl_ratio_ = ratio;
  }
  inline void set_padb_ratio(float ratio) {
    padb_ratio_ = ratio;
  }
  inline void set_padr_ratio(float ratio) {
    padr_ratio_ = ratio;
  }
  inline void set_padt_ratio(float ratio) {
    padt_ratio_ = ratio;
  }
  inline float padl_ratio() const {
    return padl_ratio_;
  }
  inline float padb_ratio() const {
    return padb_ratio_;
  }
  inline float padr_ratio() const {
    return padr_ratio_;
  }
  inline float padt_ratio() const {
    return padt_ratio_;
  }
 protected:
  std::vector<CnDetectObject> Postproc(
      const std::vector<std::pair<float*, uint64_t>>& net_outputs);
 private:
  /*******************************************************************
   * padl_ratio_: left pad / width in preprocessing.
   * padb_ratio_: bottom pad / width in preprocessing.
   * padr_ratio_: right pad / width in preprocrssing.
   * padt_ratio_: top pad / width in preprocessing.
   *******************************************************************/
  float padl_ratio_ = 0, padb_ratio_ = 0, padr_ratio_ = 0, padt_ratio_ = 0;
};  // class Yolov3Postproc

}  // namespace libstream

#endif  // ifndef _POSTPROC_H_
