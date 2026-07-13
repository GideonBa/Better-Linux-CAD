#include "assembly_part_shape_definitions.hpp"

#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include "geometry_shape_internal.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace blcad::geometry::detail {

Result<AssemblyPartShapeDefinitionsBuild>
AssemblyPartShapeDefinitionBuilder::build(const Project& project,
                                          std::vector<DocumentId> referenced_part_documents) const {
  std::sort(referenced_part_documents.begin(), referenced_part_documents.end(),
            [](const DocumentId& lhs, const DocumentId& rhs) { return lhs.value() < rhs.value(); });
  referenced_part_documents.erase(
      std::unique(referenced_part_documents.begin(), referenced_part_documents.end()),
      referenced_part_documents.end());

  const GeometryRecomputeExecutor recompute_executor;
  const BodyResultInspector body_result_inspector;
  AssemblyPartShapeDefinitionsBuild build;
  build.definitions.reserve(referenced_part_documents.size());

  for (const DocumentId& part_id : referenced_part_documents) {
    const PartDocument* part = project.find_part_document(part_id);
    if (part == nullptr) {
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(Error::validation(
          part_id.value(), "assembly export referenced part must resolve to a project part"));
    }

    auto cache =
        ShapeCache::create(ShapeCacheId("shape_cache.assembly_part_definition." + part_id.value()));
    if (cache.has_error()) {
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(cache.error());
    }

    const auto recomputed = recompute_executor.execute_document(*part, cache.value());
    if (recomputed.has_error()) {
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(recomputed.error());
    }
    auto final_shape = body_result_inspector.resolve_compatible_final_shape(*part, cache.value());
    if (final_shape.has_error())
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(final_shape.error());
    build.definitions.push_back(
        AssemblyPartShapeDefinition{part_id, final_shape.value().impl_->shape});
  }

  build.recomputed_part_count = build.definitions.size();
  return Result<AssemblyPartShapeDefinitionsBuild>::success(std::move(build));
}

const AssemblyPartShapeDefinition*
find_assembly_part_shape_definition(const std::vector<AssemblyPartShapeDefinition>& definitions,
                                    const DocumentId& part_document) noexcept {
  const auto found = std::find_if(definitions.begin(), definitions.end(),
                                  [&part_document](const AssemblyPartShapeDefinition& definition) {
                                    return definition.part_document == part_document;
                                  });
  return found == definitions.end() ? nullptr : &*found;
}

} // namespace blcad::geometry::detail
