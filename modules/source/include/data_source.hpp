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
#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#endif

#include <memory>
#include <string>
#include <utility>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_source.hpp"

namespace cnstream {

/**
 * @brief storage type of output frame data for modules, storage on cpu or mlu.
 */
enum OutputType {
  OUTPUT_CPU,  /// output decoded buffer to CPU
  OUTPUT_MLU   /// output decoded buffer to MLU
};
/**
 * @brief decoder type used in source module.
 */
enum DecoderType {
  DECODER_CPU,  /// use CPU decoder
  DECODER_MLU   /// use MLU decoder
};
/**
 * @brief a structure for private usage
 */
struct DataSourceParam {
  OutputType output_type_ = OUTPUT_CPU;         ///< output data to cpu/mlu
  size_t interval_ = 1;                         ///< output image every "interval" frames
  DecoderType decoder_type_ = DECODER_CPU;      ///< decoder type
  bool reuse_cndec_buf = false;                 ///< valid when DECODER_MLU used
  int device_id_ = -1;                          ///< mlu device id, -1 :disable mlu
  uint32_t input_buf_number_ = 2;               ///< valid when decoder_type = DECODER_MLU
  uint32_t output_buf_number_ = 3;              ///< valid when decoder_type = DECODER_MLU
  bool apply_stride_align_for_scaler_ = false;  //< recommended for use on m200 platforms
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
   *   interlaced: Interlaced mode.
   *   input_buf_number: Optional. The input buffer number. The default value is 2.
   *   output_buf_number: Optional. The output buffer number. The default value is 3.
   *   apply_stride_align_for_scaler: Optional. Apply stride align for scaler on m220(m.2/edge).
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
   * @brief Get module parameters.
   *
   * @return Returns data source parameters.
   *
   * @note This function should be called after ``Open`` function.
   */
  DataSourceParam GetSourceParam() const { return param_; }

 private:
  DataSourceParam param_;
};  // class DataSource

/**
 * @brief The struct of ES data packet.
 */
struct ESPacket {
  unsigned char *data = nullptr;  /// the data
  int size = 0;                   /// the size of the data
  uint64_t pts = 0;               /// the pts of the data
  uint32_t flags = 0;             /// the flags of the data
  /**
   * @brief The flags of frame
   */
  enum {
    FLAG_KEY_FRAME = 0x01,        /// flag of key frame
    FLAG_EOS = 0x02,              /// flag of eos frame
  };
};  // struct ESPacket

/**
 * @brief Source handler for video with format mp4, flv, matroska and USBCamera("/dev/videoxxx") etc.
 */
class FileHandlerImpl;
class FileHandler : public SourceHandler {
 public:
  /**
   * @brief Creates source handler.
   *
   * @param module The data source module.
   * @param stream_id The stream id of the stream.
   * @param filename The filename of the stream.
   * @param framerate Control sending the frames of the stream with specific rate.
   * @param loop Loop the stream.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const std::string &filename, int framerate, bool loop = false);
  /**
   * @brief The destructor of FileHandler.
   */
  ~FileHandler();
  /**
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /**
   * @brief Closes source handler.
   */
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
/**
 * @brief Source handler for rtsp stream.
 */
class RtspHandler : public SourceHandler {
 public:
  /**
   * @brief Creates source handler.
   *
   * @param module The data source module.
   * @param stream_id The stream id of the stream.
   * @param url_name The url of the stream.
   * @param use_ffmpeg Uses ffmpeg demuxer if it is true, otherwise uses live555 demuxer.
   * @param reconnect It is valid when "use_ffmpeg" set false.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const std::string &url_name, bool use_ffmpeg = false,
                                               int reconnect = 10);
  /**
   * @brief The destructor of RtspHandler.
   */
  ~RtspHandler();
  /**
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /**
   * @brief Closes source handler.
   */
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
/**
 * @brief Source handler for H264/H265 bitstreams in memory(with prefix-start-code).
 */
class ESMemHandler : public SourceHandler {
 public:
  /**
   * @brief Creates source handler.
   *
   * @param module The data source module.
   * @param stream_id The stream id of the stream.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id);
  /**
   * @brief The destructor of ESMemHandler.
   */
  ~ESMemHandler();
  /**
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /**
   * @brief Closes source handler.
   */
  void Close() override;

  /**
   * @brief The enum of data type.
   */
  enum DataType {
    INVALID,     /// Invalid data type.
    H264,        /// The data type of H264.
    H265         /// The data type of H265.
  };

