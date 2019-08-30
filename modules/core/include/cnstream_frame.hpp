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

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "cnstream_common.hpp"
#include "cnstream_syncmem.hpp"

#ifndef CN_MAX_PLANES
#define CN_MAX_PLANES 6
#endif

namespace cnstream {

typedef enum {
  CN_INVALID = -1,
  CN_PIXEL_FORMAT_YUV420_NV21 = 0,
  CN_PIXEL_FORMAT_YUV420_NV12,
  CN_PIXEL_FORMAT_BGR24,
  CN_PIXEL_FORMAT_RGB24
} CNDataFormat;

typedef struct {
  enum DevType { INVALID = -1, CPU = 0, MLU = 1 } dev_type = INVALID;
  int dev_id = 0;
  int ddr_channel = 0;
} DevContext;

enum CNFrameFlag { CN_FRAME_FLAG_EOS = 1 << 0 };

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

class Module;
struct CNDataFrame {
  int flags = 0;
  std::string stream_id;
  int64_t frame_id;
  int64_t timestamp;
  int width;
  int height;
  std::array<int, CN_MAX_PLANES> strides;
  std::array<std::shared_ptr<CNSyncedMemory>, CN_MAX_PLANES> data;
  CNDataFormat fmt;
  DevContext ctx;

  CNDataFrame();

  ~CNDataFrame();

  int GetPlanes() const { return CNGetPlanes(fmt); }

  size_t GetPlaneBytes(int plane_idx) const;

  size_t GetBytes() const;

  void CopyFrameFromMLU(int dev_id, int ddr_channel, CNDataFormat fmt, int width, int height, void** data,
                        const uint32_t* stride);

  void TransformMemory(DevContext::DevType dst_dev_type);

  void ReallocMemory(int width, int height);

  void ReallocMemory(CNDataFormat format);

  /*the pipeline stages (modules) info
   */
  void SetModuleMask(Module* module, Module* current);
  unsigned long GetModulesMask(Module* module);
  void ClearModuleMask(Module* module);

  /* the pipeline stages info */
  /*
    @brief add EOS mask. threadsafe function.
    @param
      module[in]: the mask is from this module.
    @return
      return mask after add mask.
   */
  unsigned long AddEOSMask(Module* module);

 private:
  void* mlu_data = nullptr;

  std::mutex modules_mutex;
  std::map<unsigned int, unsigned long> module_mask_map_;

  std::mutex eos_mutex;
  unsigned long eos_mask = 0;
};  // struct CNDataFrame

typedef struct {
  float x, y, w, h;
} CNInferBoundingBox;

typedef struct {
  int id = -1;
  int value = -1;
  float score = 0;
} CNInferAttr;

typedef std::vector<float> CNInferFeature;

typedef struct {
 public:
  std::string id;
  std::string track_id;
  float score;
  CNInferBoundingBox bbox;

  // threadsafe function
  bool AddAttribute(const std::string& key, const CNInferAttr& value);

  // threadsafe function
  bool AddAttribute(const std::pair<std::string, CNInferAttr>& attributes);

  // threadsafe function
  CNInferAttr GetAttribute(const std::string& key);

  // threadsafe function
  bool AddExtraAttribute(const std::string& key, const std::string& value);

  // threadsafe function
  bool AddExtraAttribute(const std::vector<std::pair<std::string, std::string>>& attributes);

  // threadsafe function
  std::string GetExtraAttribute(const std::string& key);

  // threadsafe function
  void AddFeature(const CNInferFeature& features);

  // threadsafe function
  std::vector<CNInferFeature> GetFeatures();

  void* user_data_ = nullptr;

 private:
  // name >>> attribute
  std::map<std::string, CNInferAttr> attributes_;
  std::map<std::string, std::string> extra_attributes_;
  std::vector<CNInferFeature> features_;
  std::mutex attribute_mutex_;
  std::mutex feature_mutex_;
} CNInferObject;

struct CNFrameInfo {
  static std::shared_ptr<CNFrameInfo> Create(const std::string& stream_id);
  uint32_t channel_idx; /*used by the framework*/
  CNDataFrame frame;
  std::vector<std::shared_ptr<CNInferObject>> objs;
  ~CNFrameInfo();

 private:
  CNFrameInfo() {}
  DISABLE_COPY_AND_ASSIGN(CNFrameInfo);
  static std::mutex mutex_;
  static std::map<std::string, int> stream_count_map_;

 public:
  static int parallelism_;
};

/* limit the resource for each stream,
 *    there will be no more than "parallelism" frames simultanously.
 *  Disabled as default.
 */
void SetParallelism(int parallelism);

}  // namespace cnstream

#endif  // CNSTREAM_FRAME_HPP_
