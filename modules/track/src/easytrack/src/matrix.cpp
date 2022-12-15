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
#include "matrix.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace cnstream {

const Matrix &Matrix::operator+=(const Matrix &m) {
  if (Rows() != m.Rows() || Cols() != m.Cols())
    LOGF(TRACK) << "Matrices of two different shape cannot be added";

  size_t size = Size();
  for (uint32_t i = 0; i < size; ++i) {
    arrays_[i] += m[i];
  }
  return *this;
}

const Matrix &Matrix::operator-=(const Matrix &m) {
  if (Rows() != m.Rows() || Cols() != m.Cols())
    LOGF(TRACK) << "Matrices of two different shape cannot be subtracted";

  size_t size = Size();
  for (uint32_t i = 0; i < size; ++i) {
    arrays_[i] -= m[i];
  }
  return *this;
}

const Matrix operator+(const Matrix &lhs, const Matrix &rhs) {
  Matrix m = lhs;
  m += rhs;

  return m;
}

const Matrix operator-(const Matrix &lhs, const Matrix &rhs) {
  Matrix m = lhs;
  m -= rhs;

  return m;
}

const Matrix operator*(const Matrix &lhs, const Matrix &rhs) {
  if (lhs.Cols() != rhs.Rows()) LOGF(TRACK) << "Matrices can not be multiplied";

  Matrix m(lhs.Rows(), rhs.Cols());

  uint32_t r = m.Rows();
  uint32_t c = m.Cols();
  uint32_t K = lhs.Cols();

  for (uint32_t i = 0; i < r; ++i) {
    for (uint32_t j = 0; j < c; ++j) {
      double sum = 0.0;
      for (uint32_t k = 0; k < K; ++k) {
        sum += lhs(i, k) * rhs(k, j);
      }
      m(i, j) = sum;
    }
  }
  return m;
}

Matrix Matrix::Trans() const {
  if (Empty()) LOGF(TRACK) << "Empty Matrix do not have transpose";

  uint32_t row = Cols();
  uint32_t col = Rows();
  Matrix ret(row, col);

  for (uint32_t i = 0; i < row; ++i) {
    for (uint32_t j = 0; j < col; ++j) {
      ret(i, j) = arrays_[j * cols_ + i];
    }
  }
  return ret;
}

static void SolveInverse(const float *A, int n, float *m_inv);

Matrix Matrix::Inv() const {
  if (!Square()) {
    LOGF(TRACK) << "Non-square matrix do not have inverse";
  }
  uint32_t n = Rows();
  Matrix ret(n, n);
  SolveInverse(arrays_.data(), n, ret.arrays_.data());
  return ret;
}

bool operator==(const Matrix &lhs, const Matrix &rhs) {
  if (lhs.Rows() != rhs.Rows() || lhs.Cols() != rhs.Cols()) {
    return false;
  }

  for (uint32_t i = 0; i < lhs.Rows(); ++i) {
    for (uint32_t j = 0; j < lhs.Cols(); ++j) {
      if (lhs(i, j) != rhs(i, j)) {
        return false;
      }
    }
  }

  return true;
}

bool operator!=(const Matrix &lhs, const Matrix &rhs) { return !(lhs == rhs); }

void Matrix::Show() const {
  const int nCols = Cols();
  const int nRows = Rows();
  printf("------- Matrix -------\n");
  for (int i = 0; i < nRows; i++) {
    for (int j = 0; j < nCols; j++) printf("%.2f ", arrays_[i * cols_ + j]);
    printf("\n");
  }
  printf("----------------------\n");
  printf("\n");
}

/* ------------------------------- inverse implement ------------------------------------ */
static void LupDescomposition(double *A, double *L, double *U, int *P, int n) {
  int row = 0;
  for (int i = 0; i < n; i++) {
    P[i] = i;
  }
  for (int i = 0; i < n - 1; i++) {
    double p = 0.0f;
    do {
      for (int j = i; j < n; j++) {
        if (std::abs(A[j * n + i]) > p) {
          p = std::abs(A[j * n + i]);
          row = j;
        }
      }
      if (p != 0) {
        break;
      } else {
        A[i * n + i] += 1e-5;
      }
    } while (true);

    int tmp = P[i];
    P[i] = P[row];
    P[row] = tmp;

    double tmp2 = 0.0f;
    for (int j = 0; j < n; j++) {
      tmp2 = A[i * n + j];
      A[i * n + j] = A[row * n + j];
      A[row * n + j] = tmp2;
    }

    double u = A[i * n + i], l = 0.0f;
    for (int j = i + 1; j < n; j++) {
      l = A[j * n + i] / u;
      A[j * n + i] = l;
      for (int k = i + 1; k < n; k++) {
        A[j * n + k] = A[j * n + k] - A[i * n + k] * l;
      }
    }
  }

  for (int i = 0; i < n; i++) {
    for (int j = 0; j <= i; j++) {
      if (i != j) {
        L[i * n + j] = A[i * n + j];
      } else {
        L[i * n + j] = 1;
      }
    }
    for (int k = i; k < n; k++) {
      U[i * n + k] = A[i * n + k];
    }
  }
}

// LUP solve function
static void SolveLup(const std::vector<double> &L, const std::vector<double> &U, const std::vector<int> &P,
                     const std::vector<int> &b, float *x, int n) {
  std::vector<double> y(n);

  // forward substitution
  for (int i = 0; i < n; i++) {
    y[i] = b[P[i]];
    for (int j = 0; j < i; j++) {
      y[i] = y[i] - L[i * n + j] * y[j];
    }
  }
  // backward substitution
  for (int i = n - 1; i >= 0; i--) {
    x[i] = y[i];
    for (int j = n - 1; j > i; j--) {
      x[i] = x[i] - U[i * n + j] * x[j];
    }
    x[i] /= U[i * n + i];
  }
}

static inline int GetNext(int i, int m, int n) { return (i % n) * m + i / n; }

static inline int GetPre(int i, int m, int n) { return (i % m) * n + i / m; }

static void MoveData(float *mtx, int i, int m, int n) {
  double temp = mtx[i];
  int cur = i;
  int pre = GetPre(cur, m, n);
  while (pre != i) {
    mtx[cur] = mtx[pre];
    cur = pre;
    pre = GetPre(cur, m, n);
  }
  mtx[cur] = temp;
}

static void Transpose(float *mtx, int m, int n) {
  for (int i = 0; i < m * n; ++i) {
    int next = GetNext(i, m, n);
    while (next > i) next = GetNext(next, m, n);
    if (next == i) MoveData(mtx, i, m, n);
  }
}

// LUP solve inverse API
static void SolveInverse(const float *A, int n, float *inv_A) {
  // make a copy of matrix A, since LUP descomposition will change it
  double *A_mirror = new double[n * n]();
  float *inv_A_each = new float[n]();
  std::vector<int> b;
  std::vector<double> L;
  std::vector<double> U;
  std::vector<int> P;
  L.assign(n * n, 0);
  U.assign(n * n, 0);
  P.assign(n, 0);

  for (int i = 0; i < n * n; i++) {
    A_mirror[i] = A[i];
  }
  LupDescomposition(A_mirror, L.data(), U.data(), P.data(), n);

  for (int i = 0; i < n; i++) {
    b.assign(n, 0);
    b[i] = 1;

    SolveLup(L, U, P, b, inv_A_each, n);
    memcpy(inv_A + i * n, inv_A_each, n * sizeof(float));
  }
  Transpose(inv_A, n, n);

  delete[] inv_A_each;
  delete[] A_mirror;
}

}  // namespace cnstream
