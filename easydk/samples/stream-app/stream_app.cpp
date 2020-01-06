#include <gflags/gflags.h>
#include <glog/logging.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "cnosd.h"
#include "cnpostproc.h"
#include "easybang/resize_and_colorcvt.h"
#include "easycodec/easy_decode.h"
#include "easyinfer/easy_infer.h"
#include "easyinfer/mlu_context.h"
#include "easyinfer/mlu_memory_op.h"
#include "easyinfer/model_loader.h"
#include "easytrack/easy_track.h"
#include "feature_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
}
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define FFMPEG_VERSION_3_1 AV_VERSION_INT(57, 40, 100)

DEFINE_bool(show, true, "show image");
DEFINE_string(data_path, "", "video path");
DEFINE_string(model_path, "", "infer offline model path(with rgb0 output)");
DEFINE_string(label_path, "", "label path");
DEFINE_string(func_name, "subnet0", "model function name");
DEFINE_string(track_model_path, "", "track model path");
DEFINE_string(track_func_name, "subnet0", "track model function name");
DEFINE_int32(wait_time, 0, "time of one test case");

using edk::Shape;
using edk::CnPostproc;
using edk::MluResizeConvertOp;

// send frame queue
static std::queue<edk::CnFrame> g_frames;
static std::mutex g_mut;
static std::condition_variable g_cond;

// params for ffmpeg unpack
static AVFormatContext *g_p_format_ctx;
static AVBitStreamFilterContext *g_p_bsfc;
static AVPacket g_packet;
static AVDictionary *g_options = NULL;
static int32_t g_video_index;
static const char *g_url = "";
static uint64_t g_frame_index;
static bool g_running = false;
static bool g_exit = false;
static bool g_receive_eos = false;
static edk::EasyDecode *g_decode;

bool prepare_video_resource() {
  // init ffmpeg
  avcodec_register_all();
  av_register_all();
  avformat_network_init();
  // format context
  g_p_format_ctx = avformat_alloc_context();
  // g_options
  av_dict_set(&g_options, "buffer_size", "1024000", 0);
  av_dict_set(&g_options, "stimeout", "200000", 0);
  // open input
  int ret_code = avformat_open_input(&g_p_format_ctx, g_url, NULL, &g_options);
  if (0 != ret_code) {
    LOG(ERROR) << "couldn't open input stream.";
    return false;
  }
  // find video stream information
  ret_code = avformat_find_stream_info(g_p_format_ctx, NULL);
  if (ret_code < 0) {
    LOG(ERROR) << "couldn't find stream information.";
    return false;
  }
  g_video_index = -1;
  AVStream *vstream = nullptr;
  for (uint32_t iloop = 0; iloop < g_p_format_ctx->nb_streams; iloop++) {
    vstream = g_p_format_ctx->streams[iloop];
    if (vstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      g_video_index = iloop;
      break;
    }
  }
  if (g_video_index == -1) {
    LOG(ERROR) << "didn't find a video stream.";
    return false;
  }
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  auto codec_id = vstream->codecpar->codec_id;
#else
  auto codec_id = vstream->codec->codec_id;
#endif
  // bitstream filter
  g_p_bsfc = nullptr;
  if (strstr(g_p_format_ctx->iformat->name, "mp4") || strstr(g_p_format_ctx->iformat->name, "flv") ||
      strstr(g_p_format_ctx->iformat->name, "matroska") || strstr(g_p_format_ctx->iformat->name, "rtsp")) {
    if (AV_CODEC_ID_H264 == codec_id) {
      g_p_bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    } else if (AV_CODEC_ID_HEVC == codec_id) {
      g_p_bsfc = av_bitstream_filter_init("hevc_mp4toannexb");
    } else {
      LOG(ERROR) << "nonsupport codec id.";
      return false;
    }
  }
  return true;
}

