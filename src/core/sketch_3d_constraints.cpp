#include "blcad/core/sketch_3d_constraints.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>
#include <utility>

namespace blcad {
namespace {
Error spatial_error(std::string message, std::string id = "sketch3d.constraint") {
  return Error::validation(std::move(id), std::move(message));
}
bool location_valid(SpatialCurveLocation value) {
  return value == SpatialCurveLocation::Whole || value == SpatialCurveLocation::Start ||
         value == SpatialCurveLocation::End;
}
} // namespace
SpatialConstraintTarget::SpatialConstraintTarget(SpatialConstraintTargetSource source)
    : source_(std::move(source)) {}
Result<SpatialConstraintTarget> SpatialConstraintTarget::point(SketchPointReference3D reference) {
  return Result<SpatialConstraintTarget>::success(SpatialConstraintTarget(std::move(reference)));
}
Result<SpatialConstraintTarget> SpatialConstraintTarget::local_point(SketchEntityId id) {
  auto value = SketchPointReference3D::create_local_point(std::move(id));
  if (!value)
    return Result<SpatialConstraintTarget>::failure(value.error());
  return point(std::move(value.value()));
}
Result<SpatialConstraintTarget> SpatialConstraintTarget::curve(SketchEntityId entity,
                                                               SpatialCurveLocation location) {
  if (entity.empty() || !location_valid(location))
    return Result<SpatialConstraintTarget>::failure(
        spatial_error("Curve target requires an entity and supported location"));
  return Result<SpatialConstraintTarget>::success(
      SpatialConstraintTarget(SpatialLocalCurve{std::move(entity), location}));
}
Result<SpatialConstraintTarget>
SpatialConstraintTarget::external_curve(PathSegmentReference reference,
                                        SpatialCurveLocation location) {
  if (!location_valid(location))
    return Result<SpatialConstraintTarget>::failure(
        spatial_error("External curve target requires a supported location"));
  return Result<SpatialConstraintTarget>::success(
      SpatialConstraintTarget(SpatialExternalCurve{std::move(reference), location}));
}
Result<SpatialConstraintTarget> SpatialConstraintTarget::axis(AxisReference reference) {
  if (reference.role() != PartFeatureInputRole::SpatialConstraint ||
      reference.expected_capability() != PartFeatureInputCapability::Axis)
    return Result<SpatialConstraintTarget>::failure(spatial_error(
        "Axis target requires spatial role and Axis capability; use a curve target for a line"));
  return Result<SpatialConstraintTarget>::success(SpatialConstraintTarget(std::move(reference)));
}
Result<SpatialConstraintTarget> SpatialConstraintTarget::plane(PlaneReference reference) {
  if (reference.role() != PartFeatureInputRole::SpatialConstraint)
    return Result<SpatialConstraintTarget>::failure(
        spatial_error("Plane target requires the spatial constraint role"));
  return Result<SpatialConstraintTarget>::success(SpatialConstraintTarget(std::move(reference)));
}
Result<SpatialConstraintTarget> SpatialConstraintTarget::face(FaceReference reference) {
  if (reference.role() != PartFeatureInputRole::SpatialConstraint)
    return Result<SpatialConstraintTarget>::failure(
        spatial_error("Face target requires the spatial constraint role"));
  return Result<SpatialConstraintTarget>::success(SpatialConstraintTarget(std::move(reference)));
}
SpatialTargetKind SpatialConstraintTarget::kind() const noexcept {
  if (std::holds_alternative<SketchPointReference3D>(source_))
    return SpatialTargetKind::Point;
  if (std::holds_alternative<SpatialLocalCurve>(source_) ||
      std::holds_alternative<SpatialExternalCurve>(source_))
    return SpatialTargetKind::Curve;
  if (std::holds_alternative<AxisReference>(source_))
    return SpatialTargetKind::Axis;
  if (std::holds_alternative<PlaneReference>(source_))
    return SpatialTargetKind::Plane;
  return SpatialTargetKind::Face;
}
const SpatialConstraintTargetSource& SpatialConstraintTarget::source() const noexcept {
  return source_;
}
SpatialCurveLocation SpatialConstraintTarget::curve_location() const noexcept {
  if (const auto* v = std::get_if<SpatialLocalCurve>(&source_))
    return v->location;
  if (const auto* v = std::get_if<SpatialExternalCurve>(&source_))
    return v->location;
  return SpatialCurveLocation::Whole;
}
std::optional<std::string> SpatialConstraintTarget::referenced_node_id() const {
  return std::visit(
      [](const auto& value) -> std::optional<std::string> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, SketchPointReference3D>)
          return value.referenced_node_id();
        else if constexpr (std::is_same_v<T, SpatialLocalCurve>)
          return std::nullopt;
        else if constexpr (std::is_same_v<T, SpatialExternalCurve>)
          return value.reference.source_node_id();
        else
          return value.source_node_id();
      },
      source_);
}
std::optional<SketchEntityId> SpatialConstraintTarget::local_entity() const {
  if (const auto* v = std::get_if<SpatialLocalCurve>(&source_))
    return v->entity;
  if (const auto* v = std::get_if<SketchPointReference3D>(&source_))
    return v->local_point();
  return std::nullopt;
}
SpatialDimensionValue::SpatialDimensionValue(std::variant<Quantity, ParameterId> source)
    : source_(std::move(source)) {}
