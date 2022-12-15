# ==============================================================================
# Copyright (C) [2022] by Cambricon, Inc. All rights reserved
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ==============================================================================

import os, sys
import time
import threading
import cv2
import math
import numpy as np

sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/python/lib")
import cnis
import cnstream

import observer
import utils

g_perf_print_lock = threading.Lock()
g_perf_print_stop = False

class PerfThread (threading.Thread):
  def __init__(self, pipeline):
    threading.Thread.__init__(self)
    self.pipeline = pipeline
  def run(self):
    print_performance(self.pipeline)


def print_performance(pipeline):
  global g_perf_print_lock, g_perf_print_stop
  if pipeline.is_profiling_enabled():
    last_time = time.time()
    while True:
      g_perf_print_lock.acquire()
      if g_perf_print_stop:
        break
      g_perf_print_lock.release()
      elapsed_time = time.time() - last_time
      if elapsed_time < 2:
        time.sleep(2 - elapsed_time)
      last_time = time.time()
      # print whole process performance
      cnstream.print_pipeline_performance(pipeline)
      # print real time performance (last 2 seconds)
      cnstream.print_pipeline_performance(pipeline, 2000)
    g_perf_print_lock.release()


def letterbox(img, dst_shape, pad_val):
  src_h, src_w = img.shape[0], img.shape[1]
  dst_h, dst_w = dst_shape
  ratio = min(dst_h / src_h, dst_w / src_w)
  unpad_h, unpad_w = int(math.floor(src_h * ratio)), int(math.floor(src_w * ratio))
  if ratio != 1:
    interp = cv2.INTER_AREA if ratio < 1 else cv2.INTER_LINEAR
    img = cv2.resize(img, (unpad_w, unpad_h), interp)
  # padding
  pad_t = int(math.floor((dst_h - unpad_h) / 2))
  pad_b = dst_h - unpad_h - pad_t
  pad_l = int(math.floor((dst_w - unpad_w) / 2))
  pad_r = dst_w - unpad_w - pad_l
  img = cv2.copyMakeBorder(img, pad_t, pad_b, pad_l, pad_r, cv2.BORDER_CONSTANT, value=(pad_val, pad_val, pad_val))
  return img


class Yolov5Preproc(cnstream.Preproc):
  def __init__(self):
    cnstream.Preproc.__init__(self)

  def init(self, params):
    return 0

  def on_tensor_params(self, param):
    return 0

  def execute(self, src, dst, src_rects):
    batch_size = src.get_num_filled()
    for i in range(batch_size):
      src_img_y = src.get_host_data(plane_idx = 0, batch_idx = i)
      src_img_uv = src.get_host_data(plane_idx = 1, batch_idx = i)
      src_img = np.concatenate((src_img_y, src_img_uv), axis=0)

      # convert color to model input pixel format
      src_img = cv2.cvtColor(src_img, cv2.COLOR_YUV2RGB_NV12)
      src_img = letterbox(src_img, (dst.get_height(), dst.get_width()), 114)

      dst_img = dst.get_host_data(plane_idx = 0, batch_idx = i)
      dst_img[:] = src_img
      dst.sync_host_to_device(plane_idx = 0, batch_idx = i)
    return 0

def to_range(val : float, min_val, max_val):
  return min(max(val, min_val), max_val)

