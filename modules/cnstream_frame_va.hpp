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

#ifndef CNSTREAM_FRAME_VA_HPP_
#define CNSTREAM_FRAME_VA_HPP_

/**
 *  @file cnstream_frame_va.hpp
 *
 *  This file contains a declaration of the CNFrameData & CNInferObject struct and its substructure.
 */
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#if (CV_MAJOR_VERSION >= 3)
#include <opencv2/imgcodecs/imgcodecs.hpp>
#endif

#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <utility>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_frame.hpp"
#include "cnstream_syncmem.hpp"
#include "private/cnstream_allocator.hpp"
#include "util/cnstream_any.hpp"

constexpr int CN_MAX_PLANES = 6;

namespace cnstream {
/**
 * @enum CNDataFormat
 *
 * @brief Enumeration variables describling the pixel format of the data in CNDataFrame.
 */
enum class CNDataFormat {
  CN_INVALID = -1,                 /*!< This frame is invalid. */
  CN_PIXEL_FORMAT_YUV420_NV21 = 0, /*!< This frame is in the YUV420SP(NV21) format. */
  CN_PIXEL_FORMAT_YUV420_NV12,     /*!< This frame is in the YUV420sp(NV12) format. */
  CN_PIXEL_FORMAT_BGR24,           /*!< This frame is in the BGR24 format. */
  CN_PIXEL_FORMAT_RGB24,           /*!< This frame is in the RGB24 format. */
  CN_PIXEL_FORMAT_ARGB32,          /*!< This frame is in the ARGB32 format. */
  CN_PIXEL_FORMAT_ABGR32,          /*!< This frame is in the ABGR32 format. */
  CN_PIXEL_FORMAT_RGBA32,          /*!< This frame is in the RGBA32 format. */
  CN_PIXEL_FORMAT_BGRA32           /*!< This frame is in the BGRA32 format. */
};

/**
 * @struct DevContext
 *
 * @brief DevContext is a structure holding the information that CNDataFrame data is allocated by CPU or MLU.
 */
struct DevContext {
  enum class DevType {
    INVALID = -1,                /*!< Invalid device type. */
    CPU = 0,                     /*!< The data is allocated by CPU. */
    MLU = 1,                     /*!< The data is allocated by MLU. */
  } dev_type = DevType::INVALID; /*!< Device type. The default value is ``INVALID``.*/
  int dev_id = 0;                /*!< Ordinal device ID. */
  int ddr_channel = 0;           /*!< Ordinal channel ID for MLU. The value should be in the range [0, 4). */
};

// Group:Video Analysis Function
/**
 * @brief Gets image plane number by a specified image format.
 *
 * @param[in] fmt The format of the image.
 *
 * @retval 0: Unsupported image format.
 * @retval >0: Image plane number.
 */
inline int CNGetPlanes(CNDataFormat fmt) {
  switch (fmt) {
    case CNDataFormat::CN_PIXEL_FORMAT_BGR24:
    case CNDataFormat::CN_PIXEL_FORMAT_RGB24:
      return 1;
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12:
    case CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21:
      return 2;
    default:
      return 0;
  }
  return 0;
}

/**
 * @class CNDataFrame
 *
 * @brief CNDataFrame is a class holding a data frame and the frame description.
 */
class CNDataFrame : public NonCopyable {
 public:
  /**
   * @brief Constructs an object.
   *
   * @return No return value.
   */
  CNDataFrame() = default;
  /**
   * @brief Destructs an object.
   *
   * @return No return value.
   */
  ~CNDataFrame() = default;
  /**
   * @brief Gets plane count for a specified frame.
   *
   *
   * @return Returns the plane count of this frame.
   */
  int GetPlanes() const { return CNGetPlanes(fmt); }
  /**
   * @brief Gets the number of bytes in a specified plane.
   *
   * @param[in] plane_idx The index of the plane. The index increments from 0.
   *
   * @return Returns the number of bytes in the plane.
   */
  size_t GetPlaneBytes(int plane_idx) const;
  /**
   * @brief Gets the number of bytes in a frame.
   *
   * @return Returns the number of bytes in a frame.
   */
  size_t GetBytes() const;

