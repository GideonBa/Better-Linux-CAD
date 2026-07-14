#include "blcad/core/sketch_3d.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace blcad {
namespace {
[[nodiscard]] Error validation_error(std::string id, std::string message) {
  return Error::validation(std::move(id), std::move(message));
}
} // namespace

Result<SketchCoordinate3D> SketchCoordinate3D::create_explicit(Quantity coordinate) {
  if (!coordinate.is_valid_linear_displacement())
    return Result<SketchCoordinate3D>::failure(validation_error(
        "sketch_3d_coordinate", "explicit 3D sketch coordinate must be a linear displacement"));
  return Result<SketchCoordinate3D>::success(
      SketchCoordinate3D(SketchCoordinate3DSource::Explicit, coordinate, std::nullopt));
}

Result<SketchCoordinate3D> SketchCoordinate3D::create_parameter(ParameterId parameter) {
  if (parameter.empty())
    return Result<SketchCoordinate3D>::failure(validation_error(
        "sketch_3d_coordinate", "3D sketch coordinate parameter must not be empty"));
  return Result<SketchCoordinate3D>::success(
      SketchCoordinate3D(SketchCoordinate3DSource::Parameter, std::nullopt, std::move(parameter)));
}

SketchCoordinate3DSource SketchCoordinate3D::source() const noexcept {
  return source_;
}

const std::optional<Quantity>& SketchCoordinate3D::explicit_coordinate() const noexcept {
  return explicit_coordinate_;
}

const std::optional<ParameterId>& SketchCoordinate3D::parameter() const noexcept {
  return parameter_;
}

SketchCoordinate3D::SketchCoordinate3D(SketchCoordinate3DSource source,
                                       std::optional<Quantity> explicit_coordinate,
                                       std::optional<ParameterId> parameter)
    : source_(source), explicit_coordinate_(std::move(explicit_coordinate)),
      parameter_(std::move(parameter)) {}

Result<SketchPoint3D> SketchPoint3D::create(SketchEntityId id, SketchCoordinate3D x,
                                            SketchCoordinate3D y, SketchCoordinate3D z) {
  if (id.empty())
    return Result<SketchPoint3D>::failure(
        validation_error("sketch_3d_point", "3D sketch point id must not be empty"));
  return Result<SketchPoint3D>::success(
      SketchPoint3D(std::move(id), std::move(x), std::move(y), std::move(z)));
}

const SketchEntityId& SketchPoint3D::id() const noexcept {
  return id_;
}
const SketchCoordinate3D& SketchPoint3D::x() const noexcept {
  return x_;
}
const SketchCoordinate3D& SketchPoint3D::y() const noexcept {
  return y_;
}
const SketchCoordinate3D& SketchPoint3D::z() const noexcept {
  return z_;
}

std::vector<ParameterId> SketchPoint3D::parameter_dependencies() const {
  std::vector<ParameterId> result;
  for (const SketchCoordinate3D* coordinate : {&x_, &y_, &z_})
    if (coordinate->parameter().has_value() &&
        std::find(result.begin(), result.end(), *coordinate->parameter()) == result.end())
      result.push_back(*coordinate->parameter());
  return result;
}

SketchPoint3D::SketchPoint3D(SketchEntityId id, SketchCoordinate3D x, SketchCoordinate3D y,
                             SketchCoordinate3D z)
    : id_(std::move(id)), x_(std::move(x)), y_(std::move(y)), z_(std::move(z)) {}

Result<SketchLine3D> SketchLine3D::create(SketchEntityId id, SketchEntityId start_point,
                                          SketchEntityId end_point) {
  const std::string object_id = id.empty() ? "sketch_3d_line" : id.value();
  if (id.empty() || start_point.empty() || end_point.empty())
    return Result<SketchLine3D>::failure(
        validation_error(object_id, "3D sketch line id and endpoint point ids must not be empty"));
  if (start_point == end_point)
    return Result<SketchLine3D>::failure(
        validation_error(object_id, "3D sketch line endpoints must be distinct points"));
  return Result<SketchLine3D>::success(
      SketchLine3D(std::move(id), std::move(start_point), std::move(end_point)));
}

