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
#include <memory>
#include <string>
#include <utility>

#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "cnstream_pipeline.hpp"
#include "cnstream_source.hpp"
#include "private/cnstream_param.hpp"

namespace cnstream {

/*!
 * @struct DataSourceParam
 *
 * @brief The DataSourceParam is a structure describing the parameters of a DataSource module.
 */
struct DataSourceParam {
  uint32_t interval = 1;  /*!< The interval of outputting one frame. It outputs one frame every n (interval_) frames. */
  int device_id = 0;      /*!< The device ordinal. */
  uint32_t bufpool_size = 16;    /*!< The size of the buffer pool to store output frames. */
};

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
   * @param[in] name The name of this module.
   *
   * @return No return value.
   */
  explicit DataSource(const std::string &name);

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
   * @param[in] param_set The module's parameter set to configure a DataSource module.
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

  /*!
   * @brief Checks the parameter set for the DataSource module.
   *
   * @param[in] param_set Parameters for this module.
   *
   * @return Returns true if all parameters are valid. Otherwise, returns false.
   */
  bool CheckParamSet(const ModuleParamSet &param_set) const override;

  /*!
   * @brief Gets the parameters of the DataSource module.
   *
   * @return Returns the parameters of this module.
   *
   * @note This function should be called after ``Open`` function.
   */
  DataSourceParam GetSourceParam() const { return param_; }

 private:
  std::unique_ptr<ModuleParamsHelper<DataSourceParam>> param_helper_ = nullptr;
  DataSourceParam param_;
};  // class DataSource

/*!
 * @struct Resolution
 *
 * @brief The Resolution is a structure describing the width and the height.
 */
struct Resolution {
  uint32_t width = 0;   /*!< The width. */
  uint32_t height = 0;  /*!< The height. */
};  // struct Resolution


/*!
 * @struct ESPacket
 *
 * @brief The ESPacket is a structure describing the elementary stream data packet.
 */
struct ESPacket {
  unsigned char *data = nullptr;  /*!< The video data. */
  int size = 0;                   /*!< The size of the data. */
  uint64_t pts = 0;               /*!< The presentation time stamp of the data. */
  bool has_pts = true;            /*!< Whether set pts by user. */
  uint32_t flags = 0;             /*!< The flags of the data. */
  enum class FLAG{
    FLAG_KEY_FRAME = 0x01,  /*!< The flag of key frame. */
    FLAG_EOS = 0x02,        /*!< The flag of eos (the end of the stream) frame. */
  };
};  // struct ESPacket

/*!
 * @struct ESJpegPacket
 *
 * @brief The ESJpegPacket is a structure describing the elementary stream data packet.
 */
struct ESJpegPacket {
  unsigned char *data = nullptr;  /*!< The jpeg data. */
  int size = 0;                   /*!< The size of the data. */
  uint64_t pts = 0;               /*!< The presentation time stamp of the data. */
  bool has_pts = true;            /*!< Whether set pts by user. */
};  // struct ESJpegPacket

/*!
 * @struct ImageFrame
 *
 * @brief The ImageFrame is a structure describing a image frame.
 */
struct ImageFrame {
  cnedk::BufSurfWrapperPtr data;  /*!< The BufSurface wrapper containing a image frame. */
  bool has_pts = true;            /*!< Whether set pts by user. */
};  // struct ImageFrame


/*!
 * @struct FileSourceParam
 *
 * @brief The FileSourceParam is a structure describing the parameters to create a FileHandler.
 */
struct FileSourceParam {
  std::string filename;       /*!< The filename of the stream. */
  int framerate;              /*!< The framerate of feeding the stream. */
  bool loop = false;          /*!< Whether loop the stream. */
  Resolution max_res;         /*!< The maximum input resolution. */
  Resolution out_res;         /*!< The output resolution. */
  bool only_key_frame = false;    /*!< Only decode key frame. */
};  // FileSourceParam
/*!
 * @struct RtspSourceParam
 *
 * @brief The RtspSourceParam is a structure describing the parameters to create a FileHandler.
 */
