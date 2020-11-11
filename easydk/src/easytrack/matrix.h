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

/**
 * @file matrix.h
 *
 * This file contains a declaration of the Matrix class and overload operators.
 */

#ifndef EASYTRACK_MATRIX_H_
#define EASYTRACK_MATRIX_H_

#include <iostream>
#include <string>
#include <vector>

#include "cxxutil/exception.h"

using std::vector;

namespace edk {

/**
 * @brief Matrix Base class
 */
template <typename Object>
class MatrixPrototype {
 public:
  /**
   * Constructor, initialized as empty matrix
   * @param None
   */
  MatrixPrototype() : arrays_(0) {}

  /**
   * Constructor.
   *
   * @param rows[in] Number of rows in a 2D array.
   * @param cols[in] Number of columns in a 2D array.
   */
  MatrixPrototype(int rows, int cols) : arrays_(rows) {
    for (int i = 0; i < rows; ++i) {
      arrays_[i].resize(cols);
    }
  }

  /**
   * Copy constructor.
   * @param m[in] Another matrix that is assigned to the constructed matrix.
   */
  MatrixPrototype(const MatrixPrototype<Object>& m) { *this = m; }

  /**
   * Resize matrix to specified new shape.
   *
   * @param rows[in] Number of rows in a 2D array.
   * @param cols[in] Number of columns in a 2D array.
   * @return None
   */
  void Resize(int rows, int cols);

  /**
   * @brief Fill all elements with given value
   *
   * @param ele value assigned to element
   */
  void Fill(const Object& element) {
    if (Empty()) return;

    for (auto& array : arrays_) {
      array.assign(Cols(), element);
    }
  }

  /**
   * Get number of rows from matrix.
   * @return Number of rows.
   */
  int Rows() const { return arrays_.size(); }

  /**
   * Get number of columns from matrix.
   * @return Number of columns.
   */
  int Cols() const { return Rows() ? (arrays_[0].size()) : 0; }

  /**
   * Query whether matrix is empty.
   * @return Returns true if the array has no elements.
   */
  bool Empty() const { return Rows() * Cols() == 0; }

  /**
   * Query whether matrix is square.
   * @return Returns true if not empty and number of columns equal to number of rows.
   */
  bool Square() const { return (!(Empty()) && Rows() == Cols()); }

  /**
   * Access vector of const elements at specified row number.
   * @param row[in] Accessed row number
   * @return Specified row elements.
   */
  const vector<Object>& operator[](int row) const { return arrays_[row]; }

  /**
   * Access vector of elements at specified row number.
   * @param row[in] Accessed row number
   * @return Specified row elements.
   */
  vector<Object>& operator[](int row) { return arrays_[row]; }

 protected:
  vector<vector<Object>> arrays_;
};

template <typename Object>
void MatrixPrototype<Object>::Resize(int rows, int cols) {
  int rs = this->Rows();
  int cs = this->Cols();
  if (rows == rs && cols == cs) {
    return;
  } else if (rows == rs && cols != cs) {
    for (int i = 0; i < rows; ++i) {
      arrays_[i].resize(cols);
    }
  } else if (rows != rs && cols == cs) {
    arrays_.resize(rows);
    for (int i = rs; i < rows; ++i) {
      arrays_[i].resize(cols);
    }
  } else {
    arrays_.resize(rows);
    for (int i = 0; i < rows; ++i) {
      arrays_[i].resize(cols);
    }
  }
}

/**
 * @brief Matrix specializition for float
 */
class Matrix : public MatrixPrototype<float> {
 public:
  /**
   * Constructor, initialized as empty matrix
   */
  Matrix() : MatrixPrototype<float>() {}

  /**
   * Constructor.
   *
   * @param rows[in] Number of rows in a 2D array.
   * @param cols[in] Number of columns in a 2D array.
   */
  Matrix(int rows, int cols) : MatrixPrototype<float>(rows, cols) {
    for (int i = 0; i < rows; ++i) {
      arrays_[i].assign(cols, 0);
    }
  }

  /**
   * Copy constructor.
   * @param m[in] Another matrix that is assigned to the constructed matrix.
   */
  Matrix(const Matrix& m) = default;
  Matrix& operator=(const Matrix& m) = default;

  /**
   * Destructor.
   * @brief Release resourse.
   */
  ~Matrix() = default;

  /**
   * Assignment operator with initializer_list.
   * @param init_list[in] Arrays to be assigned to matrix.
   */
  const Matrix& operator=(vector<vector<float>> init_list);

  /**
   * Overload add assignment operator, implement matrix addition.
   * @param m[in] Add operand
   * @return Addition result
   */
  const Matrix& operator+=(const Matrix& m);

  /**
   * Overload subtract assignment operator, implement matrix subtraction.
   * @param m[in] Subtract operand
   * @return Subtraction result
   */
  const Matrix& operator-=(const Matrix& m);

  /**
   * Overload multiply assignment operator, implement matrix multiplication.
   * @param m[in] Multiply operand
   * @return Multiplication result
   */
  const Matrix& operator*=(const Matrix& m);

  /**
   * Solve Matrix inversion.
   * @return Return inverse of matrix.
   * @attention Solve inverse of non-zero singular matrix may cause an error
   */
  const Matrix Inv() const;

  /**
   * Solve Matrix transposition.
   * @return Return transposed matrix
   */
  const Matrix Trans() const;

  /**
   * Print matrix element
   */
  void Show() const;
};

/**
 * Overload equal comparison operator.
 * @param lhs[in] Left hand side matrix
 * @param rhs[in] Right hand side matrix
 * @return Return true if two matrices have same shape and corresponding position elements are equal
 */
bool operator==(const Matrix& lhs, const Matrix& rhs);

/**
 * Overload non-equal comparison operator.
 *
 * @param lhs[in] Left hand side matrix
 * @param rhs[in] Right hand side matrix
 * @return Return true if two matrices are not equal.
 */
bool operator!=(const Matrix& lhs, const Matrix& rhs);

/**
 * Overload add operator
 *
 * @param lhs[in] Left hand side matrix
 * @param rhs[in] Right hand side matrix
 * @return Result of matrices addition
 */
const Matrix operator+(const Matrix& lhs, const Matrix& rhs);

/**
 * Overload substract operator
 *
 * @param lhs[in] Left hand side matrix
 * @param rhs[in] Right hand side matrix
 * @return Result of matrices subtraction
 */
const Matrix operator-(const Matrix& lhs, const Matrix& rhs);

/**
 * Overload multiply operator
 *
 * @param lhs[in] Left hand side matrix
 * @param rhs[in] Right hand side matrix
 * @return Result of matrices multiplication
 */
const Matrix operator*(const Matrix& lhs, const Matrix& rhs);

}  // namespace edk

#endif  // EASYTRACK_MATRIX_H_
