#include "blcad/geometry/construction_line_resolver.hpp"

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

  if (line->kind() == ConstructionLineKind::ThroughTwoPoints) {
    const ConstructionPoint* first = document.find_construction_point(relation.first_point());
    const ConstructionPoint* second = document.find_construction_point(relation.second_point());
    if (first == nullptr || second == nullptr) {
      return Result<ResolvedConstructionLine>::failure(validation_error(
          line->id().value(), "line through two points references must exist in part document"));
    }

    auto direction = direction_between(first->position(), second->position(), line->id().value());
    if (direction.has_error()) {
      return Result<ResolvedConstructionLine>::failure(direction.error());
    }

    return Result<ResolvedConstructionLine>::success(
        ResolvedConstructionLine{line->id(), first->position(), direction.value()});
  }

  if (line->kind() == ConstructionLineKind::ParallelToLineThroughPoint) {
    const ConstructionPoint* point = document.find_construction_point(relation.first_point());
    if (point == nullptr) {
      return Result<ResolvedConstructionLine>::failure(validation_error(
          line->id().value(), "line parallel to line through point reference point must exist"));
    }

    auto source = resolve(document, relation.source_line());
    if (source.has_error()) {
      return Result<ResolvedConstructionLine>::failure(source.error());
    }

    return Result<ResolvedConstructionLine>::success(
        ResolvedConstructionLine{line->id(), point->position(), source.value().direction});
  }

  if (line->kind() == ConstructionLineKind::ParallelToGeneratedEdgeThroughPoint) {
    const ConstructionPoint* point = document.find_construction_point(relation.first_point());
    if (point == nullptr) {
      return Result<ResolvedConstructionLine>::failure(validation_error(
          line->id().value(), "line parallel to generated edge through point reference point must exist"));
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
        ResolvedConstructionLine{line->id(), point->position(), direction.value()});
  }

  return Result<ResolvedConstructionLine>::failure(
      validation_error(line->id().value(), "unsupported construction line relation"));
}

} // namespace blcad::geometry
