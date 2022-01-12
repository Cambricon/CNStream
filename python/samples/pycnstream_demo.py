import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
import time
import threading
import cv2
import utils

g_source_lock = threading.Lock()

cur_file_dir = os.path.split(os.path.realpath(__file__))[0]

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
                self.source.remove_source(msg.stream_id, True)
                self.stream_set.remove(msg.stream_id)
            if len(self.stream_set) == 0:
                print("pipeline[{}] received all EOS".format(self.pipeline.get_name()))
                self.stop = True
        elif msg.type == StreamMsgType.error_msg:
            print("pipeline[{}] gets error".format(self.pipeline.get_name()))
            self.source.remove_sources(True)
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

class OneModuleObserver(ModuleObserver):
    def __init__(self) -> None:
        super().__init__()

    def notify(self, frame: 'probe data from one node'):
      cn_data = frame.get_cn_data_frame()
      frame_id = cn_data.frame_id
      stream_id = frame.stream_id
      print("receive the frame {} from {}".format(frame_id, stream_id))


def receive_processed_frame(frame):
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
    model_file = os.path.join(os.path.abspath(os.path.dirname(__file__)), "../../data/models/yolov3_b4c4_argb_mlu270.cambricon")
    if not os.path.exists(model_file):
        os.makedirs(os.path.dirname(model_file),exist_ok=True)
        import urllib.request
        url_str = "http://video.cambricon.com/models/MLU270/yolov3_b4c4_argb_mlu270.cambricon"
        print('Downloading {} ...'.format(url_str))
        urllib.request.urlretrieve(url_str, model_file)

    global g_source_lock
    # Build a pipeline
    pipeline = Pipeline("my_pipeline")
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
    obs = CustomObserver(pipeline, source)
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
        file_handler = FileHandler(source, stream_id, mp4_path, -1)
        g_source_lock.acquire()
        if source.add_source(file_handler) != 0:
            print("Add source failed stream []".format(stream_id))
        else:
            obs.increase_stream(stream_id)
        g_source_lock.release()

    obs.wait_for_stop()

    print_perf_loop.stop()
    utils.PrintPerformance(pipeline, perf_level=perf_level).print_whole()

    print("pipeline[{}] stops".format(pipeline.get_name()))

if __name__ == '__main__':
  main()
