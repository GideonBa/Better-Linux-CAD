#pragma once

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/project.hpp"
#include "blcad/core/result.hpp"

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <vector>

namespace blcad::geometry {
class StepImportTopologyContext;
}

namespace blcad::geometry::detail {

struct AssemblyPartShapeDefinition {
  DocumentId part_document;
  TopoDS_Shape shape;
  ModelStateId selected_model_state{primary_model_state_id()};
};

struct AssemblyPartShapeDefinitionsBuild {
  std::vector<AssemblyPartShapeDefinition> definitions;
  std::size_t recomputed_part_count = 0U;
};

// Recomputes each unique requested project-owned PartDocument exactly once into
// one private ShapeCache and returns unposed OCCT definitions in lexicographic
// PartDocumentId/state order. Presentation defaults to visible, available
// Solid/Surface Bodies; physical scope also includes hidden available Bodies.
// Legacy and immutable Reference definitions retain their
// established complete-result authority.
class AssemblyPartShapeDefinitionBuilder {
public:
  AssemblyPartShapeDefinitionBuilder() = default;
  explicit AssemblyPartShapeDefinitionBuilder(StepImportTopologyContext* import_context)
      : import_context_(import_context) {}
  [[nodiscard]] Result<AssemblyPartShapeDefinitionsBuild>
  build(const Project& project, std::vector<DocumentId> referenced_part_documents,
        AssemblyLeafParticipation participation = AssemblyLeafParticipation::VisibleActive) const;

  [[nodiscard]] Result<AssemblyPartShapeDefinitionsBuild> build_for_states(
      const Project&, std::vector<std::pair<DocumentId, ModelStateId>>,
      AssemblyLeafParticipation participation = AssemblyLeafParticipation::VisibleActive) const;

private:
  StepImportTopologyContext* import_context_{};
};

[[nodiscard]] const AssemblyPartShapeDefinition*
find_assembly_part_shape_definition(const std::vector<AssemblyPartShapeDefinition>& definitions,
                                    const DocumentId& part_document,
                                    ModelStateId state = primary_model_state_id()) noexcept;

} // namespace blcad::geometry::detail
