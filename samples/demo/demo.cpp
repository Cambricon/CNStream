/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <future>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#endif

#include "cnstream_core.hpp"
#include "data_handler_file.hpp"
#include "data_handler_mem.hpp"
#include "data_handler_rtsp.hpp"
#include "data_source.hpp"
#include "displayer.hpp"
#include "util.hpp"
#ifdef BUILD_IPC
#include "module_ipc.hpp"
#endif

DEFINE_string(data_path, "", "video file list.");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_int32(wait_time, 0, "time of one test case");
DEFINE_bool(loop, false, "display repeat");
DEFINE_string(config_fname, "", "pipeline config filename");
#ifdef HAVE_SQLITE
DEFINE_bool(perf, true, "measure performance");
#else
DEFINE_bool(perf, false, "measure performance");
#endif
DEFINE_string(perf_db_dir, "", "directory of performance database");
DEFINE_bool(jpeg_from_mem, false, "Jpeg bitstream from mem.");
DEFINE_bool(raw_img_input, false, "feed raw image to source");

cnstream::Displayer* gdisplayer = nullptr;
static std::mutex g_vcap_mtx;

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int stream_cnt, cnstream::Pipeline* pipeline) : stream_cnt_(stream_cnt), pipeline_(pipeline) {}

  void Update(const cnstream::StreamMsg& smsg) override {
    if (stop_) return;
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      eos_stream_.push_back(smsg.stream_id);
      LOG(INFO) << "[Observer] received EOS from stream:" << smsg.stream_id;
      if (static_cast<int>(eos_stream_.size()) == stream_cnt_) {
        LOG(INFO) << "[Observer] received all EOS";
        stop_ = true;
        wakener_.set_value(0);
      }
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      LOG(ERROR) << "[Observer] received ERROR_MSG";
      stop_ = true;
      wakener_.set_value(1);
    }
  }

  void WaitForStop() {
    wakener_.get_future().get();
    pipeline_->Stop();
  }

  void SetStreamCnt(int stream_cnt) { stream_cnt_ = stream_cnt; }

 private:
  int stream_cnt_ = 0;
  cnstream::Pipeline* pipeline_ = nullptr;
  bool stop_ = false;
  std::vector<std::string> eos_stream_;
  std::promise<int> wakener_;
};

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  std::cout << "\033[01;31m"
            << "CNSTREAM VERSION:" << cnstream::VersionString() << "\033[0m" << std::endl;

  /*
    flags to variables
  */
  std::list<std::string> video_urls = ::ReadFileList(FLAGS_data_path);

  /*
    build pipeline
  */
  cnstream::Pipeline pipeline("pipeline");
  // pipeline.BuildPipeline({source_config, detector_config, tracker_config});

  if (0 != pipeline.BuildPipelineByJSONFile(FLAGS_config_fname)) {
    LOG(ERROR) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  /*
    message observer
   */
  MsgObserver msg_observer(static_cast<int>(video_urls.size()), &pipeline);
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  /*
    find data source
   */
  cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule("source"));