struct RtspSourceParam {
  std::string url_name;              /*!< The url of the stream. */
  Resolution max_res;                /*!< The maximum input resolution. */
  bool use_ffmpeg = false;           /*!< Uses ffmpeg demuxer if it is true, otherwise uses live555 demuxer. */
  int reconnect = 10;                /*!< It is valid when "use_ffmpeg" set false. -1 means reconnect endless. */
  uint32_t interval = 0;             /*!< Interval, 3 means keep a frame every 3 frames. */
  bool only_key_frame = false;       /*!< Only decode key frame. */
  std::function<void(ESPacket, std::string)> callback = nullptr;  /*!< The callback for getting h264/h265 video. */
  Resolution out_res;                /*!< The output resolution. */
};  // RtspSourceParam
/*!
 * @struct SensorSourceParam
 *
 * @brief The SensorSourceParam is a structure describing the parameters to create a CameraHandler.
 */
struct SensorSourceParam {
  int sensor_id;       /*!< The sensor id. 0...n-1 */
  Resolution out_res;  /*!< The output resolution. */
};  // SensorSourceParam
/*!
 * @struct ESMemSourceParam
 *
 * @brief The ESMemSourceParam is a structure describing the parameters to create a ESMemHandler.
 */
struct ESMemSourceParam {
  Resolution max_res;  /*!< The maximum input resolution. */
  Resolution out_res;  /*!< The output resolution. */
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
  DataType data_type = DataType::INVALID;
  bool only_key_frame = false;    /*!< Only decode key frame. */
};  // ESMemSourceParam
/*!
 * @struct ESJpegMemSourceParam
 *
 * @brief The ESJpegMemSourceParam is a structure describing the parameters to create a ESJpegMemHandler.
 */
struct ESJpegMemSourceParam {
  Resolution max_res;  /*!< The maximum input resolution. */
  Resolution out_res;  /*!< The output resolution. */
};  // ESJpegMemSourceParam
/*!
 * @struct ImageFrameSourceParam
 *
 * @brief The ImageFrameSourceParam is a structure describing the parameters to create a ImageFrameHandler.
 */
struct ImageFrameSourceParam {
  Resolution out_res;  /*!< The output resolution. */
};  // ImageFrameSourceParam

// group: Source Function
/*!
 * @brief Creates a FileHandler.
 *
 * @param[in] module A pointer to DataSource module.
 * @param[in] stream_id The unique identity for this stream.
 * @param[in] param The parameter for creating the handler.
 *
 * @return Returns handler smart pointer if this function has run successfully, othersize returns nullptr.
 *
 * @note If either the param.max_res.width or param.max_res.height is 0 means that,
 *       for h264/h265, the resolution is not vairable.
 *       For Jpeg, the default value of param.max_res.width = 8192, height = 4320.
 */
std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const FileSourceParam &param);
// group: Source Function
/*!
 * @brief Creates a RtspHandler.
 *
 * @param[in] module A pointer to DataSource module.
 * @param[in] stream_id The unique identity for this stream.
 * @param[in] param The parameter for creating the handler.
 *
 * @return Returns handler smart pointer if this function has run successfully, othersize returns nullptr.
 *
 * @note If either the param.max_res.width or param.max_res.height is 0 means that,
 *       for h264/h265, the resolution is not vairable.
 *       For Jpeg, the default value of param.max_res.width = 8192, height = 4320.
 */
std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const RtspSourceParam &param);
// group: Source Function
/*!
 * @brief Creates a CameraHandler.
 *
 * @param[in] module A pointer to DataSource module.
 * @param[in] stream_id The unique identity for this stream.
 * @param[in] param The parameter for creating the handler.
 *
 * @return Returns handler smart pointer if this function has run successfully, othersize returns nullptr.
 */
