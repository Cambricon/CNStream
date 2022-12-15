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

#ifndef MODULES_TEST_INCLUDE_TEST_BASE_HPP_
#define MODULES_TEST_INCLUDE_TEST_BASE_HPP_

#include <memory>
#include <string>

#include "opencv2/opencv.hpp"

#include "cnedk_buf_surface.h"

#include "cnstream_frame_va.hpp"

#define PATH_MAX_LENGTH 1024

std::string GetExePath();
void CheckExePath(const std::string& path);
bool CvtBgrToYuv420sp(const cv::Mat &bgr_image, uint32_t alignment, CnedkBufSurface *surf);
std::shared_ptr<cnstream::CNDataFrame> GenerateCNDataFrame(cv::Mat img, int device_id);

std::string GetModelInfoStr(std::string model_name, std::string info_type);
std::string GetLabelInfoStr(std::string label_name, std::string info_type);

bool IsEdgePlatform(int device_id);

bool IsCloudPlatform(int device_id);

int InitPlatform(bool enable_vin, bool enable_vout);
#endif  // MODULES_TEST_INCLUDE_TEST_BASE_HPP_
