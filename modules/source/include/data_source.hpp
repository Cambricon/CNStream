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

#ifndef MODULES_DATA_SOURCE_HPP_
#define MODULES_DATA_SOURCE_HPP_
/**
 *  \file data_source.hpp
 *
 *  This file contains a declaration of struct DataSource and DataSourceParam.
 */

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_source.hpp"

namespace cnstream {

/**
 *  @brief The type of stream or source to be processed.
 */
enum SourceType {
  SOURCE_RAW,     ///< raw stream, send to decode directly
  SOURCE_FFMPEG   ///< normal stream, will demux with ffmpeg before send to decode
};
/**
 * @brief The storage type of the output frame data that are stored for modules on CPU or MLU.
 */
enum OutputType {
  OUTPUT_CPU,   ///< output to cpu
  OUTPUT_MLU    ///< output to mlu
};
/**
 * @brief The decoder type used in the source module.
 */
enum DecoderType {
  DECODER_CPU,   ///< cpu decoder with ffmpeg
  DECODER_MLU    ///< mlu decoder with cncodec
};
/**
 * @brief A structure for private usage.
 */
struct DataSourceParam {
  SourceType source_type_ = SOURCE_RAW;     ///< The demuxer type. The ``SOURCE_RAW`` value is set for debugging.
  OutputType output_type_ = OUTPUT_CPU;     ///< Outputs data to CPU or MLU.
  size_t interval_ = 1;                     ///< Outputs image every ``interval`` frames.
  DecoderType decoder_type_ = DECODER_CPU;  ///< The decoder type.
  bool reuse_cndec_buf = false;             ///< Valid when ``DECODER_MLU`` is used.
  int device_id_ = -1;                      ///< The MLU device ID. To disable MLU, set the value to ``-1`` .
  size_t chunk_size_ = 0;                   ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  size_t width_ = 0;                        ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  size_t height_ = 0;                       ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  bool interlaced_ = false;                 ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  size_t output_w = 0;                      ///< Valid for MLU100.
  size_t output_h = 0;                      ///< Valid for MLU100.
  uint32_t input_buf_number_ = 2;           ///< Valid when ``decoder_type`` is set to ``DECODER_MLU``.
  uint32_t output_buf_number_ = 3;          ///< Valid when ``decoder_type`` is set to ``DECODER_MLU``.
};

/**
 * @brief The class for handling input data.
 */
class DataSource : public SourceModule, public ModuleCreator<DataSource> {
 public:
  /**
   * @brief Construct DataSource object with a given module name.
   * @param
   * 	moduleName[in]: A defined module name.
   */
  explicit DataSource(const std::string &moduleName);
  /**
   * @brief Deconstruct DataSource object.
   *
   */
  ~DataSource();

  /**
   * @brief Called by pipeline when the pipeline is started.
   
   * @param paramSet：
   * @verbatim
   * source_type: Required. The demuxer type. Supported values are ``raw`` and ``ffmpeg``.
   * output_type: Required. The output type. Supported values are ``mlu`` and ``cpu``.
   * interval: Optional. The interval during which the data is handled.
   * decoder_type : Required. The decoder type. Supported values are ``mlu`` and ``cpu``.
   * reuse_cndec_buf: Optional. This parameter should be set when MLU decoder is used. Supported values are ``true`` and ``false``.
   * device_id: Required when MLU is used. Set the value to -1 for CPU. Set the value for MLU in the range 0 - N.
   * chunk_size: Required when ``source_type`` is set to ``raw``.
   * width: Required when ``source_type`` is set to ``raw``.
   * height: Required when ``source_type`` is set to ``raw``.
   * interlaced: Required when ``source_type`` is set to ``raw``.
   * input_buf_number: Optional. The input buffer number.
   * output_buf_number: Optional. The output buffer number.
   *@endverbatim
   *
   * @return
   *    Returns true if ``paramSet`` are supported and valid. Othersize, returns false.
   */
  bool Open(ModuleParamSet paramSet) override;
  /**
   * @brief Called by pipeline when the pipeline is stopped.
   */
  void Close() override;

  /**
   * @brief Checks parameters for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this function has run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &paramSet) const override;

 public:
  /**
   * @brief Adds one stream to DataSource module. This function should be called after the pipeline has been started.
   * @param stream_id[in]: The unique stream identifier.
   * @param filename[in]: The source path that supports local-file-path, rtsp-url, jpg-sequences, and so on.
   * @param framerate[in]: The input frequency of the source data.
   * @param loop[in]: Whether to reload source when EOF is reached.
   * @return
   *    Returns the source handler instance.
   */
  std::shared_ptr<cnstream::SourceHandler> CreateSource(const std::string &stream_id, const std::string &filename,
                                                        int framerate, bool loop = false);

 public:
  /**
   * @brief Gets module parameters. This function should be called after ``Open()`` has been invoked.
   */
  DataSourceParam GetSourceParam() const { return param_; }

#ifdef UNIT_TEST
  bool SendData(std::shared_ptr<CNFrameInfo> data) { return SourceModule::SendData(data); }
#endif

 private:
  DataSourceParam param_;
};  // class DataSource

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