std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const SensorSourceParam &param);
// group: Source Function
/*!
 * @brief Creates a ESMemHandler.
 *
 * @param[in] module A pointer to DataSource module.
 * @param[in] stream_id The unique identity for this stream.
 * @param[in] param The parameter for creating the handler.
 *
 * @return Returns handler smart pointer if this function has run successfully, othersize returns nullptr.
 *
 * @note If either the param.max_res.width or param.max_res.height is 0 means that the resolution is not vairable.
 */
std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const ESMemSourceParam &param);
// group: Source Function
/*!
 * @brief Creates a ESJpegMemHandler.
 *
 * @param[in] module A pointer to DataSource module.
 * @param[in] stream_id The unique identity for this stream.
 * @param[in] param The parameter for creating the handler.
 *
 * @return Returns handler smart pointer if this function has run successfully, othersize returns nullptr.
 *
 * @note The default value of param.max_res.width = 8192, height = 4320.
 */
std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const ESJpegMemSourceParam &param);
// group: Source Function
/*!
 * @brief Creates a ImageFrameHandler.
 *
 * @param[in] module A pointer to DataSource module.
 * @param[in] stream_id The unique identity for this stream.
 * @param[in] param The parameter for creating the handler.
 *
 * @return Returns handler smart pointer if this function has run successfully, othersize returns nullptr.
 */
std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const ImageFrameSourceParam &param);

// group: Source Function
/*!
 * @brief Writes data to ESMemHandler.
 *
 * @param[in] handler A smart pointer to ESMemHandler.
 * @param[in] pkt The packet containing H264/H265 bitstreams data.
 *
 * @return Returns 0 if this function writes data successfully.
 *         Returns -1 if it fails to writes data. The possible reason is the handler is closed,
 *         the pkt is nullptr or parsing failed.
 *
 * @note If the data does not end normally, must write pkt to notify the parser its the last packet,
 *       set the data of the pkt to nullptr or the size of the pkt to 0.
 *
 * @note Must write pkt to notify the parser it's the end of the stream,
 *       set FLAG_EOS to the flags of the pkt and set the data of the pkt to nullptr or the size to 0.
 */
int Write(std::shared_ptr<SourceHandler>handler, ESPacket* pkt);
// group: Source Function
/*!
 * @brief Writes data to ESJpegMemHandler.
 *
 * @param[in] handler A smart pointer to ESJpegMemHandler.
 * @param[in] pkt The packet containing Jpeg bitstreams data.
 *
 * @return Returns 0 if this function writes data successfully.
 *         Returns -1 if it fails to writes data. The possible reason is the pkt is nullptr or decoding failed.
 *
 * @note Must write pkt to notify the handler it's the end of the stream,
 *       set the data of the pkt to nullptr or the size to 0.
 */
int Write(std::shared_ptr<SourceHandler>handler, ESJpegPacket* pkt);
// group: Source Function
/*!
 * @brief Writes data to RawImgMemHandler.
 *
 *        The frame will be converted to YUV420spNV12 by default.
 *        Unless if the first frame is with YUV420spNV21 format, the frame will be converted to YUV420spNV21.
 *        The supported color formats of inputs are YUV420spNV12, YUV420spNV21, RGB24, BGR24, ARGB32, ABGR32.
 *
 * @param[in] handler A smart pointer to RawImgMemHandler.
 * @param[in] frame It Contains a image frame.
 *
 * @return Returns 0 if this function writes data successfully.
 *         Returns -1 if failed to write data. The possible reason is the frame is nullptr,
 *         invalid data or converting data failed.
 *
 * @note Must write pkt to notify the handler it's the end of the stream, set the data of the pkt to nullptr.
 * @note Must not write a set of frames with both YUV420spNV12 and YUV420spNV21 formats.
 *       If the first frame written is not YUV420spNV21 format, do not write YUV420spNV21 afterwards.
 */
int Write(std::shared_ptr<SourceHandler>handler, ImageFrame* frame);

}  // namespace cnstream

#endif  // MODULES_DATA_SOURCE_HPP_
