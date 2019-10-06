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

#ifndef CNSTREAM_FRAME_HPP_
#define CNSTREAM_FRAME_HPP_

/**
 *  \file cnstream_frame.hpp
 *
 *  This file contains a declaration of struct CNFrameInfo and its subtructure.
 */

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#ifdef HAVE_OPENCV
#include "opencv2/opencv.hpp"
#endif
#include "cnstream_common.hpp"
#include "cnstream_syncmem.hpp"

#ifndef CN_MAX_PLANES
#define CN_MAX_PLANES 6
#endif

namespace cnstream {

/**
 * Data format name of CNDataFrame, it is usually used to
 * identify the pixel format for the dat ain CNDataFrame.
 */
typedef enum {
  CN_INVALID = -1,                  ///< Invalid frame
  CN_PIXEL_FORMAT_YUV420_NV21 = 0,  ///< Identify that this frame is in YUV420SP(NV21) format
  CN_PIXEL_FORMAT_YUV420_NV12,      ///< Identify that this frame is in YUV420sp(NV12) format
  CN_PIXEL_FORMAT_BGR24,            ///< Identify that this frame is in BGR24 format
  CN_PIXEL_FORMAT_RGB24             ///< Identify that this frame is in RGB24 format
} CNDataFormat;

/**
 * It identifies which device(CPU/MLU) the data of CNDataFrame
 * will be allocated on.
 */
typedef struct {
  enum DevType {
    INVALID = -1,  ///> Invalid device type
    CPU = 0,       ///> CPU
    MLU = 1        ///> MLU
  } dev_type = INVALID;
  int dev_id = 0;       ///> Ordinal device id.
  int ddr_channel = 0;  ///> Ordinal channel id for MLU. [0, 4).
} DevContext;

/**
 * The mask of CNDataFrame
 */
enum CNFrameFlag {
  CN_FRAME_FLAG_EOS = 1 << 0  ///> Identifies the end of data stream.
};

/**
 * Get image plane number by image type.
 *
 * @param
 *   fmt Image format.
 *
 * @return
 * @retval 0: Unsupported picture format.
 * @retval >0: Image plane number.
 */
inline int CNGetPlanes(CNDataFormat fmt) {
  switch (fmt) {
    case CN_PIXEL_FORMAT_BGR24:
    case CN_PIXEL_FORMAT_RGB24:
      return 1;
    case CN_PIXEL_FORMAT_YUV420_NV12:
    case CN_PIXEL_FORMAT_YUV420_NV21:
      return 2;
    default:
      return 0;
  }
  return 0;
}

/**
 * Dedicated deallocator for CNDecoder Buffer.
 */
class IDataDeallocator {
 public:
  virtual ~IDataDeallocator() {}
};

class Module;
/**
 * The structure contains a frame of data and describes infomation
 * about this frame.
 */
struct CNDataFrame {
  std::string stream_id;  ///> The data stream alias where this frame is located.
  size_t flags = 0;       ///> The mask for this frame, CNFrameFlag.
  int64_t frame_id;       ///> Frame index, incremented from 0.
  int64_t timestamp;      ///> The timestamp of this frame.

  /**
   * Source data info, should be filled before CopyToSyncMem() called.
   */
  CNDataFormat fmt;                                          ///> Frame format.
  int width;                                                 ///> Frame width.
  int height;                                                ///> Frame height.
  int stride[CN_MAX_PLANES];                                 ///> Frame strides.
  DevContext ctx;                                            ///> Device context for this frame.
  void* ptr[CN_MAX_PLANES];                                  ///> CPU/MLU data addresses for planes.
  std::shared_ptr<IDataDeallocator> deAllocator_ = nullptr;  ///> Dedicated deallocator for CNDecoder Buffer.

  CNDataFrame(){};

  ~CNDataFrame();

  /**
   * Get plane count for this frame.
   *
   * @return Return plane count for this frame.
   */
  int GetPlanes() const { return CNGetPlanes(fmt); }

  /**
   * Get number of bytes of the specified plane.
   *
   * @param plane_idx The index of plane. Plane index incremented from 0.
   *
   * @return Return number of bytes of the plane specified by plane_idx.
   */
  size_t GetPlaneBytes(int plane_idx) const;

  /**
   * Get number of bytes of this frame.
   *
   * @return Return number of bytes of this frame.
   */
  size_t GetBytes() const;

  /**
   * Sync source-data to CNSyncedMemory
   */
  void CopyToSyncMem();

  /**
   * The pipeline stages (modules) info.
   * Do not call it.
   */
  void SetModuleMask(Module* module, Module* current);
  /**
   * The pipeline stages (modules) info.
   * Do not call it.
   */
  unsigned long GetModulesMask(Module* module);
  /**
   * The pipeline stages (modules) info.
   * Do not call it.
   */
  void ClearModuleMask(Module* module);

  /**
   * The pipeline stages (modules) info.
   * Do not call it.
   *
   * Add EOS mask. threadsafe function.
   *
   * @param
   *   module[in]: The mask is from this module.
   *
   * @return
   *   Return mask after add mask.
   */
  unsigned long AddEOSMask(Module* module);

 public:
  void* cpu_data = nullptr;  ///> CPU data pointer. Should be allocated by CNStreamMallocHost().
  void* mlu_data = nullptr;  ///> MLU data pointer.
  std::shared_ptr<CNSyncedMemory> data[CN_MAX_PLANES];  ///> Synce data helper.

#ifdef HAVE_OPENCV
  /*called after CopyToSyncMem() invoked*/
  cv::Mat* ImageBGR();

