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

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
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
DEFINE_bool(raw_img_input, false, "feed decompressed image to source");
DEFINE_bool(use_cv_mat, true, "feed cv mat to source. It is valid only if ``raw_img_input`` is set to true");

cnstream::Displayer* gdisplayer = nullptr;

std::atomic<bool> thread_running{true};

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int stream_cnt, cnstream::Pipeline* pipeline, std::string source_name) :
    stream_cnt_(stream_cnt),
    pipeline_(pipeline),
    source_name_(source_name) {}

  void Update(const cnstream::StreamMsg& smsg) override {
    std::lock_guard<std::mutex> lg(mutex_);
    if (stop_) return;
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      eos_stream_.push_back(smsg.stream_id);
      LOG(INFO) << "[Observer] received EOS from stream:" << smsg.stream_id;
      if (static_cast<int>(eos_stream_.size()) == stream_cnt_) {
        LOG(INFO) << "[Observer] received all EOS";
        stop_ = true;
      }
    } else if (smsg.type == cnstream::StreamMsgType::STREAM_ERR_MSG) {
      LOG(WARNING) << "[Observer] received stream error from stream: " << smsg.stream_id
        << ", remove it from pipeline.";
      cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline_->GetModule(source_name_));
      if (source) source->RemoveSource(smsg.stream_id);
      pipeline_->RemovePerfManager(smsg.stream_id);
      stream_cnt_--;
    } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
      LOG(ERROR) << "[Observer] received ERROR_MSG";
      stop_ = true;
    }
    if (stop_) {
      wakener_.notify_one();
    }
  }

  void WaitForStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    stop_ = false;
    wakener_.wait(lk, [this]() {return stop_; });
    pipeline_->Stop();
  }

  void IncreaseStreamCnt() { stream_cnt_++; }
  void DecreaseStreamCnt() { stream_cnt_--; }
  int GetStreamCnt() { return stream_cnt_.load(); }

 private:
  std::atomic<int> stream_cnt_;
  cnstream::Pipeline* pipeline_ = nullptr;
  std::string source_name_;
  bool stop_ = false;
  std::vector<std::string> eos_stream_;
  std::condition_variable wakener_;
  mutable std::mutex mutex_;
};

int AddSourceForRtspStream(cnstream::DataSource* source, const std::string &stream_id, const std::string &filename) {
  auto handler = cnstream::RtspHandler::Create(source, stream_id, filename);
  return source->AddSource(handler);
}

int AddSourceForUsbCam(cnstream::DataSource* source, const std::string &stream_id, const std::string &filename,
                       const int &frame_rate, const bool &loop) {
  int ret = -1;
#ifdef HAVE_FFMPEG_AVDEVICE
  auto handler = cnstream::UsbHandler::Create(source, stream_id, filename, frame_rate, loop);
  ret = source->AddSource(handler);
#endif  // HAVE_FFMPEG_AVDEVICE
  return ret;
}

int AddSourceForVideoInMem(cnstream::DataSource* source, const std::string &stream_id, const std::string &filename,
                           const bool &loop, std::vector<std::thread>* thread_vec) {
  auto handler = cnstream::ESMemHandler::Create(source, stream_id);
  int ret = source->AddSource(handler);
  if (ret != 0) return ret;
  // Start another thread to read data from file to memory and feed data to pipeline.
  thread_vec->push_back(std::thread([=]() {
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) return;
    auto memHandler = std::dynamic_pointer_cast<cnstream::ESMemHandler>(handler);
    memHandler->SetDataType(cnstream::ESMemHandler::H264);
    unsigned char buf[4096];
    while (thread_running.load()) {
      if (!feof(fp)) {
        int size = fread(buf, 1, 4096, fp);
        memHandler->Write(buf, size);
      } else if (loop) {
        fseek(fp, 0, SEEK_SET);
      } else {
        break;
      }
    }
    memHandler->Write(nullptr, 0);
  }));
  return 0;
}

int AddSourceForImageInMem(cnstream::DataSource* source, const std::string &stream_id, const std::string &filename,
                           const bool &loop, std::vector<std::thread>* thread_vec) {
  // Jpeg decoder maximum resolution 8K
  int max_width = 7680;  // FIXME
  int max_height = 4320;  // FIXME

  auto handler = cnstream::ESJpegMemHandler::Create(source, stream_id, max_width, max_height);
  int ret = source->AddSource(handler);
  if (ret != 0) return ret;
  // Start another thread to read data from files to memory and feed data to pipeline.
  thread_vec->push_back(std::thread([=]() {
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
    while (thread_running.load() && itor != files.end()) {
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
      if (itor == files.end() && loop) {
        itor = files.begin();
      }
    }
    pkt.data = nullptr;
    pkt.size = 0;
    pkt.flags = cnstream::ESPacket::FLAG_EOS;
    memHandler->Write(&pkt);
    delete [] buf, buf = nullptr;
  }));
  return 0;
}

