#include "blcad/core/path_curve.hpp"

#include <cmath>
#include <set>
#include <utility>

namespace blcad {
namespace {
Result<PathSegmentReference> invalid_segment(std::string message) {
  return Result<PathSegmentReference>::failure(
      Error::validation("path_segment", std::move(message)));
}
} // namespace

PathSegmentReference::PathSegmentReference(
    PathSegmentSourceKind source_kind, std::optional<SketchId> planar_sketch,
    std::optional<Sketch3DId> sketch_3d, std::optional<SketchEntityId> entity,
    std::optional<ConstructionLineId> construction_line,
    std::optional<SemanticEdgeReference> semantic_edge,
    std::optional<PlanarPathCurveKind> planar_curve_kind,
    std::optional<Sketch3DPathCurveKind> sketch_3d_curve_kind, bool reversed)
    : source_kind_(source_kind), planar_sketch_(std::move(planar_sketch)),
      sketch_3d_(std::move(sketch_3d)), entity_(std::move(entity)),
      construction_line_(std::move(construction_line)), semantic_edge_(std::move(semantic_edge)),
      planar_curve_kind_(planar_curve_kind), sketch_3d_curve_kind_(sketch_3d_curve_kind),
      reversed_(reversed) {}

Result<PathSegmentReference> PathSegmentReference::create_planar(SketchId sketch,
                                                                 SketchEntityId entity,
                                                                 PlanarPathCurveKind curve_kind,
                                                                 bool reversed) {
  if (sketch.empty() || entity.empty())
    return invalid_segment("planar path segment requires non-empty sketch and entity ids");
  return Result<PathSegmentReference>::success(PathSegmentReference(
      PathSegmentSourceKind::PlanarSketchCurve, std::move(sketch), std::nullopt, std::move(entity),
      std::nullopt, std::nullopt, curve_kind, std::nullopt, reversed));
}

Result<PathSegmentReference> PathSegmentReference::create_construction_line(ConstructionLineId line,
                                                                            bool reversed) {
  if (line.empty())
    return invalid_segment("construction-line path segment requires a non-empty id");
  return Result<PathSegmentReference>::success(PathSegmentReference(
      PathSegmentSourceKind::ConstructionLine, std::nullopt, std::nullopt, std::nullopt,
      std::move(line), std::nullopt, std::nullopt, std::nullopt, reversed));
}

Result<PathSegmentReference>
PathSegmentReference::create_sketch_3d(Sketch3DId sketch, SketchEntityId entity,
                                       Sketch3DPathCurveKind curve_kind, bool reversed) {
  if (sketch.empty() || entity.empty())
    return invalid_segment("3D-sketch path segment requires non-empty sketch and entity ids");
  return Result<PathSegmentReference>::success(PathSegmentReference(
      PathSegmentSourceKind::Sketch3DCurve, std::nullopt, std::move(sketch), std::move(entity),
      std::nullopt, std::nullopt, std::nullopt, curve_kind, reversed));
}

Result<PathSegmentReference> PathSegmentReference::create_semantic_edge(SemanticEdgeReference edge,
                                                                        bool reversed) {
  return Result<PathSegmentReference>::success(PathSegmentReference(
      PathSegmentSourceKind::SemanticGeneratedEdge, std::nullopt, std::nullopt, std::nullopt,
      std::nullopt, std::move(edge), std::nullopt, std::nullopt, reversed));
}

PathSegmentSourceKind PathSegmentReference::source_kind() const noexcept {
  return source_kind_;
}
const std::optional<SketchId>& PathSegmentReference::planar_sketch() const noexcept {
  return planar_sketch_;
}
const std::optional<Sketch3DId>& PathSegmentReference::sketch_3d() const noexcept {
  return sketch_3d_;
}
const std::optional<SketchEntityId>& PathSegmentReference::entity() const noexcept {
  return entity_;
}
const std::optional<ConstructionLineId>& PathSegmentReference::construction_line() const noexcept {
  return construction_line_;
}
const std::optional<SemanticEdgeReference>& PathSegmentReference::semantic_edge() const noexcept {
  return semantic_edge_;
}
const std::optional<PlanarPathCurveKind>& PathSegmentReference::planar_curve_kind() const noexcept {
  return planar_curve_kind_;
}
const std::optional<Sketch3DPathCurveKind>&
PathSegmentReference::sketch_3d_curve_kind() const noexcept {
  return sketch_3d_curve_kind_;
}
bool PathSegmentReference::reversed() const noexcept {
  return reversed_;
}

std::string PathSegmentReference::source_node_id() const {
  switch (source_kind_) {
  case PathSegmentSourceKind::PlanarSketchCurve:
    return planar_sketch_->value();
  case PathSegmentSourceKind::ConstructionLine:
    return construction_line_->value();
  case PathSegmentSourceKind::Sketch3DCurve:
    return sketch_3d_->value();
  case PathSegmentSourceKind::SemanticGeneratedEdge:
    return semantic_edge_->source_feature().value();
  }
  return {};
}

std::string PathSegmentReference::stable_key() const {
  switch (source_kind_) {
  case PathSegmentSourceKind::PlanarSketchCurve:
    return "planar:" + planar_sketch_->value() + ":" + entity_->value();
  case PathSegmentSourceKind::ConstructionLine:
    return "construction_line:" + construction_line_->value();
  case PathSegmentSourceKind::Sketch3DCurve:
    return "sketch_3d:" + sketch_3d_->value() + ":" + entity_->value();
  case PathSegmentSourceKind::SemanticGeneratedEdge:
    return "semantic_edge:" + semantic_edge_->node_id();
  }
  return {};
}

std::string_view to_string(PathSegmentSourceKind value) noexcept {
  switch (value) {
  case PathSegmentSourceKind::PlanarSketchCurve:
    return "planar_sketch_curve";
  case PathSegmentSourceKind::ConstructionLine:
    return "construction_line";
  case PathSegmentSourceKind::Sketch3DCurve:
    return "sketch_3d_curve";
  case PathSegmentSourceKind::SemanticGeneratedEdge:
    return "semantic_generated_edge";
  }
  return "planar_sketch_curve";
}
std::string_view to_string(PlanarPathCurveKind value) noexcept {
  switch (value) {
  case PlanarPathCurveKind::Line:
    return "line";
  case PlanarPathCurveKind::Arc:
    return "arc";
  case PlanarPathCurveKind::Spline:
    return "spline";
  case PlanarPathCurveKind::ProjectedLine:
    return "projected_line";
  case PlanarPathCurveKind::ReferenceGeneratedLine:
    return "reference_generated_line";
  }
  return "line";
}
std::string_view to_string(Sketch3DPathCurveKind value) noexcept {
  switch (value) {
  case Sketch3DPathCurveKind::Line:
    return "line";
  case Sketch3DPathCurveKind::Polyline:
    return "polyline";
  case Sketch3DPathCurveKind::Arc:
    return "arc";
  case Sketch3DPathCurveKind::Spline:
    return "spline";
  case Sketch3DPathCurveKind::Helix:
    return "helix";
  }
  return "line";
}
std::string_view to_string(PathClosure value) noexcept {
  return value == PathClosure::Open ? "open" : "closed";
}
std::string_view to_string(PathOrientationRule value) noexcept {
  switch (value) {
  case PathOrientationRule::ProfileNormal:
    return "profile_normal";
  case PathOrientationRule::MinimumTwist:
    return "minimum_twist";
  case PathOrientationRule::FixedUpVector:
    return "fixed_up_vector";
  }
  return "profile_normal";
}
std::string_view to_string(PathContinuityHint value) noexcept {
  switch (value) {
  case PathContinuityHint::C0:
    return "c0";
  case PathContinuityHint::G1:
    return "g1";
  case PathContinuityHint::G2:
    return "g2";
  }
  return "c0";
}

Result<PathCurve> PathCurve::create(PathCurveId id, std::string name,
                                    std::vector<PathSegmentReference> segments, PathClosure closure,
                                    PathOrientationRule orientation_rule,
                                    std::optional<Vector3> fixed_up_vector,
                                    std::optional<PathContinuityHint> continuity_hint,
                                    double connection_tolerance_mm) {
  if (id.empty())
    return Result<PathCurve>::failure(
        Error::validation("path_curve", "path curve id must not be empty"));
  if (name.empty())
    return Result<PathCurve>::failure(
        Error::validation(id.value(), "path curve name must not be empty"));
  if (segments.empty())
    return Result<PathCurve>::failure(
        Error::validation(id.value(), "path curve requires at least one segment"));
  if (!std::isfinite(connection_tolerance_mm) || connection_tolerance_mm <= 0.0)
    return Result<PathCurve>::failure(
        Error::validation(id.value(), "path connection tolerance must be finite and positive"));
  const bool fixed = orientation_rule == PathOrientationRule::FixedUpVector;
  if (fixed != fixed_up_vector.has_value())
    return Result<PathCurve>::failure(
        Error::validation(id.value(), "fixed-up orientation requires exactly one fixed up vector"));
  if (fixed) {
    const auto value = *fixed_up_vector;
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
        std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z) <= 1.0e-12)
      return Result<PathCurve>::failure(
          Error::validation(id.value(), "fixed up vector must be finite and non-zero"));
  }
  std::set<std::string> unique_sources;
  for (const auto& segment : segments)
    if (!unique_sources.insert(segment.stable_key()).second)
      return Result<PathCurve>::failure(
          Error::validation(id.value(), "path curve must not repeat a source segment"));
  return Result<PathCurve>::success(PathCurve(std::move(id), std::move(name), std::move(segments),
                                              closure, orientation_rule, fixed_up_vector,
                                              continuity_hint, connection_tolerance_mm));
}

