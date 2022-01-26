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
/*!
 *  @file data_source.hpp
 *
 *  This file contains a declaration of the DataSourceParam and ESPacket struct, and the DataSource, FileHandler,
 *  RtspHandler, ESMemHandler, ESJpegMemHandler and RawImgMemHandler class.
 */
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include <memory>
#include <string>
#include <utility>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_source.hpp"
#include "data_source_param.hpp"

namespace cnstream {

/*!
 * @class DataSource
 *
 * @brief DataSource is a class to handle encoded input data.
 *
 * @note It is always the first module in a pipeline.
 */
class DataSource : public SourceModule, public ModuleCreator<DataSource> {
 public:
  /*!
   * @brief Constructs a DataSource object.
   *
   * @param[in] moduleName The name of this module.
   *
   * @return No return value.
   */
  explicit DataSource(const std::string &moduleName);

  /*!
   * @brief Destructs a DataSource object.
   *
   * @return No return value.
   */
  ~DataSource();

  /*!
   * @brief Initializes the configuration of the DataSource module.
   *
   * This function will be called by the pipeline when the pipeline starts.
   *
   * @param[in] paramSet The module's parameter set to configure a DataSource module.
   *
   * @return Returns true if the parammeter set is supported and valid, othersize returns false.
   */
  bool Open(ModuleParamSet paramSet) override;

  /*!
   * @brief Frees the resources that the object may have acquired.
   *
   * This function will be called by the pipeline when the pipeline stops.
   *
   * @return No return value.
   */
  void Close() override;

  /*!
   * @brief Checks the parameter set for the DataSource module.
   *
   * @param[in] paramSet Parameters for this module.
   *
   * @return Returns true if all parameters are valid. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &paramSet) const override;

  /*!
   * @brief Gets the parameters of the DataSource module.
   *
   * @return Returns the parameters of this module.
   *
   * @note This function should be called after ``Open`` function.
   */
  DataSourceParam GetSourceParam() const { return param_; }

 private:
  DataSourceParam param_;
};  // class DataSource

/*!
 * @struct ESPacket
 *
 * @brief The ESPacket is a structure describing the elementary stream data packet.
 */
struct ESPacket {
  unsigned char *data = nullptr;  /*!< The video data. */
  int size = 0;                   /*!< The size of the data. */
  uint64_t pts = 0;               /*!< The presentation time stamp of the data. */
  uint32_t flags = 0;             /*!< The flags of the data. */
  enum class FLAG{
    FLAG_KEY_FRAME = 0x01,  /*!< The flag of key frame. */
    FLAG_EOS = 0x02,        /*!< The flag of eos (the end of the stream) frame. */
  };
};  // struct ESPacket

class FileHandlerImpl;

/*!
 * @struct MaximumVideoResolution
 *
 * @brief The MaximumVideoResolution (not supported on MLU220/MLU270) is a structure describing the maximum video
 * resolution parameters.
 *
 */
struct MaximumVideoResolution {
  bool enable_variable_resolutions = false;  /*!< Whether to enable variable resolutions. */
  uint32_t maximum_width;                    /*!< The maximum video width. */
  uint32_t maximum_height;                   /*!< The maximum video height. */
};  // struct MaximumVideoResolution

/*!
 * @class FileHandler
 *
 * @brief FileHandler is a class of source handler for video with format mp4, flv, matroska and USBCamera
 * ("/dev/videoxxx") .
 */
class FileHandler : public SourceHandler {
 public:
  /*!
   * @brief Creates source handler.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] filename The filename of the stream.
   * @param[in] framerate Controls sending the frames of the stream with specific rate.
   * @param[in] loop Loops the stream.
   * @param[in] maximum_resolution The maximum video resolution for variable video resolutions.
   * See ``MaximumVideoResolution`` for detail.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const std::string &filename, int framerate, bool loop = false,
                                               const MaximumVideoResolution& maximum_resolution = {});
  /*!
   * @brief The destructor of FileHandler.
   *
   * @return No return value.
   */
  ~FileHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Stops source handler. The Close() function should be called afterwards.
   *
   * @return No return value.
   */
  void Stop() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value
   */
  void Close() override;

