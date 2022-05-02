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

import threading

import cnstream

g_source_lock = threading.Lock()

# Define a message observer and register with the pipeline
class CustomObserver(cnstream.StreamMsgObserver):
    def __init__(self, pipeline, source):
        cnstream.StreamMsgObserver.__init__(self)
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
        if msg.type == cnstream.StreamMsgType.eos_msg:
            print("pipeline[{}] stream[{}] gets EOS".format(self.pipeline.get_name(), msg.stream_id))
            if msg.stream_id in self.stream_set:
                self.source.remove_source(msg.stream_id)
                self.stream_set.remove(msg.stream_id)
            if len(self.stream_set) == 0:
                print("pipeline[{}] received all EOS".format(self.pipeline.get_name()))
                self.stop = True
        elif msg.type == cnstream.StreamMsgType.stream_err_msg:
            print("pipeline[{}] stream[{}] gets stream error".format(self.pipeline.get_name(), msg.stream_id))
            if msg.stream_id in self.stream_set:
                self.source.remove_source(msg.stream_id, True)
                self.stream_set.remove(msg.stream_id)
            if len(self.stream_set) == 0:
                print("pipeline[{}] received all EOS".format(self.pipeline.get_name()))
                self.stop = True
        elif msg.type == cnstream.StreamMsgType.error_msg:
            print("pipeline[{}] gets error".format(self.pipeline.get_name()))
            self.source.remove_sources(True)
            self.stream_set.clear()
            self.stop = True
        elif msg.type == cnstream.StreamMsgType.frame_err_msg:
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
                self.stop = False
        self.wakener.release()