const SketchEntityId& SketchLine3D::id() const noexcept {
  return id_;
}
const SketchEntityId& SketchLine3D::start_point() const noexcept {
  return start_point_;
}
const SketchEntityId& SketchLine3D::end_point() const noexcept {
  return end_point_;
}

SketchLine3D::SketchLine3D(SketchEntityId id, SketchEntityId start_point, SketchEntityId end_point)
    : id_(std::move(id)), start_point_(std::move(start_point)), end_point_(std::move(end_point)) {}

Result<SketchPolyline3D> SketchPolyline3D::create(SketchEntityId id,
                                                  std::vector<SketchEntityId> ordered_vertices) {
  const std::string object_id = id.empty() ? "sketch_3d_polyline" : id.value();
  if (id.empty())
    return Result<SketchPolyline3D>::failure(
        validation_error(object_id, "3D sketch polyline id must not be empty"));
  if (ordered_vertices.size() < 2U)
    return Result<SketchPolyline3D>::failure(
        validation_error(object_id, "3D sketch polyline requires at least two vertices"));
  for (std::size_t index = 0; index < ordered_vertices.size(); ++index) {
    if (ordered_vertices[index].empty())
      return Result<SketchPolyline3D>::failure(
          validation_error(object_id, "3D sketch polyline vertex ids must not be empty"));
    if (index > 0U && ordered_vertices[index - 1U] == ordered_vertices[index])
      return Result<SketchPolyline3D>::failure(
          validation_error(object_id, "3D sketch polyline consecutive vertices must be distinct"));
  }
  return Result<SketchPolyline3D>::success(
      SketchPolyline3D(std::move(id), std::move(ordered_vertices)));
}

const SketchEntityId& SketchPolyline3D::id() const noexcept {
  return id_;
}

const std::vector<SketchEntityId>& SketchPolyline3D::ordered_vertices() const noexcept {
  return ordered_vertices_;
}

SketchPolyline3D::SketchPolyline3D(SketchEntityId id, std::vector<SketchEntityId> ordered_vertices)
    : id_(std::move(id)), ordered_vertices_(std::move(ordered_vertices)) {}

Result<SketchPointReference3D> SketchPointReference3D::create_local_point(SketchEntityId point) {
  if (point.empty())
    return Result<SketchPointReference3D>::failure(
        validation_error("sketch_3d_point_reference", "local 3D point id must not be empty"));
  return Result<SketchPointReference3D>::success(
      SketchPointReference3D(SketchPointReference3DSource::LocalPoint, std::move(point),
                             std::nullopt, std::nullopt, std::nullopt));
}

Result<SketchPointReference3D>
SketchPointReference3D::create_construction_point(ConstructionPointId point) {
  if (point.empty())
    return Result<SketchPointReference3D>::failure(
        validation_error("sketch_3d_point_reference", "construction point id must not be empty"));
  return Result<SketchPointReference3D>::success(
      SketchPointReference3D(SketchPointReference3DSource::ConstructionPoint, std::nullopt,
                             std::move(point), std::nullopt, std::nullopt));
}

Result<SketchPointReference3D>
SketchPointReference3D::create_planar_sketch_point(SketchId sketch, SketchReferenceTarget point) {
  const bool supported = point.kind() == SketchReferenceTargetKind::LineSegmentStart ||
                         point.kind() == SketchReferenceTargetKind::LineSegmentEnd ||
                         point.kind() == SketchReferenceTargetKind::ProjectedPoint;
  if (sketch.empty() || !supported)
    return Result<SketchPointReference3D>::failure(validation_error(
        sketch.empty() ? "sketch_3d_point_reference" : sketch.value(),
        "planar 3D point reference requires a sketch and line endpoint or projected point"));
  return Result<SketchPointReference3D>::success(
      SketchPointReference3D(SketchPointReference3DSource::PlanarSketchPoint, std::nullopt,
                             std::nullopt, std::move(sketch), std::move(point)));
}

SketchPointReference3DSource SketchPointReference3D::source() const noexcept {
  return source_;
}
const std::optional<SketchEntityId>& SketchPointReference3D::local_point() const noexcept {
  return local_point_;
}
const std::optional<ConstructionPointId>&
SketchPointReference3D::construction_point() const noexcept {
  return construction_point_;
}
const std::optional<SketchId>& SketchPointReference3D::planar_sketch() const noexcept {
  return planar_sketch_;
}
const std::optional<SketchReferenceTarget>& SketchPointReference3D::planar_point() const noexcept {
  return planar_point_;
}

