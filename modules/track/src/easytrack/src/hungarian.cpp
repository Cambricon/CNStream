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

#include "hungarian.h"
#include <float.h>
#include <math.h>
#include <string.h>

//********************************************************//
// A single function wrapper for solving assignment problem.
//********************************************************//
float HungarianAlgorithm::Solve(const cnstream::Matrix &DistMatrix, std::vector<int> *Assignment, void *_workspace) {
  unsigned int nRows = DistMatrix.Rows();
  unsigned int nCols = DistMatrix.Cols();

  void *buffer = _workspace ? _workspace : malloc(GetWorkspaceSize(nRows, nCols));
  float *workspace = reinterpret_cast<float *>(buffer);

  // sizeof(distMatrixIn) = nRows * nCols * sizeof(float)
  float *distMatrixIn = workspace;
  workspace += nRows * nCols;
  // sizeof(assignment) = nRows * sizeof(int)
  int *assignment = reinterpret_cast<int *>(workspace);
  workspace += nRows;

  float cost = 0.0;

  // Fill in the distMatrixIn. Mind the index is "i + nRows * j".
  // Here the cost matrix of size MxN is defined as a float precision array of
  // N*M elements. In the solving functions matrices are seen to be saved
  // MATLAB-internally in row-order. (i.e. the matrix [1 2; 3 4] will be stored
  // as a vector [1 3 2 4], NOT [1 2 3 4]).

  for (unsigned int i = 0; i < nRows; i++) {
    for (unsigned int j = 0; j < nCols; j++) {
      distMatrixIn[i + nRows * j] = DistMatrix(i, j);
    }
  }

  // call solving function
  assignmentoptimal(assignment, &cost, distMatrixIn, nRows, nCols, workspace);
  Assignment->clear();
  Assignment->reserve(nRows);
  for (unsigned int r = 0; r < nRows; r++) {
    Assignment->push_back(assignment[r]);
  }

  _workspace ? (void)0 : free(buffer);
  return cost;
}

//********************************************************//
// Solve optimal solution for assignment problem using Munkres algorithm, also
// known as Hungarian Algorithm.
//********************************************************//
void HungarianAlgorithm::assignmentoptimal(int *assignment, float *cost, float *distMatrixIn, int nOfRows,
                                           int nOfColumns, void *workspace) {
  float *distMatrix, *distMatrixTemp, *distMatrixEnd, *columnEnd, value, minValue;
  bool *coveredColumns, *coveredRows, *starMatrix, *newStarMatrix, *primeMatrix;
  int nOfElements, minDim, row, col;

  /* initialization */
  *cost = 0;
  for (row = 0; row < nOfRows; row++) assignment[row] = -1;

  /* generate working copy of distance Matrix */
  /* check if all matrix elements are positive */
  nOfElements = nOfRows * nOfColumns;
  // sizeof(distMatrix) = nOfElements * sizeof(float)
  distMatrix = reinterpret_cast<float *>(workspace);
  distMatrixEnd = distMatrix + nOfElements;

  // for (row = 0; row < nOfElements; row++) {
  //   if (distMatrixIn[row] < 0) std::cerr << "All matrix elements have to be non-negative." << std::endl;
  // }
  mempcpy(distMatrix, distMatrixIn, nOfElements * sizeof(float));

  /* memory allocation */
  memset(distMatrixEnd, 0, nOfElements * 3 + nOfColumns + nOfRows);
  // sizeof(starMatrix) = nOfElements * sizeof(bool)
  starMatrix = reinterpret_cast<bool *>(distMatrixEnd);
  // sizeof(primeMatrix) = nOfElements * sizeof(bool)
  primeMatrix = starMatrix + nOfElements;
  // sizeof(newStarMatrix) = nOfElements * sizeof(bool)
  newStarMatrix = primeMatrix + nOfElements; /* used in step4 */
  // sizeof(coveredColumns) = nOfColumns * sizeof(bool)
  coveredColumns = newStarMatrix + nOfElements;
  // sizeof(coveredRows) = nOfRows * sizeof(bool)
  coveredRows = coveredColumns + nOfColumns;

  /* preliminary steps */
  if (nOfRows <= nOfColumns) {
    minDim = nOfRows;

    for (row = 0; row < nOfRows; row++) {
      /* find the smallest element in the row */
      distMatrixTemp = distMatrix + row;
      minValue = *distMatrixTemp;
      distMatrixTemp += nOfRows;
      while (distMatrixTemp < distMatrixEnd) {
        value = *distMatrixTemp;
        if (value < minValue) minValue = value;
        distMatrixTemp += nOfRows;
      }

      /* subtract the smallest element from each element of the row */
      distMatrixTemp = distMatrix + row;
      while (distMatrixTemp < distMatrixEnd) {
        *distMatrixTemp -= minValue;
        distMatrixTemp += nOfRows;
      }
    }

    /* Steps 1 and 2a */
    for (row = 0; row < nOfRows; row++) {
      for (col = 0; col < nOfColumns; col++) {
        if (fabs(distMatrix[row + nOfRows * col]) < DBL_EPSILON) {
          if (!coveredColumns[col]) {
            starMatrix[row + nOfRows * col] = true;
            coveredColumns[col] = true;
            break;
          }
        }
      }
    }
  } else { /* if(nOfRows > nOfColumns) */
    minDim = nOfColumns;

    for (col = 0; col < nOfColumns; col++) {
      /* find the smallest element in the column */
      distMatrixTemp = distMatrix + nOfRows * col;
      columnEnd = distMatrixTemp + nOfRows;

      minValue = *distMatrixTemp++;
      while (distMatrixTemp < columnEnd) {
        value = *distMatrixTemp++;
        if (value < minValue) minValue = value;
      }

      /* subtract the smallest element from each element of the column */
      distMatrixTemp = distMatrix + nOfRows * col;
      while (distMatrixTemp < columnEnd) *distMatrixTemp++ -= minValue;
    }

    /* Steps 1 and 2a */
    for (col = 0; col < nOfColumns; col++) {
      for (row = 0; row < nOfRows; row++) {
        if (fabs(distMatrix[row + nOfRows * col]) < DBL_EPSILON) {
          if (!coveredRows[row]) {
            starMatrix[row + nOfRows * col] = true;
            coveredColumns[col] = true;
            coveredRows[row] = true;
            break;
          }
        }
      }
    }
    for (row = 0; row < nOfRows; row++) coveredRows[row] = false;
  }

  /* move to step 2b */
  step2b(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows, nOfRows,
         nOfColumns, minDim);

  /* compute cost and remove invalid assignments */
  computeassignmentcost(assignment, cost, distMatrixIn, nOfRows);

  return;
}

