#include "assembly_part_shape_definitions.hpp"

#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include "geometry_shape_internal.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace blcad::geometry::detail {

Result<AssemblyPartShapeDefinitionsBuild>
AssemblyPartShapeDefinitionBuilder::build(const Project& project,
                                          std::vector<DocumentId> referenced_part_documents) const {
  std::vector<std::pair<DocumentId, ModelStateId>> requests;
  for (auto id : referenced_part_documents)
    requests.emplace_back(std::move(id), primary_model_state_id());
  return build_for_states(project, std::move(requests));
}

Result<AssemblyPartShapeDefinitionsBuild>
AssemblyPartShapeDefinitionBuilder::build_for_states(
    const Project& project, std::vector<std::pair<DocumentId, ModelStateId>> requests) const {
  std::sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) {
    return std::pair{a.first.value(), a.second.value()} < std::pair{b.first.value(), b.second.value()};
  });
  requests.erase(std::unique(requests.begin(), requests.end()), requests.end());

  const GeometryRecomputeExecutor recompute_executor(import_context_);
  const BodyResultInspector body_result_inspector;
  AssemblyPartShapeDefinitionsBuild build;
  build.definitions.reserve(requests.size());

  for (const auto& [part_id, state] : requests) {
    const PartDocument* part = project.find_part_document(part_id);
    if (part == nullptr) {
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(Error::validation(
          part_id.value(), "assembly export referenced part must resolve to a project part"));
    }

    auto effective = part->evaluate_model_state(state);
    if (!effective)
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(effective.error());

    auto cache =
        ShapeCache::create(ShapeCacheId("shape_cache.assembly_part_definition." + part_id.value()));
    if (cache.has_error()) {
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(cache.error());
    }

    const auto recomputed = recompute_executor.execute_document(effective.value(), cache.value());
    if (recomputed.has_error()) {
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(recomputed.error());
    }
    auto final_shape = body_result_inspector.resolve_compatible_final_shape(effective.value(), cache.value());
    if (final_shape.has_error())
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(final_shape.error());
    build.definitions.push_back(
        AssemblyPartShapeDefinition{part_id, final_shape.value().impl_->shape, state});
  }

  build.recomputed_part_count = build.definitions.size();
  if (import_context_) {
    auto fresh = import_context_->validate_sources();
    if (!fresh)
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(fresh.error());
  }
  return Result<AssemblyPartShapeDefinitionsBuild>::success(std::move(build));
}

const AssemblyPartShapeDefinition*
find_assembly_part_shape_definition(const std::vector<AssemblyPartShapeDefinition>& definitions,
                                    const DocumentId& part_document, ModelStateId state) noexcept {
  const auto found = std::find_if(definitions.begin(), definitions.end(),
                                  [&part_document, &state](const AssemblyPartShapeDefinition& definition) {
                                    return definition.part_document == part_document && definition.selected_model_state == state;
                                  });
  return found == definitions.end() ? nullptr : &*found;
}

} // namespace blcad::geometry::detail
