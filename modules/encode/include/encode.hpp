/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_ENCODE_HPP_
#define MODULES_ENCODE_HPP_
/*!
 *  @file vout.hpp
 *
 *  This file contains a declaration of the VoutParam, and the Vout class.
 */
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "private/cnstream_param.hpp"
#include "cnstream_pipeline.hpp"
#include "encode_handler.hpp"
#include "tiler.hpp"

namespace cnstream {

struct VEncParam {
  int device_id = 0;                 // device id, -1 :disable mlu
  bool mlu_encoder = true;           // whether use mlu encoding, default is true
  int dst_width = 0;                 // Target width, preferred size same with input
  int dst_height = 0;                // Target height, preferred size same with input
  double frame_rate = 0;             // Target fps
  int bit_rate = 4000000;            // Target bit rate, default is 1Mbps
  int gop_size = 10;                 // Target gop, default is 10
  int tile_cols = 0;                 // Grids in horizontally of video tiling, only support cpu input
  int tile_rows = 0;                 // Grids in vertically of video tiling, only support cpu input
  bool resample = false;             // Resample frame with canvas, only support cpu input
  std::string file_name = "";        // File name to encode to
  int rtsp_port = -1;                // rtsp output port
};

class FrameRateControl;
class VEncodeImplement;

/*!
 * @class VEncode
 *
 * @brief VEncode is a class to handle picutres to be encoded.
 *
 */
class VEncode : public ModuleEx, public ModuleCreator<VEncode> {
 public:
  /*!
   * @brief Constructs a VEncode object.
   *
   * @param[in] name The name of this module.
   *
   * @return No return value.
   */
  explicit VEncode(const std::string &name);

  /*!
   * @brief Destructs a VEncode object.
   *
   * @return No return value.
   */
  ~VEncode();

  /*!
   * @brief Initializes the configuration of the Vout module.
   *
   * This function will be called by the pipeline when the pipeline starts.
   *
   * @param[in] param_set The module's parameter set to configure a Vout module.
   *
   * @return Returns true if the parammeter set is supported and valid, othersize returns false.
   */
  bool Open(ModuleParamSet param_set) override;

  /*!
   * @brief Frees the resources that the object may have acquired.
   *
   * This function will be called by the pipeline when the pipeline stops.
   *
   * @return No return value.
   */
  void Close() override;

  /**
   * @brief Process data
   *
   * @param data : data to be processed
   *
   * @return whether process succeed
   * @retval 0: succeed and do no intercept data
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

  /**
   * @brief Check ParamSet for this module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Return true if this API run successfully. Otherwise, return false.
   */
  bool CheckParamSet(const ModuleParamSet& param_set) const override;

 private:
  std::map<std::string, std::shared_ptr<FrameRateControl>> frame_rate_ctx_;
  std::map<std::string, std::shared_ptr<VEncodeImplement>> ivenc_;
  std::unique_ptr<ModuleParamsHelper<VEncParam>> param_helper_ = nullptr;
  std::mutex venc_mutex_;
  std::mutex frame_rate_mutex_;
  int stream_index_ = 0;
  std::unique_ptr<Tiler> tiler_ = nullptr;
  bool tiler_enable_ = false;
  const std::string tiler_key_name_ = "tiler";
};  // class VEncode

}  // namespace cnstream

#endif  // MODULES_ENCODE_HPP_