 public:
  /*!
   * @brief Synchronizes the source data into ::CNSyncedMemory.
   *
   * @param[in] ptr_src The source data's address. This API internally judges the address is MLU memory or not.
   * @param[in] dst_mlu The flag shows whether synchronizes the data to MLU memory.
   *
   * @note Sets the ``width``,``height``,``fmt``,``ctx``,``stride``,``dst_device_id``,``deAllocator_`` before calling
   * this function.
   * There are 5 situations:
   * 1. Reuse codec's buffer and do not copy anything. Just assign the ptr_src to CNSyncedMemory mlu_ptr_.
   * 2. This API allocates MLU buffer, and copy the source MLU data to the allocated buffer as the MLU destination.
   * 3. This API allocates MLU buffer, and copy the source CPU data to the allocated buffer as the MLU destination.
   * 4. This API allocates CPU buffer, and copy the source MLU data to the allocated buffer as the CPU destination.
   * 5. This API allocates CPU buffer, and copy the source CPU data to the allocated buffer as the CPU destination.
   * Whatever which situation happens, ::CNSyncedMemory doesn't own the buffer and it isn't responsible for releasing
   * the data.
   */
  void CopyToSyncMem(void** ptr_src, bool dst_mlu);
  /**
   * @brief Converts data to the BGR format.
   *
   * @return Returns data with OpenCV mat type.
   *
   * @note This function is called after CNDataFrame::CopyToSyncMem() is invoked.
   */
  cv::Mat ImageBGR();
  /**
   * @brief Checks whether there is BGR image stored.
   *
   * @return Returns true if has BGR image, otherwise returns false.
   */
  bool HasBGRImage() {
    std::lock_guard<std::mutex> lk(mtx);
    if (bgr_mat.empty()) return false;
    return true;
  }

  /**
   * @brief Synchronizes source data to specific device, and resets ctx.dev_id to device_id when synced, for
   * multi-device case.
   * @param[in] device_id The device id.
   * @return No return value.
   */
  void CopyToSyncMemOnDevice(int device_id);

  std::shared_ptr<void> cpu_data = nullptr;            /*!< A shared pointer to the CPU data. */
  std::shared_ptr<void> mlu_data = nullptr;            /*!< A shared pointer to the MLU data. */
  std::unique_ptr<CNSyncedMemory> data[CN_MAX_PLANES]; /*!< Synchronizes data helper. */
  uint64_t frame_id = -1;                              /*!< The frame index that incremented from 0. */

  CNDataFormat fmt;                                         /*!< The format of the frame. */
  int width;                                                /*!< The width of the frame. */
  int height;                                               /*!< The height of the frame. */
  int stride[CN_MAX_PLANES];                                /*!< The strides of the frame. */
  DevContext ctx;                                           /*!< The device context of SOURCE data (ptr_mlu/ptr_cpu). */
  std::unique_ptr<IDataDeallocator> deAllocator_ = nullptr; /*!< The dedicated deallocator for CNDecoder buffer. */
  std::atomic<int> dst_device_id{-1};                       /*!< The device context of SyncedMemory. */

 private:
  std::mutex mtx;
  cv::Mat bgr_mat; /*!< A Mat stores BGR image. */
};                 // class CNDataFrame

/**
 * @struct CNInferBoundingBox
 *
 * @brief CNInferBoundingBox is a structure holding the bounding box information of a detected object in normalized
 * coordinates.
 */
struct CNInferBoundingBox {
  float x;  ///< The x-axis coordinate in the upper left corner of the bounding box.
  float y;  ///< The y-axis coordinate in the upper left corner of the bounding box.
  float w;  ///< The width of the bounding box.
  float h;  ///< The height of the bounding box.
};

/**
 * @struct CNInferAttr
 *
 * @brief CNInferAttr is a structure holding the classification properties of an object.
 */
typedef struct {
  int id = -1;      ///< The unique ID of the classification. The value -1 means invalid.
  int value = -1;   ///< The label value of the classification.
  float score = 0;  ///< The label score of the classification.
} CNInferAttr;

/**
 *  Defines an alias for std::vector<float>. CNInferFeature contains one kind of inference feature.
 */
using CNInferFeature = std::vector<float>;

/**
 * Defines an alias for std::vector<std::pair<std::string, std::vector<float>>>. CNInferFeatures contains all kinds of
 * features for one object.
 */
using CNInferFeatures = std::vector<std::pair<std::string, CNInferFeature>>;

/**
 * Defines an alias for std::vector<std::pair<std::string, std::string>>.
 */
using StringPairs = std::vector<std::pair<std::string, std::string>>;

/**
 * @class CNInferObject
 *
 * @brief CNInferObject is a class holding the information of an object.
 */
class CNInferObject {
 public:
CNS_IGNORE_DEPRECATED_PUSH
  /**
   * @brief Constructs an instance storing inference results.
   *
   * @return No return value.
   */
  CNInferObject() = default;
  /**
   * @brief Constructs an instance.
   *
   * @return No return value.
   */
  ~CNInferObject() = default;
CNS_IGNORE_DEPRECATED_POP
  std::string id;           ///< The ID of the classification (label value).
  std::string track_id;     ///< The tracking result.
  float score;              ///< The label score.
  CNInferBoundingBox bbox;  ///< The object normalized coordinates.
  CNS_DEPRECATED std::map<int, any> datas;  ///< (Deprecated) User-defined structured information.
  Collection collection;    ///< User-defined structured information.