std::optional<std::string> SketchPointReference3D::referenced_node_id() const {
  if (construction_point_.has_value())
    return construction_point_->value();
  if (planar_sketch_.has_value())
    return planar_sketch_->value();
  return std::nullopt;
}

bool SketchPointReference3D::same_source(const SketchPointReference3D& other) const noexcept {
  if (source_ != other.source_)
    return false;
  if (local_point_ != other.local_point_ || construction_point_ != other.construction_point_ ||
      planar_sketch_ != other.planar_sketch_)
    return false;
  if (!planar_point_.has_value() || !other.planar_point_.has_value())
    return planar_point_.has_value() == other.planar_point_.has_value();
  return planar_point_->kind() == other.planar_point_->kind() &&
         planar_point_->entity() == other.planar_point_->entity();
}

SketchPointReference3D::SketchPointReference3D(
    SketchPointReference3DSource source, std::optional<SketchEntityId> local_point,
    std::optional<ConstructionPointId> construction_point, std::optional<SketchId> planar_sketch,
    std::optional<SketchReferenceTarget> planar_point)
    : source_(source), local_point_(std::move(local_point)),
      construction_point_(std::move(construction_point)), planar_sketch_(std::move(planar_sketch)),
      planar_point_(std::move(planar_point)) {}

Result<SketchArc3D> SketchArc3D::create_three_point(SketchEntityId id, SketchPointReference3D start,
                                                    SketchPointReference3D intermediate,
                                                    SketchPointReference3D end) {
  const std::string object_id = id.empty() ? "sketch_3d_arc" : id.value();
  if (id.empty())
    return Result<SketchArc3D>::failure(
        validation_error(object_id, "3D sketch arc id must not be empty"));
  if (start.same_source(intermediate) || start.same_source(end) || intermediate.same_source(end))
    return Result<SketchArc3D>::failure(
        validation_error(object_id, "3D sketch arc requires three distinct point references"));
  return Result<SketchArc3D>::success(
      SketchArc3D(std::move(id), std::move(start), std::move(intermediate), std::move(end)));
}

const SketchEntityId& SketchArc3D::id() const noexcept {
  return id_;
}
const SketchPointReference3D& SketchArc3D::start() const noexcept {
  return start_;
}
const SketchPointReference3D& SketchArc3D::intermediate() const noexcept {
  return intermediate_;
}
const SketchPointReference3D& SketchArc3D::end() const noexcept {
  return end_;
}
std::vector<SketchPointReference3D> SketchArc3D::point_references() const {
  return {start_, intermediate_, end_};
}

SketchArc3D::SketchArc3D(SketchEntityId id, SketchPointReference3D start,
                         SketchPointReference3D intermediate, SketchPointReference3D end)
    : id_(std::move(id)), start_(std::move(start)), intermediate_(std::move(intermediate)),
      end_(std::move(end)) {}

Result<SketchSpline3D> SketchSpline3D::create(SketchEntityId id,
                                              SketchSpline3DRepresentation representation,
                                              std::vector<SketchPointReference3D> ordered_points,
                                              std::size_t degree,
                                              SketchSpline3DContinuity continuity) {
  const std::string object_id = id.empty() ? "sketch_3d_spline" : id.value();
  if (id.empty())
    return Result<SketchSpline3D>::failure(
        validation_error(object_id, "3D sketch spline id must not be empty"));
  if (degree < 1U || degree > 5U)
    return Result<SketchSpline3D>::failure(
        validation_error(object_id, "3D sketch spline degree must be between 1 and 5"));
  if (ordered_points.size() <= degree)
    return Result<SketchSpline3D>::failure(validation_error(
        object_id, "3D sketch spline requires more point references than its degree"));
  if ((continuity == SketchSpline3DContinuity::Tangent && degree < 2U) ||
      (continuity == SketchSpline3DContinuity::Curvature && degree < 3U))
    return Result<SketchSpline3D>::failure(
        validation_error(object_id, "3D sketch spline continuity exceeds the selected degree"));
  for (std::size_t first = 0; first < ordered_points.size(); ++first)
    for (std::size_t second = first + 1U; second < ordered_points.size(); ++second)
      if (ordered_points[first].same_source(ordered_points[second]))
        return Result<SketchSpline3D>::failure(
            validation_error(object_id, "3D sketch spline point references must be distinct"));
  return Result<SketchSpline3D>::success(
      SketchSpline3D(std::move(id), representation, std::move(ordered_points), degree, continuity));
}