 private:
  explicit FileHandler(DataSource *module, const std::string &stream_id, const std::string &filename, int framerate,
                       bool loop, const MaximumVideoResolution& maximum_resolution);

#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  FileHandlerImpl *impl_ = nullptr;
};  // class FileHandler

class RtspHandlerImpl;
/*!
 * @class RtspHandler
 *
 * @brief RtspHandler is a class of source handler for rtsp stream.
 */
class RtspHandler : public SourceHandler {
 public:
  /*!
   * @brief Creates source handler.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream ID of the stream.
   * @param[in] url_name The url of the stream.
   * @param[in] use_ffmpeg Uses ffmpeg demuxer if it is true, otherwise uses live555 demuxer.
   * @param[in] reconnect It is valid when "use_ffmpeg" set false.
   * @param[in] maximum_resolution The maximum video resolution for variable video resolutions.
   * See ``MaximumVideoResolution`` for detail.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const std::string &url_name, bool use_ffmpeg = false, int reconnect = 10,
                                               const MaximumVideoResolution &maximum_resolution = {},
                                               std::function<void(ESPacket, std::string)> callback = nullptr);
  /*!
   * @brief The destructor of RtspHandler.
   *
   * @return No return value.
   */
  ~RtspHandler();

  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;

 private:
  explicit RtspHandler(DataSource *module, const std::string &stream_id, const std::string &url_name, bool use_ffmpeg,
                       int reconnect, const MaximumVideoResolution &maximum_resolution,
                       std::function<void(ESPacket, std::string)> callback);

#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  RtspHandlerImpl *impl_ = nullptr;
};  // class RtspHandler

class ESMemHandlerImpl;
/*!
 * @class ESMemHandler
 *
 * @brief ESMemHandler is a class of source handler for H264/H265 bitstreams in memory (with prefix-start-code).
 */
class ESMemHandler : public SourceHandler {
 public:
  /*!
   * @brief Creates source handler.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] maximum_resolution The maximum video resolution for variable video resolutions.
   * See ``MaximumVideoResolution`` for detail.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
                                               const MaximumVideoResolution& maximum_resolution = {});
  /*!
   * @brief The destructor of ESMemHandler.
   *
   * @return No return value.
   */
  ~ESMemHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Stops source handler. The Close() function should be called afterwards.
   *
   * @return No return value.
   */
  void Stop() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;

  /*!
   * @enum DataType
   *
   * @brief Enumeration variables describing ES data type.
   */
  enum class DataType {
    INVALID,  /*!< Invalid data type. */
    H264,     /*!< The data type is H264. */
    H265      /*!< The data type is H265. */
  };

  /*!
   * @brief Sets data type.
   *
   * @param[in] type The data type.
   *
   * @return Returns 0 if data type is set successfully, otherwise returns -1.
   * @note This function must be called before ``Write`` function.
   */
  int SetDataType(DataType type);

  /*!
   * @brief Sends data in frame mode.
   *
   * @param[in] pkt The data packet
   *
   * @return Returns 0 if the data is written successfully.
   *         Returns -1 if failed to write data. The possible reasons are the handler is closed,
   *         the end of the stream is received, the data is nullptr and the data is invalid,
   *         so that the video infomations can not be parsed from it.
   */
  int Write(ESPacket *pkt);
  /*!
   * @brief Sends data in chunk mode.
   *
   * @param[in] buf The data buffer
   * @param[in] len The length of the data
   *
   * @return Returns 0 if the data is written successfully.
   *         Returns -1 if failed to write data. The possible reasons are the handler is closed, the end of the stream
   *         is received and the data is invalid, so that the video infomations can not be parsed from it.
   */
  int Write(unsigned char *buf, int len);
  /*!
   * @brief Sends the end of the stream.
   *
   * The data remains in the parser will be dropped. Call this function, when the data of a stream is not completely
   * written and the stream needed to be removed.
   *
   * @return Returns 0 if the end of the stream is written successfully.
   *         Returns -1 if failed to write data. The possible reason is the handler is closed.
   */
  int WriteEos();