bool unpack_data(edk::CnPacket *frame) {
  static bool init = false;
  static bool first_frame = true;
  if (!init) {
    if (!prepare_video_resource()) {
      LOG(ERROR) << "open video file error";
      return false;
    } else {
      init = true;
    }
  }
  if (av_read_frame(g_p_format_ctx, &g_packet) >= 0) {
    if (g_packet.stream_index == g_video_index) {
      auto vstream = g_p_format_ctx->streams[g_video_index];
      if (first_frame) {
        if (g_packet.flags & AV_PKT_FLAG_KEY) first_frame = false;
      }
      if (!first_frame) {
        if (g_p_bsfc) {
          av_bitstream_filter_filter(g_p_bsfc, vstream->codec, NULL, reinterpret_cast<uint8_t **>(&frame->data),
                                     reinterpret_cast<int *>(&frame->length), g_packet.data, g_packet.size, 0);
        } else {
          frame->data = g_packet.data;
          frame->length = g_packet.size;
        }
        frame->pts = g_frame_index++;
        return true;
      }
    }
    av_packet_unref(&g_packet);
  } else {
    return false;
  }
  return true;
}

void decode_output_callback(const edk::CnFrame &info) {
  std::unique_lock<std::mutex> lk(g_mut);
  g_frames.push(info);
  g_cond.notify_one();
}

void decode_eos_callback() { g_receive_eos = true; }

void send_eos(edk::EasyDecode *decode) {
  edk::CnPacket pending_frame;
  pending_frame.data = nullptr;
  decode->SendData(pending_frame, true);
}

