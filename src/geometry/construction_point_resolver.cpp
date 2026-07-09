#include "blcad/geometry/construction_point_resolver.hpp"

#include "blcad/geometry/semantic_reference_evaluator.hpp"

#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Point3 midpoint(Point3 first, Point3 second) noexcept {
  return Point3{(first.x + second.x) / 2.0, (first.y + second.y) / 2.0,
                (first.z + second.z) / 2.0};
}

} // namespace

Result<ResolvedConstructionPoint>
ConstructionPointResolver::resolve(const PartDocument& document, ConstructionPointId point_id) const {
  if (point_id.empty()) {
    return Result<ResolvedConstructionPoint>::failure(
        validation_error("construction_point", "construction point id must not be empty"));
  }

  const ConstructionPoint* point = document.find_construction_point(point_id);
  if (point == nullptr) {
    return Result<ResolvedConstructionPoint>::failure(
        validation_error(point_id.value(), "construction point must exist in part document"));
  }

  if (point->kind() == ConstructionPointKind::Explicit) {
    return Result<ResolvedConstructionPoint>::success(
        ResolvedConstructionPoint{point->id(), point->position()});
  }

  if (!point->relation().has_value()) {
    return Result<ResolvedConstructionPoint>::failure(validation_error(
        point->id().value(), "relation-driven construction point must carry a relation"));
  }

  const ConstructionRelation& relation = point->relation().value();
  const SemanticReferenceEvaluator evaluator;

  if (point->kind() == ConstructionPointKind::OnGeneratedVertex) {
    if (!relation.generated_vertex().has_value()) {
      return Result<ResolvedConstructionPoint>::failure(validation_error(
          point->id().value(), "construction point on generated vertex must carry a vertex reference"));
    }

    auto vertex = evaluator.resolve_vertex(document, relation.generated_vertex().value());
    if (vertex.has_error()) {
      return Result<ResolvedConstructionPoint>::failure(vertex.error());
    }

    return Result<ResolvedConstructionPoint>::success(
        ResolvedConstructionPoint{point->id(), vertex.value().position});
  }

  if (point->kind() == ConstructionPointKind::OnGeneratedEdge) {
    if (!relation.generated_edge().has_value()) {
      return Result<ResolvedConstructionPoint>::failure(validation_error(
          point->id().value(), "construction point on generated edge must carry an edge reference"));
    }

    auto edge = evaluator.resolve_edge(document, relation.generated_edge().value());
    if (edge.has_error()) {
      return Result<ResolvedConstructionPoint>::failure(edge.error());
    }

    return Result<ResolvedConstructionPoint>::success(
        ResolvedConstructionPoint{point->id(), midpoint(edge.value().start, edge.value().end)});
  }

  return Result<ResolvedConstructionPoint>::failure(
      validation_error(point->id().value(), "unsupported construction point relation"));
}

} // namespace blcad::geometry
