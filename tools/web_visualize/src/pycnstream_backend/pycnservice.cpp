/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include <iostream>
#include <memory>
#include <string>

#include "cnstream_frame_va.hpp"
#include "pipeline_handler.hpp"
#include "pycnservice.hpp"
#include "cnstream_logging.hpp"

#define MIN_CACHE_QSIZE 20

class CNSEventObserver {
 public:
  enum EVENT {
    EOS,    ///< End of data stream event
    ERROR,  ///< Data stream error
  };

  explicit CNSEventObserver(PyCNService *service) : service_(service) {}
  ~CNSEventObserver() {}

  void EventNotify(const std::string &stream_id, EVENT event) {
    switch (event) {
      case EVENT::EOS:
        LOGI(WEBVISUAL) << "cnservice get eos from stream with stream_id: " << stream_id;
        if (service_) service_->WaitStop();
        break;
      case EVENT::ERROR:
        LOGI(WEBVISUAL) << "CNService error for stream with stream_id: " << stream_id;
        if (service_) service_->WaitStop();
        break;
      default:
        LOGI(WEBVISUAL) << "CNService receive unkonw msg.";
        break;
    }
  }

 private:
  PyCNService *service_ = nullptr;
};

class CNSMsgObserver : cnstream::StreamMsgObserver {
 public:
  explicit CNSMsgObserver(CNSEventObserver *observer) : observer_(observer) {}

  void Update(const cnstream::StreamMsg &smsg) override {
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      if (observer_) observer_->EventNotify(smsg.stream_id, CNSEventObserver::EOS);
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      if (observer_) observer_->EventNotify(smsg.stream_id, CNSEventObserver::ERROR);
    }
  }

 private:
  CNSEventObserver *observer_ = nullptr;
};

class CNSDataObserver : public cnstream::IModuleObserver {
 public:
  explicit CNSDataObserver(PyCNService *service) : service_(service) {}
  ~CNSDataObserver() {}

  void notify(std::shared_ptr<cnstream::CNFrameInfo> in_data) override {
    if (service_) service_->FrameDataCallBack(in_data);
  }

 private:
  PyCNService *service_ = nullptr;
};

void PyCNService::InitService(const CNServiceInfo &info) {
  memcpy(&cnsinfo_, &info, sizeof(CNServiceInfo));

  if (nullptr == ppipe_handler_) {
    ppipe_handler_ = new (std::nothrow) PipelineHandler();
  }
}

bool PyCNService::Start(const std::string &stream_url, const std::string &config_fname) {
  LOGI(WEBVISUAL) << "CNService start, stream_url: " << stream_url << ", pipeline config: " << config_fname;
  if (config_fname.empty() || !ppipe_handler_) return false;

  if (cnsinfo_.register_data) {
    LOGI(WEBVISUAL) << "CNService register data.";
  }

  bool ret = false;
  ret = ppipe_handler_->CreatePipeline(config_fname, "perf_cache");
  if (!ret) {
    LOGI(WEBVISUAL) << "CNService create pipeline failed, stream_url: " << stream_url;
    return false;
  }

  observer_ = new (std::nothrow) CNSEventObserver(this);
  msg_observer_ = new (std::nothrow) CNSMsgObserver(observer_);
  ppipe_handler_->SetMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver *>(msg_observer_));

  if (cnsinfo_.register_data) {
    data_observer_ = new (std::nothrow) CNSDataObserver(this);
    ppipe_handler_->SetDataObserver(data_observer_);
    int cache_qsize = cnsinfo_.cache_size > MIN_CACHE_QSIZE ? cnsinfo_.cache_size : MIN_CACHE_QSIZE;
    cache_frameq_ = new (std::nothrow) CNSQueue<CNSFrame>(cache_qsize);
  }

  ret = ppipe_handler_->Start();
  if (!ret) {
    LOGI(WEBVISUAL) << "CNService start pipeline failed, stream_url: " << stream_url;
    return false;
  }

  std::string stream_id = "cnservice-stream";
  ret = ppipe_handler_->AddStream(stream_url, stream_id, cnsinfo_.fps);
  if (!ret) {
    LOGI(WEBVISUAL) << "CNService add stream failed, stream_url: " << stream_url;
    return false;
  }

  is_running_.store(true);
  LOGI(WEBVISUAL) << "CNService start pipeline succeed, stream_url: " << stream_url;
  return true;
}

PyCNService::~PyCNService() {
  DestoryResource();

  if (ppipe_handler_) {
    delete ppipe_handler_;
    ppipe_handler_ = nullptr;
  }
}

