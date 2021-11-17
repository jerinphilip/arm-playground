#pragma once
#include "3rd-party/intgemm/intgemm/aligned.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

namespace pg {

enum class Order { RowMajor, ColMajor };

class Layout {
public:
  Layout(size_t rows, size_t cols, Order order)
      : rows_(rows), cols_(cols), order_(order) {}

  size_t rows() const { return rows_; }
  size_t cols() const { return cols_; }
  Order order() const { return order_; }
  inline size_t num_elem() const { return rows_ * cols_; }
  inline size_t position(size_t i, size_t j) const {
    if (order_ == Order::RowMajor) {
      return i * cols_ + j;
    } else {
      return j * rows_ + i;
    }
  }

  Layout transpose() { return Layout(cols_, rows_, order_); }

private:
  size_t rows_;
  size_t cols_;
  Order order_;
};

namespace utils {

template <class ElementType>
void printMatrix(std::ostream &out, const ElementType *data,
                 const Layout &layout) {
  for (size_t i = 0; i < layout.rows(); i++) {
    for (size_t j = 0; j < layout.cols(); j++) {
      if (j != 0) {
        out << " ";
      }
      out << data[layout.position(i, j)];
    }
    out << "\n";
  }
}
} // namespace utils

template <class ElementType> class Matrix {
public:
  using iterator = ElementType *;
  using const_iterator = const ElementType *;
  Matrix(const Layout &layout) : layout_(layout), matrix_(layout.num_elem()) {}

  const Layout &layout() const { return layout_; }
  size_t nrows() const { return layout_.rows(); }
  size_t ncols() const { return layout_.cols(); }

  ElementType *data() { return matrix_.begin(); }

  iterator begin() { return data(); }
  iterator end() { return begin() + layout_.num_elem(); }

  const_iterator cbegin() const { return matrix_.begin(); }
  const_iterator cend() const { return matrix_.begin() + layout_.num_elem(); }

  friend std::ostream &operator<<(std::ostream &out, Matrix &matrix) {
    utils::printMatrix(out, matrix.data(), matrix.layout_);
    return out;
  }

  ElementType &at(size_t i, size_t j) {
    return matrix_[layout_.position(i, j)];
  }

  const ElementType &at(size_t i, size_t j) const {
    return matrix_[layout_.position(i, j)];
  }

  float zero_point() const { return 0.0f; };

  float scale() const {
    auto maxAbs = [](const float &a, const float &b) {
      return std::max(std::abs(a), std::abs(b));
    };

    ElementType maxAbsolute = std::accumulate(cbegin(), cend(), 0, maxAbs);
    return 127.0f / static_cast<float>(maxAbsolute);
  };

private:
  const Layout layout_;
  intgemm::AlignedVector<ElementType> matrix_;
};

template <class ElementType>
Matrix<ElementType>
make_random_matrix(std::mt19937_64 &gen64, const Layout &layout,
                   const ElementType minVal, const ElementType maxVal) {
  std::cerr << "Not implemented. Specialize for a type" << std::endl;
  std::abort();
}

template <>
Matrix<int8_t>
make_random_matrix<int8_t>(std::mt19937_64 &gen64, const Layout &layout,
                           const int8_t minVal, const int8_t maxVal) {
  Matrix<int8_t> matrix(layout);
  std::uniform_int_distribution<> int8_distribution(minVal, maxVal);
  std::generate(matrix.begin(), matrix.end(), [&gen64, &int8_distribution]() {
    return int8_distribution(gen64);
  });
  return matrix;
}

template <>
Matrix<float>
make_random_matrix<float>(std::mt19937_64 &gen64, const Layout &layout,
                          const float minVal, const float maxVal) {
  std::uniform_real_distribution<> real_distribution(minVal, maxVal);
  Matrix<float> matrix(layout);
  std::generate(matrix.begin(), matrix.end(), [&gen64, &real_distribution]() {
    return real_distribution(gen64);
  });
  return matrix;
}

Matrix<float> make_random_matrix_but_int_values(std::mt19937_64 &gen64,
                                                const Layout &layout,
                                                const int8_t minVal,
                                                const int8_t maxVal) {
  std::uniform_int_distribution<> int8_distribution(minVal, maxVal);
  Matrix<float> matrix(layout);
  std::generate(matrix.begin(), matrix.end(), [&gen64, &int8_distribution]() {
    return int8_distribution(gen64);
  });
  return matrix;
}

template <class ElementType>
float MeanSquaredError(const Matrix<ElementType> &a,
                       const Matrix<ElementType> &b) {
  assert(a.layout().rows() == b.layout().rows() &&
         a.layout().cols() == b.layout().cols());
  float mse = 0.0f;
  for (size_t i = 0; i < a.layout().rows(); i++) {
    for (size_t j = 0; j < a.layout().cols(); j++) {
      float diff = (a.at(i, j) - b.at(i, j));
      mse += diff * diff;
    }
  }
  return mse;
}

} // namespace pg
