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

#ifdef ENABLE_KCF

#include <dirent.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "cnrt.h"
#include "kcf.h"
#include "kcf_macro.h"

#include "dft_mat_c20_zipped.hpp"

#define STORE_USEC_TIME_OF_DAY_TO_INT64(i)  \
  do {                                      \
    struct timeval tv;                      \
    gettimeofday(&tv, NULL);                \
    (i) = 1000000 * tv.tv_sec + tv.tv_usec; \
  } while (0);

/* zipped data format:
   0    1    2    3    4    5    6    7 ...
   total_len| offset  |block_len|non-zero data ...
*/

/*
static void compress_dft_mat(half *in, uint32_t len, half *out, uint32_t *output_len) {
  uint32_t offset = 0, block_len = 0, block_offset = 0, out_len = 0;
  half *out_ptr = out;

  out_ptr[0] = len & 0xffff;
  out_ptr[1] = (len >> 16) & 0xffff;
  out_len += 2;
  out_ptr += 2;

  for (uint32_t i = 0; i < len; i++) {
    if (in[i] == 0) {
      if (block_len > 0) {
        block_offset = offset - block_len;
        out_ptr[0] = block_offset & 0xffff;
        out_ptr[1] = (block_offset >> 16) & 0xffff;
        out_ptr[2] = block_len & 0xffff;
        out_ptr[3] = (block_len >> 16) & 0xffff;
        memcpy(out_ptr + 4, in + offset - block_len, block_len * sizeof(half));
        out_len += 4 + block_len;
        out_ptr += 4 + block_len;
      }
      block_len = 0;
    } else {
      block_len++;
    }
    offset++;

    // last non-zero data
    if (i == (len - 1) && block_len > 0) {
      block_offset = offset - block_len;
      out_ptr[0] = block_offset & 0xffff;
      out_ptr[1] = (block_offset >> 16) & 0xffff;
      out_ptr[2] = block_len & 0xffff;
      out_ptr[3] = (block_len >> 16) & 0xffff;
      memcpy(out_ptr + 4, in + offset - block_len, block_len * sizeof(half));
      out_len += 4 + block_len;
      out_ptr += 4 + block_len;
    }
  }

  *output_len = out_len;
}
*/

static void decompress_dft_mat(half* in, uint32_t len, half* out, uint32_t* output_len) {
  uint32_t offset = 0, block_len = 0, zero_len = 0, out_len = 0;
  half *in_ptr = in, *out_ptr = out;

  *output_len = in[0] + (in[1] << 16);
  in_ptr += 2;

  for (uint32_t i = 0; i < len - 2; i += 4 + block_len) {
    offset = in_ptr[0] + (in_ptr[1] << 16);
    block_len = in_ptr[2] + (in_ptr[3] << 16);
    zero_len = out + offset - out_ptr;
    memset(out_ptr, 0, zero_len * sizeof(half));
    out_ptr += zero_len;
    out_len += zero_len;
    memcpy(out_ptr, in_ptr + 4, block_len * sizeof(half));
    in_ptr += 4 + block_len;
    out_ptr += block_len;
    out_len += block_len;
  }

  if (out_len < *output_len) {
    memset(out_ptr, 0, (*output_len - out_len) * sizeof(half));
  }
}

