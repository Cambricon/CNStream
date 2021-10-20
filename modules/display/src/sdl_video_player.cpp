#include "sdl_video_player.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "cnstream_logging.hpp"

#define REFRESH_EVENT (SDL_USEREVENT + 1)
namespace cnstream {

class SdlInitTool {
 public:
  static SdlInitTool* instance() {
    static SdlInitTool instance;
    return &instance;
  }

  bool init() {
    mutex_.lock();
    if (!initialized_) {
      if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LOGE(DISPLAYER) << "Unable to initialize SDL:" << SDL_GetError();
        mutex_.unlock();
        return false;
      }
      initialized_ = true;
      atexit(SDL_Quit);
    }
    mutex_.unlock();
    return true;
  }

 private:
  SdlInitTool() : initialized_(false) {}
  ~SdlInitTool() {}
  bool initialized_;
  std::mutex mutex_;
};  // class SdlInitTool

int RefreshLoop(void* _data) {
  auto player = reinterpret_cast<SDLVideoPlayer*>(_data);
  while (player->running()) {
    SDL_Event event;
    event.type = REFRESH_EVENT;
    SDL_PushEvent(&event);
    int delay = 1000.0 / player->frame_rate();
    SDL_Delay(delay);
  }  // while (running())
  return EXIT_SUCCESS;
}

SDLVideoPlayer::SDLVideoPlayer() {}

SDLVideoPlayer::~SDLVideoPlayer() {}

bool SDLVideoPlayer::Init(int max_chn) {
  LOGI(DISPLAYER) << "before init";
  if (!SdlInitTool::instance()->init()) {
    return false;
  }
  LOGI(DISPLAYER) << "before create window";
  window_ = SDL_CreateWindow("CNStream", 0, 0, window_w_, window_h_, 0);
  if (nullptr == window_) {
    LOGE(DISPLAYER) << "Create SDL window failed." << SDL_GetError();
    return false;
  }
  renderer_ = SDL_CreateRenderer(window_, -1, 0);
  if (nullptr == renderer_) {
    LOGE(DISPLAYER) << "Create SDL renderer failed." << SDL_GetError();
    return false;
  }
  LOGI(DISPLAYER) << "before create texture";
  int pixelf = SDL_PIXELFORMAT_BGR24;
  texture_ = SDL_CreateTexture(renderer_, pixelf, SDL_TEXTUREACCESS_STREAMING, window_w_, window_h_);
  if (nullptr == texture_) {
    LOGE(DISPLAYER) << "Create SDL texture failed." << SDL_GetError();
    return false;
  }
  max_chn_ = max_chn;
  int square = std::ceil(std::sqrt(max_chn));
  rows_ = cols_ = square;
  while (rows_ * cols_ >= max_chn) {
    rows_--;
  }
  rows_++;
  chn_w_ = window_w_ / cols_;
  chn_h_ = window_h_ / rows_;
  data_queues_.resize(max_chn_);
  flags_.resize(max_chn_, 2);
  ticker_.resize(max_chn_);
  fps_.resize(max_chn_, 0);
  for (int i = 0; i < max_chn_; ++i) {
    data_queues_[i].second = std::make_shared<std::mutex>();
  }
  return true;
}

void SDLVideoPlayer::Destroy() {
  Stop();
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  data_queues_.clear();
}

void SDLVideoPlayer::ClickEventProcess(const int& mouse_x, const int& mouse_y) {
  if (mouse_x > window_w_ || mouse_y > window_h_) {
    return;
  }
  click_chn_ = (mouse_y / chn_h_) * cols_ + mouse_x / chn_w_;
}

