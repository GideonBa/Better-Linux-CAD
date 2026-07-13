#pragma once

#include "blcad/geometry/assembly_prismatic_joint_equation_builder.hpp"

namespace blcad::geometry::detail {

[[nodiscard]] PrismaticJointResidualDescriptor
build_prismatic_joint_residual(const AssemblyFrameTargetDescriptor& frame_a,
                               const AssemblyFrameTargetDescriptor& frame_b,
                               double requested_translation_mm) noexcept;

} // namespace blcad::geometry::detail
