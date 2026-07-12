#pragma once

#include "assembly_constraint_numeric_system.hpp"

#include "blcad/core/result.hpp"

#include <cstddef>
#include <string_view>

namespace blcad::geometry::detail {

struct AssemblyMatrixRankResult {
  std::size_t rank = 0U;
  double maximum_abs_entry = 0.0;
  double pivot_threshold = 0.0;
};

[[nodiscard]] Result<AssemblyMatrixRankResult> compute_assembly_matrix_rank(
    NumericMatrix matrix,
    std::size_t expected_column_count,
    double absolute_tolerance,
    double relative_tolerance,
    std::string_view object_id);

} // namespace blcad::geometry::detail
