/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#ifndef SAMPLES_DEMO_PREPROCESS_VIDEO_PREPROCESS_COMMON_HPP_
#define SAMPLES_DEMO_PREPROCESS_VIDEO_PREPROCESS_COMMON_HPP_

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "cnis/contrib/video_helper.h"
#include "cnstream_frame_va.hpp"

using VideoPixelFmt = infer_server::video::PixelFmt;

bool ConvertColorSpace(size_t width, size_t height, size_t stride, VideoPixelFmt src_fmt, VideoPixelFmt dst_fmt,
                       uint8_t* src_img_data, cv::Mat* dst_img);

#endif  // ifndef SAMPLES_DEMO_PREPROCESS_VIDEO_PREPROCESS_COMMON_HPP_