class Yolov5Postproc(cnstream.Postproc):
  def __init__(self):
    cnstream.Postproc.__init__(self)
    self.__threshold = 0.3

  def init(self, params):
    if 'threshold' in params:
        self.__threshold = float(params['threshold'])
    return 0

  def execute(self, net_outputs, model_info, packages, labels):
    output0 = net_outputs[0][0]
    output1 = net_outputs[1][0]

    model_input_w = model_info.input_shape(0)[2]
    model_input_h = model_info.input_shape(0)[1]
    if model_info.input_layout(0).order == cnis.DimOrder.NCHW:
      model_input_w = model_info.input_shape(0)[3]
      model_input_h = model_info.input_shape(0)[2]

    batch_size = len(packages)
    for b_idx in range(batch_size):
      data = output0.get_host_data(plane_idx = 0, batch_idx = b_idx, dtype=cnis.DataType.FLOAT32)
      box_num = output1.get_host_data(plane_idx = 0, batch_idx = b_idx)[0]
      if box_num == 0:
        continue

      package = packages[b_idx]
      frame = package.get_cn_data_frame()
      objs_holder = package.get_cn_infer_objects()

      image_w = frame.buf_surf.get_width()
      image_h = frame.buf_surf.get_height()

      scaling_w = model_input_w / image_w
      scaling_h = model_input_h / image_h
      scaling = min(scaling_w, scaling_h)
      scaling_factor_w = scaling_w / scaling
      scaling_factor_h = scaling_h / scaling

      box_step = 7
      for i in range(box_num):
        left = to_range(data[i * box_step + 3], 0, model_input_w)
        top = to_range(data[i * box_step + 4], 0, model_input_h)
        right = to_range(data[i * box_step + 5], 0, model_input_w)
        bottom = to_range(data[i * box_step + 6], 0, model_input_h)

        # rectify
        left = to_range((left / model_input_w - 0.5) * scaling_factor_w + 0.5, 0, 1)
        top = to_range((top / model_input_h - 0.5) * scaling_factor_h + 0.5, 0 ,1)
        right = to_range((right / model_input_w - 0.5) * scaling_factor_w + 0.5, 0, 1)
        bottom = to_range((bottom / model_input_h - 0.5) * scaling_factor_h + 0.5, 0, 1)

        if right <= left or bottom <= top:
          continue

        obj = cnstream.CNInferObject()
        obj.id = str(int(data[i * box_step + 1]))
        obj.score = data[i * box_step + 2]
        obj.bbox.x = left
        obj.bbox.y = top
        obj.bbox.w = min(1 - obj.bbox.x, right - left)
        obj.bbox.h = min(1 - obj.bbox.y, bottom - top)
        if (self.__threshold > 0 and obj.score < self.__threshold) or obj.bbox.w <= 0 or obj.bbox.h <= 0:
          continue
        objs_holder.push_back(obj)

    return 0


def main():
  model_file = os.path.join(os.path.abspath(os.path.dirname(__file__)),
      "../../data/models/yolov5m_v0.13.0_4b_rgb_uint8.magicmind")
  if not os.path.exists(model_file):
    os.makedirs(os.path.dirname(model_file),exist_ok=True)
    import urllib.request
    url_str = "http://video.cambricon.com/models/magicmind/v0.13.0/yolov5m_v0.13.0_4b_rgb_uint8.magicmind"
    print(f'Downloading {url_str} ...')
    urllib.request.urlretrieve(url_str, model_file)
  global g_perf_print_lock, g_perf_print_stop
  # Build a pipeline
  pipeline = cnstream.Pipeline("yolov5_detection_pipeline")
  pipeline.build_pipeline_by_json_file('yolov5_detection_config.json')

  # Get pipeline's source module
  source_module_name = 'source'
  source = pipeline.get_source_module(source_module_name)

  # Set message observer
  obs = observer.CustomObserver(pipeline, source)
  pipeline.stream_msg_observer = obs

  # Start the pipeline
  if not pipeline.start():
    return

  # Start a thread to print pipeline performance
  perf_th = PerfThread(pipeline)
  perf_th.start()

  # Define an input data handler
  mp4_path = "../../data/videos/cars.mp4"
  stream_num = 4
  for i in range(stream_num):
    stream_id = "stream_{}".format(i)
    param = cnstream.FileSourceParam()
    param.filename = mp4_path
    param.framerate = -1
    file_handler = cnstream.create_source(source, stream_id, param)

    observer.g_source_lock.acquire()
    if source.add_source(file_handler) != 0:
      print("Add source failed stream {}".format(stream_id))
    else:
      obs.increase_stream(stream_id)
    observer.g_source_lock.release()

  obs.wait_for_stop()

  if pipeline.is_profiling_enabled():
    g_perf_print_lock.acquire()
    g_perf_print_stop = True
    g_perf_print_lock.release()
    perf_th.join()
    cnstream.print_pipeline_performance(pipeline)

    print("pipeline[{}] stops".format(pipeline.get_name()))

if __name__ == '__main__':
  main()