 private:
  explicit ESMemHandler(DataSource *module, const std::string &stream_id,
                        const MaximumVideoResolution& maximum_resolution);

#ifdef UNIT_TEST
 public:  // NOLINT
#else
 private:  // NOLINT
#endif
  ESMemHandlerImpl *impl_ = nullptr;
};  // class ESMemHandler

class ESJpegMemHandlerImpl;
/*!
 * @class ESJpegMemHandler
 *
 * @brief ESJpegMemHandler is a class of source handler for Jpeg bitstreams in memory.
 */
class ESJpegMemHandler : public SourceHandler {
 public:
  /*!
   * @brief Creates source handler.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] max_width The maximum width of the image.
   * @param[in] max_height The maximum height of the image.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id,
      int max_width = 7680, int max_height = 4320/*Jpeg decoder maximum resolution 8K*/);
  /*!
   * @brief The destructor of ESJpegMemHandler.
   *
   * @return No return value.
   */
  ~ESJpegMemHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;

  /*!
   * @brief Sends data in frame mode.
   *
   * @param[in] pkt The data packet.
   *
   * @return Returns 0 if the data is written successfully.
   *         Returns -1 if failed to write data. The possible reason is the handler is closed or the data is nullptr.
   */
  int Write(ESPacket *pkt);

 private:
  explicit ESJpegMemHandler(DataSource *module, const std::string &stream_id, int max_width, int max_height);

#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  ESJpegMemHandlerImpl *impl_ = nullptr;
};  // class ESJpegMemHandler

class RawImgMemHandlerImpl;
/*!
 * @class RawImgMemHandler
 *
 * @brief RawImgMemHandler is a class of source handler for raw image data in memory.
 *
 * @note This handler will not send data to MLU decoder as the raw data has been decoded.
 */
class RawImgMemHandler : public SourceHandler {
 public:
  /*!
   * @brief Creates source handler.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   *
   * @return Returns source handler if it is created successfully, otherwise returns nullptr.
   */
  static std::shared_ptr<SourceHandler> Create(DataSource *module, const std::string &stream_id);
  /*!
   * @brief The destructor of RawImgMemHandler.
   *
   * @return No return value.
   */
  ~RawImgMemHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;

  /*!
   * @brief Sends raw image with cv::Mat. Only BGR data with 8UC3 type is supported, and data is continuous.
   *
   * @param[in] mat_data The bgr24 format image data.
   * @param[in] pts The pts for mat_data, should be different for each image.
   *
   * @return Returns 0 if the data is written successfully.
   *         Returns -1 if failed to write data. The possible reason is the end of the stream is received or failed to
   *         process the data.
   *         Returns -2 if the data is invalid.
   *
   * @note Sends nullptr after all data are sent.
   */
  int Write(const cv::Mat *mat_data, const uint64_t pts);

  /*!
   * @brief Sends raw image with image data and image infomation, support formats: bgr24, rgb24, nv21 and nv12.
   *
   * @param[in] data The data of the image, which is a continuous buffer.
   * @param[in] size The size of the data.
   * @param[in] pts The pts for raw image, should be different for each image.
   * @param[in] width The width of the image.
   * @param[in] height The height of the image.
   * @param[in] pixel_fmt The pixel format of the image. These formats are supported, bgr24, rgb24, nv21 and nv12.
   *
   * @return Returns 0 if the data is written successfully.
   *         Returns -1 if failed to write data. The possible reason is the end of the stream is received or failed to
   *         process the data.
   *         Returns -2 if the data is invalid.
   *
   * @note Sends nullptr as data and passes 0 as size after all data are sent.
   */
  int Write(const uint8_t *data, const int size, const uint64_t pts, const int width = 0, const int height = 0,
            const CNDataFormat pixel_fmt = CNDataFormat::CN_INVALID);

 private:
  explicit RawImgMemHandler(DataSource *module, const std::string &stream_id);

#ifdef UNIT_TEST
 public:  // NOLINT
#endif
  RawImgMemHandlerImpl *impl_ = nullptr;
};  // class RawImgMemHandler

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
