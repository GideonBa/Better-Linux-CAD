#pragma once

#include "blcad/geometry/assembly_planar_joint_equation_builder.hpp"

namespace blcad::geometry::detail {

[[nodiscard]] PlanarJointResidualDescriptor
build_planar_joint_residual(const AssemblyFrameTargetDescriptor& frame_a,
                            const AssemblyFrameTargetDescriptor& frame_b, double translation_u_mm,
                            double translation_v_mm, double rotation_normal_deg) noexcept;

} // namespace blcad::geometry::detail
