from pathlib import Path

path = Path("tests/geometry/assembly_generic_relationship_equation_tests.cpp")
text = path.read_text(encoding="utf-8")

old_target = '''  return AssemblyResolvedGeometricTarget{
      AssemblyLocalGeometricTargetEndpointIdentity{ComponentInstanceId("component.synthetic"),
                                                   semantic_reference},
      kind,
      std::move(metadata),
      std::move(descriptor),
      assembly_geometric_target_capabilities(kind),
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      {}};
'''
new_target = '''  return AssemblyResolvedGeometricTarget{
      AssemblyHierarchyGeometricTargetEndpointIdentity{
          {}, ComponentInstanceId("component.synthetic"), semantic_reference},
      kind,
      std::move(metadata),
      std::move(descriptor),
      assembly_geometric_target_capabilities(kind),
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      {identity_rigid_transform()}};
'''
if old_target not in text:
    raise RuntimeError("synthetic resolved target identity marker not found")
text = text.replace(old_target, new_target, 1)

old_axis = '''  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
                          AssemblyCylindricalSurfaceTargetDescriptor{origin, direction, 5.0},
                          reference);
'''
new_axis = '''  return synthetic_target(AssemblyGeometricTargetSourceKind::DatumAxis,
                          AssemblyAxisTargetDescriptor{origin, direction}, reference);
'''
if old_axis not in text:
    raise RuntimeError("synthetic axis target marker not found")
text = text.replace(old_axis, new_axis, 1)

old_plane = '''  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
                          AssemblyPlanarTargetDescriptor{origin, x_axis, y_axis, normal}, reference);
'''
new_plane = '''  return synthetic_target(AssemblyGeometricTargetSourceKind::DatumPlane,
                          AssemblyPlanarTargetDescriptor{origin, x_axis, y_axis, normal}, reference);
'''
if old_plane not in text:
    raise RuntimeError("synthetic plane target marker not found")
text = text.replace(old_plane, new_plane, 1)

path.write_text(text, encoding="utf-8")
print("Block 39 synthetic resolved targets normalized")