const SketchEntityId& SketchSpline3D::id() const noexcept {
  return id_;
}
SketchSpline3DRepresentation SketchSpline3D::representation() const noexcept {
  return representation_;
}
const std::vector<SketchPointReference3D>& SketchSpline3D::ordered_points() const noexcept {
  return ordered_points_;
}
std::size_t SketchSpline3D::degree() const noexcept {
  return degree_;
}
SketchSpline3DContinuity SketchSpline3D::continuity() const noexcept {
  return continuity_;
}

SketchSpline3D::SketchSpline3D(SketchEntityId id, SketchSpline3DRepresentation representation,
                               std::vector<SketchPointReference3D> ordered_points,
                               std::size_t degree, SketchSpline3DContinuity continuity)
    : id_(std::move(id)), representation_(representation),
      ordered_points_(std::move(ordered_points)), degree_(degree), continuity_(continuity) {}

Result<SketchHelixAxis3D> SketchHelixAxis3D::create_datum_axis(DatumAxisId axis) {
  if (axis.empty())
    return Result<SketchHelixAxis3D>::failure(
        validation_error("sketch_3d_helix_axis", "helix datum axis id must not be empty"));
  return Result<SketchHelixAxis3D>::success(
      SketchHelixAxis3D(SketchHelixAxis3DSource::DatumAxis, std::move(axis), std::nullopt));
}

Result<SketchHelixAxis3D> SketchHelixAxis3D::create_construction_line(ConstructionLineId line) {
  if (line.empty())
    return Result<SketchHelixAxis3D>::failure(
        validation_error("sketch_3d_helix_axis", "helix construction line id must not be empty"));
  return Result<SketchHelixAxis3D>::success(
      SketchHelixAxis3D(SketchHelixAxis3DSource::ConstructionLine, std::nullopt, std::move(line)));
}

SketchHelixAxis3DSource SketchHelixAxis3D::source() const noexcept {
  return source_;
}
const std::optional<DatumAxisId>& SketchHelixAxis3D::datum_axis() const noexcept {
  return datum_axis_;
}
const std::optional<ConstructionLineId>& SketchHelixAxis3D::construction_line() const noexcept {
  return construction_line_;
}
std::string SketchHelixAxis3D::referenced_node_id() const {
  return datum_axis_.has_value() ? datum_axis_->value() : construction_line_->value();
}

SketchHelixAxis3D::SketchHelixAxis3D(SketchHelixAxis3DSource source,
                                     std::optional<DatumAxisId> datum_axis,
                                     std::optional<ConstructionLineId> construction_line)
    : source_(source), datum_axis_(std::move(datum_axis)),
      construction_line_(std::move(construction_line)) {}

Result<SketchHelix3D> SketchHelix3D::create(SketchEntityId id, SketchHelixAxis3D axis,
                                            ParameterId radius_parameter,
                                            ParameterId pitch_parameter,
                                            ParameterId turns_parameter,
                                            SketchHelix3DHandedness handedness) {
  const std::string object_id = id.empty() ? "sketch_3d_helix" : id.value();
  if (id.empty() || radius_parameter.empty() || pitch_parameter.empty() || turns_parameter.empty())
    return Result<SketchHelix3D>::failure(validation_error(
        object_id, "3D sketch helix id and radius, pitch, and turns parameters must not be empty"));
  return Result<SketchHelix3D>::success(
      SketchHelix3D(std::move(id), std::move(axis), std::move(radius_parameter),
                    std::move(pitch_parameter), std::move(turns_parameter), handedness));
}

