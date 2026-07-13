from pathlib import Path

path = Path("src/geometry/assembly_generic_relationship_equation_builder.cpp")
text = path.read_text(encoding="utf-8")

start = text.index("  if (type == AssemblyConstraintType::Coincident) {\n")
end = text.index(
    '  return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(\n'
    '      relationship.value(), "generic relationship compatibility selected an unsupported pair"));\n',
    start,
)
end += len(
    '  return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(\n'
    '      relationship.value(), "generic relationship compatibility selected an unsupported pair"));\n'
)

replacement = r'''  if (type == AssemblyConstraintType::Coincident) {
    if (capability_a == AssemblyGeometricTargetCapability::Point &&
        capability_b == AssemblyGeometricTargetCapability::Point) {
      auto point_a = selected_point(target_a, capability_a);
      auto point_b = selected_point(target_b, capability_b);
      if (point_a.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point_a.error());
      }
      if (point_b.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point_b.error());
      }
      AssemblyGenericRelationshipEquationDescriptor descriptor{
          relationship,
          type,
          compatibility.value(),
          equation_target_a,
          equation_target_b,
          AssemblyGenericRelationshipResidualDescriptor{
              CoincidentPointPointResidualDescriptor{
                  difference(point_b.value(), point_a.value())}}};
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
    }

    const bool point_line = capability_a == AssemblyGeometricTargetCapability::Point &&
                            capability_b == AssemblyGeometricTargetCapability::Line;
    const bool line_point = capability_a == AssemblyGeometricTargetCapability::Line &&
                            capability_b == AssemblyGeometricTargetCapability::Point;
    if (point_line || line_point) {
      const AssemblyGenericRelationshipTargetRole point_role =
          point_line ? AssemblyGenericRelationshipTargetRole::TargetA
                     : AssemblyGenericRelationshipTargetRole::TargetB;
      auto point = point_line ? selected_point(target_a, capability_a)
                              : selected_point(target_b, capability_b);
      auto line = point_line ? selected_line(target_b, capability_b)
                             : selected_line(target_a, capability_a);
      if (point.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point.error());
      }
      if (line.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(line.error());
      }
      const Vector3 point_from_line = difference(point.value(), line.value().origin);
      AssemblyGenericRelationshipEquationDescriptor descriptor{
          relationship,
          type,
          compatibility.value(),
          equation_target_a,
          equation_target_b,
          AssemblyGenericRelationshipResidualDescriptor{
              CoincidentPointLineResidualDescriptor{
                  point_role, cross(point_from_line, line.value().direction)}}};
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
    }

    const bool point_plane = capability_a == AssemblyGeometricTargetCapability::Point &&
                             capability_b == AssemblyGeometricTargetCapability::Plane;
    const bool plane_point = capability_a == AssemblyGeometricTargetCapability::Plane &&
                             capability_b == AssemblyGeometricTargetCapability::Point;
    if (point_plane || plane_point) {
      const AssemblyGenericRelationshipTargetRole point_role =
          point_plane ? AssemblyGenericRelationshipTargetRole::TargetA
                      : AssemblyGenericRelationshipTargetRole::TargetB;
      auto point = point_plane ? selected_point(target_a, capability_a)
                               : selected_point(target_b, capability_b);
      auto plane = point_plane ? selected_plane(target_b, capability_b)
                               : selected_plane(target_a, capability_a);
      if (point.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(point.error());
      }
      if (plane.has_error()) {
        return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(plane.error());
      }
      const double signed_distance =
          dot(difference(point.value(), plane.value().origin), plane.value().normal);
      AssemblyGenericRelationshipEquationDescriptor descriptor{
          relationship,
          type,
          compatibility.value(),
          equation_target_a,
          equation_target_b,
          AssemblyGenericRelationshipResidualDescriptor{
              CoincidentPointPlaneResidualDescriptor{point_role, signed_distance}}};
      return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
    }
  }

  auto direction_a = selected_direction(target_a, capability_a);
  if (direction_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(direction_a.error());
  }
  auto direction_b = selected_direction(target_b, capability_b);
  if (direction_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(direction_b.error());
  }

  if (type == AssemblyConstraintType::Parallel) {
    AssemblyGenericRelationshipEquationDescriptor descriptor{
        relationship,
        type,
        compatibility.value(),
        equation_target_a,
        equation_target_b,
        AssemblyGenericRelationshipResidualDescriptor{
            ParallelDirectionResidualDescriptor{
                cross(direction_a.value(), direction_b.value())}}};
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
  }
  if (type == AssemblyConstraintType::Perpendicular) {
    AssemblyGenericRelationshipEquationDescriptor descriptor{
        relationship,
        type,
        compatibility.value(),
        equation_target_a,
        equation_target_b,
        AssemblyGenericRelationshipResidualDescriptor{
            PerpendicularDirectionResidualDescriptor{
                dot(direction_a.value(), direction_b.value())}}};
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
  }
  if (type == AssemblyConstraintType::Angle) {
    if (!angle.has_value() || angle->kind() != QuantityKind::AngleDeg) {
      return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
          relationship.value(),
          "directional Angle equation construction requires an angle value in degrees"));
    }
    const double target_angle_deg = angle->degrees();
    const double target_cosine =
        std::cos(target_angle_deg * (std::numbers::pi_v<double> / 180.0));
    const double direction_dot = dot(direction_a.value(), direction_b.value());
    AssemblyGenericRelationshipEquationDescriptor descriptor{
        relationship,
        type,
        compatibility.value(),
        equation_target_a,
        equation_target_b,
        AssemblyGenericRelationshipResidualDescriptor{DirectionalAngleResidualDescriptor{
            target_angle_deg, direction_dot, direction_dot - target_cosine}}};
    return Result<AssemblyGenericRelationshipEquationDescriptor>::success(std::move(descriptor));
  }

  return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(validation_error(
      relationship.value(), "generic relationship compatibility selected an unsupported pair"));
'''

path.write_text(text[:start] + replacement + text[end:], encoding="utf-8")
print("Block 39 generic relationship return construction normalized")