void kcf_init(KCFHandle* handle, cnrtQueue_t queue, float threshold) {
  float _cos_table[1024] = {
      1.000000,  0.999981,  0.999925,  0.999831,  0.999699,  0.999529,  0.999322,  0.999078,  0.998795,  0.998476,
      0.998118,  0.997723,  0.997290,  0.996820,  0.996313,  0.995767,  0.995185,  0.994565,  0.993907,  0.993212,
      0.992480,  0.991710,  0.990903,  0.990058,  0.989177,  0.988258,  0.987301,  0.986308,  0.985278,  0.984210,
      0.983105,  0.981964,  0.980785,  0.979570,  0.978317,  0.977028,  0.975702,  0.974339,  0.972940,  0.971504,
      0.970031,  0.968522,  0.966976,  0.965394,  0.963776,  0.962121,  0.960431,  0.958703,  0.956940,  0.955141,
      0.953306,  0.951435,  0.949528,  0.947586,  0.945607,  0.943593,  0.941544,  0.939459,  0.937339,  0.935184,
      0.932993,  0.930767,  0.928506,  0.926210,  0.923880,  0.921514,  0.919114,  0.916679,  0.914210,  0.911706,
      0.909168,  0.906596,  0.903989,  0.901349,  0.898674,  0.895966,  0.893224,  0.890449,  0.887640,  0.884797,
      0.881921,  0.879012,  0.876070,  0.873095,  0.870087,  0.867046,  0.863973,  0.860867,  0.857729,  0.854558,
      0.851355,  0.848120,  0.844854,  0.841555,  0.838225,  0.834863,  0.831470,  0.828045,  0.824589,  0.821103,
      0.817585,  0.814036,  0.810457,  0.806848,  0.803208,  0.799537,  0.795837,  0.792107,  0.788346,  0.784557,
      0.780737,  0.776888,  0.773010,  0.769103,  0.765167,  0.761202,  0.757209,  0.753187,  0.749136,  0.745058,
      0.740951,  0.736817,  0.732654,  0.728464,  0.724247,  0.720003,  0.715731,  0.711432,  0.707107,  0.702755,
      0.698376,  0.693971,  0.689541,  0.685084,  0.680601,  0.676093,  0.671559,  0.667000,  0.662416,  0.657807,
      0.653173,  0.648514,  0.643832,  0.639124,  0.634393,  0.629638,  0.624859,  0.620057,  0.615232,  0.610383,
      0.605511,  0.600616,  0.595699,  0.590760,  0.585798,  0.580814,  0.575808,  0.570781,  0.565732,  0.560662,
      0.555570,  0.550458,  0.545325,  0.540171,  0.534998,  0.529804,  0.524590,  0.519356,  0.514103,  0.508830,
      0.503538,  0.498228,  0.492898,  0.487550,  0.482184,  0.476799,  0.471397,  0.465976,  0.460539,  0.455084,
      0.449611,  0.444122,  0.438616,  0.433094,  0.427555,  0.422000,  0.416430,  0.410843,  0.405241,  0.399624,
      0.393992,  0.388345,  0.382683,  0.377007,  0.371317,  0.365613,  0.359895,  0.354164,  0.348419,  0.342661,
      0.336890,  0.331106,  0.325310,  0.319502,  0.313682,  0.307850,  0.302006,  0.296151,  0.290285,  0.284408,
      0.278520,  0.272621,  0.266713,  0.260794,  0.254866,  0.248928,  0.242980,  0.237024,  0.231058,  0.225084,
      0.219101,  0.213110,  0.207111,  0.201105,  0.195090,  0.189069,  0.183040,  0.177004,  0.170962,  0.164913,
      0.158858,  0.152797,  0.146730,  0.140658,  0.134581,  0.128498,  0.122411,  0.116319,  0.110222,  0.104122,
      0.098017,  0.091909,  0.085797,  0.079682,  0.073565,  0.067444,  0.061321,  0.055195,  0.049068,  0.042938,
      0.036807,  0.030675,  0.024541,  0.018407,  0.012272,  0.006136,  0.000000,  -0.006136, -0.012272, -0.018407,
      -0.024541, -0.030675, -0.036807, -0.042938, -0.049068, -0.055195, -0.061321, -0.067444, -0.073565, -0.079682,
      -0.085797, -0.091909, -0.098017, -0.104122, -0.110222, -0.116319, -0.122411, -0.128498, -0.134581, -0.140658,
      -0.146730, -0.152797, -0.158858, -0.164913, -0.170962, -0.177004, -0.183040, -0.189069, -0.195090, -0.201105,
      -0.207111, -0.213110, -0.219101, -0.225084, -0.231058, -0.237024, -0.242980, -0.248928, -0.254866, -0.260794,
      -0.266713, -0.272621, -0.278520, -0.284408, -0.290285, -0.296151, -0.302006, -0.307850, -0.313682, -0.319502,
      -0.325310, -0.331106, -0.336890, -0.342661, -0.348419, -0.354164, -0.359895, -0.365613, -0.371317, -0.377007,
      -0.382683, -0.388345, -0.393992, -0.399624, -0.405241, -0.410843, -0.416430, -0.422000, -0.427555, -0.433094,
      -0.438616, -0.444122, -0.449611, -0.455084, -0.460539, -0.465976, -0.471397, -0.476799, -0.482184, -0.487550,
      -0.492898, -0.498228, -0.503538, -0.508830, -0.514103, -0.519356, -0.524590, -0.529804, -0.534998, -0.540171,
      -0.545325, -0.550458, -0.555570, -0.560662, -0.565732, -0.570781, -0.575808, -0.580814, -0.585798, -0.590760,
      -0.595699, -0.600616, -0.605511, -0.610383, -0.615232, -0.620057, -0.624859, -0.629638, -0.634393, -0.639124,
      -0.643832, -0.648514, -0.653173, -0.657807, -0.662416, -0.667000, -0.671559, -0.676093, -0.680601, -0.685084,
      -0.689541, -0.693971, -0.698376, -0.702755, -0.707107, -0.711432, -0.715731, -0.720003, -0.724247, -0.728464,
      -0.732654, -0.736817, -0.740951, -0.745058, -0.749136, -0.753187, -0.757209, -0.761202, -0.765167, -0.769103,
      -0.773010, -0.776888, -0.780737, -0.784557, -0.788346, -0.792107, -0.795837, -0.799537, -0.803208, -0.806848,
      -0.810457, -0.814036, -0.817585, -0.821103, -0.824589, -0.828045, -0.831470, -0.834863, -0.838225, -0.841555,
      -0.844854, -0.848120, -0.851355, -0.854558, -0.857729, -0.860867, -0.863973, -0.867046, -0.870087, -0.873095,
      -0.876070, -0.879012, -0.881921, -0.884797, -0.887640, -0.890449, -0.893224, -0.895966, -0.898674, -0.901349,
      -0.903989, -0.906596, -0.909168, -0.911706, -0.914210, -0.916679, -0.919114, -0.921514, -0.923880, -0.926210,
      -0.928506, -0.930767, -0.932993, -0.935184, -0.937339, -0.939459, -0.941544, -0.943593, -0.945607, -0.947586,
      -0.949528, -0.951435, -0.953306, -0.955141, -0.956940, -0.958703, -0.960431, -0.962121, -0.963776, -0.965394,
      -0.966976, -0.968522, -0.970031, -0.971504, -0.972940, -0.974339, -0.975702, -0.977028, -0.978317, -0.979570,
      -0.980785, -0.981964, -0.983105, -0.984210, -0.985278, -0.986308, -0.987301, -0.988258, -0.989177, -0.990058,
      -0.990903, -0.991710, -0.992480, -0.993212, -0.993907, -0.994565, -0.995185, -0.995767, -0.996313, -0.996820,
      -0.997290, -0.997723, -0.998118, -0.998476, -0.998795, -0.999078, -0.999322, -0.999529, -0.999699, -0.999831,
      -0.999925, -0.999981, -1.000000, -0.999981, -0.999925, -0.999831, -0.999699, -0.999529, -0.999322, -0.999078,
      -0.998795, -0.998476, -0.998118, -0.997723, -0.997290, -0.996820, -0.996313, -0.995767, -0.995185, -0.994565,
      -0.993907, -0.993212, -0.992480, -0.991710, -0.990903, -0.990058, -0.989177, -0.988258, -0.987301, -0.986308,
      -0.985278, -0.984210, -0.983105, -0.981964, -0.980785, -0.979570, -0.978317, -0.977028, -0.975702, -0.974339,
      -0.972940, -0.971504, -0.970031, -0.968522, -0.966976, -0.965394, -0.963776, -0.962121, -0.960431, -0.958703,
      -0.956940, -0.955141, -0.953306, -0.951435, -0.949528, -0.947586, -0.945607, -0.943593, -0.941544, -0.939459,
      -0.937339, -0.935184, -0.932993, -0.930767, -0.928506, -0.926210, -0.923880, -0.921514, -0.919114, -0.916679,
      -0.914210, -0.911706, -0.909168, -0.906596, -0.903989, -0.901349, -0.898674, -0.895966, -0.893224, -0.890449,
      -0.887640, -0.884797, -0.881921, -0.879012, -0.876070, -0.873095, -0.870087, -0.867046, -0.863973, -0.860867,
      -0.857729, -0.854558, -0.851355, -0.848120, -0.844854, -0.841555, -0.838225, -0.834863, -0.831470, -0.828045,
      -0.824589, -0.821103, -0.817585, -0.814036, -0.810457, -0.806848, -0.803208, -0.799537, -0.795837, -0.792107,
      -0.788346, -0.784557, -0.780737, -0.776888, -0.773010, -0.769103, -0.765167, -0.761202, -0.757209, -0.753187,
      -0.749136, -0.745058, -0.740951, -0.736817, -0.732654, -0.728464, -0.724247, -0.720003, -0.715731, -0.711432,
      -0.707107, -0.702755, -0.698376, -0.693971, -0.689541, -0.685084, -0.680601, -0.676093, -0.671559, -0.667000,
      -0.662416, -0.657807, -0.653173, -0.648514, -0.643832, -0.639124, -0.634393, -0.629638, -0.624859, -0.620057,
      -0.615232, -0.610383, -0.605511, -0.600616, -0.595699, -0.590760, -0.585798, -0.580814, -0.575808, -0.570781,
      -0.565732, -0.560662, -0.555570, -0.550458, -0.545325, -0.540171, -0.534998, -0.529804, -0.524590, -0.519356,
      -0.514103, -0.508830, -0.503538, -0.498228, -0.492898, -0.487550, -0.482184, -0.476799, -0.471397, -0.465976,
      -0.460539, -0.455084, -0.449611, -0.444122, -0.438616, -0.433094, -0.427555, -0.422000, -0.416430, -0.410843,
      -0.405241, -0.399624, -0.393992, -0.388345, -0.382683, -0.377007, -0.371317, -0.365613, -0.359895, -0.354164,
      -0.348419, -0.342661, -0.336890, -0.331106, -0.325310, -0.319502, -0.313682, -0.307850, -0.302006, -0.296151,
      -0.290285, -0.284408, -0.278520, -0.272621, -0.266713, -0.260794, -0.254866, -0.248928, -0.242980, -0.237024,
      -0.231058, -0.225084, -0.219101, -0.213110, -0.207111, -0.201105, -0.195090, -0.189069, -0.183040, -0.177004,
      -0.170962, -0.164913, -0.158858, -0.152797, -0.146730, -0.140658, -0.134581, -0.128498, -0.122411, -0.116319,
      -0.110222, -0.104122, -0.098017, -0.091909, -0.085797, -0.079682, -0.073565, -0.067444, -0.061321, -0.055195,
      -0.049068, -0.042938, -0.036807, -0.030675, -0.024541, -0.018407, -0.012272, -0.006136, -0.000000, 0.006136,
      0.012272,  0.018407,  0.024541,  0.030675,  0.036807,  0.042938,  0.049068,  0.055195,  0.061321,  0.067444,
      0.073565,  0.079682,  0.085797,  0.091909,  0.098017,  0.104122,  0.110222,  0.116319,  0.122411,  0.128498,
      0.134581,  0.140658,  0.146730,  0.152797,  0.158858,  0.164913,  0.170962,  0.177004,  0.183040,  0.189069,
      0.195090,  0.201105,  0.207111,  0.213110,  0.219101,  0.225084,  0.231058,  0.237024,  0.242980,  0.248928,
      0.254866,  0.260794,  0.266713,  0.272621,  0.278520,  0.284408,  0.290285,  0.296151,  0.302006,  0.307850,
      0.313682,  0.319502,  0.325310,  0.331106,  0.336890,  0.342661,  0.348419,  0.354164,  0.359895,  0.365613,
      0.371317,  0.377007,  0.382683,  0.388345,  0.393992,  0.399624,  0.405241,  0.410843,  0.416430,  0.422000,
      0.427555,  0.433094,  0.438616,  0.444122,  0.449611,  0.455084,  0.460539,  0.465976,  0.471397,  0.476799,
      0.482184,  0.487550,  0.492898,  0.498228,  0.503538,  0.508830,  0.514103,  0.519356,  0.524590,  0.529804,
      0.534998,  0.540171,  0.545325,  0.550458,  0.555570,  0.560662,  0.565732,  0.570781,  0.575808,  0.580814,
      0.585798,  0.590760,  0.595699,  0.600616,  0.605511,  0.610383,  0.615232,  0.620057,  0.624859,  0.629638,
      0.634393,  0.639124,  0.643832,  0.648514,  0.653173,  0.657807,  0.662416,  0.667000,  0.671559,  0.676093,
      0.680601,  0.685084,  0.689541,  0.693971,  0.698376,  0.702755,  0.707107,  0.711432,  0.715731,  0.720003,
      0.724247,  0.728464,  0.732654,  0.736817,  0.740951,  0.745058,  0.749136,  0.753187,  0.757209,  0.761202,
      0.765167,  0.769103,  0.773010,  0.776888,  0.780737,  0.784557,  0.788346,  0.792107,  0.795837,  0.799537,
      0.803208,  0.806848,  0.810457,  0.814036,  0.817585,  0.821103,  0.824589,  0.828045,  0.831470,  0.834863,
      0.838225,  0.841555,  0.844854,  0.848120,  0.851355,  0.854558,  0.857729,  0.860867,  0.863973,  0.867046,
      0.870087,  0.873095,  0.876070,  0.879012,  0.881921,  0.884797,  0.887640,  0.890449,  0.893224,  0.895966,
      0.898674,  0.901349,  0.903989,  0.906596,  0.909168,  0.911706,  0.914210,  0.916679,  0.919114,  0.921514,
      0.923880,  0.926210,  0.928506,  0.930767,  0.932993,  0.935184,  0.937339,  0.939459,  0.941544,  0.943593,
      0.945607,  0.947586,  0.949528,  0.951435,  0.953306,  0.955141,  0.956940,  0.958703,  0.960431,  0.962121,
      0.963776,  0.965394,  0.966976,  0.968522,  0.970031,  0.971504,  0.972940,  0.974339,  0.975702,  0.977028,
      0.978317,  0.979570,  0.980785,  0.981964,  0.983105,  0.984210,  0.985278,  0.986308,  0.987301,  0.988258,
      0.989177,  0.990058,  0.990903,  0.991710,  0.992480,  0.993212,  0.993907,  0.994565,  0.995185,  0.995767,
      0.996313,  0.996820,  0.997290,  0.997723,  0.998118,  0.998476,  0.998795,  0.999078,  0.999322,  0.999529,
      0.999699,  0.999831,  0.999925,  0.999981};

  half threshold_half;
  CNRT_CHECK(cnrtConvertFloatToHalf(&threshold_half, threshold));
  handle->threshold = threshold_half;
  const int num_dft_mats = (TMP_SZ / 2) * 8 * TMP_SZ_64 * TMP_SZ_64;

  CNRT_CHECK(cnrtMalloc((void**)&handle->dft_mat, num_dft_mats * sizeof(half)));
  uint32_t dft_mat_zipped_size = sizeof(_dft_mat_table_zipped) / sizeof(half);
  uint32_t dft_mat_size = 0;
  half* dft_mat_half = (half*)malloc(num_dft_mats * sizeof(half));
  decompress_dft_mat(_dft_mat_table_zipped, dft_mat_zipped_size, dft_mat_half, &dft_mat_size);
  printf("decompress_dft_mat ok(%u)\n", dft_mat_size);

  CNRT_CHECK(cnrtMemcpy(handle->dft_mat, dft_mat_half, num_dft_mats * sizeof(half), CNRT_MEM_TRANS_DIR_HOST2DEV));
  free(dft_mat_half);

  half* cos_table_half = (half*)malloc(1024 * sizeof(half));
  // cnrtConvertFloatToHalfArray(cos_table_half, _cos_table, 1024);
  for (int i = 0; i < 1024; i++) {
    cnrtConvertFloatToHalf(cos_table_half + i, _cos_table[i]);
  }
  CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&handle->cos_table), 1024 * sizeof(half)));
  CNRT_CHECK(cnrtMemcpy(handle->cos_table, cos_table_half, 1024 * sizeof(half), CNRT_MEM_TRANS_DIR_HOST2DEV));
  free(cos_table_half);

  constexpr const int buffer_size_bytes = align(16 + MAX_ROI_NUM * 8) * sizeof(int);
  CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&handle->mlu_buffer), buffer_size_bytes));
  handle->cpu_buffer = (int*)malloc(buffer_size_bytes);

  CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&handle->args), MAX_ROI_NUM * BLOCK(6) * sizeof(half)));
  CNRT_CHECK(cnrtMalloc(reinterpret_cast<void**>(&handle->scale), MAX_ROI_ALIGN * sizeof(half)));
  handle->pQueue = queue;
}

