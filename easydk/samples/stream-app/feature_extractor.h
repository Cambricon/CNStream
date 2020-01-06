#ifndef FEATURE_EXTRACTOR_H_
#define FEATURE_EXTRACTOR_H_

#include <memory>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <string>
#include <utility>
#include <vector>

#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"
#include "easytrack/easy_track.h"

class FeatureExtractor {
 public:
  ~FeatureExtractor();
  bool Init(const std::string& model_path, const std::string& func_name, int dev_id = 0, uint32_t batch_size = 1);
  void Destroy();

  /*******************************************************
   * @brief inference and extract feature of an object
   * @param
   *   frame[in] full image
   *   obj[in] detected object
   * @return return a 128 dimension vector as feature of
   *         object.
   * *****************************************************/
  std::vector<float> ExtractFeature(const edk::TrackFrame& frame, const edk::DetectObject& obj);

 private:
  void Preprocess(const cv::Mat& img);

  edk::EasyInfer infer_;
  edk::MluMemoryOp mem_op_;
  std::shared_ptr<edk::ModelLoader> model_ = nullptr;
  std::mutex mlu_proc_mutex_;
  int device_id_ = 0;
  uint32_t batch_size_ = 1;
  void** input_cpu_ptr_ = nullptr;
  void** output_cpu_ptr_ = nullptr;
  void** input_mlu_ptr_ = nullptr;
  void** output_mlu_ptr_ = nullptr;
  bool extract_feature_mlu_ = false;
};  // class FeatureExtractor

#endif  // FEATURE_EXTRACTOR_H_
