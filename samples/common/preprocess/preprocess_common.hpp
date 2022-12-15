/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
#include <string>
#include <vector>

#include "cnedk_transform.h"
#include "cnstream_preproc.hpp"

int PreprocessTransform(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                        const std::vector<CnedkTransformRect> &src_rects,
                        const cnstream::CnPreprocNetworkInfo &info, infer_server::NetworkInputFormat pix_fmt,
                        bool keep_aspect_ratio = true, int pad_value = 0, bool mean_std = false,
                        std::vector<float> mean = {}, std::vector<float> std = {});

int PreprocessCpu(cnedk::BufSurfWrapperPtr src, cnedk::BufSurfWrapperPtr dst,
                  const std::vector<CnedkTransformRect> &src_rects,
                  const cnstream::CnPreprocNetworkInfo &info, infer_server::NetworkInputFormat pix_fmt,
                  bool keep_aspect_ratio = true, int pad_value = 0, bool mean_std = false,
                  std::vector<float> mean = {}, std::vector<float> std = {});

void SaveResult(const std::string &filename, int count, uint32_t batch_size, cnedk::BufSurfWrapperPtr dst,
                const cnstream::CnPreprocNetworkInfo &info);
