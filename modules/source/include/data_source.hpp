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
 *  This file contains a declaration of struct DataSource and DataSourceParam
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

#include <memory>
#include <string>
#include <utility>

#include "cnstream_frame.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_source.hpp"

namespace cnstream {

/**
 * @brief storage type of output frame data for modules, storage on cpu or mlu.
 */
enum OutputType { OUTPUT_CPU, OUTPUT_MLU };
/**
 * @brief decoder type used in source module.
 */
enum DecoderType { DECODER_CPU, DECODER_MLU };
/**
 * @brief a structure for private usage
 */
struct DataSourceParam {
  OutputType output_type_ = OUTPUT_CPU;     ///< output data to cpu/mlu
  size_t interval_ = 1;                     ///< output image every "interval" frames
  DecoderType decoder_type_ = DECODER_CPU;  ///< decoder type
  bool reuse_cndec_buf = false;             ///< valid when DECODER_MLU used
  int device_id_ = -1;                      ///< mlu device id, -1 :disable mlu
  uint32_t input_buf_number_ = 2;           ///< valid when decoder_type = DECODER_MLU
  uint32_t output_buf_number_ = 3;          ///< valid when decoder_type = DECODER_MLU
};

/**
 * @brief Class for handling input data
 */
class DataSource : public SourceModule, public ModuleCreator<DataSource> {
 public:
  /**
   * @brief Construct DataSource object with a given moduleName
   * @param
   * 	moduleName[in]:defined module name
   */
  explicit DataSource(const std::string &moduleName);
  /**
   * @brief Deconstruct DataSource object
   *
   */
  ~DataSource();

  /**
   * @brief Called by pipeline when the pipeline is started.

   * @param paramSet
   * @verbatim
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
   *   interlaced: Required when ``source_type`` is set to ``raw``. Interlaced mode.
   *   input_buf_number: Optional. The input buffer number. The default value is 2.
   *   output_buf_number: Optional. The output buffer number. The default value is 3.
   * @endverbatim
   *
   * @return
   *    true if paramSet are supported and valid, othersize false
   */
  bool Open(ModuleParamSet paramSet) override;
  /**
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;

  /**
   * @brief Check ParamSet for a module.
   *
   * @param paramSet Parameters for this module.
   *
   * @return Returns true if this API run successfully. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &paramSet) const override;

 public:
  /**
   * @brief Get module parameters, should be called after Open() invoked.
   */
  DataSourceParam GetSourceParam() const { return param_; }

 private:
  DataSourceParam param_;
};  // class DataSource

/*SourceHandler for H264/H265 bitstreams in memory(with prefix-start-code)
*/
struct ESPacket {
  unsigned char *data = nullptr;
  int size = 0;
  uint64_t pts = 0;
  uint32_t flags = 0;
  enum {
    FLAG_KEY_FRAME = 0x01,
    FLAG_EOS = 0x02,
  };
};

class FileHandlerImpl;
class FileHandler : public SourceHandler {
 public:
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const std::string &filename, int framerate, bool loop = false);
  ~FileHandler();
  /**/
  bool Open() override;
  void Close() override;

 private:
  explicit FileHandler(DataSource *module, const std::string &stream_id, const std::string &filename, int framerate,
                       bool loop);

 private:
#ifdef UNIT_TEST
 public:
#endif
  FileHandlerImpl *impl_ = nullptr;
};  // class FileHandler

class RtspHandlerImpl;
/*SourceHandler for rtsp as input
 * use_ffmpeg:
 *   true: use ffmppeg-rtspclient
 *   false: use live555-rtspclient, FIXME(provide interface to set strategies:  reconnect, stream_over_tcp, etc...)
 */
class RtspHandler : public SourceHandler {
 public:
  /*
   * "reconnect" is valid when "use_ffmpeg" set false
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const std::string &url_name, bool use_ffmpeg = false, int reconnect = 1);
  ~RtspHandler();
  /**/
  bool Open() override;
  void Close() override;

 private:
  explicit RtspHandler(DataSource *module, const std::string &stream_id, const std::string &url_name, bool use_ffmpeg,
                       int reconnect);

 private:
#ifdef UNIT_TEST
 public:
#endif
  RtspHandlerImpl *impl_ = nullptr;
};  // class RtspHandler

class ESMemHandlerImpl;
/*SourceHandler for H264/H265 bitstreams in memory(with prefix-start-code)
 */
class ESMemHandler : public SourceHandler {
 public:
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id);
  ~ESMemHandler();
  /**/
  bool Open() override;
  void Close() override;

  /*Write()
   *   Send H264/H265 bitstream with prefix-startcode.
   */
  enum DataType { AUTO, H264, H265 };
  void SetDataType(DataType type = AUTO);
  int Write(ESPacket *pkt);
  int Write(unsigned char *buf, int len);

 private:
  explicit ESMemHandler(DataSource *module, const std::string &stream_id);

 private:
#ifdef UNIT_TEST
 public:
#endif
  ESMemHandlerImpl *impl_ = nullptr;
};  // class ESMemHandler

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