void SDLVideoPlayer::EventLoop(const std::function<void()>& quit_callback) {
  running_ = true;
  refresh_th_ = SDL_CreateThread(RefreshLoop, NULL, this);
  SDL_Event event;
  int mouse_x;
  int mouse_y;
  while (running()) {
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_MOUSEBUTTONDOWN:
        LOGI(DISPLAYER) << "Get SDL_MOUSEBUTTONDOWN EVENT";
        mouse_x = event.button.x;
        mouse_y = event.button.y;
        ClickEventProcess(mouse_x, mouse_y);
        break;
      case REFRESH_EVENT:
        Refresh();
        break;
      case SDL_WINDOWEVENT:
        if (SDL_WINDOWEVENT_CLOSE == event.window.event) {
          LOGI(DISPLAYER) << "Get SDL Close Window EVENT";
          if (quit_callback) quit_callback();
        }
        break;
      case SDL_QUIT:
        LOGI(DISPLAYER) << "Get SDL QUIT EVENT";
        if (quit_callback) quit_callback();
        break;
    }  // switch (event.type)
  }    // while(running())
}

void SDLVideoPlayer::Stop() {
  running_ = false;
  // wait thread
  int value;
  SDL_WaitThread(refresh_th_, &value);
}

void SDLVideoPlayer::Refresh() {
  auto datas = PopDataBatch();
  uint8_t* texture_data = nullptr;
  int pitch = 0;
  SDL_LockTexture(texture_, NULL, reinterpret_cast<void**>(&texture_data), &pitch);
  for (auto& it : datas) {
    std::string fps_info = CalcFps(it);
    cv::Point font_point(0.6 * chn_w_, 0.1 * chn_h_);
    cv::putText(it.img, fps_info, font_point, CV_FONT_HERSHEY_SIMPLEX, 0.5, cvScalar(255, 0, 0));

    auto len = it.img.cols * 3;
    int x = GetXByChnId(it.chn_idx);
    int y = GetYByChnId(it.chn_idx);
    for (int c = 0; c < it.img.rows; ++c) {
      auto dst_ptr = texture_data + pitch * (y + c) + 3 * x;
      memcpy(dst_ptr, it.img.data + c * len, len);
    }
  }
  SDL_UnlockTexture(texture_);
  SDL_RenderCopy(renderer_, texture_, NULL, NULL);
  SDL_RenderPresent(renderer_);
}

void SDLVideoPlayer::SetFullScreen() { SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN); }

bool SDLVideoPlayer::FeedData(const UpdateData& data) {
  if (data.chn_idx < 0 || data.chn_idx >= max_chn_) return false;

  std::shared_ptr<std::mutex> pmtx = data_queues_[data.chn_idx].second;
  std::queue<UpdateData>& q = data_queues_[data.chn_idx].first;
  cv::Size show_size(chn_w_, chn_h_);
  auto t = data;
  cv::resize(t.img, t.img, show_size);
  std::lock_guard<std::mutex> lk(*pmtx);
  if (q.size() > 10) q.pop();
  q.push(t);
  return true;
}

std::vector<UpdateData> SDLVideoPlayer::PopDataBatch() {
  std::vector<UpdateData> ret;
  for (auto& q : data_queues_) {
    auto pmtx = q.second;
    std::lock_guard<std::mutex> lk(*pmtx);
    if (!q.first.empty()) {
      ret.push_back(q.first.front());
      q.first.pop();
    }
  }
  return ret;
}

std::string SDLVideoPlayer::CalcFps(const UpdateData& data) {
  ticker_[data.chn_idx].Tick();
  --flags_[data.chn_idx];
  if (flags_[data.chn_idx] == 0) {
    int elapsed = ticker_[data.chn_idx].ElapsedAverage().count();
    fps_[data.chn_idx] = 1000000.f / elapsed;
    ticker_[data.chn_idx].Clear();
    return "fps : " + std::to_string(fps_[data.chn_idx]);
  }
  if (flags_[data.chn_idx] < 0) {
    flags_[data.chn_idx] = 0;
    if (ticker_[data.chn_idx].ElapsedTotal() > milliseconds(300)) {
      auto elapsed = ticker_[data.chn_idx].ElapsedAverage().count();
      fps_[data.chn_idx] = 1000000.f / elapsed;
      ticker_[data.chn_idx].Clear();
    }
    return "fps : " + std::to_string(fps_[data.chn_idx]);
  }
  return "";
}

}  // namespace cnstream