void PyCNService::Stop() {
  is_running_.store(false);
  DestoryResource();
}

void PyCNService::WaitStop() {
  while (cache_frameq_ && !cache_frameq_->Empty() && is_running_.load()) {
    std::this_thread::yield();
  }

  is_running_.store(false);
}

void PyCNService::DestoryResource() {
  if (is_running_.load()) return;
  std::lock_guard<std::mutex> lock(stop_mtx_);
  if (ppipe_handler_) {
    ppipe_handler_->Stop();
  }

  if (observer_) {
    delete observer_;
    observer_ = nullptr;
  }

  if (msg_observer_) {
    delete msg_observer_;
    msg_observer_ = nullptr;
  }

  if (data_observer_) {
    delete data_observer_;
    data_observer_ = nullptr;
  }

  if (cache_frameq_) {
    CNSFrame frame;
    while (cache_frameq_->Pop(1, frame)) {
      if (frame.bgr_mat) delete frame.bgr_mat;
    }

    delete cache_frameq_;
    cache_frameq_ = nullptr;
  }

  LOGI(WEBVISUAL) << "CNService stop succeed.";
}

void PyCNService::FrameDataCallBack(std::shared_ptr<cnstream::CNFrameInfo> in_data) {
  if (nullptr == data_observer_ || !is_running_.load()) return;

  std::unique_lock<std::mutex> lock(mutex_);
  CNSFrame cnsframe;
  if (in_data->IsEos()) {
    cnsframe.frame_info.eos_flag = 1;
  } else {
    auto data = cnstream::any_cast<std::shared_ptr<cnstream::CNDataFrame>>(in_data->datas[cnstream::CNDataFramePtrKey]);
    cnsframe.frame_info.eos_flag = 0;
    cnsframe.frame_info.frame_id = data->frame_id;
    cnsframe.frame_info.width = cnsinfo_.dst_width;
    cnsframe.frame_info.height = cnsinfo_.dst_height;
    cnsframe.bgr_mat = new (std::nothrow) cv::Mat(cnsinfo_.dst_width, cnsinfo_.dst_height, CV_8UC3);
    cv::resize(data->ImageBGR(), *cnsframe.bgr_mat, cv::Size(cnsinfo_.dst_width, cnsinfo_.dst_height), 0, 0,
               cv::INTER_LINEAR);
  }

  if (cache_frameq_->Full()) {
    CNSFrame discard_cnsframe;
    cache_frameq_->Pop(10, discard_cnsframe);
    LOGW(WEBVISUAL) << "cache frame queue is full, discard frame, frame_id: ." << discard_cnsframe.frame_info.frame_id;
    delete discard_cnsframe.bgr_mat;
    return;
  }
  if (!cache_frameq_->Push(10, cnsframe)) {
    LOGW(WEBVISUAL) << "cache frame queue is full, discard frame, frame_id: ." << cnsframe.frame_info.frame_id;
    delete cnsframe.bgr_mat;
  }
}

#ifdef MAKE_PYTHONAPI
bool PyCNService::ReadOneFrame(CNSFrameInfo *ret_frame, py::array_t<uint8_t> *img_data) {
  if (!ret_frame || !img_data || !data_observer_
      || !cache_frameq_ || !is_running_.load()) return false;

  std::lock_guard<std::mutex> lock(mutex_);
  CNSFrame cnsframe;
  if (cache_frameq_->Pop(1, cnsframe)) {
    memcpy(ret_frame, &cnsframe.frame_info, sizeof(CNSFrameInfo));
    if (1 != ret_frame->eos_flag) {
      memcpy(img_data->mutable_data(), cnsframe.bgr_mat->data, ret_frame->width * ret_frame->height * 3);
      delete cnsframe.bgr_mat;
    }
    return true;
  }

  return false;
}
#else
bool PyCNService::ReadOneFrame(CNSFrameInfo *ret_frame, uint8_t *img_data) {
  if (!img_data || !ret_frame || !data_observer_
      || !cache_frameq_ || !is_running_.load()) return false;

  std::lock_guard<std::mutex> lock(mutex_);
  CNSFrame cnsframe;
  if (cache_frameq_->Pop(1, cnsframe)) {
    memcpy(ret_frame, &cnsframe.frame_info, sizeof(CNSFrameInfo));
    if (1 != ret_frame->eos_flag) {
      memcpy(img_data, cnsframe.bgr_mat->data, ret_frame->width * ret_frame->height * 3);
      delete cnsframe.bgr_mat;
    } else {
      is_running_.store(false);
    }
    return true;
  }

  return false;
}
#endif