void kcf_destroy(KCFHandle* handle) {
  CNRT_CHECK(cnrtFree(handle->mlu_buffer));
  CNRT_CHECK(cnrtFree(handle->dft_mat));
  CNRT_CHECK(cnrtFree(handle->cos_table));
  CNRT_CHECK(cnrtFree(handle->scale));
  CNRT_CHECK(cnrtFree(handle->args));
  free(handle->cpu_buffer);
}

extern "C" {
void initKernel(void);
void updateKernel(void);
}

void kcf_initKernel(KCFHandle* handle, half* frame, half* rois_mlu, __Rect* out_roi, int* p_roi_num) {
  uint64_t time_a, time_b;
  STORE_USEC_TIME_OF_DAY_TO_INT64(time_a);

  cnrtQueue_t pQueue = handle->pQueue;
  cnrtDim3_t dim;
  dim.x = 1;
  dim.y = 1;
  dim.z = 1;
  constexpr const cnrtFunctionType_t c = CNRT_FUNC_TYPE_BLOCK;

  cnrtKernelParamsBuffer_t params;
  CNRT_CHECK(cnrtGetKernelParamsBuffer(&params));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &rois_mlu, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->threshold, sizeof(half)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &frame, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->mlu_buffer, sizeof(int*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->dft_mat, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->cos_table, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->args, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->scale, sizeof(half*)));

  CNRT_CHECK(cnrtInvokeKernel_V2(reinterpret_cast<void*>(&initKernel), dim, params, c, pQueue));
  CNRT_CHECK(cnrtSyncQueue(pQueue));
  CNRT_CHECK(cnrtDestroyKernelParamsBuffer(params));

  int* cpu_buffer = handle->cpu_buffer;
  int* mlu_buffer = handle->mlu_buffer;
  constexpr const int roi_len = align(16 + MAX_ROI_NUM * 6);

  STORE_USEC_TIME_OF_DAY_TO_INT64(time_a);
  CNRT_CHECK(cnrtMemcpy(cpu_buffer, mlu_buffer, roi_len * sizeof(int), CNRT_MEM_TRANS_DIR_DEV2HOST));
  STORE_USEC_TIME_OF_DAY_TO_INT64(time_b);

  *(p_roi_num) = *cpu_buffer;

  int* detect_roi = cpu_buffer + 16;

  for (int i = 0; i < *p_roi_num; ++i) {
    out_roi[i] = {detect_roi[i * 6],
                  detect_roi[i * 6 + 1],
                  detect_roi[i * 6 + 2],
                  detect_roi[i * 6 + 3],
                  static_cast<float>(detect_roi[i * 6 + 4]),
                  detect_roi[i * 6 + 5]};
  }
  STORE_USEC_TIME_OF_DAY_TO_INT64(time_b);
  printf("kcf function run time (init)= %ld.%ldms\n", (time_b - time_a) / 1000, (time_b - time_a) % 1000);
}

