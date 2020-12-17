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
#include "data_handler_util.hpp"

#include <memory>

#if HAVE_LIBYUV
  #include "libyuv.h"
#else
  #error "LIBYUV must be selected";
#endif

#include "cnstream_allocator.hpp"

namespace cnstream {

// #define DEBUG_DUMP_IMAGE 1

int SourceRender::Process(std::shared_ptr<CNFrameInfo> frame_info,
                          DecodeFrame *frame, uint64_t frame_id, const DataSourceParam &param_) {
  CNDataFramePtr dataframe = cnstream::GetCNDataFramePtr(frame_info);
  if (!dataframe) return -1;

  dataframe->frame_id = frame_id;
  /*fill source data info*/
  dataframe->width = frame->width;
  dataframe->height = frame->height;
  if (frame->mlu_addr) {
    switch (frame->fmt) {
      case DecodeFrame::FMT_NV12:
        dataframe->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;
        break;
      case DecodeFrame::FMT_NV21:
        dataframe->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21;
        break;
      default: {
        dataframe->fmt = CNDataFormat::CN_INVALID;
        LOGF(SOURCE) << " Unsupported format";
        return -1;
      }
    }
#ifdef CNS_MLU220_SOC    
    dataframe->ctx.dev_type = DevContext::MLU_CPU;
#else
    dataframe->ctx.dev_type = DevContext::MLU;
#endif
    // device_id in callback-frame incorrect, use device_id saved in params instead
    dataframe->ctx.dev_id = param_.device_id_;
    dataframe->ctx.ddr_channel = -1;
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      dataframe->stride[i] = frame->stride[i];
      dataframe->ptr_mlu[i] = reinterpret_cast<void *>(frame->plane[i]);
#ifdef CNS_MLU220_SOC    
      dataframe->ptr_cpu[i] = reinterpret_cast<void *>(frame->plane[i]);
#endif
    }

    if (OUTPUT_MLU == param_.output_type_) {
      if (param_.reuse_cndec_buf) {
        class Deallocator : public IDataDeallocator {
         public:
          explicit Deallocator(IDecBufRef *ptr) {
            ptr_.reset(ptr);
          }
          ~Deallocator() = default;

         private:
          std::unique_ptr<IDecBufRef> ptr_;
        };
        IDecBufRef *ptr = frame->buf_ref.release();
        dataframe->deAllocator_.reset(new Deallocator(ptr));
      }
      dataframe->dst_device_id = param_.device_id_;
      dataframe->CopyToSyncMem(true);
    } else {
      dataframe->dst_device_id = -1;  // unused
      dataframe->CopyToSyncMem(false);
    }

    #ifdef DEBUG_DUMP_IMAGE
      static bool flag = false;
      if (!flag) {
        imwrite("test_mlu.jpg", *dataframe->ImageBGR());
        flag = true;
      }
    #endif
    return 0;
  }

  // decoder with cpu-output
  if (frame->fmt != DecodeFrame::FMT_I420
     && frame->fmt != DecodeFrame::FMT_J420
     && frame->fmt != DecodeFrame::FMT_YUYV) {
    LOGF(SOURCE) << " Unsupported format";
    return -1;
  }

  // we use NV21 as source output-format
  dataframe->fmt = CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12;

  // convert to cpu first always
  dataframe->ctx.dev_type = DevContext::CPU;
  dataframe->ctx.dev_id = -1;
  dataframe->ctx.ddr_channel = -1;  // unused for cpu
  if (param_.apply_stride_align_for_scaler_) {
    dataframe->stride[0] = ROUND_UP(frame->width, 128);
    dataframe->stride[1] = ROUND_UP(frame->width, 128);
  } else {
    dataframe->stride[0] = frame->stride[0];
    dataframe->stride[1] = frame->stride[0];
  }

  size_t bytes = dataframe->GetBytes();
  bytes = ROUND_UP(bytes, 64 * 1024);
  dataframe->cpu_data = cnCpuMemAlloc(bytes);
  if (nullptr == dataframe->cpu_data) {
    LOGF(SOURCE) << "failed to alloc cpu memory";
    return -1;
  }

  switch (frame->fmt) {
    case DecodeFrame::FMT_I420:
    case DecodeFrame::FMT_J420: {
      uint8_t *dst_y = static_cast<uint8_t*>(dataframe->cpu_data.get());
      uint8_t *dst_uv = dst_y + dataframe->GetPlaneBytes(0);
      libyuv::I420ToNV12(static_cast<uint8_t*>(frame->plane[0]),
             frame->stride[0],
             static_cast<uint8_t*>(frame->plane[1]),
             frame->stride[1],
             static_cast<uint8_t*>(frame->plane[2]),
             frame->stride[2],
             dst_y,
             dataframe->stride[0],
             dst_uv,
             dataframe->stride[1],
             dataframe->width,
             dataframe->height);
      break;
    }
    case DecodeFrame::FMT_YUYV: {
      int tmp_stride = (frame->width + 1) / 2 * 2;
      int tmp_height = (frame->height + 1) / 2 * 2;
      std::unique_ptr<uint8_t[]> tmp_i420_y(new uint8_t[tmp_stride * tmp_height]);
      std::unique_ptr<uint8_t[]> tmp_i420_u(new uint8_t[tmp_stride * tmp_height/4]);
      std::unique_ptr<uint8_t[]> tmp_i420_v(new uint8_t[tmp_stride * tmp_height/4]);

      libyuv::YUY2ToI420(static_cast<uint8_t*>(frame->plane[0]),
             frame->stride[0],
             tmp_i420_y.get(),
             tmp_stride,
             tmp_i420_u.get(),
             tmp_stride/2,
             tmp_i420_v.get(),
             tmp_stride/2,
             frame->width,
             frame->height);

      uint8_t *dst_y = static_cast<uint8_t*>(dataframe->cpu_data.get());
      uint8_t *dst_uv = dst_y + dataframe->GetPlaneBytes(0);
      libyuv::I420ToNV12(tmp_i420_y.get(),
             tmp_stride,
             tmp_i420_u.get(),
             tmp_stride/2,
             tmp_i420_v.get(),
             tmp_stride/2,
             dst_y,
             dataframe->stride[0],
             dst_uv,
             dataframe->stride[1],
             dataframe->width,
             dataframe->height);
      break;
    }
    default : {
      LOGF(SOURCE) << "Should not come here";
      return -1;
    }
  }

  // fill data to dataframe
  void *dst = dataframe->cpu_data.get();
  for (int i = 0; i < dataframe->GetPlanes(); i++) {
    size_t plane_size = dataframe->GetPlaneBytes(i);
    dataframe->data[i].reset(new CNSyncedMemory(plane_size));
    dataframe->data[i]->SetCpuData(dst);
    dst = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(dst) + plane_size);
  }

  // sync memory from cpu if needed
  if (OUTPUT_MLU == param_.output_type_) {
    dataframe->dst_device_id = param_.device_id_;  // assume the param_.device_id is dst_device_id
    for (int i = 0; i < dataframe->GetPlanes(); i++) {
      dataframe->data[i]->SetMluDevContext(dataframe->dst_device_id);
      const void * plane_data = dataframe->data[i]->GetMluData();
      plane_data = plane_data;  // disable warnings
    }
  }

#ifdef DEBUG_DUMP_IMAGE
  static bool flag = false;
  if (!flag) {
    imwrite("test_cpu.jpg", *dataframe->ImageBGR());
    flag = true;
  }
#endif
  return 0;
}

}  // namespace cnstream
