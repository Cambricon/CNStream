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
#ifndef LIBSTREAM_INCLUDE_CNVFORMAT_H_
#define LIBSTREAM_INCLUDE_CNVFORMAT_H_

#define CN_MAXIMUM_PLANE 6


namespace libstream {

enum CnPixelFormat {
  NON_FORMAT,
  YUV420SP_NV21,
  YUV420SP_NV12,
  BGR24,
  RGB24
};

enum CnCodecType {
  MPEG4,
  H264,
  H265,
  JPEG,
  MJPEG
};

typedef struct {
  /**************************************
   * release it by CnDecode::ReleaseBuffer
   * when memory from decoder is useless.
   * useless in encoder.
   **************************************/
  uint32_t buf_id;
  uint64_t pts;
  uint32_t height;
  uint32_t width;
  uint64_t frame_size;
  CnPixelFormat pformat;
  /**************************************
   * plane count for this frame.
   * useless on mlu100.
   **************************************/
  uint32_t planes;
  uint32_t strides[CN_MAXIMUM_PLANE];
  struct {
    uint64_t ptrs[CN_MAXIMUM_PLANE];
    /***************************************
     * ptr_len always be 1 on mlu100.
     * or ptr_len is equal to CnFrame::planes
     ***************************************/
    uint32_t ptr_len;
  } data;
}CnFrame;

typedef struct {
  /**************************************
   * release it by CnEncode::ReleaseBuffer
   * when memory from encoder is useless.
   * useless in decoder.
   **************************************/
  uint32_t buf_id;
  void* data;
  uint64_t length;
  uint64_t pts;
  CnCodecType codec_type;
}CnPacket;

}  // namespace libstream

#endif  // LIBSTREAM_INCLUDE_CNVFORMAT_H_