Result<SpatialDimensionValue> SpatialDimensionValue::literal(Quantity value) {
  if (!value.is_positive_length() && !value.is_valid_angle())
    return Result<SpatialDimensionValue>::failure(
        spatial_error("Dimension requires a positive Length or signed Angle quantity"));
  return Result<SpatialDimensionValue>::success(SpatialDimensionValue(std::move(value)));
}
Result<SpatialDimensionValue> SpatialDimensionValue::parameter(ParameterId id) {
  if (id.empty())
    return Result<SpatialDimensionValue>::failure(
        spatial_error("Dimension parameter id must not be empty"));
  return Result<SpatialDimensionValue>::success(SpatialDimensionValue(std::move(id)));
}
const std::variant<Quantity, ParameterId>& SpatialDimensionValue::source() const noexcept {
  return source_;
}
Result<Quantity> SpatialDimensionValue::resolve(const PartDocument& part) const {
  if (const auto* v = std::get_if<Quantity>(&source_))
    return Result<Quantity>::success(*v);
  const auto& id = std::get<ParameterId>(source_);
  const auto* parameter = part.find_parameter(id);
  if (!parameter ||
      (parameter->type() != ParameterType::Length && parameter->type() != ParameterType::Angle))
    return Result<Quantity>::failure(
        spatial_error("Dimension requires an existing Length or Angle parameter", id.value()));
  return Result<Quantity>::success(parameter->value());
}