// Wrap interface
void kcf_updateKernel(KCFHandle* handle, half* frame, __Rect* out_roi, int roi_num) {
  uint64_t time_a, time_b;
  STORE_USEC_TIME_OF_DAY_TO_INT64(time_a);

  cnrtQueue_t pQueue = handle->pQueue;
  cnrtDim3_t dim;
  dim.x = 1;
  dim.y = 1;
  dim.z = 1;
  cnrtFunctionType_t c = CNRT_FUNC_TYPE_BLOCK;

  cnrtKernelParamsBuffer_t params;
  CNRT_CHECK(cnrtGetKernelParamsBuffer(&params));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->threshold, sizeof(half)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &frame, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->mlu_buffer, sizeof(int*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->dft_mat, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->args, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &handle->scale, sizeof(half*)));
  CNRT_CHECK(cnrtKernelParamsBufferAddParam(params, &roi_num, sizeof(int)));

  CNRT_CHECK(cnrtInvokeKernel_V2(reinterpret_cast<void*>(&updateKernel), dim, params, c, pQueue));
  // FIXME(dingminghui): do not sync?
  CNRT_CHECK(cnrtSyncQueue(pQueue));
  CNRT_CHECK(cnrtDestroyKernelParamsBuffer(params));

  int* cpu_buffer = handle->cpu_buffer;
  int* mlu_buffer = handle->mlu_buffer;
  // roi_num , out roi
  constexpr const int roi_len = align(16 + MAX_ROI_NUM * 6);

  STORE_USEC_TIME_OF_DAY_TO_INT64(time_a);
  CNRT_CHECK(cnrtMemcpy(cpu_buffer, mlu_buffer, roi_len * sizeof(int), CNRT_MEM_TRANS_DIR_DEV2HOST));
  STORE_USEC_TIME_OF_DAY_TO_INT64(time_b);

  int* detect_roi = cpu_buffer + 16;

  for (int i = 0; i < roi_num; ++i) {
    out_roi[i] = {detect_roi[i * 6],
                  detect_roi[i * 6 + 1],
                  detect_roi[i * 6 + 2],
                  detect_roi[i * 6 + 3],
                  static_cast<float>(detect_roi[i * 6 + 4]),
                  detect_roi[i * 6 + 5]};
  }
  STORE_USEC_TIME_OF_DAY_TO_INT64(time_b);
  printf("kcf function run time (update)= %ld.%ldms\n", (time_b - time_a) / 1000, (time_b - time_a) % 1000);
}

#endif  // ENABLE_KCF

