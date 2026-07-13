#pragma once

#include "blcad/geometry/assembly_cylindrical_joint_equation_builder.hpp"

namespace blcad::geometry::detail {

[[nodiscard]] CylindricalJointResidualDescriptor build_cylindrical_joint_residual(
    const AssemblyFrameTargetDescriptor& frame_a, const AssemblyFrameTargetDescriptor& frame_b,
    double requested_translation_mm, double requested_rotation_deg) noexcept;

} // namespace blcad::geometry::detail