namespace {
bool point_target(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Point ||
         (t.kind() == SpatialTargetKind::Curve &&
          t.curve_location() != SpatialCurveLocation::Whole);
}
bool whole_curve(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Curve && t.curve_location() == SpatialCurveLocation::Whole;
}
bool endpoint_curve(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Curve && t.curve_location() != SpatialCurveLocation::Whole;
}
bool direction_target(const SpatialConstraintTarget& t) {
  return t.kind() == SpatialTargetKind::Axis || whole_curve(t);
}
bool finite_point(Point3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
} // namespace
SpatialConstraint::SpatialConstraint(SketchConstraintId id, SpatialConstraintKind kind,
                                     std::vector<SpatialConstraintTarget> targets,
                                     std::optional<SpatialDimensionValue> value,
                                     std::optional<Point3> fixed, SpatialAngleConvention angle,
                                     std::optional<double> seed)
    : id_(std::move(id)), kind_(kind), targets_(std::move(targets)), value_(std::move(value)),
      fixed_position_(fixed), angle_(angle), curve_parameter_seed_(seed) {}
Result<SpatialConstraint>
SpatialConstraint::create(SketchConstraintId id, SpatialConstraintKind kind,
                          std::vector<SpatialConstraintTarget> targets,
                          std::optional<SpatialDimensionValue> value, std::optional<Point3> fixed,
                          SpatialAngleConvention angle, std::optional<double> seed) {
  const auto fail = [&](std::string message) {
    return Result<SpatialConstraint>::failure(Error::validation(id.value(), std::move(message)));
  };
  if (id.empty())
    return fail("Spatial constraint id must not be empty");
  if (angle != SpatialAngleConvention::Unsigned &&
      angle != SpatialAngleConvention::OrientedAboutAxis)
    return fail("Unsupported angle convention");
  if (kind != SpatialConstraintKind::Angle && angle != SpatialAngleConvention::Unsigned)
    return fail("Only an Angle may carry an angle convention");
  const bool dimensional =
      kind == SpatialConstraintKind::Distance || kind == SpatialConstraintKind::Angle ||
      kind == SpatialConstraintKind::Radius || kind == SpatialConstraintKind::Length;
  if (dimensional != value.has_value())
    return fail("Only dimensional constraints require a typed value");
  if ((kind == SpatialConstraintKind::Fixed) != fixed.has_value() ||
      (fixed && !finite_point(*fixed)))
    return fail("Fixed requires a persistent finite anchor; other kinds do not carry an anchor");
  if (kind == SpatialConstraintKind::PointOnCurve) {
    if (!seed)
      seed = .5;
    if (!std::isfinite(*seed) || *seed < 0 || *seed > 1)
      return fail("Point-on-curve branch seed must lie in [0,1]");
  } else if (seed)
    return fail("Only point-on-curve carries a normalized curve parameter seed");
  const auto size = targets.size();
  bool valid = false;
  switch (kind) {
  case SpatialConstraintKind::Coincident:
  case SpatialConstraintKind::Distance:
    valid = size == 2 && point_target(targets[0]) && point_target(targets[1]);
    break;
  case SpatialConstraintKind::Fixed:
    valid = size == 1 && point_target(targets[0]) && targets[0].local_entity().has_value();
    break;
  case SpatialConstraintKind::AxisParallel:
    valid = size == 2 && whole_curve(targets[0]) && targets[1].kind() == SpatialTargetKind::Axis;
    break;
  case SpatialConstraintKind::PlaneParallel:
    valid = size == 2 && whole_curve(targets[0]) && targets[1].kind() == SpatialTargetKind::Plane;
    break;
  case SpatialConstraintKind::PointOnLine:
    valid = size == 2 && point_target(targets[0]) && direction_target(targets[1]);
    break;
  case SpatialConstraintKind::PointOnPlane:
    valid = size == 2 && point_target(targets[0]) && targets[1].kind() == SpatialTargetKind::Plane;
    break;
  case SpatialConstraintKind::PointOnCurve:
    valid = size == 2 && point_target(targets[0]) && whole_curve(targets[1]);
    break;
  case SpatialConstraintKind::PointOnFace:
    valid = size == 2 && point_target(targets[0]) && targets[1].kind() == SpatialTargetKind::Face;
    break;
  case SpatialConstraintKind::Parallel:
  case SpatialConstraintKind::Perpendicular:
    valid = size == 2 && direction_target(targets[0]) && direction_target(targets[1]);
    break;
  case SpatialConstraintKind::Tangent:
  case SpatialConstraintKind::Smooth:
    valid = size == 2 && endpoint_curve(targets[0]) && endpoint_curve(targets[1]);
    break;
  case SpatialConstraintKind::Angle:
    valid = (angle == SpatialAngleConvention::Unsigned ? size == 2 : size == 3) &&
            direction_target(targets[0]) && direction_target(targets[1]) &&
            (size == 2 || targets[2].kind() == SpatialTargetKind::Axis);
    break;
  case SpatialConstraintKind::Radius:
  case SpatialConstraintKind::Length:
    valid = size == 1 && whole_curve(targets[0]);
    break;
  }
  if (!valid)
    return fail("Unsupported spatial constraint target tuple");
  if (value)
    if (const auto* literal = std::get_if<Quantity>(&value->source())) {
      if (kind == SpatialConstraintKind::Angle) {
        if (!literal->is_valid_angle())
          return fail("Angle requires an Angle quantity");
        const double v = literal->degrees();
        if (angle == SpatialAngleConvention::Unsigned && (v < 0 || v > 180))
          return fail("Unsigned Angle lies in [0,180] degrees");
        if (angle == SpatialAngleConvention::OrientedAboutAxis && (v <= -180 || v > 180))
          return fail("Oriented Angle lies in (-180,180] degrees");
      } else if (!literal->is_positive_length())
        return fail("Distance, radius and length require positive Length");
    }
  return Result<SpatialConstraint>::success(SpatialConstraint(
      std::move(id), kind, std::move(targets), std::move(value), fixed, angle, seed));
}
const SketchConstraintId& SpatialConstraint::id() const noexcept {
  return id_;
}
SpatialConstraintKind SpatialConstraint::kind() const noexcept {
  return kind_;
}
const std::vector<SpatialConstraintTarget>& SpatialConstraint::targets() const noexcept {
  return targets_;
}
const std::optional<SpatialDimensionValue>& SpatialConstraint::value() const noexcept {
  return value_;
}
const std::optional<Point3>& SpatialConstraint::fixed_position() const noexcept {
  return fixed_position_;
}
SpatialAngleConvention SpatialConstraint::angle_convention() const noexcept {
  return angle_;
}
const std::optional<double>& SpatialConstraint::curve_parameter_seed() const noexcept {
  return curve_parameter_seed_;
}
std::vector<ParameterId> SpatialConstraint::parameter_dependencies() const {
  if (value_)
    if (const auto* id = std::get_if<ParameterId>(&value_->source()))
      return {*id};
  return {};
}
std::vector<std::string> SpatialConstraint::referenced_node_ids() const {
  std::vector<std::string> result;
  for (const auto& target : targets_)
    if (auto id = target.referenced_node_id();
        id && std::find(result.begin(), result.end(), *id) == result.end())
      result.push_back(std::move(*id));
  return result;
}

namespace {
enum class SpatialCurveType { Missing, Line, Polyline, Arc, Spline, Helix, Equation };
SpatialCurveType curve_type(const Sketch3D& sketch, const SpatialConstraintTarget& target) {
  if (const auto* local = std::get_if<SpatialLocalCurve>(&target.source())) {
    const auto& id = local->entity;
    if (sketch.find_line(id))
      return SpatialCurveType::Line;
    if (sketch.find_polyline(id))
      return SpatialCurveType::Polyline;
    if (sketch.find_arc(id))
      return SpatialCurveType::Arc;
    if (sketch.find_spline(id))
      return SpatialCurveType::Spline;
    if (sketch.find_helix(id))
      return SpatialCurveType::Helix;
    if (sketch.find_equation_curve(id))
      return SpatialCurveType::Equation;
    return SpatialCurveType::Missing;
  }
  const auto* external = std::get_if<SpatialExternalCurve>(&target.source());
  if (!external)
    return SpatialCurveType::Missing;
  const auto& ref = external->reference;
  if (ref.source_kind() == PathSegmentSourceKind::ConstructionLine ||
      ref.source_kind() == PathSegmentSourceKind::SemanticGeneratedEdge)
    return SpatialCurveType::Line;
  if (ref.source_kind() == PathSegmentSourceKind::PlanarSketchCurve) {
    switch (*ref.planar_curve_kind()) {
    case PlanarPathCurveKind::Line:
    case PlanarPathCurveKind::ProjectedLine:
    case PlanarPathCurveKind::ReferenceGeneratedLine:
      return SpatialCurveType::Line;
    case PlanarPathCurveKind::Arc:
      return SpatialCurveType::Arc;
    case PlanarPathCurveKind::Spline:
      return SpatialCurveType::Spline;
    case PlanarPathCurveKind::EquationCurve:
      return SpatialCurveType::Equation;
    }
  } else if (ref.source_kind() == PathSegmentSourceKind::Sketch3DCurve) {
    switch (*ref.sketch_3d_curve_kind()) {
    case Sketch3DPathCurveKind::Line:
      return SpatialCurveType::Line;
    case Sketch3DPathCurveKind::Polyline:
      return SpatialCurveType::Polyline;
    case Sketch3DPathCurveKind::Arc:
      return SpatialCurveType::Arc;
    case Sketch3DPathCurveKind::Spline:
      return SpatialCurveType::Spline;
    case Sketch3DPathCurveKind::Helix:
      return SpatialCurveType::Helix;
    case Sketch3DPathCurveKind::EquationCurve:
      return SpatialCurveType::Equation;
    }
  }
  return SpatialCurveType::Missing;
}
bool direction_capability(const Sketch3D& sketch, const SpatialConstraintTarget& target) {
  return target.kind() == SpatialTargetKind::Axis ||
         (target.curve_location() == SpatialCurveLocation::Whole &&
          curve_type(sketch, target) == SpatialCurveType::Line);
}
Result<std::size_t> validate_spatial_point(const PartDocument& part, const Sketch3D& owner,
                                           const SketchPointReference3D& ref) {
  const auto fail = [&](std::string message) {
    return Result<std::size_t>::failure(Error::validation(owner.id().value(), std::move(message)));
  };
  switch (ref.source()) {
  case SketchPointReference3DSource::LocalPoint:
    if (!owner.find_point(*ref.local_point()))
      return fail("Constraint local point must exist");
    break;
  case SketchPointReference3DSource::ConstructionPoint:
    if (!part.find_construction_point(*ref.construction_point()))
      return fail("Constraint construction point must exist");
    break;
  case SketchPointReference3DSource::PlanarSketchPoint: {
    const auto* source = part.find_sketch(*ref.planar_sketch());
    if (!source)
      return fail("Constraint planar point owner must exist");
    const auto& target = *ref.planar_point();
    const bool found = target.kind() == SketchReferenceTargetKind::ProjectedPoint
                           ? source->find_projected_point(target.entity()) != nullptr
                           : source->find_line_segment(target.entity()) != nullptr;
    if (!found)
      return fail("Constraint planar point target must exist");
    break;
  }
  }
  return Result<std::size_t>::success(1);
}
} // namespace
Result<std::size_t> validate_spatial_constraint(const Sketch3D& sketch,
                                                const SpatialConstraint& constraint) {
  const auto fail = [&](std::string message) {
    return Result<std::size_t>::failure(
        Error::validation(constraint.id().value(), std::move(message)));
  };
  const auto& targets = constraint.targets();
  for (const auto& target : targets) {
    if (const auto* point = std::get_if<SketchPointReference3D>(&target.source());
        point && point->local_point() && !sketch.find_point(*point->local_point()))
      return fail("Constraint point target must be owned by this Sketch3D");
    if (target.kind() == SpatialTargetKind::Curve &&
        curve_type(sketch, target) == SpatialCurveType::Missing)
      return fail("Constraint curve target must exist and support a curve role");
    if (const auto* external = std::get_if<SpatialExternalCurve>(&target.source());
        external && external->reference.sketch_3d() == sketch.id())
      return fail("A same-owner curve must use the local role, not an external dependency");
  }
  switch (constraint.kind()) {
  case SpatialConstraintKind::AxisParallel:
  case SpatialConstraintKind::PlaneParallel:
    if (curve_type(sketch, targets[0]) != SpatialCurveType::Line)
      return fail("Axis/plane parallelism requires a straight line");
    break;
  case SpatialConstraintKind::PointOnLine:
    if (!direction_capability(sketch, targets[1]))
      return fail("Point-on-line requires a straight line or axis");
    break;
  case SpatialConstraintKind::Parallel:
  case SpatialConstraintKind::Perpendicular:
  case SpatialConstraintKind::Angle:
    if (!direction_capability(sketch, targets[0]) || !direction_capability(sketch, targets[1]))
      return fail("Direction relations require straight lines or axes");
    break;
  case SpatialConstraintKind::Radius: {
    const auto kind = curve_type(sketch, targets[0]);
    if (kind != SpatialCurveType::Arc && kind != SpatialCurveType::Helix)
      return fail("Radius requires an arc or helix");
    break;
  }
  default:
    break;
  }
  if (constraint.kind() == SpatialConstraintKind::Coincident ||
      constraint.kind() == SpatialConstraintKind::Distance) {
    const auto* a = std::get_if<SketchPointReference3D>(&targets[0].source());
    const auto* b = std::get_if<SketchPointReference3D>(&targets[1].source());
    if (a && b && a->same_source(*b))
      return fail("Point relation requires two distinct point identities");
  }
  return Result<std::size_t>::success(targets.size());
}
Result<std::size_t> validate_spatial_constraints(const PartDocument& part, const Sketch3D& sketch) {
  for (const auto& point : sketch.points())
    for (const auto* c : {&point.x(), &point.y(), &point.z()})
      if (c->parameter()) {
        const auto* p = part.find_parameter(*c->parameter());
        if (!p || p->type() != ParameterType::Length)
          return Result<std::size_t>::failure(Error::validation(
              point.id().value(), "Coordinate requires an existing Length parameter"));
      }
  for (const auto& h : sketch.helices())
    for (const auto* b : {&h.radius_binding(), &h.pitch_binding()}) {
      const auto value = b->resolve(part);
      if (!value)
        return Result<std::size_t>::failure(value.error());
    }
  for (const auto& h : sketch.helices()) {
    const auto* turns = part.find_parameter(h.turns_parameter());
    const bool axis = h.axis().source() == SketchHelixAxis3DSource::DatumAxis
                          ? part.find_datum_axis(*h.axis().datum_axis()) != nullptr
                          : part.find_construction_line(*h.axis().construction_line()) != nullptr;
    if (!turns || turns->type() != ParameterType::Count || turns->value().count_value() < 1 ||
        !axis)
      return Result<std::size_t>::failure(Error::validation(
          h.id().value(), "Helix requires an existing axis and positive integer Count turns"));
  }
  for (const auto& constraint : sketch.constraints()) {
    auto structural = validate_spatial_constraint(sketch, constraint);
    if (!structural)
      return structural;
    for (const auto& target : constraint.targets()) {
      const auto valid = std::visit(
          [&](const auto& value) -> Result<std::size_t> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, SketchPointReference3D>)
              return validate_spatial_point(part, sketch, value);
            else if constexpr (std::is_same_v<T, SpatialLocalCurve>)
              return Result<std::size_t>::success(1);
            else if constexpr (std::is_same_v<T, SpatialExternalCurve>)
              return validate_path_segment_source(part, value.reference);
            else
              return validate_part_feature_input_reference(part, value);
          },
          target.source());
      if (!valid)
        return Result<std::size_t>::failure(
            Error::validation(constraint.id().value(), valid.error().message()));
    }
    if (constraint.value()) {
      const auto resolved = constraint.value()->resolve(part);
      if (!resolved)
        return Result<std::size_t>::failure(
            Error::validation(constraint.id().value(), resolved.error().message()));
      const auto typed = SpatialDimensionValue::literal(resolved.value());
      if (!typed)
        return Result<std::size_t>::failure(typed.error());
      const auto checked = SpatialConstraint::create(
          constraint.id(), constraint.kind(), constraint.targets(), typed.value(),
          constraint.fixed_position(), constraint.angle_convention(),
          constraint.curve_parameter_seed());
      if (!checked)
        return Result<std::size_t>::failure(checked.error());
    }
  }
  return Result<std::size_t>::success(sketch.constraints().size());
}
Result<std::vector<SpatialVariable>> spatial_variable_layout(const PartDocument& part,
                                                             const Sketch3D& sketch) {
  const auto valid = validate_spatial_constraints(part, sketch);
  if (!valid)
    return Result<std::vector<SpatialVariable>>::failure(valid.error());
  std::vector<SpatialVariable> variables;
  std::vector<const SketchPoint3D*> points;
  for (const auto& p : sketch.points())
    points.push_back(&p);
  std::sort(points.begin(), points.end(),
            [](const auto* a, const auto* b) { return a->id().value() < b->id().value(); });
  for (const auto* p : points) {
    const std::array<const SketchCoordinate3D*, 3> coordinates{&p->x(), &p->y(), &p->z()};
    const std::array<SpatialVariableKind, 3> kinds{
        SpatialVariableKind::PointX, SpatialVariableKind::PointY, SpatialVariableKind::PointZ};
    for (std::size_t i = 0; i < 3; ++i)
      if (coordinates[i]->source() == SketchCoordinate3DSource::Explicit)
        variables.push_back(
            {kinds[i], p->id().value(), coordinates[i]->explicit_coordinate()->millimeters()});
  }
  std::vector<const SketchHelix3D*> helices;
  for (const auto& h : sketch.helices())
    helices.push_back(&h);
  std::sort(helices.begin(), helices.end(),
            [](const auto* a, const auto* b) { return a->id().value() < b->id().value(); });
  for (const auto* h : helices) {
    if (h->radius_binding().explicit_length())
      variables.push_back({SpatialVariableKind::HelixRadius, h->id().value(),
                           h->radius_binding().explicit_length()->millimeters()});
    if (h->pitch_binding().explicit_length())
      variables.push_back({SpatialVariableKind::HelixPitch, h->id().value(),
                           h->pitch_binding().explicit_length()->millimeters()});
  }
  std::vector<const SpatialConstraint*> constraints;
  for (const auto& c : sketch.constraints())
    if (c.kind() == SpatialConstraintKind::PointOnCurve)
      constraints.push_back(&c);
  std::sort(constraints.begin(), constraints.end(),
            [](const auto* a, const auto* b) { return a->id().value() < b->id().value(); });
  for (const auto* c : constraints)
    variables.push_back(
        {SpatialVariableKind::CurveParameter, c->id().value(), *c->curve_parameter_seed()});
  return Result<std::vector<SpatialVariable>>::success(std::move(variables));
}
std::string_view to_string(SpatialCurveLocation value) noexcept {
  switch (value) {
  case SpatialCurveLocation::Whole:
    return "whole";
  case SpatialCurveLocation::Start:
    return "start";
  case SpatialCurveLocation::End:
    return "end";
  }
  return "unsupported";
}
std::string_view to_string(SpatialAngleConvention value) noexcept {
  switch (value) {
  case SpatialAngleConvention::Unsigned:
    return "unsigned";
  case SpatialAngleConvention::OrientedAboutAxis:
    return "oriented_about_axis";
  }
  return "unsupported";
}
std::string_view to_string(SpatialConstraintKind value) noexcept {
  switch (value) {
  case SpatialConstraintKind::Coincident:
    return "coincident";
  case SpatialConstraintKind::Fixed:
    return "fixed";
  case SpatialConstraintKind::AxisParallel:
    return "axis_parallel";
  case SpatialConstraintKind::PlaneParallel:
    return "plane_parallel";
  case SpatialConstraintKind::PointOnLine:
    return "point_on_line";
  case SpatialConstraintKind::PointOnPlane:
    return "point_on_plane";
  case SpatialConstraintKind::PointOnCurve:
    return "point_on_curve";
  case SpatialConstraintKind::PointOnFace:
    return "point_on_face";
  case SpatialConstraintKind::Parallel:
    return "parallel";
  case SpatialConstraintKind::Perpendicular:
    return "perpendicular";
  case SpatialConstraintKind::Tangent:
    return "tangent";
  case SpatialConstraintKind::Smooth:
    return "smooth";
  case SpatialConstraintKind::Distance:
    return "distance";
  case SpatialConstraintKind::Angle:
    return "angle";
  case SpatialConstraintKind::Radius:
    return "radius";
  case SpatialConstraintKind::Length:
    return "length";
  }
  return "unsupported";
}
} // namespace blcad
