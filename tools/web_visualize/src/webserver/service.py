###############################################################################
# Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
###############################################################################

import os
import cv2
import time
import queue
import sys
from webserver.logger import *
import webserver.perf as perf

sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../../../../python/lib")

from cnstream import *

preview_video_size = (1080, 720)
render_fps = 30
timeouts = 100
data_path = "./webui/static/data/"
json_path = "./webui/static/json/"
upload_json_path = "./webui/static/json/user/"
user_media = "user_media"

result_queue = queue.Queue()

class CNStreamService:
  def __init__(self):
    self.running = False
    self.pipeline = None
    self.source_module_name = 'source'
    self.source = None
    self.stream_id = 'stream_0'

  def receive_processed_frame(self, frame):
    if self.is_running():
      result_queue.put(frame)

  def is_running(self):
    return self.running

  def Start(self, input_file, config_json):
    self.pipeline = Pipeline("WebPipeline")

    if not self.pipeline.build_pipeline_by_json_file(config_json):
      logger.error("Build pipeline failed, the JSON configuration is {}".format(config_json))

    self.pipeline.register_frame_done_callback(self.receive_processed_frame)

    self.source = self.pipeline.get_source_module(self.source_module_name)

    if not self.pipeline.start():
      logger.error("Start pipeline failed.")
      return False
    else:
      logger.info("Start pipeline done.")

    # Start a thread to print pipeline performance
    self.print_perf_loop = perf.PrintPerformanceLoop(self.pipeline, perf_level=0)
    self.print_perf_loop.start()

    file_handler = FileHandler(self.source, self.stream_id, input_file, 30)
    if self.source.add_source(file_handler) != 0:
      logger.error("Failed to add stream {}".format(input_file))
      return False

    self.running = True
    return True

  def Stop(self):
    self.running = False
    result_queue.queue.clear()
    self.source.remove_source(self.stream_id, True)
    self.pipeline.stop()
    self.print_perf_loop.stop()
    logger.info("Pipeline stop done.")

cnstream_service = CNStreamService()

def getPreviewFrame():
  render_with_interval = False

  background_img = cv2.imread(data_path + "black.jpg")
  ret, encode_img = cv2.imencode('.jpg', background_img)
  background_img_bytes = encode_img.tobytes()

  while True:
    if cnstream_service.is_running():
      start = time.time()
      frame_info = result_queue.get()

      if (True == frame_info.is_eos()):
        logger.info("read EOS frame.")
        frame_info = None
        cnstream_service.Stop()
      else:
        cn_data = frame_info.get_cn_data_frame()
        if not cn_data.has_bgr_image():
          continue
        frame_data = cn_data.image_bgr()
        ret, jpg_data = cv2.imencode(".jpg", cv2.resize(frame_data, preview_video_size))
        if ret:
          frame_bytes = jpg_data.tobytes()
          yield (b'--frame\r\n'
                 b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
      done = time.time()
      elapsed = done - start
      if render_with_interval:
        if 1/render_fps - elapsed > 0:
          time.sleep(1/render_fps - elapsed)
      render_with_interval = True
    else:
      yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + background_img_bytes + b'Content-Type: image/jpeg\r\n\r\n')
      time.sleep(1)
  yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + background_img_bytes + b'Content-Type: image/jpeg\r\n\r\n')


def getSourceUrl(filename):
  global render_fps
  if filename:
    render_fps = 30
    if filename == "cars":
      filename = "cars.mp4"
    elif filename == "people":
      filename = "1080P.h264"
    elif filename == "images":
      filename = "%d.jpg"
      render_fps = 1
    elif filename == "objects":
      filename = "objects.mp4"
    else :
      filename = user_media + "/" + filename
    filename = data_path + filename
  return filename


def getDemoConsoleOutput():
  while not log_queue.empty():
    yield log_queue.get().getMessage()+"\n"