const SketchEntityId& SketchHelix3D::id() const noexcept {
  return id_;
}
const SketchHelixAxis3D& SketchHelix3D::axis() const noexcept {
  return axis_;
}
const ParameterId& SketchHelix3D::radius_parameter() const noexcept {
  return radius_parameter_;
}
const ParameterId& SketchHelix3D::pitch_parameter() const noexcept {
  return pitch_parameter_;
}
const ParameterId& SketchHelix3D::turns_parameter() const noexcept {
  return turns_parameter_;
}
SketchHelix3DHandedness SketchHelix3D::handedness() const noexcept {
  return handedness_;
}

SketchHelix3D::SketchHelix3D(SketchEntityId id, SketchHelixAxis3D axis,
                             ParameterId radius_parameter, ParameterId pitch_parameter,
                             ParameterId turns_parameter, SketchHelix3DHandedness handedness)
    : id_(std::move(id)), axis_(std::move(axis)), radius_parameter_(std::move(radius_parameter)),
      pitch_parameter_(std::move(pitch_parameter)), turns_parameter_(std::move(turns_parameter)),
      handedness_(handedness) {}

Result<SketchGuideCurve3D> SketchGuideCurve3D::create(SketchEntityId id,
                                                      SketchEntityId source_curve,
                                                      SketchGuideCurve3DRole role,
                                                      std::string label) {
  const std::string object_id = id.empty() ? "sketch_3d_guide_curve" : id.value();
  if (id.empty() || source_curve.empty())
    return Result<SketchGuideCurve3D>::failure(
        validation_error(object_id, "3D guide curve id and source curve id must not be empty"));
  if (id == source_curve)
    return Result<SketchGuideCurve3D>::failure(
        validation_error(object_id, "3D guide curve must not reference itself"));
  return Result<SketchGuideCurve3D>::success(
      SketchGuideCurve3D(std::move(id), std::move(source_curve), role, std::move(label)));
}

const SketchEntityId& SketchGuideCurve3D::id() const noexcept {
  return id_;
}
const SketchEntityId& SketchGuideCurve3D::source_curve() const noexcept {
  return source_curve_;
}
SketchGuideCurve3DRole SketchGuideCurve3D::role() const noexcept {
  return role_;
}
const std::string& SketchGuideCurve3D::label() const noexcept {
  return label_;
}

SketchGuideCurve3D::SketchGuideCurve3D(SketchEntityId id, SketchEntityId source_curve,
                                       SketchGuideCurve3DRole role, std::string label)
    : id_(std::move(id)), source_curve_(std::move(source_curve)), role_(role),
      label_(std::move(label)) {}

Result<Sketch3D> Sketch3D::create(Sketch3DId id, std::string name) {
  const std::string object_id = id.empty() ? "sketch_3d" : id.value();
  if (id.empty())
    return Result<Sketch3D>::failure(validation_error(object_id, "3D sketch id must not be empty"));
  if (name.empty())
    return Result<Sketch3D>::failure(
        validation_error(object_id, "3D sketch name must not be empty"));
  return Result<Sketch3D>::success(Sketch3D(std::move(id), std::move(name)));
}

Result<std::size_t> Sketch3D::add_point(SketchPoint3D point) {
  if (has_entity_id(point.id()))
    return Result<std::size_t>::failure(
        validation_error(point.id().value(), "3D sketch entity id must be unique within sketch"));
  points_.push_back(std::move(point));
  return Result<std::size_t>::success(points_.size() - 1U);
}

Result<std::size_t> Sketch3D::add_line(SketchLine3D line) {
  if (has_entity_id(line.id()))
    return Result<std::size_t>::failure(
        validation_error(line.id().value(), "3D sketch entity id must be unique within sketch"));
  if (find_point(line.start_point()) == nullptr || find_point(line.end_point()) == nullptr)
    return Result<std::size_t>::failure(validation_error(
        line.id().value(), "3D sketch line endpoints must be owned by the same sketch"));
  lines_.push_back(std::move(line));
  return Result<std::size_t>::success(lines_.size() - 1U);
}