/********************************************************/
void HungarianAlgorithm::buildassignmentvector(int *assignment, bool *starMatrix, int nOfRows, int nOfColumns) {
  int row, col;

  for (row = 0; row < nOfRows; row++)
    for (col = 0; col < nOfColumns; col++)
      if (starMatrix[row + nOfRows * col]) {
#ifdef ONE_INDEXING
        assignment[row] = col + 1; /* MATLAB-Indexing */
#else
        assignment[row] = col;
#endif
        break;
      }
}

/********************************************************/
void HungarianAlgorithm::computeassignmentcost(int *assignment, float *cost, float *distMatrix, int nOfRows) {
  int row, col;

  for (row = 0; row < nOfRows; row++) {
    col = assignment[row];
    if (col >= 0) *cost += distMatrix[row + nOfRows * col];
  }
}

/********************************************************/
void HungarianAlgorithm::step2a(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix,
                                bool *primeMatrix, bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns,
                                int minDim) {
  bool *starMatrixTemp, *columnEnd;
  int col;

  /* cover every column containing a starred zero */
  for (col = 0; col < nOfColumns; col++) {
    starMatrixTemp = starMatrix + nOfRows * col;
    columnEnd = starMatrixTemp + nOfRows;
    while (starMatrixTemp < columnEnd) {
      if (*starMatrixTemp++) {
        coveredColumns[col] = true;
        break;
      }
    }
  }

  /* move to step 3 */
  step2b(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows, nOfRows,
         nOfColumns, minDim);
}

/********************************************************/
void HungarianAlgorithm::step2b(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix,
                                bool *primeMatrix, bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns,
                                int minDim) {
  int col, nOfCoveredColumns;

  /* count covered columns */
  nOfCoveredColumns = 0;
  for (col = 0; col < nOfColumns; col++)
    if (coveredColumns[col]) nOfCoveredColumns++;

  if (nOfCoveredColumns == minDim) {
    /* algorithm finished */
    buildassignmentvector(assignment, starMatrix, nOfRows, nOfColumns);
  } else {
    /* move to step 3 */
    step3(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows, nOfRows,
          nOfColumns, minDim);
  }
}