  /**
   * @brief Sets data type.
   *
   * @param type The data type.
   *
   * @return Returns 0 if data type is set successfully, otherwise returns -1.
   * @note This function must be called before ``Write`` function
   */
  int SetDataType(DataType type);

  /**
   * @brief Sends data in frame mode.
   *
   * @param pkt The data packet
   *
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe the handler is closed.
   * @retval -2: Invalid data. Can not parse video infomations from `pkt`.
   */
  int Write(ESPacket *pkt);                // frame mode
  /**
   * @brief Sends data in chunk mode.
   *
   * @param buf The data buffer
   * @param len The len of the data
   *
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe the handler is closed.
   * @retval -2: Invalid data. Can not parse video infomations from `buf`.
   */
  int Write(unsigned char *buf, int len);

 private:
  explicit ESMemHandler(DataSource *module, const std::string &stream_id);

 private:
#ifdef UNIT_TEST
 public:
#endif
  ESMemHandlerImpl *impl_ = nullptr;
};  // class ESMemHandler

class ESJpegMemHandlerImpl;
/**
 * @brief Source handler for Jpeg bitstreams in memory
 */
class ESJpegMemHandler : public SourceHandler {
 public:
  /**
   * @brief Creates source handler.
   *
   * @param module The data source module.
   * @param stream_id The stream id of the stream.
   * @param max_width The maximum width of the image.
   * @param max_height The maximum height of the image.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
      int max_width = 7680, int max_height = 4320/*Jpeg decoder maximum resolution 8K*/);
  /**
   * @brief The destructor of ESJpegMemHandler.
   */
  ~ESJpegMemHandler();
  /**
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /**
   * @brief Closes source handler.
   */
  void Close() override;

  /**
   * @brief Sends data in frame mode.
   *
   * @param pkt The data packet.
   *
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe the handler is closed.
   * @retval -2: Invalid data. Can not parse image infomations from `pkt`.
   */
  int Write(ESPacket *pkt);

 private:
  explicit ESJpegMemHandler(DataSource *module, const std::string &stream_id, int max_width, int max_height);

 private:
#ifdef UNIT_TEST
 public:
#endif
  ESJpegMemHandlerImpl *impl_ = nullptr;
};  // class ESJpegMemHandler

class RawImgMemHandlerImpl;
/**
 * @brief Source handler for raw image data in memory.
 */
class RawImgMemHandler : public SourceHandler {
 public:
  /**
   * @brief Creates source handler.
   *
   * @param module The data source module.
   * @param stream_id The stream id of the stream.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id);
  /**
   * @brief The destructor of RawImgMemHandler.
   */
  ~RawImgMemHandler();
  /**
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /**
   * @brief Closes source handler.
   */
  void Close() override;

#ifdef HAVE_OPENCV
  /**
   * @brief Sends raw image with cv::Mat. Only BGR data with 8UC3 type is supported, and data is continuous.
   *
   * @param mat_data The bgr24 format image data.
   * @param pts The pts for mat_data, should be different for each image.
   *
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe eos got or handler is closed.
   * @retval -2: Invalid data.
   *
   * @note Sends nullptr after all data are sent.
   */
  int Write(const cv::Mat *mat_data, const uint64_t pts);
#endif
  /**
   * @brief Sends raw image with image data and image infomation, support formats: bgr24, rgb24, nv21 and nv12.
   *
   * @param data The data of the image, which is a continuous buffer.
   * @param size The size of the data.
   * @param pts The pts for raw image, should be different for each image.
   * @param width The width of the image.
   * @param height The height of the image.
   * @param pixel_fmt The pixel format of the image. These formats are supported, bgr24, rgb24, nv21 and nv12.
   *
   * @retval 0: The data is write successfully,
   * @retval -1: Write failed, maybe eos got or handler is closed.
   * @retval -2: Invalid data.
   *
   * @note Sends nullptr as data and passes 0 as size after all data are sent.
   */
  int Write(const uint8_t *data, const int size, const uint64_t pts, const int width = 0,
      const int height = 0, const CNDataFormat pixel_fmt = CN_INVALID);

 private:
  explicit RawImgMemHandler(DataSource *module, const std::string &stream_id);

#ifdef UNIT_TEST

 public:
#endif
  RawImgMemHandlerImpl *impl_ = nullptr;
};  // class RawImgMemHandler

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
