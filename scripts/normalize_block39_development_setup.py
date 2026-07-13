from pathlib import Path

path = Path("docs/development-setup.md")
text = path.read_text(encoding="utf-8")

compatibility_tag = './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"\n'
extra_tags = (
    compatibility_tag
    + './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"\n'
    + './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"\n'
    + './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"\n'
    + './build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"\n'
)
if compatibility_tag not in text:
    raise RuntimeError("development setup compatibility tag marker not found")
text = text.replace(compatibility_tag, extra_tags, 1)

old_bullet = "- Block-38 generic relationships excluded from current solve/motion graph participation until Block 39;"
new_bullet = (
    "- Block-39 Coincident/Parallel/Perpendicular compatibility, residuals, graph participation, "
    "local/cross solving, freshness/application, and Jacobian-rank diagnostics;\n"
    "- non-planar Line/Axis Angle execution through shared directional equations;"
)
if old_bullet not in text:
    raise RuntimeError("development setup Block-38 bullet marker not found")
text = text.replace(old_bullet, new_bullet, 1)

old_entries = (
    "- `docs/assembly-generic-relationship-intent-mvp5.md`: Block-38 generic relationship intent/JSON contract\n"
    "- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–38 and planned Blocks 39–47"
)
new_entries = (
    "- `docs/assembly-generic-relationship-intent-mvp5.md`: Block-38 generic relationship intent/JSON contract\n"
    "- `docs/assembly-generic-relationship-equations-mvp5.md`: Block-39 generic relationship equation/solve contract\n"
    "- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–39 and planned Blocks 40–47"
)
if old_entries not in text:
    raise RuntimeError("development setup documentation entry marker not found")
text = text.replace(old_entries, new_entries, 1)

format_start = text.index("For Block 38 production/test files:\n")
format_end = text.index("\n## Clean generated files", format_start)
format_block = '''For Block 39 production/test files:

```bash
clang-format -i \\
  include/blcad/geometry/assembly_generic_relationship_equation_builder.hpp \\
  include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp \\
  src/core/assembly_constraint_graph_participation.hpp \\
  src/geometry/assembly_generic_relationship_equation_builder.cpp \\
  src/geometry/assembly_target_compatibility.cpp \\
  src/geometry/assembly_constraint_numeric_system.hpp \\
  src/geometry/assembly_constraint_numeric_system.cpp \\
  src/geometry/assembly_hierarchy_constraint_equation_builder.cpp \\
  tests/geometry/assembly_generic_relationship_equation_tests.cpp
```
'''
text = text[:format_start] + format_block + text[format_end:]

text = text.replace("Blocks 23–38 are implemented.", "Blocks 23–39 are implemented.", 1)
old_boundary = (
    "Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target "
    "compatibility selection for existing relationship types. Block 38 adds persistent local/Project-level "
    "Coincident, Parallel, and Perpendicular intent plus lowercase JSON spellings while keeping the three "
    "families out of current solve/motion graphs until Block 39."
)
new_boundary = (
    "Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target "
    "compatibility selection. Block 38 adds persistent local/Project-level Coincident, Parallel, and "
    "Perpendicular intent plus lowercase JSON spellings. Block 39 adds their capability-driven equations, "
    "enables shared graph participation and numeric execution, and closes non-planar Line/Axis Angle execution."
)
if old_boundary not in text:
    raise RuntimeError("development setup boundary marker not found")
text = text.replace(old_boundary, new_boundary, 1)

focused_start = text.index("Focused Blocks 36–38 tests:\n")
focused_block = '''Focused Blocks 36–39 tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"
```

The immediate next step is Block 40: joint target compatibility and the oriented `Frame` contract. Block 39 generic relationship equations and shared solve integration are implemented without new execution infrastructure or persistence. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
'''
text = text[:focused_start] + focused_block

path.write_text(text, encoding="utf-8")
print("Block 39 development setup normalization applied")
