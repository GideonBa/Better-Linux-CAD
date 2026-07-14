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
  points_.erase(point);
  return Result<std::size_t>::success(1U);
}

Result<std::size_t> Sketch3D::remove_line(SketchEntityId id) {
  const auto line = std::find_if(lines_.begin(), lines_.end(),
                                 [&id](const SketchLine3D& value) { return value.id() == id; });
  if (line == lines_.end())
    return Result<std::size_t>::failure(validation_error(id.value(), "3D sketch line must exist"));
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
  polylines_.erase(polyline);
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

std::vector<ParameterId> Sketch3D::parameter_dependencies() const {
  std::vector<ParameterId> result;
  for (const SketchPoint3D& point : points_)
    for (const ParameterId& parameter : point.parameter_dependencies())
      if (std::find(result.begin(), result.end(), parameter) == result.end())
        result.push_back(parameter);
  return result;
}

std::size_t Sketch3D::entity_count() const noexcept {
  return points_.size() + lines_.size() + polylines_.size();
}

Sketch3D::Sketch3D(Sketch3DId id, std::string name) : id_(std::move(id)), name_(std::move(name)) {}

bool Sketch3D::has_entity_id(const SketchEntityId& id) const noexcept {
  return find_point(id) != nullptr || find_line(id) != nullptr || find_polyline(id) != nullptr;
}

} // namespace blcad