  /**
   * @brief Adds the key of an attribute to a specified object.
   *
   * @param[in] key The Key of the attribute you want to add to. See GetAttribute().
   * @param[in] value The value of the attribute.
   *
   * @return Returns true if the attribute has been added successfully. Returns false if the attribute
   *         already existed.
   *
   * @note This is a thread-safe function.
   */
  bool AddAttribute(const std::string& key, const CNInferAttr& value);

  /**
   * @brief Adds the key pairs of an attribute to a specified object.
   *
   * @param[in] attribute The attribute pair (key, value) to be added.
   *
   * @return Returns true if the attribute has been added successfully. Returns false if the attribute
   *         has already existed.
   *
   * @note This is a thread-safe function.
   */
  bool AddAttribute(const std::pair<std::string, CNInferAttr>& attribute);

  /**
   * @brief Gets an attribute by key.
   *
   * @param[in] key The key of an attribute you want to query. See AddAttribute().
   *
   * @return Returns the attribute key. If the attribute
   *         does not exist, CNInferAttr::id will be set to -1.
   *
   * @note This is a thread-safe function.
   */
  CNInferAttr GetAttribute(const std::string& key);

  /**
   * @brief Adds the key of the extended attribute to a specified object.
   *
   * @param[in] key The key of an attribute. You can get this attribute by key. See GetExtraAttribute().
   * @param[in] value The value of the attribute.
   *
   * @return Returns true if the attribute has been added successfully. Returns false if the attribute
   *         has already existed in the object.
   *
   * @note This is a thread-safe function.
   */
  bool AddExtraAttribute(const std::string& key, const std::string& value);

  /**
   * @brief Adds the key pairs of the extended attributes to a specified object.
   *
   * @param[in] attributes Attributes to be added.
   *
   * @return Returns true if the attribute has been added successfully. Returns false if the attribute
   *         has already existed.
   * @note This is a thread-safe function.
   */
  bool AddExtraAttributes(const std::vector<std::pair<std::string, std::string>>& attributes);

  /**
   * @brief Gets an extended attribute by key.
   *
   * @param[in] key The key of an identified attribute. See AddExtraAttribute().
   *
   * @return Returns the attribute that is identified by the key. If the attribute
   *         does not exist, returns NULL.
   *
   * @note This is a thread-safe function.
   */
  std::string GetExtraAttribute(const std::string& key);

  /**
   * @brief Removes an attribute by key.
   *
   * @param[in] key The key of an attribute you want to remove. See AddAttribute.
   *
   * @return Return true.
   *
   * @note This is a thread-safe function.
   */
  bool RemoveExtraAttribute(const std::string& key);

  /**
   * @brief Gets all extended attributes of an object.
   *
   * @return Returns all extended attributes.
   *
   * @note This is a thread-safe function.
   */
  StringPairs GetExtraAttributes();

  /**
   * @brief Adds the key of feature to a specified object.
   *
   * @param[in] key The Key of feature you want to add the feature to. See GetFeature.
   * @param[in] value The value of the feature.
   *
   * @return Returns true if the feature is added successfully. Returns false if the feature
   *         identified by the key already exists.
   *
   * @note This is a thread-safe function.
   */
  bool AddFeature(const std::string &key, const CNInferFeature &feature);

  /**
   * @brief Gets an feature by key.
   *
   * @param[in] key The key of an feature you want to query. See AddFeature.
   *
   * @return Return the feature of the key. If the feature identified by the key
   *         is not exists, CNInferFeature will be empty.
   *
   * @note This is a thread-safe function.
   */
  CNInferFeature GetFeature(const std::string &key);

  /**
   * @brief Gets the features of an object.
   *
   * @return Returns the features of an object.
   *
   * @note This is a thread-safe function.
   */
  CNInferFeatures GetFeatures();

 private:
  std::map<std::string, CNInferAttr> attributes_;
  std::map<std::string, std::string> extra_attributes_;
  std::map<std::string, CNInferFeature> features_;
  std::mutex attribute_mutex_;
  std::mutex feature_mutex_;
};

/*!
 * Defines an alias for the std::shared_ptr<CNInferObject>. CNInferObjectPtr now denotes a shared pointer of inference
 * objects.
 */
using CNInferObjectPtr = std::shared_ptr<CNInferObject>;

/**
 * @struct CNInferObjs
 *
 * @brief CNInferObjs is a structure holding inference results.
 */
struct CNInferObjs : public NonCopyable {
  std::vector<std::shared_ptr<CNInferObject>> objs_;  /// The objects storing inference results.
  std::mutex mutex_;   /// mutex of CNInferObjs
};

/**
 * @struct InferData
 *
 * @brief InferData is a structure holding the information of raw inference input & outputs.
 */
struct InferData {
  // infer input
  CNDataFormat input_fmt_;               /*!< The input image's pixel format.*/
  int input_width_;                      /*!< The input image's width.*/
  int input_height_;                     /*!< The input image's height. */
  std::shared_ptr<void> input_cpu_addr_; /*!< The input data's CPU address.*/
  size_t input_size_;                    /*!< The input data's size. */

