#include "matrix.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace edk {

const Matrix &Matrix::operator=(std::vector<std::vector<float>> init_list) {
  arrays_ = init_list;
  return *this;
}

const Matrix &Matrix::operator+=(const Matrix &m) {
  if (Rows() != m.Rows() || Cols() != m.Cols())
    THROW_EXCEPTION(Exception::INVALID_ARG, "Matrices of two different shape cannot be added");

  int r = Rows();
  int c = Cols();
  for (int i = 0; i < r; ++i) {
    for (int j = 0; j < c; ++j) {
      arrays_[i][j] += m[i][j];
    }
  }
  return *this;
}

const Matrix &Matrix::operator-=(const Matrix &m) {
  if (Rows() != m.Rows() || Cols() != m.Cols())
    THROW_EXCEPTION(Exception::INVALID_ARG, "Matrices of two different shape cannot be subtracted");

  int r = Rows();
  int c = Cols();

  for (int i = 0; i < r; ++i) {
    for (int j = 0; j < c; ++j) {
      arrays_[i][j] -= m[i][j];
    }
  }
  return *this;
}

const Matrix &Matrix::operator*=(const Matrix &m) {
  if (Cols() != m.Rows() || !m.Square()) THROW_EXCEPTION(Exception::INVALID_ARG, "Matrices can not be multiplied");

  Matrix ret(Rows(), Cols());

  int r = Rows();
  int c = Cols();

  for (int i = 0; i < r; ++i) {
    for (int j = 0; j < c; ++j) {
      double sum = 0.0;
      for (int k = 0; k < c; ++k) {
        sum += arrays_[i][k] * m[k][j];
      }
      ret[i][j] = sum;
    }
  }
  *this = ret;
  return *this;
}

const Matrix operator+(const Matrix &lhs, const Matrix &rhs) {
  Matrix m;
  if (lhs.Rows() != rhs.Rows() || lhs.Cols() != rhs.Cols())
    THROW_EXCEPTION(Exception::INVALID_ARG, "Matrices of two different shape cannot be added");

  m = lhs;
  m += rhs;

  return m;
}

const Matrix operator-(const Matrix &lhs, const Matrix &rhs) {
  if (lhs.Rows() != rhs.Rows() || lhs.Cols() != rhs.Cols())
    THROW_EXCEPTION(Exception::INVALID_ARG, "Matrices of two different shape cannot be subtracted");

  Matrix m = lhs;
  m = lhs;
  m -= rhs;

  return m;
}

const Matrix operator*(const Matrix &lhs, const Matrix &rhs) {
  if (lhs.Cols() != rhs.Rows()) THROW_EXCEPTION(Exception::INVALID_ARG, "Matrices can not be multiplied");

  Matrix m(lhs.Rows(), rhs.Cols());

  int r = m.Rows();
  int c = m.Cols();
  int K = lhs.Cols();

  for (int i = 0; i < r; ++i) {
    for (int j = 0; j < c; ++j) {
      double sum = 0.0;
      for (int k = 0; k < K; ++k) {
        sum += lhs[i][k] * rhs[k][j];
      }
      m[i][j] = sum;
    }
  }
  return m;
}

const Matrix Matrix::Trans() const {
  if (Empty()) THROW_EXCEPTION(Exception::INVALID_ARG, "Empty Matrix do not have transpose");

  int row = Cols();
  int col = Rows();
  Matrix ret(row, col);

  for (int i = 0; i < row; ++i) {
    for (int j = 0; j < col; ++j) {
      ret[i][j] = arrays_[j][i];
    }
  }
  return ret;
}

static void SolveInverse(float *A, int n, float *m_inv);

const Matrix Matrix::Inv() const {
  if (!Square()) {
    THROW_EXCEPTION(Exception::INVALID_ARG, "Non-square matrix do not have inverse");
  }
  int n = Rows();
  float *m = new float[n * n];
  for (int i = 0; i < n; ++i) {
    memcpy(m + i * n, arrays_[i].data(), n * sizeof(float));
  }
  float *m_inv = new float[n * n];
  SolveInverse(m, n, m_inv);
  Matrix ret(n, n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      ret[i][j] = m_inv[i * n + j];
    }
  }
  delete[] m_inv;
  delete[] m;
  return ret;
}

bool operator==(const Matrix &lhs, const Matrix &rhs) {
  if (lhs.Rows() != rhs.Rows() || lhs.Cols() != rhs.Cols()) {
    return false;
  }

  for (int i = 0; i < lhs.Rows(); ++i) {
    for (int j = 0; j < lhs.Cols(); ++j) {
      if (lhs[i][j] != rhs[i][j]) {
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
    for (int j = 0; j < nCols; j++) printf("%.2f ", arrays_[i][j]);
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
static void SolveInverse(float *A, int n, float *inv_A) {
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

}  // namespace edk
