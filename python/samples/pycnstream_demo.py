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

sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../easydk/python/lib")
import cnis
import cnstream

import observer
import utils

cur_file_dir = os.path.split(os.path.realpath(__file__))[0]

class OneModuleObserver(cnstream.ModuleObserver):
  def __init__(self) -> None:
    super().__init__()

  def notify(self, frame: 'probe data from one node'):
    if frame.is_eos():
      print("OneModuleObserver notify: receive eos")
      return
    cn_data = frame.get_cn_data_frame()
    frame_id = cn_data.frame_id
    stream_id = frame.stream_id
    print("receive the frame {} from {}".format(frame_id, stream_id))


def receive_processed_frame(frame):
  if frame.is_eos():
    print("receive_processed_frame: receive eos")
    return
  cn_data = frame.get_cn_data_frame()
  frame_id = cn_data.frame_id
  stream_id = frame.stream_id
  print("receive the frame {} from {}".format(frame_id, stream_id))
  if cn_data.has_bgr_image():
    cv2.imwrite('{}/output/{}_frame_{}.jpg'.format(cur_file_dir, stream_id, frame_id), cn_data.image_bgr())
  objs = frame.get_cn_infer_objects().objs
  print("objects number: ", len(objs))
  for obj in objs:
    print("obj: id: {} score: {:.4f}  bbox: {:.4f}, {:.4f}, {:.4f}, {:.4f}".format(
        obj.id, obj.score, obj.bbox.x, obj.bbox.y, obj.bbox.w, obj.bbox.h))

def main():
  if not os.path.exists(cur_file_dir + "/output"):
    os.mkdir(cur_file_dir + "/output")
  model_file = os.path.join(os.path.abspath(os.path.dirname(__file__)),
      "../../data/models/yolov5m_v0.13.0_4b_rgb_uint8.magicmind")
  if not os.path.exists(model_file):
    os.makedirs(os.path.dirname(model_file),exist_ok=True)
    import urllib.request
    url_str = "http://video.cambricon.com/models/magicmind/v0.13.0/yolov5m_v0.13.0_4b_rgb_uint8.magicmind"
    print('Downloading {} ...'.format(url_str))
    urllib.request.urlretrieve(url_str, model_file)

  # Build a pipeline
  pipeline = cnstream.Pipeline("my_pipeline")
  pipeline.build_pipeline_by_json_file('python_demo_config.json')


  # Set frame done callback
  pipeline.register_frame_done_callback(receive_processed_frame)

  # Probe one module's output in the pipeline, it's just for debugging
  # infer_module_observer = OneModuleObserver()
  # infer = pipeline.get_module('detector')
  # infer.set_module_observer(infer_module_observer)

  # Get pipeline's source module
  source_module_name = 'source'
  source = pipeline.get_source_module(source_module_name)

  # Set message observer
  obs = observer.CustomObserver(pipeline, source)
  pipeline.stream_msg_observer = obs

  # Start the pipeline
  if not pipeline.start():
    print("Start pipeline failed.")
    return

  # Start a thread to print pipeline performance
  perf_level = 0
  print_perf_loop = utils.PrintPerformanceLoop(pipeline, perf_level=perf_level)
  print_perf_loop.start()

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

  print_perf_loop.stop()
  utils.PrintPerformance(pipeline, perf_level=perf_level).print_whole()

  print("pipeline[{}] stops".format(pipeline.get_name()))

if __name__ == '__main__':
  main()
