/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_DATA_SOURCE_PARAM_HPP_
#define MODULES_DATA_SOURCE_PARAM_HPP_

namespace cnstream {
/*!
 * @enum OutputType
 * @brief Enumeration variables describing the storage type of the output frame data of a module.
 */
enum class OutputType {
  OUTPUT_CPU,  /*!< CPU is the used storage type. */
  OUTPUT_MLU   /*!< MLU is the used storage type. */
};
/*!
 * @enum DecoderType
 * @brief Enumeration variables describing the decoder type used in source module.
 */
enum class DecoderType {
  DECODER_CPU,  /*!< CPU decoder is used. */
  DECODER_MLU   /*!< MLU decoder is used. */
};
/*!
 * @brief DataSourceParam is a structure for private usage.
 */
struct DataSourceParam {
  OutputType output_type_ = OutputType::OUTPUT_CPU;  /*!< The output type. The data is output to CPU or MLU. */
  size_t interval_ = 1;  /*!< The interval of outputting one frame. It outputs one frame every n (interval_) frames. */
  DecoderType decoder_type_ = DecoderType::DECODER_CPU;  /*!< The decoder type. */
  bool reuse_cndec_buf = false;  /*!< Whether to enable the mechanism to reuse MLU codec's buffers by next modules. */
  int device_id_ = -1;              /*!< The device ordinal. -1 is for CPU and >=0 is for MLU. */
  uint32_t input_buf_number_ = 2;   /*!< Input buffer's number used by MLU codec. */
  uint32_t output_buf_number_ = 3;  /*!< Output buffer's number used by MLU codec. */
  bool apply_stride_align_for_scaler_ = false;  /*!< Whether to set outputs meet the Scaler alignment requirement. */
  bool only_key_frame_ = false;                   /*!< Whether only to decode key frames. */
};
}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_PARAM_HPP_
