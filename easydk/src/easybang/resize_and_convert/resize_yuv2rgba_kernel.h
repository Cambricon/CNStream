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

#include "half.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void ResizeYuvToRgbaKernel(
    half* dst_gdram,
    half** src_y_gdram,
    half** src_uv_gdram,
    int** src_wh_gdram,
    int** roi_rect_gdram,
    half* fill_color_gdram,
    half* yuv_filter_gdram,
    half* yuv_bias_gdram,
    int* mult_gdram,
    half** mask_gdram,
    half** weight_gdram,
    int8_t** copy_filter_gdram,
    int dst_row,
    int dst_col,
    int input2half,
    int output2uint,
    int batch_num,
    int keep_aspect_ratio,
    int pad_method);

#ifdef __cplusplus
}
#endif
