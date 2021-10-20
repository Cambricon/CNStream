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
    mem_op.SetModel(this->model_);

    mem_op.MemcpyInputH2D(mlu_value.ptrs, cpu_value.ptrs);

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
    LOGF_IF(INFERENCER, mlu_value.datas.size() != 1) << "Internal error, maybe model input num not 1";

    std::shared_ptr<CNFrameInfo> info = nullptr;
    std::string pts_str;

    if (profiler_) {
      for (auto it : finfos)
        profiler_->RecordProcessStart("RESIZE CONVERT", std::make_pair(it.first->stream_id, it.first->timestamp));
    }
    cnrtMemset(mlu_value.datas[0].ptr, 0, mlu_value.datas[0].batch_offset * finfos.size());

    bool ret = rcop_value->op.SyncOneOutput(mlu_value.datas[0].ptr);
    if (profiler_) {
      for (auto it : finfos)
        profiler_->RecordProcessEnd("RESIZE CONVERT", std::make_pair(it.first->stream_id, it.first->timestamp));
    }
    this->rcop_res_->DeallingDone();
    this->mlu_input_res_->DeallingDone();

    if (!ret) {
      throw CnstreamError("resize convert failed.");
    }
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

InferBatchingDoneStage::InferBatchingDoneStage(std::shared_ptr<edk::ModelLoader> model,
                                               cnstream::CNDataFormat model_input_fmt, uint32_t batchsize, int dev_id,
                                               std::shared_ptr<MluInputResource> mlu_input_res,
                                               std::shared_ptr<MluOutputResource> mlu_output_res)
    : BatchingDoneStage(model, batchsize, dev_id),
      model_input_fmt_(model_input_fmt),
      mlu_input_res_(mlu_input_res),
      mlu_output_res_(mlu_output_res) {
  easyinfer_ = std::make_shared<edk::EasyInfer>();
  easyinfer_->Init(model_, dev_id);
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
    if (profiler_) {
      for (auto it : finfos)
        profiler_->RecordProcessStart("RUN MODEL", std::make_pair(it.first->stream_id, it.first->timestamp));
    }
    if (!dump_resized_image_dir_.empty()) {
      int batch_offset = mlu_input_value.datas[0].batch_offset;
      int frame_num = finfos.size();
      int len = batch_offset * frame_num;
      std::vector<char> cpu_input_value(len);
      for (const auto& data : mlu_input_value.datas) {
        cnrtMemcpy(reinterpret_cast<void*>(cpu_input_value.data()), data.ptr, len, CNRT_MEM_TRANS_DIR_DEV2HOST);
        for (int i = 0; i < frame_num; i++) {
          info = finfos[i].first;
          CNDataFramePtr frame = info->collection.Get<CNDataFramePtr>(kCNDataFrameTag);
          char* img = reinterpret_cast<char*>(cpu_input_value.data()) + batch_offset * i;
          cv::Mat bgr(data.shape.H(), data.shape.W(), CV_8UC3);
          cv::Mat bgra(data.shape.H(), data.shape.W(), CV_8UC4, img);
          switch (model_input_fmt_) {
            case CNDataFormat::CN_PIXEL_FORMAT_RGBA32:
              cv::cvtColor(bgra, bgr, cv::COLOR_RGBA2BGR);
              break;
            case CNDataFormat::CN_PIXEL_FORMAT_BGRA32:
              cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
              break;
            case CNDataFormat::CN_PIXEL_FORMAT_ARGB32: {
              std::vector<cv::Mat> chns;
              cv::split(bgra, chns);
              std::swap(chns[1], chns[3]);
              chns.erase(chns.begin());
              cv::merge(chns, bgr);
              break;
            }
            case CNDataFormat::CN_PIXEL_FORMAT_ABGR32: {
              std::vector<cv::Mat> chns;
              cv::split(bgra, chns);
              chns.erase(chns.begin());
              cv::merge(chns, bgr);
              break;
            }
            default:
              LOGE(INFERENCER) << "Unsupported fmt, dump resized image failed."
                               << "fmt :" << static_cast<int>(model_input_fmt_);
              break;
          }
          std::string stream_index = std::to_string(info->GetStreamIndex());
          if (0 != access(dump_resized_image_dir_.c_str(), 0)) {
            mkdir(dump_resized_image_dir_.c_str(), 0777);
          }
          int obj_id = 0;
          std::string dump_img_path_prefix =
              dump_resized_image_dir_ + "/stream" + stream_index + "_frame" + std::to_string(frame->frame_id);
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

    if (saving_infer_input_) {
      int frame_num = finfos.size();

      // alloc cpu memory to save model output
      CpuOutputResource alloc_cpu_output_mem(this->easyinfer_->Model(), batchsize_);
      alloc_cpu_output_mem.Init();
      IOResValue cpu_output_value = alloc_cpu_output_mem.GetDataDirectly();
      edk::MluMemoryOp mem_op;
      mem_op.SetModel(this->easyinfer_->Model());
      mem_op.MemcpyOutputD2H(cpu_output_value.ptrs, mlu_output_value.ptrs);

      if (mlu_input_value.datas.size() == 1) {
        for (int j = 0; j < frame_num; ++j) {
          std::shared_ptr<InferData> iodata(new (std::nothrow) InferData);
          iodata->input_height_ = mlu_input_value.datas[0].shape.H();
          iodata->input_width_ = mlu_input_value.datas[0].shape.W();

          // infer model input_fmt only support RBGA32, ARGB32, BGRA32, ABGR32
          iodata->input_size_ = iodata->input_height_ * iodata->input_width_ * 4;
          iodata->input_fmt_ = model_input_fmt_;
          iodata->output_num_ = cpu_output_value.datas.size();

          // save model input
          iodata->input_cpu_addr_ = cnCpuMemAlloc(iodata->input_size_);
          cnrtMemcpy(iodata->input_cpu_addr_.get(), mlu_input_value.datas[0].Offset(j), iodata->input_size_,
                     CNRT_MEM_TRANS_DIR_DEV2HOST);

          // save model output
          for (size_t k = 0; k < iodata->output_num_; ++k) {
            iodata->output_sizes_.push_back(cpu_output_value.datas[k].shape.DataCount());
            std::shared_ptr<void> output_cpu_addr = cnCpuMemAlloc(iodata->output_sizes_[k] * sizeof(float*));
            memcpy(output_cpu_addr.get(),
                   cpu_output_value.datas[k].Offset(j), sizeof(float*) * iodata->output_sizes_[k]);
            iodata->output_cpu_addr_.push_back(output_cpu_addr);
          }

          // save iodata
          auto data_map = finfos[j].first->collection.Get<CNInferDataPtr>(kCNInferDataTag);
          std::lock_guard<std::mutex> lk(data_map->mutex_);
          if (data_map->datas_map_.find(module_name_) != data_map->datas_map_.end()) {
            data_map->datas_map_[module_name_].push_back(iodata);
          } else {
            std::vector<std::shared_ptr<InferData>> vec{iodata};
            data_map->datas_map_[module_name_] = vec;
          }
        }
      } else {
        LOGE(INFERENCER) << "Module input num is " << mlu_input_value.datas.size()
                         << " , input num not supports greater than 1!";
      }
      alloc_cpu_output_mem.Destroy();
    }

    if (profiler_) {
      for (auto it : finfos)
        profiler_->RecordProcessEnd("RUN MODEL", std::make_pair(it.first->stream_id, it.first->timestamp));
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
    mem_op.SetModel(this->model_);
    mem_op.MemcpyOutputD2H(cpu_output_value.ptrs, mlu_output_value.ptrs);
    this->mlu_output_res_->DeallingDone();
    this->cpu_output_res_->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> PostprocessingBatchingDoneStage::BatchingDone(const BatchingDoneInput& finfos) {
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
    const BatchingDoneInput& finfos, const std::shared_ptr<CpuOutputResource>& cpu_output_res) {
  std::vector<InferTaskSptr> tasks;
  for (int bidx = 0; bidx < static_cast<int>(finfos.size()); ++bidx) {
    auto finfo = finfos[bidx];
    QueuingTicket cpu_output_res_ticket;
    if (0 == bidx) {
      cpu_output_res_ticket = cpu_output_res->PickUpNewTicket(true);
    } else {
      cpu_output_res_ticket = cpu_output_res->PickUpTicket(true);
    }
    InferTaskSptr task =
        std::make_shared<InferTask>([cpu_output_res_ticket, cpu_output_res, this, finfo, bidx]() -> int {
          QueuingTicket cor_ticket = cpu_output_res_ticket;
          IOResValue cpu_output_value = cpu_output_res->WaitResourceByTicket(&cor_ticket);
          std::vector<float*> net_outputs;
          for (size_t output_idx = 0; output_idx < cpu_output_value.datas.size(); ++output_idx) {
            net_outputs.push_back(reinterpret_cast<float*>(cpu_output_value.datas[output_idx].Offset(bidx)));
          }
          if (!cnstream::IsStreamRemoved(finfo.first->stream_id)) {
            this->postprocessor_->Execute(net_outputs, this->model_, finfo.first);
          }
          cpu_output_res->DeallingDone();
          return 0;
        });
    tasks.push_back(task);
  }
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> PostprocessingBatchingDoneStage::BatchingDone(
    const BatchingDoneInput& finfos, const std::shared_ptr<MluOutputResource>& mlu_output_res) {
  QueuingTicket mlu_output_res_ticket = mlu_output_res->PickUpNewTicket(false);

  std::vector<InferTaskSptr> tasks;
  InferTaskSptr task = std::make_shared<InferTask>([mlu_output_res_ticket, mlu_output_res, this, finfos]() -> int {
    QueuingTicket mor_ticket = mlu_output_res_ticket;
    IOResValue mlu_output_value = mlu_output_res->WaitResourceByTicket(&mor_ticket);
    std::vector<void*> net_outputs;
    for (size_t output_idx = 0; output_idx < mlu_output_value.datas.size(); ++output_idx) {
      net_outputs.push_back(mlu_output_value.datas[output_idx].ptr);
    }

    std::vector<CNFrameInfoPtr> batched_finfos;
    for (const auto& it : finfos) batched_finfos.push_back(it.first);

    this->postprocessor_->Execute(net_outputs, this->model_, batched_finfos);
    mlu_output_res->DeallingDone();
    return 0;
  });
  tasks.push_back(task);
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> ObjPostprocessingBatchingDoneStage::ObjBatchingDone(
    const BatchingDoneInput& finfos, const std::vector<std::shared_ptr<CNInferObject>>& objs) {
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
    const BatchingDoneInput& finfos, const std::vector<std::shared_ptr<CNInferObject>>& objs,
    const std::shared_ptr<CpuOutputResource>& cpu_output_res) {
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
    InferTaskSptr task =
        std::make_shared<InferTask>([cpu_output_res_ticket, cpu_output_res, this, finfo, obj, bidx]() -> int {
          QueuingTicket cor_ticket = cpu_output_res_ticket;
          IOResValue cpu_output_value = cpu_output_res->WaitResourceByTicket(&cor_ticket);
          std::vector<float*> net_outputs;
          for (size_t output_idx = 0; output_idx < cpu_output_value.datas.size(); ++output_idx) {
            net_outputs.push_back(reinterpret_cast<float*>(cpu_output_value.datas[output_idx].Offset(bidx)));
          }
          if (!cnstream::IsStreamRemoved(finfo.first->stream_id)) {
            this->postprocessor_->Execute(net_outputs, this->model_, finfo.first, obj);
          }
          cpu_output_res->DeallingDone();
          return 0;
        });
    tasks.push_back(task);
  }
  return tasks;
}

std::vector<std::shared_ptr<InferTask>> ObjPostprocessingBatchingDoneStage::ObjBatchingDone(
    const BatchingDoneInput& finfos, const std::vector<std::shared_ptr<CNInferObject>>& objs,
    const std::shared_ptr<MluOutputResource>& mlu_output_res) {
  std::vector<InferTaskSptr> tasks;
  QueuingTicket mlu_output_res_ticket = mlu_output_res_->PickUpNewTicket(false);
  InferTaskSptr task =
      std::make_shared<InferTask>([mlu_output_res_ticket, mlu_output_res, this, finfos, objs]() -> int {
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
