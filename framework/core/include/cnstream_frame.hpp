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

#include <memory>
#include <string>
#include <map>
#include <vector>

#include "cnstream_collection.hpp"
#include "cnstream_common.hpp"
#include "util/cnstream_any.hpp"

/**
 *  @file cnstream_frame.hpp
 *
 *  This file contains a declaration of the CNFrameInfo class.
 */
namespace cnstream {

class Module;
class Pipeline;

/**
 * @enum CNFrameFlag
 *
 * @brief Enumeration variables describing the mask of CNDataFrame.
 */
enum class CNFrameFlag {
  CN_FRAME_FLAG_EOS = 1 << 0,     /*!< This enumeration indicates the end of data stream. */
  CN_FRAME_FLAG_INVALID = 1 << 1, /*!< This enumeration indicates an invalid frame. */
  CN_FRAME_FLAG_REMOVED = 1 << 2  /*!< This enumeration indicates that the stream has been removed. */
};

/**
 * @class CNFrameInfo
 *
 * @brief CNFrameInfo is a class holding the information of a frame.
 *
 */
class CNFrameInfo : private NonCopyable {
 public:
  /**
   * @brief Creates a CNFrameInfo instance.
   *
   * @param[in] stream_id The data stream alias. Identifies which data stream the frame data comes from.
   * @param[in] eos  Whether this is the end of the stream. This parameter is set to false by default to
   *                 create a CNFrameInfo instance. If you set this parameter to true,
   *                 CNDataFrame::flags will be set to ``CN_FRAME_FLAG_EOS``. Then, the modules
   *                 do not have permission to process this frame. This frame should be handed over to
   *                 the pipeline for processing.
   *
   * @return Returns ``shared_ptr`` of ``CNFrameInfo`` if this function has run successfully. Otherwise, returns NULL.
   */
  static std::shared_ptr<CNFrameInfo> Create(const std::string& stream_id, bool eos = false,
                                            std::shared_ptr<CNFrameInfo> payload = nullptr);

CNS_IGNORE_DEPRECATED_PUSH

 private:
  CNFrameInfo() = default;

 public:
  /**
   * @brief Destructs CNFrameInfo object.
   *
   * @return No return value.
   */
  ~CNFrameInfo();
CNS_IGNORE_DEPRECATED_POP

  /**
   * @brief Checks whether DataFrame is end of stream (EOS) or not.
   *
   * @return Returns true if the frame is EOS. Returns false if the frame is not EOS.
   */
  bool IsEos() { return (flags & static_cast<size_t>(cnstream::CNFrameFlag::CN_FRAME_FLAG_EOS)) ? true : false; }

  /**
   * @brief Checks whether DataFrame is removed or not.
   *
   * @return Returns true if the frame is EOS. Returns false if the frame is not EOS.
   */
  bool IsRemoved() {
    return (flags & static_cast<size_t>(cnstream::CNFrameFlag::CN_FRAME_FLAG_REMOVED)) ? true : false;
  }

  /**
   * @brief Checks if DataFrame is valid or not.
   *
   *
   *
   * @return Returns true if frame is invalid, otherwise returns false.
   */
  bool IsInvalid() {
    return (flags & static_cast<size_t>(cnstream::CNFrameFlag::CN_FRAME_FLAG_INVALID)) ? true : false;
  }

  /**
   * @brief Sets index (usually the index is a number) to identify stream.
   *
   * @param[in] index Number to identify stream.
   *
   * @return No return value.
   *
   * @note This is only used for distributing each stream data to the appropriate thread.
   * We do not recommend SDK users to use this API because it will be removed later.
   */
  void SetStreamIndex(uint32_t index) { channel_idx = index; }

  /**
   * @brief Gets index number which identifies stream.
   *
   *
   *
   * @return Index number.
   *
   * @note This is only used for distributing each stream data to the appropriate thread.
   * We do not recommend SDK users to use this API because it will be removed later.
   */
  uint32_t GetStreamIndex() const { return channel_idx; }

  std::string stream_id;  /*!< The data stream aliases where this frame is located to. */
  int64_t timestamp = -1; /*!< The time stamp of this frame. */
  size_t flags = 0;       /*!< The mask for this frame, ``CNFrameFlag``. */

  // user-defined DataFrame，InferResult etc...
  CNS_DEPRECATED std::map<int, any> datas; /*!< (Deprecated) Uses CNFrameInfo::collection instead. */
  CNS_DEPRECATED std::mutex datas_lock_;             /*!< (Deprecated) Uses CNFrameInfo::collection instead. */

  Collection collection;                                    /*!< Stored structured data.  */
  std::shared_ptr<cnstream::CNFrameInfo> payload = nullptr; /*!< CNFrameInfo instance of parent pipeline. */

 private:
  /**
   * The below methods and members are used by the framework.
   */
  friend class Pipeline;
  mutable uint32_t channel_idx = INVALID_STREAM_IDX;        ///< The index of the channel, stream_index
  void SetModulesMask(uint64_t mask);
  uint64_t GetModulesMask();
  uint64_t MarkPassed(Module* current);  // return changed mask

  std::mutex mask_lock_;
  /* Identifies which modules have processed this data */
  uint64_t modules_mask_ = 0;
};

/*!
 * Defines an alias for the std::shared_ptr<CNFrameInfo>. CNFrameInfoPtr now denotes a shared pointer of frame
 * information.
 */
using CNFrameInfoPtr = std::shared_ptr<CNFrameInfo>;

}  // namespace cnstream

#endif  // CNSTREAM_FRAME_HPP_
