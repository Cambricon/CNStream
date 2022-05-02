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
import cnstream
import observer

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

class Yolov3Preproc(cnstream.Preproc):
    def __init__(self):
        cnstream.Preproc.__init__(self)

    def init(self, params):
        return True

    def execute(self, input_shapes, frame_info):
        data_frame = frame_info.get_cn_data_frame()
        bgr = data_frame.image_bgr()
        src_w = data_frame.width
        src_h = data_frame.height
        dst_w = input_shapes[0][2]
        dst_h = input_shapes[0][1]
        # resize as letterbox
        scaling_factor = min(dst_w / src_w, dst_h / src_h)
        unpad_w = math.ceil(scaling_factor * src_w)
        unpad_h = math.ceil(scaling_factor * src_h)
        resized = cv2.resize(bgr, (unpad_w, unpad_h))
        # to rgb
        rgb = resized[:,:,::-1]
        # padding
        pad_w = dst_w - unpad_w
        pad_h = dst_h - unpad_h
        pad_l = math.floor(pad_w / 2)
        pad_t = math.floor(pad_h / 2)
        pad_r = pad_w - pad_l
        pad_b = pad_h - pad_t
        dst_img = cv2.copyMakeBorder(rgb, pad_t, pad_b, pad_l, pad_r, cv2.BORDER_CONSTANT, (128, 128, 128))
        # to 0rgb
        argb = cv2.merge([np.zeros((dst_h, dst_w, 1), np.uint8), dst_img])
        # save pad params to frame_info
        collection = frame_info.get_py_collection()
        collection['unpad_h'] = unpad_h
        collection['unpad_w'] = unpad_w
        collection['pad_l'] = pad_l
        collection['pad_t'] = pad_t
        return [np.asarray(argb).flatten().astype(np.float32)]

def to_range(val : float, min_val, max_val):
    return min(max(val, min_val), max_val)

class Yolov3Postproc(cnstream.Postproc):
    def __init__(self):
        cnstream.Postproc.__init__(self)
        self.__threshold = 0.3

    def init(self, params):
        if 'threshold' in params:
            self.__threshold = float(params['threshold'])
        return True

    def execute(self, net_outputs, input_shapes, frame_info):
        collection = frame_info.get_py_collection()
        unpad_h = collection['unpad_h']
        unpad_w = collection['unpad_w']
        pad_l = collection['pad_l']
        pad_t = collection['pad_t']
        # model input height
        input_h = input_shapes[0][1]
        # model input width
        input_w = input_shapes[0][2]
        net_output = net_outputs[0].flatten()
        box_num = int(net_output[0])
        # get bboxes
        for box_id in range(box_num):
            label = str(int(net_output[64 + box_id * 7 + 1]))
            score = net_output[64 + box_id * 7 + 2]
            left = to_range((net_output[64 + box_id * 7 + 3] * input_w - pad_l) / unpad_w, 0, 1)
            top = to_range((net_output[64 + box_id * 7 + 4] * input_h - pad_t) / unpad_h, 0, 1)
            right = to_range((net_output[64 + box_id * 7 + 5] * input_w - pad_l) / unpad_w, 0, 1)
            bottom = to_range((net_output[64 + box_id * 7 + 6] * input_h - pad_t) / unpad_h, 0, 1)
            if left >= right or top >= bottom:
                continue
            if score < self.__threshold:
                continue
            # add detection object to frame_info
            detection_object = cnstream.CNInferObject()
            detection_object.id = label
            detection_object.score = score
            detection_object.bbox.x = left
            detection_object.bbox.y = top
            detection_object.bbox.w = right - left
            detection_object.bbox.h = bottom - top
            frame_info.get_cn_infer_objects().push_back(detection_object)

def main():
    model_file = os.path.join(os.path.abspath(os.path.dirname(__file__)),
        "../../data/models/yolov3_b4c4_argb_mlu270.cambricon")
    if not os.path.exists(model_file):
        os.makedirs(os.path.dirname(model_file),exist_ok=True)
        import urllib.request
        url_str = "http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon"
        print(f'Downloading {url_str} ...')
        urllib.request.urlretrieve(url_str, model_file)
    global g_perf_print_lock, g_perf_print_stop
    # Build a pipeline
    pipeline = cnstream.Pipeline("yolov3_detection_pipeline")
    pipeline.build_pipeline_by_json_file('yolov3_detection_config.json')

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
    stream_num = 1
    for i in range(stream_num):
        stream_id = "stream_id_{}".format(i)
        file_handler = cnstream.FileHandler(source, stream_id, mp4_path, -1)
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
