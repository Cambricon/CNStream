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

/*************************************************************************
 * Hungarian.h: Header file for Class HungarianAlgorithm.
 *
 * This is a C++ wrapper with slight modification of a hungarian algorithm
 * implementation by Markus Buehren. The original implementation is a few
 * mex-functions for use in MATLAB, found here:
 * http://www.mathworks.com/matlabcentral/fileexchange/6543-functions-for-the-rectangular-assignment-problem
 *
 * Both this code and the orignal code are published under the BSD license.
 * by Cong Ma, 2016
 ************************************************************************/
#ifndef EASYTRACK_HUNGARIAN_H_
#define EASYTRACK_HUNGARIAN_H_

#include <iostream>
#include <vector>

#include "matrix.h"

class HungarianAlgorithm {
 public:
  float Solve(const cnstream::Matrix &DistMatrix, std::vector<int> *Assignment,
              void *workspace = nullptr);
  size_t GetWorkspaceSize(size_t rows, size_t cols) {
    return cols * rows * 11 + rows * 5 + cols;
  }

 private:
  void assignmentoptimal(int *assignment, float *cost, float *distMatrix,
                         int nOfRows, int nOfColumns, void* workspace);
  void buildassignmentvector(int *assignment, bool *starMatrix, int nOfRows, int nOfColumns);
  void computeassignmentcost(int *assignment, float *cost, float *distMatrix, int nOfRows);
  void step2a(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix, bool *primeMatrix,
              bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns, int minDim);
  void step2b(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix, bool *primeMatrix,
              bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns, int minDim);
  void step3(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix, bool *primeMatrix,
             bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns, int minDim);
  void step4(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix, bool *primeMatrix,
             bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns, int minDim, int row, int col);
  void step5(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix, bool *primeMatrix,
             bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns, int minDim);
};

#endif  // EASYTRACK_HUNGARIAN_H_