/********************************************************/
void HungarianAlgorithm::step3(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix,
                               bool *primeMatrix, bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns,
                               int minDim) {
  bool zerosFound;
  int row, col, starCol;

  zerosFound = true;
  while (zerosFound) {
    zerosFound = false;
    for (col = 0; col < nOfColumns; col++) {
      if (!coveredColumns[col]) {
        for (row = 0; row < nOfRows; row++) {
          if ((!coveredRows[row]) && (fabs(distMatrix[row + nOfRows * col]) < DBL_EPSILON)) {
            /* prime zero */
            primeMatrix[row + nOfRows * col] = true;

            /* find starred zero in current row */
            for (starCol = 0; starCol < nOfColumns; starCol++) {
              if (starMatrix[row + nOfRows * starCol]) break;
            }

            if (starCol == nOfColumns) { /* no starred zero found */
              /* move to step 4 */
              step4(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows,
                    nOfRows, nOfColumns, minDim, row, col);
              return;
            } else {
              coveredRows[row] = true;
              coveredColumns[starCol] = false;
              zerosFound = true;
              break;
            }
          }
        }
      }
    }
  }

  /* move to step 5 */
  step5(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows, nOfRows,
        nOfColumns, minDim);
}

/********************************************************/
void HungarianAlgorithm::step4(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix,
                               bool *primeMatrix, bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns,
                               int minDim, int row, int col) {
  int n, starRow, starCol, primeRow, primeCol;
  int nOfElements = nOfRows * nOfColumns;

  /* generate temporary copy of starMatrix */
  for (n = 0; n < nOfElements; n++) newStarMatrix[n] = starMatrix[n];

  /* star current zero */
  newStarMatrix[row + nOfRows * col] = true;

  /* find starred zero in current column */
  starCol = col;
  for (starRow = 0; starRow < nOfRows; starRow++)
    if (starMatrix[starRow + nOfRows * starCol]) break;

  while (starRow < nOfRows) {
    /* unstar the starred zero */
    newStarMatrix[starRow + nOfRows * starCol] = false;

    /* find primed zero in current row */
    primeRow = starRow;
    for (primeCol = 0; primeCol < nOfColumns; primeCol++)
      if (primeMatrix[primeRow + nOfRows * primeCol]) break;

    /* star the primed zero */
    newStarMatrix[primeRow + nOfRows * primeCol] = true;

    /* find starred zero in current column */
    starCol = primeCol;
    for (starRow = 0; starRow < nOfRows; starRow++)
      if (starMatrix[starRow + nOfRows * starCol]) break;
  }

  /* use temporary copy as new starMatrix */
  /* delete all primes, uncover all rows */
  for (n = 0; n < nOfElements; n++) {
    primeMatrix[n] = false;
    starMatrix[n] = newStarMatrix[n];
  }
  for (n = 0; n < nOfRows; n++) coveredRows[n] = false;

  /* move to step 2a */
  step2a(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows, nOfRows,
         nOfColumns, minDim);
}

/********************************************************/
void HungarianAlgorithm::step5(int *assignment, float *distMatrix, bool *starMatrix, bool *newStarMatrix,
                               bool *primeMatrix, bool *coveredColumns, bool *coveredRows, int nOfRows, int nOfColumns,
                               int minDim) {
  float h, value;
  int row, col;

  /* find smallest uncovered element h */
  h = DBL_MAX;
  for (row = 0; row < nOfRows; row++) {
    if (!coveredRows[row]) {
      for (col = 0; col < nOfColumns; col++) {
        if (!coveredColumns[col]) {
          value = distMatrix[row + nOfRows * col];
          if (value < h) h = value;
        }
      }
    }
  }

  /* add h to each covered row */
  for (row = 0; row < nOfRows; row++) {
    if (coveredRows[row]) {
      for (col = 0; col < nOfColumns; col++) {
        distMatrix[row + nOfRows * col] += h;
      }
    }
  }

  /* subtract h from each uncovered column */
  for (col = 0; col < nOfColumns; col++) {
    if (!coveredColumns[col]) {
      for (row = 0; row < nOfRows; row++) {
        distMatrix[row + nOfRows * col] -= h;
      }
    }
  }

  /* move to step 3 */
  step3(assignment, distMatrix, starMatrix, newStarMatrix, primeMatrix, coveredColumns, coveredRows, nOfRows,
        nOfColumns, minDim);
}
