#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <vector>

namespace blcad::geometry::detail {

struct AssemblyPartShapeDefinition {
  DocumentId part_document;
  TopoDS_Shape shape;
};

struct AssemblyPartShapeDefinitionsBuild {
  std::vector<AssemblyPartShapeDefinition> definitions;
  std::size_t recomputed_part_count = 0U;
};

// Recomputes each unique requested project-owned PartDocument exactly once into
// one private ShapeCache and returns the final unposed OCCT shape definitions in
// lexicographic PartDocumentId order.
class AssemblyPartShapeDefinitionBuilder {
public:
  [[nodiscard]] Result<AssemblyPartShapeDefinitionsBuild>
  build(const Project& project, std::vector<DocumentId> referenced_part_documents) const;
};

[[nodiscard]] const AssemblyPartShapeDefinition* find_assembly_part_shape_definition(
    const std::vector<AssemblyPartShapeDefinition>& definitions,
    const DocumentId& part_document) noexcept;

} // namespace blcad::geometry::detail