int AddSourceForDecompressedImage(cnstream::DataSource* source, const std::string &stream_id,
                                  const std::string &filename, const bool &loop,
                                  const bool &use_cv_mat, std::vector<std::thread>* thread_vec) {
  // The following code is only for image input. For video input, you could use opencv VideoCapture.
  int ret = -1;
#ifdef HAVE_OPENCV
  auto handler = cnstream::RawImgMemHandler::Create(source, stream_id);
  ret = source->AddSource(handler);
  if (ret != 0) return ret;
  // Start another thread to read data from files to cv mat and feed data to pipeline.
  thread_vec->push_back(std::thread([=]() {
    int index = filename.find_last_of("/");
    std::string dir_path = filename.substr(0, index);
    std::list<std::string> files = GetFileNameFromDir(dir_path, "*.jpg");
    auto memHandler = std::dynamic_pointer_cast<cnstream::RawImgMemHandler>(handler);
    auto itor = files.begin();

    while (thread_running.load() && itor != files.end()) {
      cv::Mat bgr_frame = cv::imread(*itor);
      if (!bgr_frame.empty()) {
        if (use_cv_mat) {
          // feed bgr24 image mat, with api-Write(cv::Mat)
          memHandler->Write(&bgr_frame);
        } else {
          // feed rgb24 image data, with api-Write(unsigned char* data, int size, int w, int h, cnstream::CNDataFormat)
          cv::Mat rgb_frame(bgr_frame.rows, bgr_frame.cols, CV_8UC3);
          cv::cvtColor(bgr_frame, rgb_frame, cv::COLOR_BGR2RGB);
          memHandler->Write(rgb_frame.data, rgb_frame.cols * rgb_frame.rows * 3, rgb_frame.cols, rgb_frame.rows,
                            cnstream::CN_PIXEL_FORMAT_RGB24);
        }
      }
      itor++;
      if (itor == files.end() && loop) {
        itor = files.begin();
      }
    }
    memHandler->Write(nullptr);
  }));
#else
  LOG(ERROR) << "OPENCV is not linked, can not support cv::mat or raw image data with bgr24/rgb24 "
    "format.";
#endif
  return ret;
}

int AddSourceForFile(cnstream::DataSource* source, const std::string &stream_id, const std::string &filename,
                     const int &frame_rate, const bool &loop) {
  auto handler = cnstream::FileHandler::Create(source, stream_id, filename, frame_rate, loop);
  return source->AddSource(handler);
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  LOG(INFO) << "CNSTREAM VERSION:" << cnstream::VersionString();

  /*
    flags to variables
  */
  std::list<std::string> video_urls = ::ReadFileList(FLAGS_data_path);
  std::string source_name = "source";  // source module name, which is defined in pipeline json config

  /*
    build pipeline
  */
  cnstream::Pipeline pipeline("MyPipeline");
  // pipeline.BuildPipeline({source_config, detector_config, tracker_config});

  if (0 != pipeline.BuildPipelineByJSONFile(FLAGS_config_fname)) {
    LOG(ERROR) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  /*
    message observer
   */
  MsgObserver msg_observer(static_cast<int>(video_urls.size()), &pipeline, source_name);
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  /*
    find data source
   */
  cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule(source_name));
#ifdef BUILD_IPC
  cnstream::ModuleIPC* ipc = dynamic_cast<cnstream::ModuleIPC*>(pipeline.GetModule("ipc"));
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
    if (!pipeline.CreatePerfManager({}, FLAGS_perf_db_dir)) {
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
  int streams = static_cast<int>(video_urls.size());
  auto url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string& filename = *url_iter;
    std::string stream_id = "stream_" + std::to_string(i);
    if (FLAGS_perf) {
      pipeline.AddPerfManager(stream_id, FLAGS_perf_db_dir);
    }
    int ret = 0;

    if (filename.find("rtsp://") != std::string::npos) {
      ret = AddSourceForRtspStream(source, stream_id, filename);
    } else if (filename.find("/dev/video") != std::string::npos) {  // only support linux
      ret = AddSourceForUsbCam(source, stream_id, filename,  FLAGS_src_frame_rate, FLAGS_loop);
    } else if (filename.find(".jpg") != std::string::npos && FLAGS_jpeg_from_mem) {
      ret = AddSourceForImageInMem(source, stream_id, filename, FLAGS_loop, &vec_threads_mem);
    } else if (filename.find(".jpg") != std::string::npos && FLAGS_raw_img_input) {
      ret = AddSourceForDecompressedImage(source, stream_id, filename, FLAGS_loop, FLAGS_use_cv_mat, &vec_threads_mem);
    } else if (filename.find(".h264") != std::string::npos) {
      ret = AddSourceForVideoInMem(source, stream_id, filename, FLAGS_loop, &vec_threads_mem);
    } else {
      ret = AddSourceForFile(source, stream_id, filename, FLAGS_src_frame_rate, FLAGS_loop);
    }
    if (ret != 0) {
      msg_observer.DecreaseStreamCnt();
      if (FLAGS_perf) pipeline.RemovePerfManager(stream_id);
    }
  }

#ifdef BUILD_IPC
  if (nullptr != ipc) {
    ipc->SetChannelCount(msg_observer.GetStreamCnt());
  }
#endif

  auto quit_callback = [&pipeline, streams, &source]() {
    // stop feed-data threads before remove-sources...
    thread_running.store(false);
    for (int i = 0; i < streams; i++) {
      source->RemoveSource("stream_" + std::to_string(i));
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

      thread_running.store(false);

      for (int i = 0; i < streams; i++) {
        source->RemoveSource("stream_" + std::to_string(i));
      }

      pipeline.Stop();
    } else {
      /*
       * stop by hand or by FLGAS_wait_time
       */
      if (FLAGS_wait_time) {
        std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
        thread_running.store(false);
        for (int i = 0; i < streams; i++) {
          source->RemoveSource("stream_" + std::to_string(i));
        }
        pipeline.Stop();
      } else {
        msg_observer.WaitForStop();
        thread_running.store(false);
      }
    }
  }

  for (auto& thread_id : vec_threads_mem) {
    if (thread_id.joinable()) {
      thread_id.join();
    }
  }
  vec_threads_mem.clear();

  google::ShutdownGoogleLogging();
  return EXIT_SUCCESS;
}
