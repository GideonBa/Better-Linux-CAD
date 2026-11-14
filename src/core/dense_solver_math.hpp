#pragma once
#include "blcad/core/sketch_constraint_solver.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
namespace blcad::detail {
using NumericVector = std::vector<double>;
using NumericMatrix = std::vector<NumericVector>;
inline constexpr double kPivotTolerance = 1.0e-14;
[[nodiscard]] inline bool solve_linear_system(NumericMatrix matrix, NumericVector right_hand_side,
                                              NumericVector& solution) {
  const std::size_t dimension = right_hand_side.size();
  if (matrix.size() != dimension)
    return false;
  for (const auto& row : matrix)
    if (row.size() != dimension)
      return false;

  for (std::size_t column = 0U; column < dimension; ++column) {
    std::size_t pivot_row = column;
    double pivot_magnitude = std::abs(matrix[column][column]);
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double magnitude = std::abs(matrix[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }
    if (!std::isfinite(pivot_magnitude) || pivot_magnitude <= kPivotTolerance)
      return false;
    if (pivot_row != column) {
      std::swap(matrix[pivot_row], matrix[column]);
      std::swap(right_hand_side[pivot_row], right_hand_side[column]);
    }
    const double pivot = matrix[column][column];
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double factor = matrix[row][column] / pivot;
      if (!std::isfinite(factor))
        return false;
      for (std::size_t entry = column; entry < dimension; ++entry)
        matrix[row][entry] -= factor * matrix[column][entry];
      right_hand_side[row] -= factor * right_hand_side[column];
    }
  }

  solution.assign(dimension, 0.0);
  for (std::size_t reverse = 0U; reverse < dimension; ++reverse) {
    const std::size_t row = dimension - 1U - reverse;
    double value = right_hand_side[row];
    for (std::size_t column = row + 1U; column < dimension; ++column)
      value -= matrix[row][column] * solution[column];
    const double pivot = matrix[row][row];
    if (!std::isfinite(pivot) || std::abs(pivot) <= kPivotTolerance)
      return false;
    solution[row] = value / pivot;
    if (!std::isfinite(solution[row]))
      return false;
  }
  return true;
}

[[nodiscard]] inline Result<std::size_t> matrix_rank(NumericMatrix matrix, std::size_t column_count,
                                                     const SketchSolverOptions& options) {
  double maximum = 0.0;
  for (const auto& row : matrix) {
    if (row.size() != column_count)
      return Result<std::size_t>::failure(Error::internal(
          "sketch.solver", "solver Jacobian row width does not match variable count"));
    for (double value : row) {
      if (!std::isfinite(value))
        return Result<std::size_t>::failure(
            Error::internal("sketch.solver", "solver Jacobian entries must be finite"));
      maximum = std::max(maximum, std::abs(value));
    }
  }
  const double threshold =
      std::max(options.rank_absolute_tolerance, options.rank_relative_tolerance * maximum);
  std::size_t rank = 0U;
  std::size_t column = 0U;
  while (rank < matrix.size() && column < column_count) {
    std::size_t pivot_row = rank;
    double pivot_magnitude = std::abs(matrix[rank][column]);
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double candidate = std::abs(matrix[row][column]);
      if (candidate > pivot_magnitude) {
        pivot_magnitude = candidate;
        pivot_row = row;
      }
    }
    if (pivot_magnitude <= threshold) {
      ++column;
      continue;
    }
    if (pivot_row != rank)
      std::swap(matrix[pivot_row], matrix[rank]);
    const double pivot = matrix[rank][column];
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double factor = matrix[row][column] / pivot;
      matrix[row][column] = 0.0;
      for (std::size_t entry = column + 1U; entry < column_count; ++entry)
        matrix[row][entry] -= factor * matrix[rank][entry];
    }
    ++rank;
    ++column;
  }
  return Result<std::size_t>::success(rank);
}

} // namespace blcad::detail
