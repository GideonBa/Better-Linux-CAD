#include "spatial_sketch_task_context.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include <algorithm>

namespace blcad::gui {
Result<std::size_t> SpatialSketchTaskResources::add_to(PartDocument& part) const {
  for (const auto& value : parameters) {
    auto added = part.add_parameter(value);
    if (!added)
      return added;
  }
  for (const auto& value : axes) {
    auto added = part.add_datum_axis(value);
    if (!added)
      return added;
  }
  return Result<std::size_t>::success(parameters.size() + axes.size());
}
SpatialSketchTaskContext::SpatialSketchTaskContext(
    PartDocument part, geometry::ShapeCache cache,
    std::unique_ptr<geometry::StepImportTopologyContext> imports, std::optional<std::size_t> prefix)
    : part_(std::move(part)), cache_(std::move(cache)), imports_(std::move(imports)),
      prefix_(prefix) {}
Result<std::shared_ptr<SpatialSketchTaskContext>>
SpatialSketchTaskContext::create(const GuiDocumentSession& session, Sketch3DId owner,
                                 const SpatialSketchTaskResources& resources) {
  using Return = Result<std::shared_ptr<SpatialSketchTaskContext>>;
  if (!session.part_document() || !session.part_modeling_ready())
    return Return::failure(
        Error::validation(owner.value(), "A current modeling context is required"));
  auto part = *session.part_document();
  auto added = resources.add_to(part);
  if (!added)
    return Return::failure(added.error());
  auto imports = make_step_import_context(session);
  if (!imports)
    return Return::failure(imports.error());
  auto plan = part.create_history_evaluation_plan();
  if (!plan)
    return Return::failure(plan.error());
  auto prefix = session.history_prefix_count();
  const auto dependents = part.dependency_graph().transitive_dependents(owner.value());
  for (std::size_t i = 0; i < plan.value().entries.size(); ++i)
    if (std::find(dependents.begin(), dependents.end(), plan.value().entries[i].feature.node_id) !=
        dependents.end()) {
      prefix = prefix ? std::min(*prefix, i) : i;
      break;
    }
  auto cache = geometry::ShapeCache::create(ShapeCacheId("spatial.task.input"));
  if (!cache)
    return Return::failure(cache.error());
  const geometry::GeometryRecomputeExecutor executor{imports.value().get()};
  auto executed = prefix ? executor.execute_history_prefix(part, *prefix, cache.value())
                         : executor.execute_document(part, cache.value());
  if (!executed)
    return Return::failure(executed.error());
  return Return::success(std::shared_ptr<SpatialSketchTaskContext>(new SpatialSketchTaskContext(
      std::move(part), std::move(cache.value()), std::move(imports.value()), prefix)));
}
geometry::SpatialGeometryScope SpatialSketchTaskContext::scope() const {
  return prefix_ ? geometry::SpatialGeometryScope{geometry::SpatialHistoryPrefixScope{*prefix_}}
                 : geometry::SpatialGeometryScope{geometry::SpatialCompleteGeometryScope{}};
}
SpatialSolverGeometry SpatialSketchTaskContext::queries() const {
  return geometry::SpatialConstraintGeometry{}.queries(
      part_, cache_, imports_.get(), scope(), [this](Sketch3DId owner) {
        return geometry::resolve_spatial_sketch_geometry(part_, std::move(owner), cache_,
                                                         imports_.get(), scope());
      });
}
SpatialSolveResult SpatialSketchTaskContext::solve(const Sketch3D& source,
                                                   const SpatialSolverOptions& options) const {
  auto graph = part_;
  auto checked = graph.find_sketch_3d(source.id()) ? graph.update_sketch_3d(source)
                                                   : graph.add_sketch_3d(source);
  if (!checked) {
    SpatialSolveResult result{source};
    result.diagnostic = checked.error().message();
    return result;
  }
  return solve_spatial_sketch(part_, source, queries(), options);
}
Result<SpatialCurveJet> SpatialSketchTaskContext::jet(const Sketch3D& candidate,
                                                      const SpatialConstraintTarget& target,
                                                      double u) const {
  auto query = queries();
  auto ready = query.prepare_candidate(candidate);
  if (!ready)
    return Result<SpatialCurveJet>::failure(ready.error());
  return query.curve(candidate, target, u);
}
Result<Point3> SpatialSketchTaskContext::position(const Sketch3D& candidate,
                                                  const SpatialConstraintTarget& target) const {
  if (const auto* reference = std::get_if<SketchPointReference3D>(&target.source())) {
    if (reference->local_point()) {
      const auto* p = candidate.find_point(*reference->local_point());
      if (!p)
        return Result<Point3>::failure(
            Error::validation(candidate.id().value(), "Point is unavailable"));
      const auto coordinate = [&](const SketchCoordinate3D& value) {
        return value.explicit_coordinate()
                   ? value.explicit_coordinate()->millimeters()
                   : part_.find_parameter(*value.parameter())->value().millimeters();
      };
      return Result<Point3>::success({coordinate(p->x()), coordinate(p->y()), coordinate(p->z())});
    }
    return queries().external_point(*reference);
  }
  if (target.kind() == SpatialTargetKind::Curve &&
      target.curve_location() != SpatialCurveLocation::Whole) {
    auto value =
        jet(candidate, target, target.curve_location() == SpatialCurveLocation::Start ? 0 : 1);
    return value ? Result<Point3>::success(value.value().point)
                 : Result<Point3>::failure(value.error());
  }
  return Result<Point3>::failure(
      Error::validation(candidate.id().value(), "Select a point or curve endpoint"));
}
} // namespace blcad::gui