#ifdef BUILD_IPC
  cnstream::ModuleIPC* ipc = dynamic_cast<cnstream::ModuleIPC*>(pipeline.GetModule("ipc"));
  if (ipc != nullptr) {
    ipc->SetChannelCount(video_urls.size());
  }
  if (nullptr == source && (nullptr == ipc)) {
    LOG(ERROR) << "DataSource && ModuleIPC module both not found.";
#else
  if (nullptr == source) {
    LOG(ERROR) << "DataSource module not found.";
#endif
    return EXIT_FAILURE;
  }

  /*
    create perf recorder
  */
  if (FLAGS_perf) {
    std::vector<std::string> stream_ids;
    for (int i = 0; i < static_cast<int>(video_urls.size()); i++) {
      stream_ids.push_back(std::to_string(i));
    }
    if (!pipeline.CreatePerfManager(stream_ids, FLAGS_perf_db_dir)) {
      LOG(ERROR) << "Pipeline Create Perf Manager failed.";
      return EXIT_FAILURE;
    }
  }

  /*
    start pipeline
  */
  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*
    add stream sources...
  */
  std::vector<std::thread> vec_threads_mem;
  bool thread_running_ = true;
  int streams = static_cast<int>(video_urls.size());
  auto url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string& filename = *url_iter;
    if (filename.find("rtsp://") != std::string::npos) {
      auto handler = cnstream::RtspHandler::Create(source, std::to_string(i), filename);
      source->AddSource(handler);
    } else if (filename.find(".h264") != std::string::npos) {
      // es-mem handler
      auto handler = cnstream::ESMemHandler::Create(source, std::to_string(i));
      source->AddSource(handler);
      // use a separate thread to read data from memory and feed pipeline
      vec_threads_mem.push_back(std::thread([=]() {
        FILE* fp = fopen(filename.c_str(), "rb");
        if (fp) {
          auto memHandler = std::dynamic_pointer_cast<cnstream::ESMemHandler>(handler);
          memHandler->SetDataType(cnstream::ESMemHandler::H264);
          unsigned char buf[4096];
          while (thread_running_) {
            if (!feof(fp)) {
              int size = fread(buf, 1, 4096, fp);
              memHandler->Write(buf, size);
            } else {
              if (FLAGS_loop) {
                fseek(fp, 0, SEEK_SET);
              } else {
                break;
              }
            }
          }
          memHandler->Write(nullptr, 0);
          fclose(fp);
        }
      }));
    } else if (filename.find(".jpg") != std::string::npos && FLAGS_jpeg_from_mem) {
      // Jpeg decoder maximum resolution 8K
      int max_width = 7680;  // FIXME
      int max_height = 4320;  // FIXME
      // es-jpeg-mem handler
      auto handler = cnstream::ESJpegMemHandler::Create(source, std::to_string(i), max_width, max_height);
      source->AddSource(handler);
      // use a separate thread to read data from memory and feed pipeline
      vec_threads_mem.push_back(
        std::thread([=]() {
        int index = filename.find_last_of("/");
        std::string dir_path = filename.substr(0, index);
        std::list<std::string> files = GetFileNameFromDir(dir_path, "*.jpg");

        auto memHandler = std::dynamic_pointer_cast<cnstream::ESJpegMemHandler>(handler);
        size_t jpeg_buffer_size_ = 4 * 1024 * 1024;  // FIXME
        unsigned char *buf = new(std::nothrow) unsigned char[jpeg_buffer_size_];

        if (!buf) {
          LOG(FATAL) << "malloc buf failed, size: " << jpeg_buffer_size_;
        }

        cnstream::ESPacket pkt;
        uint64_t pts_ = 0;
        auto itor = files.begin();
        while (thread_running_ && itor != files.end()) {
          size_t file_size = GetFileSize(*itor);
          if (file_size > jpeg_buffer_size_) {
            delete [] buf;
            buf = new(std::nothrow) unsigned char[file_size];
            if (!buf) {
              LOG(FATAL) << "malloc buf failed, size: " << file_size;
            }
            jpeg_buffer_size_ = file_size;
          }
          FILE *fp = fopen((*itor).c_str(), "rb");
          if (fp) {
            int size = fread(buf, 1, jpeg_buffer_size_, fp);
            pkt.data = buf;
            pkt.size = size;
            pkt.pts = pts_++;
            memHandler->Write(&pkt);
            fclose(fp);
          }
          itor++;
          if (itor == files.end() && FLAGS_loop) {
            itor = files.begin();
          }
        }
        pkt.data = nullptr;
        pkt.size = 0;
        pkt.flags = cnstream::ESPacket::FLAG_EOS;
        memHandler->Write(&pkt);
        delete [] buf, buf = nullptr;
      }));
    } else {
      if (FLAGS_raw_img_input) {
        LOG(INFO) << "feed source with raw image mem.";
#ifdef HAVE_OPENCV
        // raw image mem(from cv::Mat or image description) handler
        auto handler = cnstream::RawImgMemHandler::Create(source, std::to_string(i));
        source->AddSource(handler);
        // use a separate thread to read image data from video and feed pipeline
        vec_threads_mem.push_back(std::thread([=]() {
          cv::VideoCapture vcapture;
          g_vcap_mtx.lock();
          vcapture.open(filename);
          g_vcap_mtx.unlock();

          if (!vcapture.isOpened()) {
            LOG(ERROR) << "open file: " << filename << " failed with RawImgMemHandler source type.";
            return;
          }
          cv::Mat bgr_frame;
          auto memHandler = std::dynamic_pointer_cast<cnstream::RawImgMemHandler>(handler);
          while (thread_running_) {
            vcapture >> bgr_frame;
#if 1
            // feed bgr24 image mat, with api-Write(cv::Mat)
            if (bgr_frame.empty()) {
              memHandler->Write(nullptr);
              vcapture.release();
              break;
            }

            memHandler->Write(&bgr_frame);
#else
            // feed rgb24 image data, with api-Write(unsigned char* data, int size, int w, int h,
            // cnstream::CNDataFormat)
            if (bgr_frame.empty()) {
              memHandler->Write(nullptr, 0);
              vcapture.release();
              break;
            }

            cv::Mat rgb_frame(bgr_frame.rows, bgr_frame.cols, CV_8UC3);
            cv::cvtColor(bgr_frame, rgb_frame, cv::COLOR_BGR2RGB);
            memHandler->Write(rgb_frame.data, rgb_frame.cols * rgb_frame.rows * 3, rgb_frame.cols, rgb_frame.rows,
                              cnstream::CN_PIXEL_FORMAT_RGB24);
#endif
          }
#else
        LOG(ERROR) << "OPENCV is not linked, can not support cv::mat or raw image data with bgr24/rgb24 "
                      "format." return EXIT_FAILURE;
#endif
        }));
      } else {
        auto handler =
            cnstream::FileHandler::Create(source, std::to_string(i), filename, FLAGS_src_frame_rate, FLAGS_loop);
        source->AddSource(handler);
      }
    }
  }

  auto quit_callback = [&pipeline, streams, &source, &thread_running_]() {
    // stop feed-data threads before remove-sources...
    thread_running_ = false;
    for (int i = 0; i < streams; i++) {
      source->RemoveSource(std::to_string(i));
    }
    pipeline.Stop();
  };

  gdisplayer = dynamic_cast<cnstream::Displayer*>(pipeline.GetModule("displayer"));

  if (gdisplayer && gdisplayer->Show()) {
    gdisplayer->GUILoop(quit_callback);
  } else {
    /*
     * close pipeline
     */
    if (FLAGS_loop) {
      /*
       * loop, must stop by hand or by FLAGS_wait_time
       */
      if (FLAGS_wait_time) {
        std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
      } else {
        getchar();
      }

      thread_running_ = false;

      for (int i = 0; i < streams; i++) {
        source->RemoveSource(std::to_string(i));
      }

      pipeline.Stop();
    } else {
      /*
       * stop by hand or by FLGAS_wait_time
       */
      if (FLAGS_wait_time) {
        std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
        thread_running_ = false;
        for (int i = 0; i < streams; i++) {
          source->RemoveSource(std::to_string(i));
        }
        pipeline.Stop();
      } else {
        msg_observer.WaitForStop();
        thread_running_ = false;
      }
    }
  }

  for (auto& thread_id : vec_threads_mem) {
    thread_id.join();
  }

  google::ShutdownGoogleLogging();
  return EXIT_SUCCESS;
}