bool run() {
  std::unique_lock<std::mutex> lk(g_mut);
  edk::MluContext context;
  std::shared_ptr<edk::ModelLoader> loader;
  std::shared_ptr<FeatureExtractor> feature_extractor;
  edk::MluMemoryOp mem_op;
  edk::EasyInfer infer;
  edk::EasyDecode *decode = nullptr;
  edk::FeatureMatchTrack *tracker = nullptr;

  CnOsd osd;
  osd.set_rows(1);
  osd.set_cols(1);
  osd.LoadLabels(FLAGS_label_path);

  Shape in_shape;
  std::vector<Shape> out_shapes;
  std::vector<edk::DetectObject> track_result;
  std::vector<edk::DetectObject> detect_result;
  void **mlu_output = nullptr, **cpu_output = nullptr;
  const uint32_t batch_size = 1;

  try {
    // load offline model
    loader = std::make_shared<edk::ModelLoader>(FLAGS_model_path.c_str(), FLAGS_func_name.c_str());
    loader->InitLayout();
    in_shape = loader->InputShapes()[0];
    out_shapes = loader->OutputShapes();

    // set mlu environment
    context.SetDeviceId(0);
    context.SetChannelId(0);
    context.ConfigureForThisThread();

    // prepare mlu memory operator and memory
    mem_op.SetLoader(loader);

    // init cninfer
    infer.Init(loader, 1, 0);

    // create decoder
    edk::EasyDecode::Attr attr;
    attr.drop_rate = 0;
    attr.maximum_geometry.w = 1920;
    attr.maximum_geometry.h = 1080;
    attr.output_geometry.w = 1280;
    attr.output_geometry.h = 720;
    if (in_shape.c == 1) {
      attr.output_geometry.w = in_shape.w;
      attr.output_geometry.h = in_shape.h / 1.5;
    }
    attr.substream_geometry.w = 0;
    attr.substream_geometry.h = 0;
    attr.codec_type = edk::CodecType::H264;
    attr.video_mode = edk::VideoMode::FRAME_MODE;
    attr.pixel_format = edk::PixelFmt::YUV420SP_NV21;
    attr.dev_id = 0;
    attr.frame_callback = decode_output_callback;
    attr.eos_callback = decode_eos_callback;
    attr.silent = false;
    decode = edk::EasyDecode::Create(attr);
    g_decode = decode;

    tracker = new edk::FeatureMatchTrack;
    feature_extractor = std::make_shared<FeatureExtractor>();
    if (FLAGS_track_model_path != "" && FLAGS_track_model_path != "cpu") {
      feature_extractor->Init(FLAGS_track_model_path.c_str(), FLAGS_track_func_name.c_str());
    }
  } catch (edk::Exception &err) {
    LOG(ERROR) << err.what();
    g_cond.notify_one();
    delete decode;
    lk.unlock();
    g_running = false;
    return false;
  }

  g_running = true;
  g_cond.notify_one();
  lk.unlock();

  // create postprocessor
  auto postproc = new edk::SsdPostproc;
  postproc->set_threshold(0.6);
  assert(nullptr != postproc);

  // alloc memory to store image
  uint32_t show_w = decode->GetAttr().output_geometry.w;
  uint32_t show_h = decode->GetAttr().output_geometry.h;
  auto img_data = new uint8_t[show_w * show_h * 3 / 2];

  void **mlu_input = mem_op.AllocMluInput(batch_size);
  try {
    mlu_output = mem_op.AllocMluOutput(batch_size);
    cpu_output = mem_op.AllocCpuOutput(batch_size);
    int buf_id = 0;

    // create mlu resize and convert op
    MluResizeConvertOp rc_op;
    MluResizeConvertOp::Attr attr;
    attr.src_h = show_h;
    attr.src_w = show_w;
    attr.dst_h = in_shape.h;
    attr.dst_w = in_shape.w;
    attr.batch_size = 1;
    rc_op.SetMluQueue(infer.GetMluQueue());
    if (!rc_op.Init(attr)) {
      LOG(ERROR) << rc_op.GetLastError();
      exit(-1);
    }

    while ((g_running || !g_frames.empty()) && !g_exit) {
      // inference
      std::unique_lock<std::mutex> inner_lk(g_mut);

      if (!g_cond.wait_for(inner_lk, std::chrono::milliseconds(100), [] { return !g_frames.empty(); })) {
        continue;
      }
      edk::CnFrame frame = g_frames.front();
      g_frames.pop();

      // run resize and convert
      if (in_shape.c != 1) {
        void *rc_output = mlu_input[0];
        auto src_y = frame.ptrs[0];
        auto src_uv = frame.ptrs[1];
        if (-1 == rc_op.InvokeOp(rc_output, src_y, src_uv)) {
          LOG(ERROR) << rc_op.GetLastError();
          exit(-1);
        }
      }

      // run inference
      if (in_shape.c == 1) {
        void *decode_output[] = {frame.ptrs[0]};
        infer.Run(decode_output, mlu_output);
      } else {
        infer.Run(mlu_input, mlu_output);
      }
      mem_op.MemcpyOutputD2H(cpu_output, mlu_output, batch_size);

      // copy out frame
      decode->CopyFrame(img_data, frame);

      // release codec buffer
      buf_id = frame.buf_id;
      decode->ReleaseBuffer(buf_id);
      // yuv to bgr
      cv::Mat yuv(show_h * 3 / 2, show_w, CV_8UC1, img_data);
      cv::Mat img;
      cv::cvtColor(yuv, img, CV_YUV2BGR_NV21);

      // resize to show
      cv::resize(img, img, cv::Size(1280, 720));

      // post process
      std::vector<std::pair<float *, uint64_t>> postproc_param;
      postproc_param.push_back(std::make_pair(reinterpret_cast<float *>(cpu_output[0]), out_shapes[0].DataCount()));
      detect_result = postproc->Execute(postproc_param);

      // track
      edk::TrackFrame track_img;
      track_img.data = img.data;
      track_img.width = img.cols;
      track_img.height = img.rows;
      track_img.format = edk::TrackFrame::ColorSpace::RGB24;
      static int64_t frame_id = 0;
      track_img.frame_id = frame_id++;
      // extract feature
      for (auto &obj : detect_result) {
        obj.feature = feature_extractor->ExtractFeature(track_img, obj);
      }
      track_result.clear();
      tracker->UpdateFrame(track_img, detect_result, &track_result);

      osd.DrawLabel(img, track_result);
      osd.DrawChannels(img);
      osd.DrawFps(img, 20);

      if (g_frames.size() == 0 && g_receive_eos) {
        break;
      }
      if (FLAGS_show) {
        auto window_name = "use yuv2rgb";
        if (in_shape.c != 1) window_name = "use mlu resize_and convert";
        cv::imshow(window_name, img);
        cv::waitKey(5);
        // std::string fn = std::to_string(frame.frame_id) + ".jpg";
        // cv::imwrite(fn.c_str(), img);
      }
    }
  } catch (edk::Exception &err) {
    LOG(ERROR) << err.what();
    return false;
  }

  // uninitialize
  g_running = false;
  if (nullptr != img_data) delete[] img_data;
  if (nullptr != mlu_output) mem_op.FreeArrayMlu(mlu_output, loader->OutputNum());
  if (nullptr != cpu_output) mem_op.FreeCpuOutput(cpu_output);
  if (nullptr != mlu_input) mem_op.FreeArrayMlu(mlu_input, loader->InputNum());
  if (nullptr != decode) delete decode;
  if (nullptr != postproc) delete postproc;
  if (nullptr != tracker) delete tracker;
  return true;
}

