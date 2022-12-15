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
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "opencv2/imgproc/imgproc.hpp"

#include "cnedk_buf_surface.h"
#include "cnedk_buf_surface_util.hpp"
#include "cnis/processor.h"
#include "cnstream_common.hpp"
#include "cnstream_frame.hpp"
#include "private/cnstream_allocator.hpp"
#include "util/cnstream_any.hpp"

namespace cnstream {

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
  ~CNDataFrame() {
    if (buf_surf) buf_surf.reset();
  }

 public:
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

  uint64_t frame_id = -1;  /*!< The frame index that incremented from 0. */

  cnedk::BufSurfWrapperPtr buf_surf = nullptr;

 private:
  std::mutex mtx;
  cv::Mat bgr_mat;  /*!< A Mat stores BGR image. */
};  // class CNDataFrame

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

using CnInferBbox = infer_server::CNInferBoundingBox;
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
  std::string id;                                   ///< The ID of the classification (label value).
  std::string track_id;                             ///< The tracking result.
  float score;                                      ///< The label score.
  CnInferBbox bbox;                                 ///< The relative object normalized coordinates.
  Collection collection;                            ///< User-defined structured information.
  std::shared_ptr<CNInferObject> parent = nullptr;  ///< The parent object ptr.
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
  bool AddFeature(const std::string& key, const CNInferFeature& feature);

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
  CNInferFeature GetFeature(const std::string& key);

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

/**
 * @brief Gets the absolute normalized coordinates of the object.
 *
 * @param[in] curr The object.
 *
 * @return Returns the bounding box coordinates.
 */
static inline CnInferBbox GetFullFovBbox(const CNInferObject* curr) {
  if (!curr) {
    return CnInferBbox(0.0, 0.0, 1.0, 1.0);
  }
  CnInferBbox parent = GetFullFovBbox(curr->parent.get());
  float x = parent.x + curr->bbox.x * parent.w;
  float y = parent.y + curr->bbox.y * parent.h;
  float w = curr->bbox.w * parent.w;
  float h = curr->bbox.h * parent.h;
  return CnInferBbox(x, y, w, h);
}

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
  std::mutex mutex_;                                  /// mutex of CNInferObjs
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

// Used by CNFrameInfo::Collection, the tags of data used by modules
static constexpr char kCNDataFrameTag[] = "CNDataFrame"; /*!< value type in CNFrameInfo::Collection : CNDataFramePtr. */
static constexpr char kCNInferObjsTag[] = "CNInferObjs"; /*!< value type in CNFrameInfo::Collection : CNInferObjsPtr. */

}  // namespace cnstream

#endif  // CNSTREAM_FRAME_VA_HPP_
