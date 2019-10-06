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

#include <memory>
#include <utility>
#include "cnstream_frame.hpp"
#include "cnstream_module.hpp"
#include "cnstream_pipeline.hpp"

namespace cnstream {

/*
 *  @brief SourceType
 */
enum SourceType { SOURCE_RAW, SOURCE_FFMPEG };
/*
 * @brief OutputType
 */
enum OutputType { OUTPUT_CPU, OUTPUT_MLU };
/*
 * @brief DecoderType
 */
enum DecoderType { DECODER_CPU, DECODER_MLU };
/*
 * @hidebrief a structure for private usage
 */
struct DataSourceParam {
  SourceType source_type_ = SOURCE_RAW;    /*demuxer type, SOURCE_RAW is for debug purpose*/
  OutputType output_type_ = OUTPUT_CPU;    /* output data to cpu/mlu*/
  size_t interval_ = 1;                    /*output image every "interval" frames */
  DecoderType decoder_type_ = DECODER_CPU; /* decoder type*/
  bool reuse_cndec_buf = false;            /*valid when DECODER_MLU used*/
  int device_id_ = -1;                     /*mlu device id, -1 :disable mlu*/
  size_t chunk_size_ = 0;                  /*valid when SOURCE_RAW used, for H264,H265 only*/
  size_t width_ = 0;                       /*valid when SOURCE_RAW used, for H264,H265 only*/
  size_t height_ = 0;                      /*valid when SOURCE_RAW used, for H264,H265 only*/
  bool interlaced_ = false;                /*valid when SOURCE_RAW used, for H264,H265 only*/
};

/*
 * @class Data handler to process data.
 */
class DataHandler;

/*
 * @brief Class for handling input data
 */
class DataSource : public Module, public ModuleCreator<DataSource> {
 public:
  /*
   * @brief Construct DataSource object with a given moduleName
   * @param
   * 	moduleName[in]:defined module name
   */
  explicit DataSource(const std::string &moduleName);
  /*
   * @brief Deconstruct DataSource object
   *
   */
  ~DataSource();

  /*
   * @brief Called by pipeline when pipeline start.
   * @param
   *   paramSet[in]:  prameterSet set by user, supported paramSet as below,
   *      "source_type": demuxer type, required, "raw", "ffmpeg",...
   *      "output_type": required, "mlu","cpu"...
   *      "interval": optional, handle data every "interval"
   *      "decoder_type" : required, "mlu","cpu"
   *      "reuse_cndec_buf": optional when mlu decoder used,"true" or "false".
   *      "device_id": required when "mlu" used, -1 for cpu, 0..N for mlu
   *      "chunk_size": required when source_type is raw"
   *      "width": required when source_type is raw"
   *      "height": required when source_type is raw"
   *      "interlaced": required when source_type is raw"
   * @return
   *    true if paramSet are supported and valid, othersize false
   */
  bool Open(ModuleParamSet paramSet) override;
  /*
   * @brief Called by pipeline when pipeline stop.
   */
  void Close() override;
  /*
   * @brief do demux-decode for each frame
   * @param
   *   data[in]: data to be processed.
   */
  int Process(std::shared_ptr<CNFrameInfo> data) override;

 public:
  /*
   * @brief Add one stream to DataSource module, should be called after pipeline starts.
   * @param
   *   stream_id[in]: unique stream identifier.
   *   filename[in]: source path, local-file-path/rtsp-url/jpg-sequences, etc.
   *   framerate[in]: source data input frequency
   *   loop[in]: whether to reload source when EOF is reached or not
   * @return
   *    0: success,
   *   -1: error occurs
   */
  int AddVideoSource(const std::string &stream_id, const std::string &filename, int framerate, bool loop = false);
  /*
   * @brief Add one stream to DataSource module, should be called after pipeline starts.
   * @param
   *   stream_id[in]: unique stream identifier.
   *   filename[in]: jpg file path.
   *   loop[in]: whether to reload source when EOF is reached or not
   * @return
   *    0: success,
   *   -1: error occurs
   */
  int AddImageSource(const std::string &stream_id, const std::string &filename, bool loop = false);
  /*
   * @brief Remove one stream from DataSource module,should be called before pipeline stops.
   * @param
   *   stream_id[in]: unique stream identifier.
   * @return
   *    0: success (always success by now)
   */
  int RemoveSource(const std::string &stream_id);

 public:
  /*
   * @brief Transmit data to next stage(s) of the pipeline
   * @param
   *   data[in]: data to be transmitted.
   * @return
   *   true if data is transmitted successfully,othersize false
   */
  bool SendData(std::shared_ptr<CNFrameInfo> data);

  /*
   * @brief Get module parameters, should be called after Open() invoked.
   */
  DataSourceParam GetSourceParam() const { return param_; }

 private:
  /*
   * @hidebrief Remove all streams from DataSource module
   * @param
   * @return
   *    0: success (always success by now)
   */
  int RemoveSources();

 private:
  DataSourceParam param_;
  std::mutex mutex_;
  std::map<std::string /*stream_id*/, std::shared_ptr<DataHandler>> source_map_;
};  // class DataSource

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
