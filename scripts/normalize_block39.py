from pathlib import Path

path = Path("src/geometry/assembly_generic_relationship_equation_builder.cpp")
text = path.read_text(encoding="utf-8")
start = text.index("[[nodiscard]] AssemblyGeometricTargetDescriptor\nevaluate_descriptor(")
end = text.index("[[nodiscard]] Result<Point3> selected_point", start)
replacement = r'''[[nodiscard]] Point3 evaluate_point_to_root(
    const AssemblyResolvedGeometricTarget& target, const Point3& point) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return point;
  }
  const AssemblyHierarchyTransformEvaluator evaluator;
  return evaluator.evaluate_point(target.transforms_inner_to_outer, point);
}

[[nodiscard]] Vector3 evaluate_vector_to_root(
    const AssemblyResolvedGeometricTarget& target, const Vector3& vector) {
  if (target.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly) {
    return vector;
  }
  const AssemblyHierarchyTransformEvaluator evaluator;
  return evaluator.evaluate_vector(target.transforms_inner_to_outer, vector);
}

'''
text = text[:start] + replacement + text[end:]

text = text.replace(
    "  return Result<Point3>::success(point.value().point);",
    "  return Result<Point3>::success(evaluate_point_to_root(target, point.value().point));",
)
text = text.replace(
    "  return project_line(target);",
    "  auto line = project_line(target);\n"
    "  if (line.has_error()) {\n"
    "    return Result<AssemblyLineTargetDescriptor>::failure(line.error());\n"
    "  }\n"
    "  return Result<AssemblyLineTargetDescriptor>::success(AssemblyLineTargetDescriptor{\n"
    "      evaluate_point_to_root(target, line.value().origin),\n"
    "      evaluate_vector_to_root(target, line.value().direction)});",
    1,
)
text = text.replace(
    "  return project_plane(target);",
    "  auto plane = project_plane(target);\n"
    "  if (plane.has_error()) {\n"
    "    return Result<AssemblyPlanarTargetDescriptor>::failure(plane.error());\n"
    "  }\n"
    "  return Result<AssemblyPlanarTargetDescriptor>::success(AssemblyPlanarTargetDescriptor{\n"
    "      evaluate_point_to_root(target, plane.value().origin),\n"
    "      evaluate_vector_to_root(target, plane.value().x_axis),\n"
    "      evaluate_vector_to_root(target, plane.value().y_axis),\n"
    "      evaluate_vector_to_root(target, plane.value().normal)});",
    1,
)
text = text.replace(
    "    return Result<Vector3>::success(line.value().direction);",
    "    return Result<Vector3>::success(\n"
    "        evaluate_vector_to_root(target, line.value().direction));",
)
text = text.replace(
    "    return Result<Vector3>::success(axis.value().direction);",
    "    return Result<Vector3>::success(\n"
    "        evaluate_vector_to_root(target, axis.value().direction));",
)
text = text.replace(
    "    return Result<Vector3>::success(plane.value().normal);",
    "    return Result<Vector3>::success(\n"
    "        evaluate_vector_to_root(target, plane.value().normal));",
)

old = r'''  auto normalized_a = normalize_to_root_space(target_a);
  if (normalized_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(normalized_a.error());
  }
  auto normalized_b = normalize_to_root_space(target_b);
  if (normalized_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(normalized_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(type, normalized_a.value(), normalized_b.value());
'''
new = r'''  auto valid_a = validate_resolved_geometric_target(target_a);
  if (valid_a.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(valid_a.error());
  }
  auto valid_b = validate_resolved_geometric_target(target_b);
  if (valid_b.has_error()) {
    return Result<AssemblyGenericRelationshipEquationDescriptor>::failure(valid_b.error());
  }

  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto compatibility = compatibility_resolver.resolve(type, target_a, target_b);
'''
if old not in text:
    raise RuntimeError("generic relationship normalized-target seed not found")
text = text.replace(old, new)
text = text.replace("normalized_a.value()", "target_a")
text = text.replace("normalized_b.value()", "target_b")
path.write_text(text, encoding="utf-8")

test = Path("tests/geometry/assembly_generic_relationship_equation_tests.cpp")
content = test.read_text(encoding="utf-8")
content = content.replace("#include <initializer_list>", "#include <cmath>\n#include <initializer_list>")
for raw in (
    "Vector3{3.0, 4.0, 0.0}",
    "Vector3{0.0, 0.0, -2.0}",
):
    content = content.replace(f"== {raw}", f"== ({raw})")
content = content.replace(
    '''CHECK(graph.value().solve_groups().front().authorities[0] ==
        ComponentTransformAuthority{DocumentId("assembly.child"),
                                    ComponentInstanceId("component.child")});''',
    '''const ComponentTransformAuthority child_authority{
      DocumentId("assembly.child"), ComponentInstanceId("component.child")};
  CHECK(graph.value().solve_groups().front().authorities[0] == child_authority);''',
)
content = content.replace(
    '''CHECK(graph.value().solve_groups().front().authorities[1] ==
        ComponentTransformAuthority{DocumentId("assembly.root"),
                                    ComponentInstanceId("component.root")});''',
    '''const ComponentTransformAuthority root_authority{
      DocumentId("assembly.root"), ComponentInstanceId("component.root")};
  CHECK(graph.value().solve_groups().front().authorities[1] == root_authority);''',
)
test.write_text(content, encoding="utf-8")

doc = Path("docs/assembly-generic-relationship-equations-mvp5.md")
content = doc.read_text(encoding="utf-8")
old_doc = r'''## Shared target normalization

`AssemblyGenericRelationshipEquationBuilder` consumes typed `AssemblyResolvedGeometricTarget` values.

A component-local target is normalized as:

```text
persistent semantic endpoint
  -> typed component-local descriptor
  -> exact transforms_inner_to_outer
  -> AssemblyHierarchyTransformEvaluator
  -> root-assembly descriptor
```

An already root-space hierarchy target is reused as-is after typed target validation.

The normalized target retains:

```text
exact endpoint identity
source kind
source metadata
capability vector
exact transform chain
```

Only the descriptor coordinate space changes from `ComponentLocal` to `RootAssembly` for local equation evaluation.

No Euler-angle recomposition, OCCT topology identity, or persisted Geometry product is introduced.
'''
new_doc = r'''## Shared target projection into equation space

`AssemblyGenericRelationshipEquationBuilder` consumes typed `AssemblyResolvedGeometricTarget` values and validates them without changing endpoint or coordinate-space identity.

For a component-local target the selected capability projection is evaluated as:

```text
persistent semantic endpoint
  -> validated typed component-local target
  -> selected Point/Line/Axis/Plane projection
  -> exact transforms_inner_to_outer
  -> AssemblyHierarchyTransformEvaluator
  -> root-assembly equation value
```

An already root-space hierarchy target reuses its selected projected value directly after typed target validation.

The authoritative resolved target remains unchanged and retains:

```text
exact endpoint identity
original ComponentLocal or RootAssembly coordinate space
source kind
source metadata
capability vector
exact transform chain
```

Equation construction transforms only the selected projected point, origin, direction, or normal needed by the residual. It does not relabel a local endpoint as a hierarchy endpoint and does not mutate the resolved target record.

No Euler-angle recomposition, OCCT topology identity, or persisted Geometry product is introduced.
'''
if old_doc not in content:
    raise RuntimeError("generic relationship documentation projection seed not found")
doc.write_text(content.replace(old_doc, new_doc), encoding="utf-8")

print("Block 39 normalization applied")
