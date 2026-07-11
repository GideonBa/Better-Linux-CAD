#pragma once

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <vector>

namespace blcad::geometry::detail {

// One flattened visible-active leaf occurrence together with its posed OCCT
// shape. Derived data only; nothing here is persisted.
struct PosedLeafShape {
  AssemblyLeafOccurrenceDescriptor leaf;
  TopoDS_Shape shape;
};

struct PosedLeafShapesBuild {
  std::vector<PosedLeafShape> leaves;
  std::size_t recomputed_part_count = 0U;
};

// Shared posed-geometry boundary for assembly-level consumers (posed STEP
// export, interference analysis). Validates the project hierarchy, flattens
// visible-active leaves, recomputes each unique referenced part document
// exactly once, and poses every leaf shape through its exact inner-to-outer
// authored transform chain with X-then-Y-then-Z semantics per level.
class AssemblyPosedLeafShapeBuilder {
public:
  [[nodiscard]] Result<PosedLeafShapesBuild> build(const Project& project) const;
};

[[nodiscard]] Result<PosedLeafShapesBuild> build_posed_leaf_shapes(const Project& project);

// Stable leaf occurrence identity key: assembly id, subassembly path, and
// component id joined with '/'. Never an OCCT topology id.
[[nodiscard]] std::string leaf_occurrence_key(const AssemblyLeafOccurrenceDescriptor& leaf);

} // namespace blcad::geometry::detail
