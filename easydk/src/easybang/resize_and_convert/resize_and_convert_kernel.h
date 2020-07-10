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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t half;

void ResizeYuvToRgbaKernel_V2_MLU270(
     half* dst_gdram,
     half** srcY_gdram,
     half** srcUV_gdram,
     int** srcWH_gdram,
     int** roiRect_gdram,
     half* fill_color_gdram,
     half* yuvFilter_gdram,
     half* yuvBias_gdram,
     int d_row_final, int d_col_final,
     int input2half, int output2uint,
     int batchNum,
     int keepAspectRatio,
     int padMethod);

void ResizeYuvToRgbaKernel_V2_MLU220(
     half* dst_gdram,
     half** srcY_gdram,
     half** srcUV_gdram,
     int** srcWH_gdram,
     int** roiRect_gdram,
     half* fill_color_gdram,
     half* yuvFilter_gdram,
     half* yuvBias_gdram,
     int d_row_final, int d_col_final,
     int input2half, int output2uint,
     int batchNum,
     int keepAspectRatio,
     int padMethod);

#ifdef __cplusplus
}
#endif
