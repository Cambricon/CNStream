#ifndef _SDL_VIDEO_PLAYER_HPP_
#define _SDL_VIDEO_PLAYER_HPP_

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif

#ifdef HAVE_SDL
#include <SDL2/SDL.h>
#endif

#include "util/cnstream_time_utility.hpp"

namespace cnstream {

struct UpdateData {
  cv::Mat img;
  int chn_idx = -1;
};  // struct UpdateData

#ifdef HAVE_SDL
class SDLVideoPlayer {  // BGR
 public:
  SDLVideoPlayer();
  ~SDLVideoPlayer();
  bool Init(int max_chn);
  void Destroy();
  void Refresh();
  void SetFullScreen();

  bool FeedData(const UpdateData &data);
  std::string CalcFps(const UpdateData &data);

  void EventLoop(const std::function<void()> &quit_callback);

  void ClickEventProcess(const int &mouse_x, const int &mouse_y);
  inline void set_frame_rate(int frame_rate) { frame_rate_ = frame_rate; }
  inline int frame_rate() const { return frame_rate_; }
  inline void set_window_w(int w) { window_w_ = w; }
  inline int window_w() const { return window_w_; }
  inline void set_window_h(int h) { window_h_ = h; }
  inline int window_h() const { return window_h_; }
  inline bool running() const { return running_; }
  inline SDL_Window *window() const { return window_; }
  inline SDL_Renderer *renderer() const { return renderer_; }
  inline SDL_Texture *texture() const { return texture_; }

 private:
  void Stop();
  std::vector<UpdateData> PopDataBatch();
  int GetRowIdByChnId(int chn_id) { return chn_id / cols_; }
  int GetColIdByChnId(int chn_id) { return chn_id % cols_; }
  int GetXByChnId(int chn_id) { return chn_w_ * GetColIdByChnId(chn_id); }
  int GetYByChnId(int chn_id) { return chn_h_ * GetRowIdByChnId(chn_id); }
  int frame_rate_ = 10;
  int window_w_ = 1920;
  int window_h_ = 1080;
  int cols_, rows_;
  int max_chn_ = 32;
  int chn_w_, chn_h_;
  bool running_ = false;
  int click_chn_ = -1;
  std::vector<int> flags_;
  std::vector<TickClock> ticker_;
  std::vector<int> fps_;
  std::vector<std::pair<std::queue<UpdateData>, std::shared_ptr<std::mutex>>> data_queues_;
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;
  SDL_Thread *refresh_th_ = nullptr;
};  // class SDLVideoPlayer

#else
class SDLVideoPlayer {  // BGR
 public:
  inline bool Init(int max_chn) { return true; }
  inline void Destroy() {}
  inline void SetFullScreen() {}
  inline void set_window_w(int w) {}
  inline void set_window_h(int h) {}
  inline void set_frame_rate(int frame_rate) {}
  inline bool FeedData(const UpdateData &data) { return true; }
  void EventLoop(const std::function<void()> &quit_callback) {}
  std::string CalcFps(const UpdateData &data) { return ""; }

 private:
  std::vector<int> flags_;
  std::vector<TickClock> ticker_;
  std::vector<int> fps_;
};  // class SDLVideoPlayer
#endif  // HAVE_SDL

}  // namespace cnstream

#endif  // _SDL_VIDEO_PLAYER_HPP_
