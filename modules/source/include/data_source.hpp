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
  SOURCE_RAW,     ///< Represents the raw stream. The source is sent for decoding directly.
  SOURCE_FFMPEG,   ///< Represents the normal stream. The source is demuxed with FFmpeg before sent for decoding.
  SOURCE_MEM
};
/**
 * @brief The storage type of the output frame data that are stored for modules on CPU or MLU.
 */
enum OutputType {
  OUTPUT_CPU,   ///< Outputs to CPU.
  OUTPUT_MLU    ///< Outputs to MLU.
};
/**
 * @brief The decoder type used in the source module.
 */
enum DecoderType {
  DECODER_CPU,   ///< CPU decoder with FFmpeg.
  DECODER_MLU    ///< MLU decoder with CNCodec.
};
/**
 * @brief A structure for private usage.
 */
struct DataSourceParam {
  SourceType source_type_ = SOURCE_RAW;            ///< The demuxer type. The ``SOURCE_RAW`` value is set for debugging.
  OutputType output_type_ = OUTPUT_CPU;            ///< Outputs data to CPU or MLU.
  size_t interval_ = 1;                            ///< Outputs image for every ``interval`` frames.
  DecoderType decoder_type_ = DECODER_CPU;         ///< The decoder type.
  bool reuse_cndec_buf = false;                    ///< Valid when ``DECODER_MLU`` is used.
  int device_id_ = -1;                             ///< The MLU device ID. To disable MLU, set the value to ``-1``.
  size_t chunk_size_ = 0;                          ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  size_t width_ = 0;                               ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  size_t height_ = 0;                              ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  bool interlaced_ = false;                        ///< Valid when ``SOURCE_RAW`` is used. For H264 and H265 only.
  size_t output_w = 0;                             ///< Invalid for MLU200 series.
  size_t output_h = 0;                             ///< Invalid for MLU200 series.
  uint32_t input_buf_number_ = 2;                  ///< Valid when ``decoder_type`` is set to ``DECODER_MLU``.
  uint32_t output_buf_number_ = 3;                 ///< Valid when ``decoder_type`` is set to ``DECODER_MLU``.
  bool apply_stride_align_for_scaler_ = true;      ///< Only Valid on mlu220 platform.
};

/**
 * @brief The class for handling input data.
 */
class DataSource : public SourceModule, public ModuleCreator<DataSource> {
 public:
  /**
   * @brief Constructs DataSource object with a given module name.
   * @param
   * 	moduleName[in]: A defined module name.
   */
  explicit DataSource(const std::string &moduleName);
  /**
   * @brief Deconstructs DataSource object.
   *
   */
  ~DataSource();

  /**
   * @brief Called by pipeline when the pipeline is started.
   
   * @param paramSet
   * @verbatim
   *   source_type: Optional. The demuxer type. The default value is SOURCE_RAW.
   *                Supported values are ``raw``, ``ffmpeg`` and "mem".
   *   output_type: Optional. The output type. The default output_type is cpu.
   *                Supported values are ``mlu`` and ``cpu``.
   *   interval: Optional. Process one frame for every ``interval`` frames. Process every frame by default.
   *   decoder_type : Optional. The decoder type. The default decoder_type is cpu.
   *                  Supported values are ``mlu`` and ``cpu``.
   *   reuse_cndec_buf: Optional. Whether the codec buffer will be reused. The default value is false.
                        This parameter is used when decoder type is ``mlu``.
                        Supported values are ``true`` and ``false``.
   *   device_id: Required when MLU is used. Device id. Set the value to -1 for CPU.
                  Set the value for MLU in the range 0 - N.
   *   chunk_size: Required when ``source_type`` is set to ``raw``. The size of the input data chunk.
   *   width: Required when ``source_type`` is set to ``raw``. The width of the frame.
   *   height: Required when ``source_type`` is set to ``raw``. The height of the frame.
   *   interlaced: Required when ``source_type`` is set to ``raw``. Interlaced mode.
   *   input_buf_number: Optional. The input buffer number. The default value is 2.
   *   output_buf_number: Optional. The output buffer number. The default value is 3.
   * @endverbatim
   *
   * @return
   *    Returns true if ``paramSet`` are supported and valid. Otherwise, returns false.
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
