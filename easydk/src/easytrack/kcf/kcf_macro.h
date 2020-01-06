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

#ifndef __KCF_MACRO_H__
#define __KCF_MACRO_H__

#define PRI_STEP 4
#define ROWS 1080
#define COLS 1920
#define OUT(a, r, c)                                                                                        \
  for (int i = 0; i < 10; i++) {                                                                            \
    for (int j = 0; j < 10; j++) printf("<%f,%f>, ", at(a, i, j, r, c), at(a, r - i - 1, c - j - 1, r, c)); \
    printf(";\n");                                                                                          \
  }
#define MAX_ROI_NUM 10
#define MAX_ROI_ALIGN 16
#define align(x) (((x - 1) / 16 + 1) * 16)

#define ROW 1080
#define COL 1920
#define CHANNEL 1
#define PAD_ROW 741
#define PAD_COL 2095
#define TMP_SZ 96
#define _KI (2 * 3.1415926 / TMP_SZ)
#define BLOCK(a) (TMP_SZ * TMP_SZ * a)

#define TMP_SZ_64 128

#define KCF_LAMBDA 0.0001
#define KCF_PADDING 2.5
#define KCF_OUTPUT_SIGMA_FACTOR 0.125

#define KCF_INTERP_FACTOR 0.075
// used in ExpSigmaKernel function
#define KCF_SIGMA 0.2
#define KCF_NE_1_SIGMA_2 -25.0

#define KCF_CELL_SIZE 1

#define KCF_TEMPLATE_SIZE 96
#define KCF_SCALE_STEP 1

#define FRAMES 4

#define ROI_LENGTH 224
#define SX 371
#define SY 410
#define SW 80
#define SH 239
#define GT \
  { 1338, 418, 167, 379 }

// for subwindow_func.h
#define ALIGN_DN_TO_64(x) ((int)(x)) / 64 * 64
#define ALIGN_UP_TO_64(x) ((int)(x) + 63) / 64 * 64

#define ALIGNED_TMP_SZ 128
#define aligned_BLOCK_1 TMP_SZ* ALIGNED_TMP_SZ
#define PADDED_WIDTH_ONE_SIDE (1440 + 32)

#endif  // __KCF_MACRO_H__
