#include "blcad/geometry/construction_line_resolver.hpp"

#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr double k_tolerance = 1.0e-9;

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Vector3 vector_between(Point3 from, Point3 to) noexcept {
  return Vector3{to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] double length(Vector3 vector) noexcept {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

[[nodiscard]] Vector3 normalize(Vector3 vector) noexcept {
  const double vector_length = length(vector);
  return Vector3{vector.x / vector_length, vector.y / vector_length, vector.z / vector_length};
}

[[nodiscard]] Result<Vector3> direction_between(Point3 start, Point3 end,
                                                const std::string& object_id) {
  const Vector3 direction = vector_between(start, end);
  if (length(direction) <= k_tolerance) {
    return Result<Vector3>::failure(
        validation_error(object_id, "construction line relation resolves to a zero-length direction"));
  }

  return Result<Vector3>::success(normalize(direction));
}

} // namespace

Result<ResolvedConstructionLine>
ConstructionLineResolver::resolve(const PartDocument& document, ConstructionLineId line_id) const {
  if (line_id.empty()) {
    return Result<ResolvedConstructionLine>::failure(
        validation_error("construction_line", "construction line id must not be empty"));
  }

  const ConstructionLine* line = document.find_construction_line(line_id);
  if (line == nullptr) {
    return Result<ResolvedConstructionLine>::failure(
        validation_error(line_id.value(), "construction line must exist in part document"));
  }

  if (line->kind() == ConstructionLineKind::Explicit) {
    return Result<ResolvedConstructionLine>::success(
        ResolvedConstructionLine{line->id(), line->point(), line->direction()});
  }

  if (!line->relation().has_value()) {
    return Result<ResolvedConstructionLine>::failure(validation_error(
        line->id().value(), "relation-driven construction line must carry a relation"));
  }

  const ConstructionRelation& relation = line->relation().value();
  const ConstructionPointResolver point_resolver;

  if (line->kind() == ConstructionLineKind::ThroughTwoPoints) {
    auto first = point_resolver.resolve(document, relation.first_point());
    if (first.has_error()) {
      return Result<ResolvedConstructionLine>::failure(first.error());
    }

    auto second = point_resolver.resolve(document, relation.second_point());
    if (second.has_error()) {
      return Result<ResolvedConstructionLine>::failure(second.error());
    }

    auto direction = direction_between(first.value().position, second.value().position,
                                       line->id().value());
    if (direction.has_error()) {
      return Result<ResolvedConstructionLine>::failure(direction.error());
    }

    return Result<ResolvedConstructionLine>::success(
        ResolvedConstructionLine{line->id(), first.value().position, direction.value()});
  }

  if (line->kind() == ConstructionLineKind::ParallelToLineThroughPoint) {
    auto point = point_resolver.resolve(document, relation.first_point());
    if (point.has_error()) {
      return Result<ResolvedConstructionLine>::failure(point.error());
    }

    auto source = resolve(document, relation.source_line());
    if (source.has_error()) {
      return Result<ResolvedConstructionLine>::failure(source.error());
    }

    return Result<ResolvedConstructionLine>::success(
        ResolvedConstructionLine{line->id(), point.value().position, source.value().direction});
  }

  if (line->kind() == ConstructionLineKind::ParallelToGeneratedEdgeThroughPoint) {
    auto point = point_resolver.resolve(document, relation.first_point());
    if (point.has_error()) {
      return Result<ResolvedConstructionLine>::failure(point.error());
    }

    if (!relation.generated_edge().has_value()) {
      return Result<ResolvedConstructionLine>::failure(validation_error(
          line->id().value(), "line parallel to generated edge must carry a generated edge reference"));
    }

    const SemanticReferenceEvaluator evaluator;
    auto edge = evaluator.resolve_edge(document, relation.generated_edge().value());
    if (edge.has_error()) {
      return Result<ResolvedConstructionLine>::failure(edge.error());
    }

    auto direction = direction_between(edge.value().start, edge.value().end, line->id().value());
    if (direction.has_error()) {
      return Result<ResolvedConstructionLine>::failure(direction.error());
    }

    return Result<ResolvedConstructionLine>::success(
        ResolvedConstructionLine{line->id(), point.value().position, direction.value()});
  }

  return Result<ResolvedConstructionLine>::failure(
      validation_error(line->id().value(), "unsupported construction line relation"));
}

} // namespace blcad::geometry
