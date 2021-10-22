import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
from cnstream import *
import time
import threading

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


def main():
    global g_source_lock, g_perf_print_lock, g_perf_print_stop
    # Build a pipeline
    pipeline = Pipeline("my_pipeline")
    pipeline.build_pipeline_by_json_file('python_demo_config.json')

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
    stream_num = 4
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