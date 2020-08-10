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

#include <easybang/resize_and_colorcvt.h>
#include <easyinfer/easy_infer.h>
#include <easyinfer/mlu_memory_op.h>
#include <glog/logging.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cnrt.h"
#include "infer_engine.hpp"
#include "infer_resource.hpp"
#include "infer_task.hpp"
#include "postproc.hpp"
#include "queuing_server.hpp"

#include "batching_done_stage.hpp"
#include "cnstream_frame_va.hpp"
#include "perf_manager.hpp"

namespace cnstream {

std::vector<std::shared_ptr<InferTask>> H2DBatchingDoneStage::BatchingDone(const BatchingDoneInput& finfos) {
  std::vector<InferTaskSptr> tasks;
  InferTaskSptr task;
  QueuingTicket cpu_input_res_ticket = cpu_input_res_->PickUpNewTicket();
  QueuingTicket mlu_input_res_ticket = mlu_input_res_->PickUpNewTicket();
  task = std::make_shared<InferTask>([cpu_input_res_ticket, mlu_input_res_ticket, this, finfos]() -> int {
    QueuingTicket cir_ticket = cpu_input_res_ticket;
    QueuingTicket mir_ticket = mlu_input_res_ticket;
    IOResValue cpu_value = this->cpu_input_res_->WaitResourceByTicket(&cir_ticket);
    IOResValue mlu_value = this->mlu_input_res_->WaitResourceByTicket(&mir_ticket);
    edk::MluMemoryOp mem_op;
    mem_op.SetLoader(this->model_);

    mem_op.MemcpyInputH2D(mlu_value.ptrs, cpu_value.ptrs, 1);

    this->cpu_input_res_->DeallingDone();
    this->mlu_input_res_->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> ResizeConvertBatchingDoneStage::BatchingDone(const BatchingDoneInput& finfos) {
  std::vector<InferTaskSptr> tasks;
  InferTaskSptr task;
  QueuingTicket rcop_res_ticket = rcop_res_->PickUpNewTicket();
  QueuingTicket mlu_input_res_ticket = mlu_input_res_->PickUpNewTicket();
  task = std::make_shared<InferTask>([rcop_res_ticket, mlu_input_res_ticket, this, finfos]() -> int {
    QueuingTicket rcopr_ticket = rcop_res_ticket;
    QueuingTicket mir_tickett = mlu_input_res_ticket;
    std::shared_ptr<RCOpValue> rcop_value = this->rcop_res_->WaitResourceByTicket(&rcopr_ticket);
    IOResValue mlu_value = this->mlu_input_res_->WaitResourceByTicket(&mir_tickett);
    CHECK_EQ(mlu_value.datas.size(), 1) << "Internal error, maybe model input num not 1";

    std::shared_ptr<CNFrameInfo> info = nullptr;
    std::string pts_str;

    if (perf_manager_) {
      info = finfos.back().first;
      if (!info->IsEos()) {
        CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(info->datas[CNDataFramePtrKey]);
        pts_str = std::to_string(frame->frame_id * 100 + info->GetStreamIndex());
        perf_manager_->Record(perf_type_, PerfManager::GetPrimaryKey(), pts_str, "resize_start_time");
        perf_manager_->Record(perf_type_, PerfManager::GetPrimaryKey(), pts_str, "resize_cnt",
                              std::to_string(finfos.size()));
      }
    }

    cnrtMemset(mlu_value.datas[0].ptr, 0, mlu_value.datas[0].batch_offset * finfos.size());

    if (!rcop_value->op.SyncOneOutput(mlu_value.datas[0].ptr)) {
      throw CnstreamError("resize convert failed.");
    }

    if (perf_manager_) {
      perf_manager_->Record(perf_type_, PerfManager::GetPrimaryKey(), pts_str, "resize_end_time");
    }

    this->rcop_res_->DeallingDone();
    this->mlu_input_res_->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

InferBatchingDoneStage::InferBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model,
                                               cnstream::CNDataFormat model_input_fmt,
                                               uint32_t batchsize, int dev_id,
                                               std::shared_ptr<MluInputResource> mlu_input_res,
                                               std::shared_ptr<MluOutputResource> mlu_output_res)
    : BatchingDoneStage(model, batchsize, dev_id), model_input_fmt_(model_input_fmt),
      mlu_input_res_(mlu_input_res), mlu_output_res_(mlu_output_res) {
  easyinfer_ = std::make_shared<edk::EasyInfer>();
#ifdef CNS_MLU100
  easyinfer_->Init(model_, batchsize_, dev_id);
#elif CNS_MLU270
  easyinfer_->Init(model_, 1, dev_id);
#endif
}

InferBatchingDoneStage::~InferBatchingDoneStage() {}

std::shared_ptr<edk::MluTaskQueue> InferBatchingDoneStage::SharedMluQueue() const { return easyinfer_->GetMluQueue(); }

std::vector<std::shared_ptr<InferTask>> InferBatchingDoneStage::BatchingDone(const BatchingDoneInput& finfos) {
  std::vector<InferTaskSptr> tasks;
  InferTaskSptr task;
  QueuingTicket mlu_input_res_ticket = mlu_input_res_->PickUpNewTicket();
  QueuingTicket mlu_output_res_ticket = mlu_output_res_->PickUpNewTicket();
  task = std::make_shared<InferTask>([mlu_input_res_ticket, mlu_output_res_ticket, this, finfos]() -> int {
    QueuingTicket mir_ticket = mlu_input_res_ticket;
    QueuingTicket mor_ticket = mlu_output_res_ticket;
    IOResValue mlu_input_value = this->mlu_input_res_->WaitResourceByTicket(&mir_ticket);
    IOResValue mlu_output_value = this->mlu_output_res_->WaitResourceByTicket(&mor_ticket);

    std::shared_ptr<CNFrameInfo> info = nullptr;
    std::string pts_str;
    if (perf_manager_) {
      info = finfos.back().first;
      if (!info->IsEos()) {
        CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(info->datas[CNDataFramePtrKey]);
        pts_str = std::to_string(frame->frame_id * 100 + info->GetStreamIndex());
        perf_manager_->Record(perf_type_, PerfManager::GetPrimaryKey(), pts_str, "infer_start_time");
        perf_manager_->Record(perf_type_, PerfManager::GetPrimaryKey(), pts_str, "infer_cnt",
                              std::to_string(finfos.size()));
      }
    }
    if (!dump_resized_image_dir_.empty()) {
      int batch_offset = mlu_input_value.datas[0].batch_offset;
      int frame_num = finfos.size();
      int len = batch_offset * frame_num;
      std::vector<char> cpu_input_value(len);
      for (const auto& data : mlu_input_value.datas) {
        cnrtMemcpy(reinterpret_cast<void*>(cpu_input_value.data()), data.ptr, len, CNRT_MEM_TRANS_DIR_DEV2HOST);
        for (int  i = 0; i < frame_num; i++) {
          info = finfos[i].first;
          CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(info->datas[CNDataFramePtrKey]);
          char *img = reinterpret_cast<char*>(cpu_input_value.data()) + batch_offset * i;
          cv::Mat bgr(data.shape.h, data.shape.w, CV_8UC3);
          cv::Mat bgra(data.shape.h, data.shape.w, CV_8UC4, img);
          switch (model_input_fmt_) {
            case CN_PIXEL_FORMAT_RGBA32:
              cv::cvtColor(bgra, bgr, cv::COLOR_RGBA2BGR);
              break;
            case CN_PIXEL_FORMAT_BGRA32:
              cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
              break;
            case CN_PIXEL_FORMAT_ARGB32: {
              std::vector<cv::Mat> chns;
              cv::split(bgra, chns);
              std::swap(chns[1], chns[3]);
              chns.erase(chns.begin());
              cv::merge(chns, bgr);
              break;
            }
            case CN_PIXEL_FORMAT_ABGR32: {
              std::vector<cv::Mat> chns;
              cv::split(bgra, chns);
              chns.erase(chns.begin());
              cv::merge(chns, bgr);
              break;
            }
            default:
              LOG(ERROR) << "Unsupported fmt, dump resized image failed." << "fmt :" << model_input_fmt_;
              break;
          }
          std::string stream_index = std::to_string(info->GetStreamIndex());
          if (0 != access(dump_resized_image_dir_.c_str(), 0)) {
            mkdir(dump_resized_image_dir_.c_str(), 0777);
          }
          int obj_id = 0;
          std::string dump_img_path_prefix = dump_resized_image_dir_ + "/stream" + stream_index + "_frame" +
                                             std::to_string(frame->frame_id);
          std::string dump_img_path = dump_img_path_prefix + "_obj" + std::to_string(obj_id) + ".jpg";
          while (0 == access(dump_img_path.c_str(), 0)) {
            obj_id++;
            dump_img_path = dump_img_path_prefix + "_obj" + std::to_string(obj_id) + ".jpg";
          }

          cv::imwrite(dump_img_path, bgr);
        }
      }
    }
    this->easyinfer_->Run(mlu_input_value.ptrs, mlu_output_value.ptrs);

    if (perf_manager_) {
      perf_manager_->Record(perf_type_, PerfManager::GetPrimaryKey(), pts_str, "infer_end_time");
    }

    this->mlu_input_res_->DeallingDone();
    this->mlu_output_res_->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> D2HBatchingDoneStage::BatchingDone(const BatchingDoneInput& finfos) {
  std::vector<InferTaskSptr> tasks;
  InferTaskSptr task;
  QueuingTicket mlu_output_res_ticket = mlu_output_res_->PickUpNewTicket();
  QueuingTicket cpu_output_res_ticket = cpu_output_res_->PickUpNewTicket();
  task = std::make_shared<InferTask>([mlu_output_res_ticket, cpu_output_res_ticket, this]() -> int {
    QueuingTicket mor_ticket = mlu_output_res_ticket;
    QueuingTicket cor_ticket = cpu_output_res_ticket;
    IOResValue mlu_output_value = this->mlu_output_res_->WaitResourceByTicket(&mor_ticket);
    IOResValue cpu_output_value = this->cpu_output_res_->WaitResourceByTicket(&cor_ticket);
    edk::MluMemoryOp mem_op;
    mem_op.SetLoader(this->model_);
#ifdef CNS_MLU100
    mem_op.MemcpyOutputD2H(cpu_output_value.ptrs, mlu_output_value.ptrs, this->batchsize_);
#elif CNS_MLU270
    mem_op.MemcpyOutputD2H(cpu_output_value.ptrs, mlu_output_value.ptrs, 1);
#endif
    this->mlu_output_res_->DeallingDone();
    this->cpu_output_res_->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> PostprocessingBatchingDoneStage::BatchingDone(
    const BatchingDoneInput& finfos) {
  if (cpu_output_res_ != nullptr) {
    return BatchingDone(finfos, cpu_output_res_);
  } else if (mlu_output_res_ != nullptr) {
    return BatchingDone(finfos, mlu_output_res_);
  } else {
    assert(false);
  }
  return {};
}

std::vector<std::shared_ptr<InferTask>> PostprocessingBatchingDoneStage::BatchingDone(
    const BatchingDoneInput& finfos,
    const std::shared_ptr<CpuOutputResource> &cpu_output_res) {
  std::vector<InferTaskSptr> tasks;
  for (int bidx = 0; bidx < static_cast<int>(finfos.size()); ++bidx) {
    auto finfo = finfos[bidx];
    QueuingTicket cpu_output_res_ticket;
    if (0 == bidx) {
      cpu_output_res_ticket = cpu_output_res->PickUpNewTicket(true);
    } else {
      cpu_output_res_ticket = cpu_output_res->PickUpTicket(true);
    }
    InferTaskSptr task = std::make_shared<InferTask>([cpu_output_res_ticket,
                                                      cpu_output_res,
                                                      this,
                                                      finfo,
                                                      bidx]() -> int {
      QueuingTicket cor_ticket = cpu_output_res_ticket;
      IOResValue cpu_output_value = cpu_output_res->WaitResourceByTicket(&cor_ticket);
      std::vector<float*> net_outputs;
      for (size_t output_idx = 0; output_idx < cpu_output_value.datas.size(); ++output_idx) {
        net_outputs.push_back(reinterpret_cast<float*>(cpu_output_value.datas[output_idx].Offset(bidx)));
      }
      this->postprocessor_->Execute(net_outputs, this->model_, finfo.first);
      cpu_output_res->DeallingDone();
      return 0;
    });
    tasks.push_back(task);
  }
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> PostprocessingBatchingDoneStage::BatchingDone(
    const BatchingDoneInput& finfos,
    const std::shared_ptr<MluOutputResource> &mlu_output_res) {
  QueuingTicket mlu_output_res_ticket = mlu_output_res->PickUpNewTicket(false);

  std::vector<InferTaskSptr> tasks;
  InferTaskSptr task = std::make_shared<InferTask>([mlu_output_res_ticket,
                                                    mlu_output_res,
                                                    this,
                                                    finfos]() -> int {
    QueuingTicket mor_ticket = mlu_output_res_ticket;
    IOResValue mlu_output_value = mlu_output_res->WaitResourceByTicket(&mor_ticket);
    std::vector<void*> net_outputs;
    for (size_t output_idx = 0; output_idx < mlu_output_value.datas.size(); ++output_idx) {
      net_outputs.push_back(mlu_output_value.datas[output_idx].ptr);
    }

    std::vector<CNFrameInfoPtr> batched_finfos;
    for (const auto &it : finfos) batched_finfos.push_back(it.first);

    this->postprocessor_->Execute(net_outputs, this->model_, batched_finfos);
    mlu_output_res->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> ObjPostprocessingBatchingDoneStage::ObjBatchingDone(
    const BatchingDoneInput& finfos,
    const std::vector<std::shared_ptr<CNInferObject>>& objs) {
  if (cpu_output_res_ != nullptr) {
    return ObjBatchingDone(finfos, objs, cpu_output_res_);
  } else if (mlu_output_res_ != nullptr) {
    return ObjBatchingDone(finfos, objs, mlu_output_res_);
  } else {
    assert(false);
  }
  return {};
}

std::vector<std::shared_ptr<InferTask>> ObjPostprocessingBatchingDoneStage::ObjBatchingDone(
    const BatchingDoneInput& finfos,
    const std::vector<std::shared_ptr<CNInferObject>>& objs,
    const std::shared_ptr<CpuOutputResource> &cpu_output_res) {
  std::vector<InferTaskSptr> tasks;
  for (int bidx = 0; bidx < static_cast<int>(finfos.size()); ++bidx) {
    auto finfo = finfos[bidx];
    auto obj = objs[bidx];
    QueuingTicket cpu_output_res_ticket;
    if (0 == bidx) {
      cpu_output_res_ticket = cpu_output_res_->PickUpNewTicket(true);
    } else {
      cpu_output_res_ticket = cpu_output_res_->PickUpTicket(true);
    }
    InferTaskSptr task = std::make_shared<InferTask>([cpu_output_res_ticket,
                                                      cpu_output_res,
                                                      this,
                                                      finfo,
                                                      obj,
                                                      bidx]() -> int {
      QueuingTicket cor_ticket = cpu_output_res_ticket;
      IOResValue cpu_output_value = cpu_output_res->WaitResourceByTicket(&cor_ticket);
      std::vector<float*> net_outputs;
      for (size_t output_idx = 0; output_idx < cpu_output_value.datas.size(); ++output_idx) {
        net_outputs.push_back(reinterpret_cast<float*>(cpu_output_value.datas[output_idx].Offset(bidx)));
      }
      this->postprocessor_->Execute(net_outputs, this->model_, finfo.first, obj);
      cpu_output_res->DeallingDone();
      return 0;
    });
    tasks.push_back(task);
  }
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> ObjPostprocessingBatchingDoneStage::ObjBatchingDone(
    const BatchingDoneInput& finfos,
    const std::vector<std::shared_ptr<CNInferObject>>& objs,
    const std::shared_ptr<MluOutputResource> &mlu_output_res) {
  std::vector<InferTaskSptr> tasks;
  QueuingTicket mlu_output_res_ticket = mlu_output_res_->PickUpNewTicket(false);
  InferTaskSptr task = std::make_shared<InferTask>([mlu_output_res_ticket,
                                                    mlu_output_res,
                                                    this,
                                                    finfos,
                                                    objs]() -> int {
    QueuingTicket mor_ticket = mlu_output_res_ticket;
    IOResValue mlu_output_value = mlu_output_res->WaitResourceByTicket(&mor_ticket);
    std::vector<void*> net_outputs;
    for (size_t output_idx = 0; output_idx < mlu_output_value.datas.size(); ++output_idx) {
      net_outputs.push_back(mlu_output_value.datas[output_idx].ptr);
    }

    std::vector<std::pair<CNFrameInfoPtr, std::shared_ptr<CNInferObject>>> batched_objs;
    for (int bidx = 0; bidx < static_cast<int>(finfos.size()); ++bidx) {
      auto finfo = finfos[bidx];
      auto obj = objs[bidx];
      batched_objs.push_back(std::make_pair(std::move(finfo.first), std::move(obj)));
    }

    this->postprocessor_->Execute(net_outputs, this->model_, batched_objs);
    mlu_output_res->DeallingDone();
    return 0;
  });
  tasks.push_back(task);

  return tasks;
}

}  // namespace cnstream
