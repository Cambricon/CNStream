/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef CNSTREAM_VIDEO_PARSER_HPP_
#define CNSTREAM_VIDEO_PARSER_HPP_

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef HAVE_FFMPEG_AVDEVICE
#include <libavdevice/avdevice.h>
#endif
#ifdef __cplusplus
}
#endif

#include <memory>
#include <string>
#include <vector>

#include "data_source.hpp"

namespace cnstream {

struct VideoInfo {
  AVCodecID codec_id;
#ifdef HAVE_FFMPEG_AVDEVICE  // for usb camera
  int format;
  int width;;
  int height;
#endif
  int progressive;
  MaximumVideoResolution maximum_resolution;
  std::vector<unsigned char> extra_data;
};

/* data == null && len == 0  indicates EOS
*/
struct VideoEsFrame {
  uint8_t *data = nullptr;
  size_t len = 0;
  int64_t pts = 0;
  uint32_t flags = 0;
  enum {FLAG_KEY_FRAME = 0x01};
  bool IsEos();
};

inline
bool VideoEsFrame::IsEos() {
  return data == nullptr && len == 0;
}

class IParserResult {
 public:
  virtual ~IParserResult() = default;
  virtual void OnParserInfo(VideoInfo *info) = 0;
  virtual void OnParserFrame(VideoEsFrame *frame) = 0;
};

struct VideoEsPacket {
  uint8_t *data = nullptr;
  size_t len = 0;
  int64_t pts = -1;
};

// FFmpeg demuxer and parser
class FFParserImpl;
class FFParser {
 public:
  explicit FFParser(const std::string& stream_id);
  ~FFParser();
  int Open(const std::string& url, IParserResult* result, bool only_key_frame = false);
  void Close();
  int Parse();
  std::string GetStreamID();

 private:
  FFParser(const FFParser& ) = delete;
  FFParser(FFParser&& ) = delete;
  FFParser& operator=(const FFParser& ) = delete;
  FFParser& operator=(FFParser&& ) = delete;
  FFParserImpl *impl_ = nullptr;
};

// H264 & H265 ES Parser
class EsParserImpl;
class EsParser {
 public:
  EsParser();
  ~EsParser();
  int Open(AVCodecID codec_id, IParserResult* result, uint8_t* paramset = nullptr, uint32_t paramset_size = 0,
           bool only_key_frame = false);
  void Close();
  int Parse(const VideoEsPacket &pkt);
  int ParseEos();

 private:
  EsParser(const EsParser& ) = delete;
  EsParser(EsParser&& ) = delete;
  EsParser& operator=(const EsParser& ) = delete;
  EsParser& operator=(EsParser&& ) = delete;
  EsParserImpl *impl_ = nullptr;
};  // class StreamParser

}  // namespace cnstream

#endif  // CNSTREAM_VIDEO_PARSER_HPP_