  // infer output
  std::vector<std::shared_ptr<void>> output_cpu_addr_; /*!< The corresponding inference outputs to the input data. */
  std::vector<size_t> output_sizes_;                   /*!< The inference outputs' sizes.*/
  size_t output_num_;                                  /*!< The inference output count.*/
};

/**
 * @struct CNInferData
 *
 * @brief CNInferData is a structure holding a map between module name and InferData.
 */
struct CNInferData : public NonCopyable {
  std::map<std::string, std::vector<std::shared_ptr<InferData>>> datas_map_;
  /*!< The map between module name and InferData.*/
  std::mutex mutex_; /*!< Inference data mutex.*/
};

/*!
 * Defines an alias for the std::shared_ptr<CNDataFrame>.
 */
using CNDataFramePtr = std::shared_ptr<CNDataFrame>;
/*!
 * Defines an alias for the std::shared_ptr<CNInferObjs>.
 */
using CNInferObjsPtr = std::shared_ptr<CNInferObjs>;
/*!
 * Defines an alias for the std::vector<std::shared_ptr<CNInferObject>>.
 */
using CNObjsVec = std::vector<std::shared_ptr<CNInferObject>>;
/*!
 * Defines an alias for the std::shared_ptr<CNInferData>.
 */
using CNInferDataPtr = std::shared_ptr<CNInferData>;

/*
 * @deprecated
 * user-defined data structure: Key-value
 *   key type-- int
 *   value type -- cnstream::any, since we store it in an map, std::share_ptr<T> should be used
 */
CNS_DEPRECATED static constexpr int CNDataFramePtrKey = 0;
CNS_DEPRECATED static constexpr int CNInferObjsPtrKey = 1;
CNS_DEPRECATED static constexpr int CNInferDataPtrKey = 2;

CNS_IGNORE_DEPRECATED_PUSH
// Group:Video Analysis Function
/**
 *  @brief This helper will be deprecated in the future versions. Uses ``Collection::Get<CNDataFramePtr>(kCNDataFrameTag)`` instead.
 */
CNS_DEPRECATED static inline
CNDataFramePtr GetCNDataFramePtr(std::shared_ptr<CNFrameInfo> frameInfo) {
  std::lock_guard<std::mutex> guard(frameInfo->datas_lock_);
  return cnstream::any_cast<CNDataFramePtr>(frameInfo->datas[CNDataFramePtrKey]);
}

// Group:Video Analysis Function
/**
 *  @brief This helper will be deprecated in the future versions. Uses ``Collection::Get<CNInferObjsPtr>(kCNInferObjsTag)`` instead.
 */
CNS_DEPRECATED static inline
CNInferObjsPtr GetCNInferObjsPtr(std::shared_ptr<CNFrameInfo> frameInfo) {
  std::lock_guard<std::mutex> guard(frameInfo->datas_lock_);
  return cnstream::any_cast<CNInferObjsPtr>(frameInfo->datas[CNInferObjsPtrKey]);
}

// Group:Video Analysis Function
/**
 *  @brief This helper will be deprecated in the future versions. Uses ``Collection::Get<CNInferDataPtr>(kCNInferDataTag)`` instead.
 */
CNS_DEPRECATED static inline
CNInferDataPtr GetCNInferDataPtr(std::shared_ptr<CNFrameInfo> frameInfo) {
  std::lock_guard<std::mutex> guard(frameInfo->datas_lock_);
  return cnstream::any_cast<CNInferDataPtr>(frameInfo->datas[CNInferDataPtrKey]);
}
CNS_IGNORE_DEPRECATED_POP

// Used by CNFrameInfo::Collection, the tags of data used by modules
static constexpr char kCNDataFrameTag[] = "CNDataFrame"; /*!< value type in CNFrameInfo::Collection : CNDataFramePtr. */
static constexpr char kCNInferObjsTag[] = "CNInferObjs"; /*!< value type in CNFrameInfo::Collection : CNInferObjsPtr. */
static constexpr char kCNInferDataTag[] = "CNInferData"; /*!< value type in CNFrameInfo::Collection : CNInferDataPtr. */

}  // namespace cnstream

#endif  // CNSTREAM_FRAME_VA_HPP_