Result<std::size_t> Sketch3D::add_polyline(SketchPolyline3D polyline) {
  if (has_entity_id(polyline.id()))
    return Result<std::size_t>::failure(validation_error(
        polyline.id().value(), "3D sketch entity id must be unique within sketch"));
  if (std::ranges::any_of(polyline.ordered_vertices(),
                          [this](const SketchEntityId& id) { return find_point(id) == nullptr; }))
    return Result<std::size_t>::failure(validation_error(
        polyline.id().value(), "3D sketch polyline vertices must be owned by the same sketch"));
  polylines_.push_back(std::move(polyline));
  return Result<std::size_t>::success(polylines_.size() - 1U);
}

Result<std::size_t> Sketch3D::add_arc(SketchArc3D arc) {
  if (has_entity_id(arc.id()))
    return Result<std::size_t>::failure(
        validation_error(arc.id().value(), "3D sketch entity id must be unique within sketch"));
  auto valid = validate_point_references(arc.point_references(), arc.id());
  if (valid.has_error())
    return valid;
  arcs_.push_back(std::move(arc));
  return Result<std::size_t>::success(arcs_.size() - 1U);
}

Result<std::size_t> Sketch3D::add_spline(SketchSpline3D spline) {
  if (has_entity_id(spline.id()))
    return Result<std::size_t>::failure(
        validation_error(spline.id().value(), "3D sketch entity id must be unique within sketch"));
  auto valid = validate_point_references(spline.ordered_points(), spline.id());
  if (valid.has_error())
    return valid;
  splines_.push_back(std::move(spline));
  return Result<std::size_t>::success(splines_.size() - 1U);
}

Result<std::size_t> Sketch3D::add_helix(SketchHelix3D helix) {
  if (has_entity_id(helix.id()))
    return Result<std::size_t>::failure(
        validation_error(helix.id().value(), "3D sketch entity id must be unique within sketch"));
  helices_.push_back(std::move(helix));
  return Result<std::size_t>::success(helices_.size() - 1U);
}

Result<std::size_t> Sketch3D::add_guide_curve(SketchGuideCurve3D guide_curve) {
  if (has_entity_id(guide_curve.id()))
    return Result<std::size_t>::failure(validation_error(
        guide_curve.id().value(), "3D sketch entity id must be unique within sketch"));
  if (!has_curve_id(guide_curve.source_curve()))
    return Result<std::size_t>::failure(
        validation_error(guide_curve.id().value(),
                         "3D guide source must be an owned line, polyline, arc, spline, or helix"));
  guide_curves_.push_back(std::move(guide_curve));
  return Result<std::size_t>::success(guide_curves_.size() - 1U);
}