 private:
  cv::Mat* bgr_mat = nullptr;
#endif
 private:
  std::mutex modules_mutex;
  /*Module mask map, identify which modules the data can already be processed by.*/
  std::map<unsigned int, unsigned long> module_mask_map_;

  std::mutex eos_mutex;
  unsigned long eos_mask = 0;
};  // struct CNDataFrame

/**
 * Bounding box for detection information for one object.
 * Normalized coordinates.
 */
typedef struct {
  float x, y, w, h;
} CNInferBoundingBox;

/**
 * Classification Properties for one object.
 */
typedef struct {
  int id = -1;      ///> Classification unique id. -1 is invalid.
  int value = -1;   ///> Classification value(label value).
  float score = 0;  ///> Label score.
} CNInferAttr;

/**
 * Feature value for one object.
 */
typedef std::vector<float> CNInferFeature;

/**
 * Object structured information.
 * Structured information for one object.
 */
typedef struct {
 public:
  std::string id;           ///> Classification id(label value).
  std::string track_id;     ///> Track result.
  float score;              ///> Label score.
  CNInferBoundingBox bbox;  ///> Object normalized coordinates.

  /**
   * Add attribute for this object.
   *
   * @param key Key of attribute, you can get this attribute by key. see GetAttribute.
   * @param value Attribute value.
   *
   * @return Returns true for attribute successfully added. Returns false when an attribute
   *         identified by the key already exists.
   *
   * @note This is a thread-safe function.
   */
  bool AddAttribute(const std::string& key, const CNInferAttr& value);

  /**
   * Add attribute for this object.
   *
   * @param attribute Attribute pair (key, value) to be added.
   *
   * @return Returns true for attribute successfully added. Returns false when an attribute
   *         identified by the key already exists.
   *
   * @note This is a thread-safe function.
   */
  bool AddAttribute(const std::pair<std::string, CNInferAttr>& attribute);

  /**
   * Get attribute by key.
   *
   * @param key Key for identify attribute. See AddAttribute
   *
   * @return Return attribute identified by the key. If the attribute identified by the key
   *         is not exists, CNInferAttr::id will be set to -1.
   *
   * @note This is a thread-safe function.
   */
  CNInferAttr GetAttribute(const std::string& key);

  /**
   * Add extended attribute for this object.
   *
   * @param key Key of attribute, you can get this attribute by key. see GetExtraAttribute.
   * @param value Attribute value.
   *
   * @return Returns true for attribute successfully added. Returns false when an attribute
   *         identified by the key already exists.
   *
   * @note This is a thread-safe function.
   */
  bool AddExtraAttribute(const std::string& key, const std::string& value);

  /**
   * Add extended attributes for this object.
   *
   * @param attributes Attributes to be add.
   *
   * @return Returns true for attribute successfully added. Returns false when an attribute
   *         identified by the key already exists.
   *
   * @note This is a thread-safe function.
   */
  bool AddExtraAttribute(const std::vector<std::pair<std::string, std::string>>& attributes);

  /**
   * Get extended attribute by key.
   *
   * @param key Key for identify attribute. See AddExtraAttribute
   *
   * @return Return attribute identified by the key. If the attribute identified by the key
   *         is not exists, NULL will be returned.
   *
   * @note This is a thread-safe function.
   */
  std::string GetExtraAttribute(const std::string& key);

  /**
   * Add feature value for this object.
   *
   * @param features Feature value.
   *
   * @return void.
   *
   * @note This is a thread-safe function.
   */
  void AddFeature(const CNInferFeature& features);

  /**
   * Get features for this object.
   *
   * @return Return features for this object.
   *
   * @note This is a thread-safe function.
   */
  std::vector<CNInferFeature> GetFeatures();

  void* user_data_ = nullptr;  ///> User data. User can store their own data here.

 private:
  // name >>> attribute
  std::map<std::string, CNInferAttr> attributes_;
  std::map<std::string, std::string> extra_attributes_;
  std::vector<CNInferFeature> features_;
  std::mutex attribute_mutex_;
  std::mutex feature_mutex_;
} CNInferObject;

/**
 * Structured information of one frame.
 */
struct CNFrameInfo {
  /**
   * Create an CNFrameInfo instance.
   *
   * @param stream_id Data stream alias. Identify which data stream the frame data comes from.
   * @param eos If true, CNDataFrame::flags will set to CN_FRAME_FLAG_EOS. Then, the modules
   *            do not have permission to process this frame. This frame should be handed over to the pipeline
   *            for processing.
   *
   * @return Return a shared_ptr of CNFrameInfo for success. Otherwise, NULL will be returned.
   */
  static std::shared_ptr<CNFrameInfo> Create(const std::string& stream_id, bool eos = false);
  uint32_t channel_idx;                              ///> Channel index.
  CNDataFrame frame;                                 ///> Frame data.
  std::vector<std::shared_ptr<CNInferObject>> objs;  ///> Structured informations of objects for this frame.
  ~CNFrameInfo();

 private:
  CNFrameInfo() {}
  DISABLE_COPY_AND_ASSIGN(CNFrameInfo);
  static std::mutex mutex_;
  static std::map<std::string, int> stream_count_map_;

 public:
  static int parallelism_;
};

/**
 * Limit the resource for each stream,
 * there will be no more than "parallelism" frames simultanously.
 * Disabled as default.
 */
void SetParallelism(int parallelism);

}  // namespace cnstream

#endif  // CNSTREAM_FRAME_HPP_
