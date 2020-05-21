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

#ifndef MODULES_RTSP_SINK_INCLUDE_RTSP_STREAM_PIPE_HPP_
#define MODULES_RTSP_SINK_INCLUDE_RTSP_STREAM_PIPE_HPP_

#include <stdint.h>
#include <string>

namespace cnstream {

class RtspParam;
class StreamPipeCtx;
typedef void* RtspStreamHandle;

#ifdef __cplusplus
extern "C" {
#endif

StreamPipeCtx *StreamPipeCreate(const RtspParam& rtsp_param);
int StreamPipePutPacket(StreamPipeCtx* ctx, uint8_t* data, int64_t timestamp = 0);
int StreamPipePutPacketMlu(StreamPipeCtx* ctx, void *y, void *uv, int64_t timestamp = 0);
int StreamPipeClose(StreamPipeCtx* ctx);

#ifdef __cplusplus
}
#endif

}  // namespace cnstream

#endif  // MODULES_RTSP_SINK_INCLUDE_RTSP_STREAM_PIPE_HPP_
