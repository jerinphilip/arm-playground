#include "3rd-party/intgemm/intgemm/aligned.h"
#include "firefox_interface.h"
#include "matrix.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>

namespace {

template <class T>
std::ostream &operator<<(std::ostream &out,
                         const intgemm::AlignedVector<T> &v) {
  for (size_t i = 0; i < v.size(); i++) {
    if (i != 0)
      out << " ";
    const T *data = v.begin();
    std::cout << (int)data[i];
  }
  return out;
}

using namespace pg;

#define DEBUG_MATRIX(x)                                                        \
  do {                                                                         \
    if (std::getenv("ARM_PLAYGROUND_DEBUG")) {                                 \
      std::cout << #x << ": " << x << std::endl;                               \
    }                                                                          \
  } while (0)

#define forward(ns, fn)                                                        \
  template <class... Args> static void fn(Args... args) { ns::fn(args...); }

#define createStruct(ns)                                                       \
  struct _##ns {                                                               \
    forward(ns, int8PrepareA);                                                 \
    forward(ns, int8PrepareB);                                                 \
    forward(ns, int8PrepareBias);                                              \
    forward(ns, int8MultiplyAndAddBias);                                       \
  }

createStruct(Intgemm);
createStruct(Ruy);

#define STR(...) STR_(__VA_ARGS__)
#define STR_(...) #__VA_ARGS__

#pragma message "Intgemm: " STR(createStruct(Intgemm))

// Repeats path for lib with matrices A, B and bias. Final result goes into
// output, applied with an optional scale.
template <class Lib, class ElementType>
void MulABAddBias(Matrix<ElementType> &A, Matrix<ElementType> &B,
                  Matrix<ElementType> &bias, ElementType *output,
                  ElementType output_scale) {
  intgemm::AlignedVector<int8_t> mA_prepared(A.layout().num_elem()),
      mB_prepared(B.layout().num_elem());
  intgemm::AlignedVector<float> mBias_prepared(bias.layout().num_elem());

  DEBUG_MATRIX(A);
  DEBUG_MATRIX(B);
  DEBUG_MATRIX(bias);
  int8_t *A_prepared = mA_prepared.begin();
  int8_t *B_prepared = mB_prepared.begin();
  float *bias_prepared = mBias_prepared.begin();

  Lib::int8PrepareA(A.data(), A.scale(), A.zero_point(), A.nrows(), A.ncols(),
                    A_prepared);

  Lib::int8PrepareB(B.data(), B.scale(), B.zero_point(), B.nrows(), B.ncols(),
                    B_prepared);

  Lib::int8PrepareBias(B_prepared, A.scale(), A.zero_point(), B.scale(),
                       B.zero_point(), B.nrows(), B.ncols(), bias.data(),
                       bias_prepared);

  Lib::int8MultiplyAndAddBias(A_prepared, A.scale(), A.zero_point(), B_prepared,
                              B.scale(), B.zero_point(), bias_prepared,
                              output_scale, A.nrows(), A.ncols(), B.ncols(),
                              output);
}

TEST(EndToEnd, EndToEnd) {
  std::mt19937_64 gen64;
  gen64.seed(42);
  constexpr size_t DIM_MAX = 32;
  constexpr size_t DIM_MIN = 16;
  constexpr size_t MC_RUNS = 10000;
  for (size_t i = 0; i < MC_RUNS; i++) {
    std::uniform_int_distribution<> distribution(DIM_MIN, DIM_MAX);

    size_t M, N, P; // A = M x N matrix, B = N x P

    M = distribution(gen64);
    N = distribution(gen64);
    P = distribution(gen64);

    // Do some stuff to get stuff rounded to multiples of 8
    const size_t _WIDTH = 64;
    M = ((M / _WIDTH) + 1) * _WIDTH;
    N = ((N / _WIDTH) + 1) * _WIDTH;
    P = ((P / _WIDTH) + 1) * _WIDTH;
    // M = 1, N = 16, P = 8;
    // M = 32, N = 32, P = 32;

    Layout a_layout(M, N, Order::RowMajor);
    Layout b_layout(N, P, Order::RowMajor);
    Layout bias_layout(1, P, Order::RowMajor);

    auto A = make_random_matrix<float>(gen64, a_layout, -1.0f, 1.0f);
    auto B = make_random_matrix<float>(gen64, b_layout, -1.0f, 1.0f);
    auto bias = make_random_matrix<float>(gen64, bias_layout, -1.0f, 1.0f);

    float output_scale = 1.0f;
    Layout productLayout(M, P, Order::RowMajor);
    Matrix<float> intgemmProduct(productLayout);
    MulABAddBias<_Intgemm>(A, B, bias, intgemmProduct.data(), output_scale);

    Matrix<float> ruyProduct(productLayout);
    MulABAddBias<_Ruy>(A, B, bias, ruyProduct.data(), output_scale);
    float mse = MeanSquaredError(ruyProduct, intgemmProduct);
    DEBUG_MATRIX(ruyProduct);
    DEBUG_MATRIX(intgemmProduct);
    ASSERT_NEAR(mse, 0.0f, /*abs_error=*/1e-7);
  }
}

} // namespace
