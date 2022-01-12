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

import threading
from concurrent.futures import ThreadPoolExecutor, wait, ALL_COMPLETED
import time
from webserver.logger import *

class PrintPerformance:
    def __init__(self, pipeline, perf_level=0):
        self.pipeline = pipeline
        self.perf_level = perf_level

    def print_process_perf(self, profile):
        if self.perf_level <= 1:
            if self.perf_level == 1:
                logger.info("[Latency]: (Avg): {:.4f}ms, (Min): {:.4f}ms, (Max): {:.4f}ms".format(
                        profile.latency, profile.minimum_latency, profile.maximum_latency))
            logger.info("[Counter]: {}, [Throughput]: {:.4f}fps".format(profile.counter, profile.fps))
        elif self.perf_level >= 2:
            logger.info("[Counter]: {}, [Completed]: {}, [Dropped]: {}, [Ongoing]: {}".format(
                    profile.counter, profile.completed, profile.dropped, profile.ongoing))
            logger.info("[Latency]: (Avg): {:.4f}ms, (Min): {:.4f}ms, (Max): {:.4f}ms".format(
                    profile.latency, profile.minimum_latency, profile.maximum_latency))
            logger.info("[Throughput]: {:.4f}fps".format(profile.fps))

        if self.perf_level >= 3:
            logger.info("\n------ Stream ------\n")
            for stream_profile in profile.stream_profiles:
                logger.info("{: <15s} [Counter]: {}, [Completed]: {}, [Dropped]: {}".format("[" + stream_profile.stream_name + "]",
                       stream_profile.counter, stream_profile.completed, stream_profile.dropped))
                logger.info("{: <15s} [Latency]: (Avg): {:.4f}ms, (Min): {:.4f}ms, (Max): {:.4f}ms".format("",
                       stream_profile.latency, stream_profile.minimum_latency, stream_profile.maximum_latency))
                logger.info("{: <15s} [Throughput]: {:.4f}fps".format("", stream_profile.fps))

    def print_pipeline_perf(self, profile, prefix_str):
        logger.info(" {:*^65s}".format("  Performance Print Start  (" + prefix_str + ")  "))
        logger.info("{:=^65s}".format("  Pipeline: [" + profile.pipeline_name + "]  "))

        for module_profile in profile.module_profiles:
          logger.info("{:-^65s}".format(" Module: [" + module_profile.module_name + "] "))
          for process_profile in module_profile.process_profiles:
            logger.info("{:->10s}{}".format("", " Process Name: [" + process_profile.process_name + "]"))
            self.print_process_perf(process_profile)
        logger.info("{:-^65s}".format("  Overall  "))
        self.print_process_perf(profile.overall_profile)
        logger.info("{:*^65s}".format("  Performance Print End  (" + prefix_str + ")  "))

    def print_whole(self):
        if self.pipeline.is_profiling_enabled():
            self.print_pipeline_perf(self.pipeline.get_profile(), "Whole")

    def print_latest(self, duration):
        if self.pipeline.is_profiling_enabled():
            self.print_pipeline_perf(self.pipeline.get_profile_before(duration),
                                     "Last {:d} Seconds".format(duration//1000))


class PrintPerformanceLoop():
    def __init__(self, pipeline, perf_level=0):
        self.condition = threading.Condition()
        self.stop_flag = False
        self.print_perf = PrintPerformance(pipeline, perf_level)
        self.executor = ThreadPoolExecutor(max_workers=1)
        self.task = []

    def print_perf_loop(self):
        print_interval = 2  # print every 2 seconds
        last_time = time.time()
        while True:
            if self.condition.acquire():
                elapsed_time = time.time() - last_time
                if not self.stop_flag:
                    self.condition.wait(print_interval - elapsed_time)
                else:
                    self.condition.release()
                    break
                self.condition.release()
            last_time = time.time()
            # print whole process performance
            self.print_perf.print_whole()
            # print real time performance (last 2 seconds)
            self.print_perf.print_latest(print_interval * 1000)

    def start(self):
        self.task.append(self.executor.submit(self.print_perf_loop))

    def stop(self):
        if self.condition.acquire():
            self.stop_flag = True
            self.condition.notify()
            self.condition.release()
        wait(self.task, return_when=ALL_COMPLETED)
