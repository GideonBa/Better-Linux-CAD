from pathlib import Path

path = Path("src/geometry/assembly_constraint_numeric_system.cpp")
text = path.read_text(encoding="utf-8")
marker = '''Result<std::size_t> append_scaled_residuals(
    const AssemblyHierarchyConstraintResidualDescriptor& residual,
'''
insert = '''Result<std::size_t> append_scaled_residuals(
    const AssemblyGenericRelationshipResidualDescriptor& residual,
    double length_residual_scale_mm, NumericVector& residuals) {
  return append_scaled_residual(residual, length_residual_scale_mm, residuals);
}

'''
if marker not in text:
    raise RuntimeError("hierarchy residual wrapper marker not found")
if "const AssemblyGenericRelationshipResidualDescriptor& residual" in text:
    raise RuntimeError("generic relationship residual wrapper already present")
path.write_text(text.replace(marker, insert + marker, 1), encoding="utf-8")

print("Block 39 numeric wrapper normalization applied")