void handle_sig(int sig) {
  g_running = false;
#ifdef CNSTK_MLU270
  g_exit = true;
#endif
  LOG(INFO) << "Got INT signal, ready to exit!";
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // check params
  CHECK_NE(FLAGS_data_path.size(), 0);
  CHECK_NE(FLAGS_model_path.size(), 0);
  CHECK_NE(FLAGS_func_name.size(), 0);
  CHECK_NE(FLAGS_label_path.size(), 0);
  CHECK_LE(FLAGS_wait_time, 0);

  g_url = FLAGS_data_path.c_str();

  edk::CnPacket pending_frame;

  std::unique_lock<std::mutex> lk(g_mut);
  std::future<bool> loop_return = std::async(std::launch::async, &run);
  // wait for init done
  g_cond.wait(lk);
  lk.unlock();

  if (0 < FLAGS_wait_time) {
    alarm(FLAGS_wait_time);
  }
  signal(SIGALRM, handle_sig);
  signal(SIGINT, handle_sig);

  // set mlu environment
  edk::MluContext context;
  context.SetDeviceId(0);
  context.SetChannelId(0);
  context.ConfigureForThisThread();

  auto now_time = std::chrono::steady_clock::now();
  auto last_time = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dura;
  try {
    while (g_running) {
      // sync decode
      if (!unpack_data(&pending_frame)) {
        send_eos(g_decode);
        break;
      }

      if (g_decode == nullptr) break;
      if (!g_decode->SendData(pending_frame)) break;
      if (g_p_bsfc) {
        av_free(reinterpret_cast<void *>(pending_frame.data));
      }
      av_packet_unref(&g_packet);
      now_time = std::chrono::steady_clock::now();
      dura = now_time - last_time;
      if (40 > dura.count()) {
        std::chrono::duration<double, std::milli> sleep_t(40 - dura.count());
        std::this_thread::sleep_for(sleep_t);
      }
      last_time = std::chrono::steady_clock::now();
    }
  } catch (edk::Exception &err) {
    LOG(ERROR) << err.what();
  }

  if (g_exit) {
    send_eos(g_decode);
  }

  if (g_p_format_ctx) {
    avformat_close_input(&g_p_format_ctx);
    avformat_free_context(g_p_format_ctx);
    av_dict_free(&g_options);
    g_p_format_ctx = nullptr;
    g_options = nullptr;
  }
  if (g_p_bsfc) {
    av_bitstream_filter_close(g_p_bsfc);
    g_p_bsfc = nullptr;
  }

  g_running = false;

  bool ret = loop_return.get();
  if (ret) {
    std::cout << "run stream app SUCCEED!!!" << std::endl;
  }

  return ret;
}