PathCurve::PathCurve(PathCurveId id, std::string name, std::vector<PathSegmentReference> segments,
                     PathClosure closure, PathOrientationRule orientation_rule,
                     std::optional<Vector3> fixed_up_vector,
                     std::optional<PathContinuityHint> continuity_hint,
                     double connection_tolerance_mm)
    : id_(std::move(id)), name_(std::move(name)), segments_(std::move(segments)), closure_(closure),
      orientation_rule_(orientation_rule), fixed_up_vector_(fixed_up_vector),
      continuity_hint_(continuity_hint), connection_tolerance_mm_(connection_tolerance_mm) {}
const PathCurveId& PathCurve::id() const noexcept {
  return id_;
}
const std::string& PathCurve::name() const noexcept {
  return name_;
}
const std::vector<PathSegmentReference>& PathCurve::segments() const noexcept {
  return segments_;
}
PathClosure PathCurve::closure() const noexcept {
  return closure_;
}
PathOrientationRule PathCurve::orientation_rule() const noexcept {
  return orientation_rule_;
}
const std::optional<Vector3>& PathCurve::fixed_up_vector() const noexcept {
  return fixed_up_vector_;
}
const std::optional<PathContinuityHint>& PathCurve::continuity_hint() const noexcept {
  return continuity_hint_;
}
double PathCurve::connection_tolerance_mm() const noexcept {
  return connection_tolerance_mm_;
}

} // namespace blcad
