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

#ifndef __VIDEO_COMMON__
#define __VIDEO_COMMON__

#include <cstdint>

namespace cnstream {

#define NUM_DATA_POINTERS 6
#define INVALID_TIMESTAMP ((int64_t)UINT64_C(0x8000000000000000))

enum VideoCodecType {
  AUTO = -1,
  H264 = 0,
  H265,
  MPEG4,
  JPEG,
  RAW,
};

enum VideoPixelFormat {
  I420 = 0,
  NV12,
  NV21,
  I422,
  I444,
  BGR,
  RGB,
};

struct VideoPacket {
  /// width and height of the packet
  uint32_t width, height;
  /// data pointer to the packet
  uint8_t *data;
  /// size of the packet
  uint32_t size;
  /// presentation and decoding timestamp
  int64_t pts, dts;
  /// flags for the packet
  uint32_t flags;
  /// user data
  void *user_data;

  enum Flags {
    /// end of stream
    EOS = (1 << 0),
    /// key frame
    KEY = (1 << 1),
    /// VPS/SPS/PPS
    PS = (1 << 2),
  };

/// raw format
#define RAW_FORMAT_BITS 4
#define RAW_FORMAT_SHIFT 16
#define RAW_FORMAT_MASK (((1 << RAW_FORMAT_BITS) - 1) << RAW_FORMAT_SHIFT)

  bool HasEOS() const { return (flags & EOS) == static_cast<uint32_t>(EOS); }
  void SetEOS() { flags |= EOS; }
  bool IsKey() const { return (flags & KEY) == static_cast<uint32_t>(KEY); }
  void SetKey() { flags |= KEY; }
  bool IsPS() const { return (flags & PS) == static_cast<uint32_t>(PS); }
  void SetPS() { flags |= PS; }

  VideoPixelFormat GetFormat() const {
    return static_cast<VideoPixelFormat>((flags & RAW_FORMAT_MASK) >> RAW_FORMAT_SHIFT);
  }
  void SetFormat(VideoPixelFormat f) { flags |= (f << RAW_FORMAT_SHIFT); }
};

struct VideoFrame {
  /// width and height of the frame
  uint32_t width, height;
  /// data pointer of the frame planes
  uint8_t *data[NUM_DATA_POINTERS];
  /// stride of the frame planes
  uint32_t stride[NUM_DATA_POINTERS];
  /// presentation and decoding timestamp
  int64_t pts, dts;
  /// pixel format of the frame
  VideoPixelFormat pixel_format;
  /// flags for the frame
  uint32_t flags;
  /// user data
  void *user_data;

  enum Flags {
    /// end of stream
    EOS = (1 << 0),
    /// memory in MLU
    MLU_MEMORY = (1 << 31),
  };

/// buffer index
#define BUFFER_INDEX_BITS 7
#define BUFFER_INDEX_SHIFT 16
#define BUFFER_INDEX_MASK (((1 << BUFFER_INDEX_BITS) - 1) << BUFFER_INDEX_SHIFT)
/// MLU memory channel
#define MLU_MEMORY_CHANNEL_BITS 4
#define MLU_MEMORY_CHANNEL_SHIFT (BUFFER_INDEX_SHIFT + BUFFER_INDEX_BITS)
#define MLU_MEMORY_CHANNEL_MASK (((1 << MLU_MEMORY_CHANNEL_BITS) - 1) << MLU_MEMORY_CHANNEL_BITS)
/// MLU device id
#define MLU_DEVICE_ID_BITS 4
#define MLU_DEVICE_ID_SHIFT (MLU_MEMORY_CHANNEL_SHIFT + MLU_MEMORY_CHANNEL_BITS)
#define MLU_DEVICE_ID_MASK (((1 << MLU_DEVICE_ID_BITS) - 1) << MLU_DEVICE_ID_SHIFT)

  bool HasEOS() const { return (flags & EOS) == static_cast<uint32_t>(EOS); }
  void SetEOS() { flags |= EOS; }
  int GetBufferIndex() const { return ((flags & BUFFER_INDEX_MASK) >> BUFFER_INDEX_SHIFT); }
  void SetBufferIndex(int index) { flags |= (index << BUFFER_INDEX_SHIFT); }
  bool IsMluMemory() const { return (flags & MLU_MEMORY) == static_cast<uint32_t>(MLU_MEMORY); }
  int GetMluMemoryChannel() const { return ((flags & MLU_MEMORY_CHANNEL_MASK) >> MLU_MEMORY_CHANNEL_SHIFT); }
  void SetMluMemoryChannel(int channel) { flags |= ((channel << MLU_MEMORY_CHANNEL_SHIFT) | MLU_MEMORY); }
  int GetMluDeviceId() const { return ((flags & MLU_DEVICE_ID_MASK) >> MLU_DEVICE_ID_SHIFT); }
  void SetMluDeviceId(int device_id) { flags |= ((device_id << MLU_DEVICE_ID_SHIFT) | MLU_MEMORY); }
};

}  // namespace cnstream

#endif  // __VIDEO_COMMON__
