#pragma once

#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

namespace blcad::geometry::detail {

[[nodiscard]] RevoluteJointResidualDescriptor build_revolute_joint_residual(
    const AssemblySpaceAxisDescriptor& axis_a, const AssemblySpacePlanarDescriptor& seat_a,
    const AssemblySpaceAxisDescriptor& axis_b, const AssemblySpacePlanarDescriptor& seat_b,
    double requested_coordinate_deg) noexcept;

} // namespace blcad::geometry::detail