Result<std::size_t> Sketch3D::remove_point(SketchEntityId id) {
  const auto point = std::find_if(points_.begin(), points_.end(),
                                  [&id](const SketchPoint3D& value) { return value.id() == id; });
  if (point == points_.end())
    return Result<std::size_t>::failure(validation_error(id.value(), "3D sketch point must exist"));
  for (const SketchLine3D& line : lines_)
    if (line.start_point() == id || line.end_point() == id)
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "3D sketch point is referenced by a line"));
  for (const SketchPolyline3D& polyline : polylines_)
    if (std::find(polyline.ordered_vertices().begin(), polyline.ordered_vertices().end(), id) !=
        polyline.ordered_vertices().end())
      return Result<std::size_t>::failure(
          Error::dependency(id.value(), "3D sketch point is referenced by a polyline"));
  for (const SketchArc3D& arc : arcs_)
    for (const SketchPointReference3D& reference : arc.point_references())
      if (reference.local_point().has_value() && *reference.local_point() == id)
        return Result<std::size_t>::failure(
            Error::dependency(id.value(), "3D sketch point is referenced by an arc"));
  for (const SketchSpline3D& spline : splines_)
    for (const SketchPointReference3D& reference : spline.ordered_points())
      if (reference.local_point().has_value() && *reference.local_point() == id)
        return Result<std::size_t>::failure(
            Error::dependency(id.value(), "3D sketch point is referenced by a spline"));
  points_.erase(point);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_line(SketchEntityId id) {
  const auto line = std::find_if(lines_.begin(), lines_.end(),
                                 [&id](const SketchLine3D& value) { return value.id() == id; });
  if (line == lines_.end())
    return Result<std::size_t>::failure(validation_error(id.value(), "3D sketch line must exist"));
  if (guide_references(id))
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "3D sketch line is referenced by a guide curve"));
  lines_.erase(line);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_polyline(SketchEntityId id) {
  const auto polyline =
      std::find_if(polylines_.begin(), polylines_.end(),
                   [&id](const SketchPolyline3D& value) { return value.id() == id; });
  if (polyline == polylines_.end())
    return Result<std::size_t>::failure(
        validation_error(id.value(), "3D sketch polyline must exist"));
  if (guide_references(id))
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "3D sketch polyline is referenced by a guide curve"));
  polylines_.erase(polyline);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_arc(SketchEntityId id) {
  const auto found = std::find_if(arcs_.begin(), arcs_.end(),
                                  [&id](const SketchArc3D& value) { return value.id() == id; });
  if (found == arcs_.end())
    return Result<std::size_t>::failure(validation_error(id.value(), "3D sketch arc must exist"));
  if (guide_references(id))
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "3D sketch arc is referenced by a guide curve"));
  arcs_.erase(found);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_spline(SketchEntityId id) {
  const auto found = std::find_if(splines_.begin(), splines_.end(),
                                  [&id](const SketchSpline3D& value) { return value.id() == id; });
  if (found == splines_.end())
    return Result<std::size_t>::failure(
        validation_error(id.value(), "3D sketch spline must exist"));
  if (guide_references(id))
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "3D sketch spline is referenced by a guide curve"));
  splines_.erase(found);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_helix(SketchEntityId id) {
  const auto found = std::find_if(helices_.begin(), helices_.end(),
                                  [&id](const SketchHelix3D& value) { return value.id() == id; });
  if (found == helices_.end())
    return Result<std::size_t>::failure(validation_error(id.value(), "3D sketch helix must exist"));
  if (guide_references(id))
    return Result<std::size_t>::failure(
        Error::dependency(id.value(), "3D sketch helix is referenced by a guide curve"));
  helices_.erase(found);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_guide_curve(SketchEntityId id) {
  const auto found =
      std::find_if(guide_curves_.begin(), guide_curves_.end(),
                   [&id](const SketchGuideCurve3D& value) { return value.id() == id; });
  if (found == guide_curves_.end())
    return Result<std::size_t>::failure(
        validation_error(id.value(), "3D sketch guide curve must exist"));
  guide_curves_.erase(found);
  return Result<std::size_t>::success(1U);
}

const Sketch3DId& Sketch3D::id() const noexcept {
  return id_;
}
const std::string& Sketch3D::name() const noexcept {
  return name_;
}
const std::vector<SketchPoint3D>& Sketch3D::points() const noexcept {
  return points_;
}
const std::vector<SketchLine3D>& Sketch3D::lines() const noexcept {
  return lines_;
}
const std::vector<SketchPolyline3D>& Sketch3D::polylines() const noexcept {
  return polylines_;
}
const std::vector<SketchArc3D>& Sketch3D::arcs() const noexcept {
  return arcs_;
}
const std::vector<SketchSpline3D>& Sketch3D::splines() const noexcept {
  return splines_;
}
const std::vector<SketchHelix3D>& Sketch3D::helices() const noexcept {
  return helices_;
}
const std::vector<SketchGuideCurve3D>& Sketch3D::guide_curves() const noexcept {
  return guide_curves_;
}

const SketchPoint3D* Sketch3D::find_point(SketchEntityId id) const noexcept {
  const auto found = std::find_if(points_.begin(), points_.end(),
                                  [&id](const SketchPoint3D& value) { return value.id() == id; });
  return found == points_.end() ? nullptr : &*found;
}

const SketchLine3D* Sketch3D::find_line(SketchEntityId id) const noexcept {
  const auto found = std::find_if(lines_.begin(), lines_.end(),
                                  [&id](const SketchLine3D& value) { return value.id() == id; });
  return found == lines_.end() ? nullptr : &*found;
}

const SketchPolyline3D* Sketch3D::find_polyline(SketchEntityId id) const noexcept {
  const auto found =
      std::find_if(polylines_.begin(), polylines_.end(),
                   [&id](const SketchPolyline3D& value) { return value.id() == id; });
  return found == polylines_.end() ? nullptr : &*found;
}

const SketchArc3D* Sketch3D::find_arc(SketchEntityId id) const noexcept {
  const auto found = std::find_if(arcs_.begin(), arcs_.end(),
                                  [&id](const SketchArc3D& value) { return value.id() == id; });
  return found == arcs_.end() ? nullptr : &*found;
}

const SketchSpline3D* Sketch3D::find_spline(SketchEntityId id) const noexcept {
  const auto found = std::find_if(splines_.begin(), splines_.end(),
                                  [&id](const SketchSpline3D& value) { return value.id() == id; });
  return found == splines_.end() ? nullptr : &*found;
}

const SketchHelix3D* Sketch3D::find_helix(SketchEntityId id) const noexcept {
  const auto found = std::find_if(helices_.begin(), helices_.end(),
                                  [&id](const SketchHelix3D& value) { return value.id() == id; });
  return found == helices_.end() ? nullptr : &*found;
}

const SketchGuideCurve3D* Sketch3D::find_guide_curve(SketchEntityId id) const noexcept {
  const auto found =
      std::find_if(guide_curves_.begin(), guide_curves_.end(),
                   [&id](const SketchGuideCurve3D& value) { return value.id() == id; });
  return found == guide_curves_.end() ? nullptr : &*found;
}

std::vector<ParameterId> Sketch3D::parameter_dependencies() const {
  std::vector<ParameterId> result;
  for (const SketchPoint3D& point : points_)
    for (const ParameterId& parameter : point.parameter_dependencies())
      if (std::find(result.begin(), result.end(), parameter) == result.end())
        result.push_back(parameter);
  for (const SketchHelix3D& helix : helices_)
    for (const ParameterId* parameter :
         {&helix.radius_parameter(), &helix.pitch_parameter(), &helix.turns_parameter()})
      if (std::find(result.begin(), result.end(), *parameter) == result.end())
        result.push_back(*parameter);
  return result;
}

std::vector<std::string> Sketch3D::referenced_node_ids() const {
  std::vector<std::string> result;
  const auto collect = [&result](const SketchPointReference3D& reference) {
    const auto node = reference.referenced_node_id();
    if (node.has_value() && std::find(result.begin(), result.end(), *node) == result.end())
      result.push_back(*node);
  };
  for (const SketchArc3D& arc : arcs_)
    for (const auto& reference : arc.point_references())
      collect(reference);
  for (const SketchSpline3D& spline : splines_)
    for (const auto& reference : spline.ordered_points())
      collect(reference);
  for (const SketchHelix3D& helix : helices_) {
    const std::string node = helix.axis().referenced_node_id();
    if (std::find(result.begin(), result.end(), node) == result.end())
      result.push_back(node);
  }
  return result;
}

std::size_t Sketch3D::entity_count() const noexcept {
  return points_.size() + lines_.size() + polylines_.size() + arcs_.size() + splines_.size() +
         helices_.size() + guide_curves_.size();
}

Sketch3D::Sketch3D(Sketch3DId id, std::string name) : id_(std::move(id)), name_(std::move(name)) {}

bool Sketch3D::has_entity_id(const SketchEntityId& id) const noexcept {
  return find_point(id) != nullptr || has_curve_id(id) || find_guide_curve(id) != nullptr;
}

bool Sketch3D::has_curve_id(const SketchEntityId& id) const noexcept {
  return find_line(id) != nullptr || find_polyline(id) != nullptr || find_arc(id) != nullptr ||
         find_spline(id) != nullptr || find_helix(id) != nullptr;
}

Result<std::size_t>
Sketch3D::validate_point_references(const std::vector<SketchPointReference3D>& references,
                                    const SketchEntityId& owner) const {
  for (const SketchPointReference3D& reference : references)
    if (reference.source() == SketchPointReference3DSource::LocalPoint &&
        find_point(*reference.local_point()) == nullptr)
      return Result<std::size_t>::failure(
          validation_error(owner.value(), "local 3D curve point must be owned by the same sketch"));
  return Result<std::size_t>::success(references.size());
}

bool Sketch3D::guide_references(const SketchEntityId& id) const noexcept {
  return std::ranges::any_of(
      guide_curves_, [&id](const SketchGuideCurve3D& guide) { return guide.source_curve() == id; });
}

} // namespace blcad
