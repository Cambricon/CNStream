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
#include <utility>
#include <vector>

#include "cnstream_logging.hpp"

namespace cnstream {

/**
 * @brief Matrix Base class
 */
template <typename Object>
class MatrixPrototype {
 public:
  using underlying_type = std::vector<Object>;

  /**
   * Constructor.
   *
   * @param rows[in] Number of rows in a 2D array.
   * @param cols[in] Number of columns in a 2D array.
   */
  MatrixPrototype(uint32_t rows, uint32_t cols) : arrays_(rows * cols), rows_(rows), cols_(cols) {}

  MatrixPrototype() = default;
  MatrixPrototype(const MatrixPrototype<Object>& m) = default;
  MatrixPrototype(MatrixPrototype<Object>&& m) = default;
  MatrixPrototype& operator=(const MatrixPrototype<Object>& m) = default;
  MatrixPrototype& operator=(MatrixPrototype<Object>&& m) = default;

  MatrixPrototype(const underlying_type& init_list, uint32_t rows, uint32_t cols)
      : arrays_(init_list), rows_(rows), cols_(cols) {
    if (arrays_.size() != rows_ * cols_) {
      LOGF(TRACK) << "matrix size mismatch with rows and cols";
    }
  }

  MatrixPrototype(underlying_type&& init_list, uint32_t rows, uint32_t cols)
      : arrays_(std::forward<underlying_type>(init_list)), rows_(rows), cols_(cols) {
    if (arrays_.size() != rows_ * cols_) {
      LOGF(TRACK) << "matrix size mismatch with rows and cols";
    }
  }

  /**
   * Resize matrix to specified new shape.
   *
   * @param rows[in] Number of rows in a 2D array.
   * @param cols[in] Number of columns in a 2D array.
   * @return None
   */
  void Resize(uint32_t rows, uint32_t cols);

  /**
   * @brief Fill all elements with given value
   *
   * @param ele value assigned to element
   */
  void Fill(const Object& element) {
    if (Empty()) return;

    arrays_.assign(Size(), element);
  }

  /**
   * Get number of elements from matrix.
   * @return Number of elements.
   */
  uint32_t Size() const {
    return cols_ * rows_;
  }

  /**
   * Get number of rows from matrix.
   * @return Number of rows.
   */
  uint32_t Rows() const { return rows_; }

  /**
   * Get number of columns from matrix.
   * @return Number of columns.
   */
  uint32_t Cols() const { return cols_; }

  /**
   * Query whether matrix is empty.
   * @return Returns true if the array has no elements.
   */
  bool Empty() const { return Size() == 0; }

  /**
   * Query whether matrix is square.
   * @return Returns true if not empty and number of columns equal to number of rows.
   */
  bool Square() const { return (!(Empty()) && rows_ == cols_); }

  /**
   * Access vector of const elements at specified row number.
   * @param row[in] Accessed row number
   * @param col[in] Accessed col number
   * @return Specified row elements.
   */
  const Object& operator()(uint32_t row, uint32_t col) const { return arrays_[row * cols_ + col]; }

  /**
   * Access vector of elements at specified row number.
   * @param row[in] Accessed row number
   * @param col[in] Accessed col number
   * @return Specified row elements.
   */
  Object& operator()(uint32_t row, uint32_t col) { return arrays_[row * cols_ + col]; }

 protected:
  const Object& operator[](uint32_t idx) const { return arrays_[idx]; }
  Object& operator[](uint32_t idx) { return arrays_[idx]; }

  std::vector<Object> arrays_;
  uint32_t rows_{0};
  uint32_t cols_{0};
};

template <typename Object>
void MatrixPrototype<Object>::Resize(uint32_t rows, uint32_t cols) {
  if (rows == rows_ && cols == cols_) {
    return;
  }
  rows_ = rows;
  cols_ = cols;
  arrays_.resize(Size());
}

/**
 * @brief Matrix specializition for float
 */
class Matrix : public MatrixPrototype<float> {
 public:
  using underlying_type = MatrixPrototype<float>::underlying_type;
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
  Matrix(uint32_t rows, uint32_t cols) : MatrixPrototype<float>(rows, cols) {
    arrays_.assign(Size(), 0.f);
  }

  Matrix(const underlying_type& init_list, uint32_t rows, uint32_t cols)
      : MatrixPrototype<float>(init_list, rows, cols) {
  }

  Matrix(underlying_type&& init_list, uint32_t rows, uint32_t cols)
      : MatrixPrototype<float>(std::forward<underlying_type>(init_list), rows, cols) {
  }

  Matrix(const Matrix& m) = default;
  Matrix& operator=(const Matrix& m) = default;
  Matrix(Matrix&& m) = default;
  Matrix& operator=(Matrix&& m) = default;
  /**
   * Destructor.
   * @brief Release resourse.
   */
  ~Matrix() = default;

  /**
   * Assignment operator with initializer_list.
   * @param init_list[in] Arrays to be assigned to matrix.
   */
  Matrix& operator=(const underlying_type& init_list) {
    if (init_list.size() != Size()) {
      LOGF(TRACK) << "matrix size mismatch with rows and cols";
    }
    arrays_ = init_list;
    return *this;
  }

  Matrix& operator=(underlying_type&& init_list) {
    if (init_list.size() != Size()) {
      LOGE(TRACK) << "list_size: " << init_list.size() << " | matrix size: " << Size();
      LOGF(TRACK) << "matrix size mismatch with rows and cols";
    }
    arrays_ = std::move(init_list);
    return *this;
  }

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
   * Solve Matrix inversion.
   * @return Return inverse of matrix.
   * @attention Solve inverse of non-zero singular matrix may cause an error
   */
  Matrix Inv() const;

  /**
   * Solve Matrix transposition.
   * @return Return transposed matrix
   */
  Matrix Trans() const;

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

}  // namespace cnstream

#endif  // EASYTRACK_MATRIX_H_
