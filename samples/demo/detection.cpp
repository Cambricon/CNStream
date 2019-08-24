/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *       http://www.apache.org/licenses/LICENSE-2.0
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

#include <future>
#include <iostream>
#include <mutex>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <cnstream_core.hpp>
#include <data_src.hpp>
#include <decoder.hpp>
#include <encoder.hpp>
#include <inferencer.hpp>
#include <osd.hpp>

#include "util.hpp"

DEFINE_string(data_path, "", "video file list.");
DEFINE_int32(src_w, 1920, "max video resolution(w)");
DEFINE_int32(src_h, 1080, "max video resolution(h)");
DEFINE_int32(target_w, 1920, "decoder output frame size(w)");
DEFINE_int32(target_h, 1080, "decoder output frame size(h)");
DEFINE_double(drop_rate, 0, "Decode drop frame rate (0~1)");
DEFINE_int32(src_frame_rate, 25, "frame rate for send data");
DEFINE_int32(wait_time, 0, "time of one test case");
DEFINE_bool(rtsp, false, "use rtsp");
DEFINE_bool(input_image, false, "input image");
DEFINE_string(dump_dir, "", "dump result images to this directory");
DEFINE_string(label_path, "", "label path");
DEFINE_bool(loop, false, "display repeat");
DEFINE_string(model_path, "", "offline model path");
DEFINE_string(postproc_name, "", "postproc class name");
DEFINE_string(preproc_name, "", "preproc class name");
DEFINE_int32(device_id, 0, "mlu device index");

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int chn_cnt, cnstream::Pipeline* pipeline) : chn_cnt_(chn_cnt), pipeline_(pipeline) {}

  void Update(const cnstream::StreamMsg& smsg) override {
    if (stop_) return;
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      eos_chn_.push_back(smsg.chn_idx);
      if (static_cast<int>(eos_chn_.size()) == chn_cnt_) {
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

 private:
  const int chn_cnt_ = 0;
  cnstream::Pipeline* pipeline_ = nullptr;
  bool stop_ = false;
  std::vector<int> eos_chn_;
  std::promise<int> wakener_;
};

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  std::cout << "\033[01;31m"
            << "CNSTREAM VERSION:" << cnstream::VersionString() << "\033[0m" << std::endl;

  // When the input is images, the loop mode is not supported
  if (FLAGS_loop && FLAGS_input_image) {
    LOG(WARNING) << "When the input is images, the loop mode is not supported!";
  }

  /*
    flags to variables
  */
  std::list<std::string> video_urls = ::ReadFileList(FLAGS_data_path);

  /*
    create pipeline
  */
  cnstream::Pipeline pipeline("pipeline");

  /*
    module configs
  */
  cnstream::CNModuleConfig decoder_config = {"decoder",           /*name*/
                                             "cnstream::Decoder", /*className*/
                                             {
                                                 /*paramSet */
                                                 {"device_id", std::to_string(FLAGS_device_id)},
                                             },
                                             {
                                                 /* next, downstream module names */
                                                 "infer",
                                             }};
  cnstream::CNModuleConfig detector_config = {"infer",                /*name*/
                                              "cnstream::Inferencer", /*className*/
                                              {
                                                  /*paramSet */
                                                  {"model_path", FLAGS_model_path},
                                                  {"func_name", "subnet0"},
                                                  {"postproc_name", FLAGS_postproc_name},
                                                  {"device_id", std::to_string(FLAGS_device_id)},
                                              },
                                              {
                                                  /* next, downstream module names */
                                                  "osd",
                                              }};
  cnstream::CNModuleConfig osd_config = {"osd",           /*name*/
                                         "cnstream::Osd", /*className*/
                                         {
                                             /*paramSet */
                                             {"label_path", FLAGS_label_path},
                                         },
                                         {
                                             /* next, downstream module names */
                                             "encoder",
                                         }};
  cnstream::CNModuleConfig encoder_config = {"encoder", /*name*/
                                             "Encoder", /*className*/
                                             {
                                                 /*paramSet */
                                                 {"dump_dir", FLAGS_dump_dir},
                                             },
                                             {
                                                 /* next, downstream module names */
                                                 /*the last stage*/
                                             }};
  pipeline.AddModuleConfig(decoder_config);
  pipeline.AddModuleConfig(detector_config);
  pipeline.AddModuleConfig(osd_config);
  pipeline.AddModuleConfig(encoder_config);

  /*
   * create modules.
  */
  auto decoder = std::make_shared<cnstream::Decoder>(decoder_config.name);
  auto detector = std::make_shared<cnstream::Inferencer>(detector_config.name);
  auto osd = std::make_shared<cnstream::Osd>(osd_config.name);
  auto encoder = std::make_shared<Encoder>(encoder_config.name);

  /*
    register modules to pipeline
  */
  {
    if (!pipeline.AddModule({decoder, detector, osd, encoder})) {
      LOG(ERROR) << "Add modules failed";
      return -1;
    }

    /*
     * default value 1 will take effect if module parallelism not set.
     * the recommended value of inferencer's thread is the number of video source.
     * decoder is a special module with no need for thread.
     */
    pipeline.SetModuleParallelism(decoder, 0);
    pipeline.SetModuleParallelism(detector, video_urls.size());
    pipeline.SetModuleParallelism(osd, video_urls.size());
    pipeline.SetModuleParallelism(encoder, video_urls.size());
  }

  /*
    link modules
  */
  {
    if (pipeline.LinkModules(decoder, detector).empty()) {
      LOG(ERROR) << "link decoder with detector failed.";
      return EXIT_FAILURE;
    }

    if (pipeline.LinkModules(detector, osd).empty()) {
      LOG(ERROR) << "link detector with osd failed.";
      return EXIT_FAILURE;
    }

    if (pipeline.LinkModules(osd, encoder).empty()) {
      LOG(ERROR) << "link osd with encoder failed.";
      return EXIT_FAILURE;
    }
  }

  /*
    message observer
   */
  MsgObserver msg_observer(static_cast<int>(video_urls.size()), &pipeline);
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  /*
    start pipeline
  */
  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  /*
    open codec channels.
  */
  std::vector<int> codec_chn_idxs;
  int streams = static_cast<int>(video_urls.size());
  while (streams--) {
    cnstream::DecoderAttribute attr;
    attr.max_video_w = FLAGS_src_w;
    attr.max_video_h = FLAGS_src_h;
    /*
      change this value if your video stream is not in h264 formmat.
      see more supported codec type in libstream::CnCodecType.
      use cnstream::Decoder::IsSupported(libstream::CnCodecType) to make sure the type you set is vaild.
    */
    attr.codec_type = libstream::H264;
    if (FLAGS_input_image) attr.codec_type = libstream::JPEG;
    attr.pixel_format = libstream::YUV420SP_NV21;
    /*
      false == cnstream::Decoder::IsSupported(cnstream::SPECIFY_THE_OUTPUT_FRAME_SIZE)
      output_frame_w and output_frame_h will be not active.
    */
    attr.output_frame_w = FLAGS_target_w;
    attr.output_frame_h = FLAGS_target_h;
    /*
      false == cnstream::Decoder::IsSupported(cnstream::SPECIFY_DROP_RATE)
      drop_rate will be not active.
    */
    attr.drop_rate = FLAGS_drop_rate;
    /*
      decoder frame buffer number, recommended value is 3.
    */
    attr.frame_buffer_num = 3;
    attr.dev_id = FLAGS_device_id;
    /*
      frame will be output to mlu.
      if you need to use the CPU for pre-processing, you can set output_on_cpu to true.
    */
    attr.output_on_cpu = false;
    if (FLAGS_rtsp) {
      attr.video_mode = libstream::STREAM_MODE;
    } else {
      attr.video_mode = libstream::FRAME_MODE;
    }

    int chn_idx = decoder->OpenDecodeChannel(attr);
    if (-1 == chn_idx) {
      /*
        open failed, release resouces.
        ~Decoder() will do this, so you do not have to do this.
      */
      for (auto& codec_chn_id : codec_chn_idxs) {
        if (!decoder->CloseDecodeChannel(codec_chn_id)) LOG(FATAL) << "Close decode channel failed.";
      }
      return EXIT_FAILURE;
    }

    codec_chn_idxs.push_back(chn_idx);
  }

  /*
    create data source to send data to decoder,
    data source send data to decoder by Decoder::SendPacket,
    and Decoder::SendPacket will be bind to PostDataFunction.
  */
  assert(codec_chn_idxs.size() == video_urls.size());
  DataSrc data_src;
  std::vector<SourceHandle> source_handles;
  auto url_iter = video_urls.begin();
  for (size_t chn_idx = 0; chn_idx < video_urls.size(); ++chn_idx, url_iter++) {
    const std::string& url = *url_iter;
    PostDataFunction post_data_func =
        std::bind(&cnstream::Decoder::SendPacket, decoder.get(), chn_idx, std::placeholders::_1, std::placeholders::_2);
    SrcType type = VIDEO;
    if (FLAGS_input_image) {
      type = IMAGE;
    } else if (FLAGS_rtsp) {
      type = RTSP;
    }

    SourceHandle handle = data_src.OpenVideoSource(url, FLAGS_src_frame_rate, post_data_func, type, FLAGS_loop);
    if (-1 == handle) {
      LOG(ERROR) << "open video stream failed. url: " << url;
      /*
        open failed, release resouces.
        destructor will do this, so you do not have to do this.
      */
      for (auto& source_handle : source_handles) {
        if (!data_src.CloseVideoSource(source_handle)) LOG(FATAL) << "Close video stream failed.";
      }
      for (auto& codec_chn_id : codec_chn_idxs) {
        if (!decoder->CloseDecodeChannel(codec_chn_id)) LOG(FATAL) << "Close decode channel failed.";
      }
      return EXIT_FAILURE;
    }

    source_handles.push_back(handle);
  }

  /*
    close pipeline
  */
  if (FLAGS_loop) {
    /*
      loop, must stop by hand or by FLAGS_wait_time
    */
    if (FLAGS_wait_time) {
      std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
    } else {
      getchar();
    }

    pipeline.Stop();
  } else {
    /*
      stop by hand or by FLGAS_wait_time
    */
    if (FLAGS_wait_time) {
      std::this_thread::sleep_for(std::chrono::seconds(FLAGS_wait_time));
      pipeline.Stop();
    } else {
      msg_observer.WaitForStop();
    }
  }

  /*
    clear resources in DataSrc and Decoder
    destructor will do this, so you do not have to do this.
  */
  for (auto& source_handle : source_handles) {
    if (!data_src.CloseVideoSource(source_handle)) LOG(FATAL) << "Close video stream failed.";
  }

  pipeline.PrintPerformanceInformation();

  printf("************************Decode Performance*************************\n");
  for (auto& codec_chn_id : codec_chn_idxs) {
    if (!decoder->CloseDecodeChannel(codec_chn_id, true)) LOG(FATAL) << "Close decode channel failed.";
  }
  printf("*******************************************************************\n");

  return EXIT_SUCCESS;
}
