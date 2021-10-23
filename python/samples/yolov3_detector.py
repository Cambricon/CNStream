import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
import time
import threading
import cv2
import math
import numpy as np

g_source_lock = threading.Lock()
g_perf_print_lock = threading.Lock()
g_perf_print_stop = False

class CustomObserver(StreamMsgObserver):
    def __init__(self, pipeline, source):
        StreamMsgObserver.__init__(self)
        self.pipeline = pipeline
        self.source = source
        self.stop = False
        self.wakener = threading.Condition()
        self.stream_set = set()

    def update(self, msg):
        global g_source_lock
        g_source_lock.acquire()
        self.wakener.acquire()
        if self.stop:
            return
        if msg.type == StreamMsgType.eos_msg:
            print("pipeline[{}] stream[{}] gets EOS".format(self.pipeline.get_name(), msg.stream_id))
            if msg.stream_id in self.stream_set:
                self.source.remove_source(msg.stream_id)
                self.stream_set.remove(msg.stream_id)
            if len(self.stream_set) == 0:
                print("pipeline[{}] received all EOS".format(self.pipeline.get_name()))
                self.stop = True
        elif msg.type == StreamMsgType.stream_err_msg:
            print("pipeline[{}] stream[{}] gets stream error".format(self.pipeline.get_name(), msg.stream_id))
            if msg.stream_id in self.stream_set:
                self.source.remove_source(msg.stream_id)
                self.stream_set.remove(msg.stream_id)
            if len(self.stream_set) == 0:
                print("pipeline[{}] received all EOS".format(self.pipeline.get_name()))
                self.stop = True
        elif msg.type == StreamMsgType.error_msg:
            print("pipeline[{}] gets error".format(self.pipeline.get_name()))
            self.source.remove_sources()
            self.stream_set.clear()
            self.stop = True
        elif msg.type == StreamMsgType.frame_err_msg:
            print("pipeline[{}] stream[{}] gets frame error".format(self.pipeline.get_name(), msg.stream_id))
        else:
            print("pipeline[{}] unknown message type".format(self.pipeline.get_name()))
        if self.stop:
          self.wakener.notify()

        self.wakener.release()
        g_source_lock.release()


    def wait_for_stop(self):
        self.wakener.acquire()
        if len(self.stream_set) == 0:
            self.stop = True
        self.wakener.release()
        while True:
            if self.wakener.acquire():
                if not self.stop:
                    self.wakener.wait()
                else:
                    self.pipeline.stop()
                    break
        self.wakener.release()


    def increase_stream(self, stream_id):
        self.wakener.acquire()
        if stream_id in self.stream_set:
            print("increase_stream() The stream is ongoing [{}]".format(stream_id))
        else:
            self.stream_set.add(stream_id)
            if self.stop:
                stop = False
        self.wakener.release()

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
            print_pipeline_performance(pipeline)
            # print real time performance (last 2 seconds)
            print_pipeline_performance(pipeline, 2000)
        g_perf_print_lock.release()

class Yolov3Preproc(Preproc):
    def __init__(self):
        Preproc.__init__(self)

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

class Yolov3Postproc(Postproc):
    def __init__(self):
        Postproc.__init__(self)
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
        input_h = input_shapes[0][1]  # model input height
        input_w = input_shapes[0][2]  # model input width
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
            if left >= right or top >= bottom: continue
            if score < self.__threshold: continue
            # add detection object to frame_info
            detection_object = CNInferObject()
            detection_object.id = label
            detection_object.score = score
            detection_object.bbox.x = left
            detection_object.bbox.y = top
            detection_object.bbox.w = right - left
            detection_object.bbox.h = bottom - top
            frame_info.get_cn_infer_objects().push_back(detection_object)

def main():
    model_file = os.path.join(os.path.abspath(os.path.dirname(__file__)), "../../data/models/yolov3_b4c4_argb_mlu270.cambricon")
    if not os.path.exists(model_file):
        os.makedirs(os.path.dirname(model_file),exist_ok=True)
        import urllib.request
        url_str = "http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon"
        print(f'Downloading {url_str} ...')
        urllib.request.urlretrieve(url_str, model_file)
    global g_source_lock, g_perf_print_lock, g_perf_print_stop
    # Build a pipeline
    pipeline = Pipeline("yolov3_detection_pipeline")
    pipeline.build_pipeline_by_json_file('yolov3_detection_config.json')

    # Get pipeline's source module
    source_module_name = 'source'
    source = pipeline.get_source_module(source_module_name)

    # Set message observer
    obs = CustomObserver(pipeline, source)
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
        file_handler = FileHandler(source, stream_id, mp4_path, -1)
        g_source_lock.acquire()
        if source.add_source(file_handler) != 0:
            print("Add source failed stream []".format(stream_id))
        else:
            obs.increase_stream(stream_id)
        g_source_lock.release()

    obs.wait_for_stop()

    if pipeline.is_profiling_enabled():
        g_perf_print_lock.acquire()
        g_perf_print_stop = True
        g_perf_print_lock.release()
        perf_th.join()
        print_pipeline_performance(pipeline)

    print("pipeline[{}] stops".format(pipeline.get_name()))

if __name__ == '__main__':
  main()
