#include "assembly_matrix_rank.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace blcad::geometry::detail {
namespace {

[[nodiscard]] Error internal_error(std::string object_id, std::string message) {
  return Error::internal(std::move(object_id), std::move(message));
}

} // namespace

Result<AssemblyMatrixRankResult> compute_assembly_matrix_rank(
    NumericMatrix matrix, std::size_t expected_column_count, double absolute_tolerance,
    double relative_tolerance, std::string_view object_id) {
  double maximum_abs_entry = 0.0;
  for (const auto& row : matrix) {
    if (row.size() != expected_column_count) {
      return Result<AssemblyMatrixRankResult>::failure(internal_error(
          std::string(object_id), "assembly diagnostics Jacobian row width does not match variable count"));
    }
    for (double entry : row) {
      if (!std::isfinite(entry)) {
        return Result<AssemblyMatrixRankResult>::failure(internal_error(
            std::string(object_id), "assembly diagnostics Jacobian must be finite"));
      }
      maximum_abs_entry = std::max(maximum_abs_entry, std::abs(entry));
    }
  }

  const double pivot_threshold =
      std::max(absolute_tolerance, relative_tolerance * maximum_abs_entry);
  std::size_t rank = 0U;
  std::size_t column = 0U;

  while (rank < matrix.size() && column < expected_column_count) {
    std::size_t pivot_row = rank;
    double pivot_magnitude = std::abs(matrix[rank][column]);
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double magnitude = std::abs(matrix[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }

    if (pivot_magnitude <= pivot_threshold) {
      ++column;
      continue;
    }

    if (pivot_row != rank) {
      std::swap(matrix[pivot_row], matrix[rank]);
    }

    const double pivot = matrix[rank][column];
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double factor = matrix[row][column] / pivot;
      matrix[row][column] = 0.0;
      for (std::size_t entry = column + 1U; entry < expected_column_count; ++entry) {
        matrix[row][entry] -= factor * matrix[rank][entry];
      }
    }

    ++rank;
    ++column;
  }

  return Result<AssemblyMatrixRankResult>::success(
      AssemblyMatrixRankResult{rank, maximum_abs_entry, pivot_threshold});
}

} // namespace blcad::geometry::detail
