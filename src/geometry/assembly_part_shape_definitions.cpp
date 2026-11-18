#include "assembly_part_shape_definitions.hpp"

#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <string>
#include <utility>

namespace blcad::geometry::detail {

Result<AssemblyPartShapeDefinitionsBuild>
AssemblyPartShapeDefinitionBuilder::build(const Project& project,
                                          std::vector<DocumentId> referenced_part_documents,
                                          AssemblyLeafParticipation participation) const {
  std::vector<std::pair<DocumentId, ModelStateId>> requests;
  for (auto id : referenced_part_documents)
    requests.emplace_back(std::move(id), primary_model_state_id());
  return build_for_states(project, std::move(requests), participation);
}

Result<AssemblyPartShapeDefinitionsBuild> AssemblyPartShapeDefinitionBuilder::build_for_states(
    const Project& project, std::vector<std::pair<DocumentId, ModelStateId>> requests,
    AssemblyLeafParticipation participation) const {
  std::sort(requests.begin(), requests.end(), [](const auto& a, const auto& b) {
    return std::pair{a.first.value(), a.second.value()} <
           std::pair{b.first.value(), b.second.value()};
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
    auto final_shape = [&]() -> Result<TopoDS_Shape> {
      const auto& document = effective.value();
      if (document.is_reference_part() || document.bodies().empty()) {
        auto result = body_result_inspector.resolve_compatible_final_shape(document, cache.value());
        if (!result)
          return Result<TopoDS_Shape>::failure(result.error());
        return Result<TopoDS_Shape>::success(result.value().impl_->shape);
      }
      auto provenance = cache.value().validate_complete_geometry_result(document);
      if (!provenance)
        return Result<TopoDS_Shape>::failure(provenance.error());
      std::vector<const Body*> bodies;
      for (const auto& body : document.bodies())
        if ((participation == AssemblyLeafParticipation::PhysicalActive ||
             body.visibility() == BodyVisibility::Visible) &&
            cache.value().history_body_available(body.id()))
          bodies.push_back(&body);
      std::sort(bodies.begin(), bodies.end(),
                [](const auto* a, const auto* b) { return a->id().value() < b->id().value(); });
      if (bodies.empty())
        return Result<TopoDS_Shape>::failure(
            Error::geometry(document.id().value(),
                            participation == AssemblyLeafParticipation::PhysicalActive
                                ? "Physical Assembly Part requires at least one available Body"
                                : "Assembly Part requires at least one visible available Body"));
      try {
        std::vector<TopoDS_Shape> shapes;
        for (const auto* body : bodies) {
          const auto* result = cache.value().find_body_shape(body->id());
          if (!result || result->empty())
            return Result<TopoDS_Shape>::failure(Error::geometry(
                body->id().value(), "Visible Assembly Body has no current cached shape"));
          const auto& shape = result->impl_->shape;
          const bool solid = TopExp_Explorer(shape, TopAbs_SOLID).More();
          const bool face = TopExp_Explorer(shape, TopAbs_FACE).More();
          if ((body->kind() == BodyKind::Solid && !solid) ||
              (body->kind() == BodyKind::Surface && (solid || !face)))
            return Result<TopoDS_Shape>::failure(Error::geometry(
                body->id().value(), "Visible Assembly Body topology differs from its Body kind"));
          shapes.push_back(shape);
        }
        if (shapes.size() == 1)
          return Result<TopoDS_Shape>::success(std::move(shapes.front()));
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const auto& shape : shapes)
          builder.Add(compound, shape);
        return Result<TopoDS_Shape>::success(compound);
      } catch (const Standard_Failure& failure) {
        return Result<TopoDS_Shape>::failure(
            Error::geometry(document.id().value(), standard_failure_message(failure)));
      }
    }();
    if (!final_shape)
      return Result<AssemblyPartShapeDefinitionsBuild>::failure(final_shape.error());
    build.definitions.push_back(
        AssemblyPartShapeDefinition{part_id, std::move(final_shape.value()), state});
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
  const auto found =
      std::find_if(definitions.begin(), definitions.end(),
                   [&part_document, &state](const AssemblyPartShapeDefinition& definition) {
                     return definition.part_document == part_document &&
                            definition.selected_model_state == state;
                   });
  return found == definitions.end() ? nullptr : &*found;
}

} // namespace blcad::geometry::detail
