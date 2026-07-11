#include "blcad/core/datum_plane.hpp"

#include <string>
#include <utility>

namespace blcad {

std::string_view to_string(SemanticFace face) noexcept {
  switch (face) {
  case SemanticFace::Top:
    return "top";
  case SemanticFace::Bottom:
    return "bottom";
  case SemanticFace::Right:
    return "right";
  case SemanticFace::Left:
    return "left";
  case SemanticFace::Front:
    return "front";
  case SemanticFace::Back:
    return "back";
  }

  return "top";
}

std::string_view to_string(SemanticEdge edge) noexcept {
  switch (edge) {
  case SemanticEdge::TopFront:
    return "top_front";
  case SemanticEdge::TopBack:
    return "top_back";
  case SemanticEdge::TopRight:
    return "top_right";
  case SemanticEdge::TopLeft:
    return "top_left";
  case SemanticEdge::BottomFront:
    return "bottom_front";
  case SemanticEdge::BottomBack:
    return "bottom_back";
  case SemanticEdge::BottomRight:
    return "bottom_right";
  case SemanticEdge::BottomLeft:
    return "bottom_left";
  case SemanticEdge::FrontRight:
    return "front_right";
  case SemanticEdge::FrontLeft:
    return "front_left";
  case SemanticEdge::BackRight:
    return "back_right";
  case SemanticEdge::BackLeft:
    return "back_left";
  }

  return "top_front";
}

std::string_view to_string(SemanticVertex vertex) noexcept {
  switch (vertex) {
  case SemanticVertex::TopFrontRight:
    return "top_front_right";
  case SemanticVertex::TopFrontLeft:
    return "top_front_left";
  case SemanticVertex::TopBackRight:
    return "top_back_right";
  case SemanticVertex::TopBackLeft:
    return "top_back_left";
  case SemanticVertex::BottomFrontRight:
    return "bottom_front_right";
  case SemanticVertex::BottomFrontLeft:
    return "bottom_front_left";
  case SemanticVertex::BottomBackRight:
    return "bottom_back_right";
  case SemanticVertex::BottomBackLeft:
    return "bottom_back_left";
  }

  return "top_front_right";
}

std::string_view to_string(SemanticAxis axis) noexcept {
  switch (axis) {
  case SemanticAxis::Primary:
    return "axis";
  }

  return "axis";
}

std::string_view to_string(SemanticSeatingPlane plane) noexcept {
  switch (plane) {
  case SemanticSeatingPlane::Primary:
    return "seat";
  }

  return "seat";
}

Result<SemanticFaceReference> SemanticFaceReference::create(FeatureId source_feature,
                                                             SemanticFace face) {
  const auto object_id = source_feature.empty()
                             ? std::string("semantic_face")
                             : source_feature.value() + "." + std::string(to_string(face));

  if (source_feature.empty()) {
    return Result<SemanticFaceReference>::failure(
        Error::validation(object_id, "semantic face source feature id must not be empty"));
  }

  return Result<SemanticFaceReference>::success(
      SemanticFaceReference(std::move(source_feature), face));
}

const FeatureId& SemanticFaceReference::source_feature() const noexcept {
  return source_feature_;
}

SemanticFace SemanticFaceReference::face() const noexcept {
  return face_;
}

SemanticFaceReference::SemanticFaceReference(FeatureId source_feature, SemanticFace face)
    : source_feature_(std::move(source_feature)), face_(face) {}

Result<SemanticAxisReference> SemanticAxisReference::create(FeatureId source_feature,
                                                             SemanticAxis axis) {
  const auto object_id = source_feature.empty()
                             ? std::string("semantic_axis")
                             : source_feature.value() + "." + std::string(to_string(axis));

  if (source_feature.empty()) {
    return Result<SemanticAxisReference>::failure(
        Error::validation(object_id, "semantic axis source feature id must not be empty"));
  }

  return Result<SemanticAxisReference>::success(
      SemanticAxisReference(std::move(source_feature), axis));
}

const FeatureId& SemanticAxisReference::source_feature() const noexcept {
  return source_feature_;
}

SemanticAxis SemanticAxisReference::axis() const noexcept {
  return axis_;
}

std::string SemanticAxisReference::node_id() const {
  return source_feature_.value() + "." + std::string(to_string(axis_));
}

SemanticAxisReference::SemanticAxisReference(FeatureId source_feature, SemanticAxis axis)
    : source_feature_(std::move(source_feature)), axis_(axis) {}

Result<SemanticSeatingPlaneReference>
SemanticSeatingPlaneReference::create(FeatureId source_feature, SemanticSeatingPlane plane) {
  const auto object_id = source_feature.empty()
                             ? std::string("semantic_seating_plane")
                             : source_feature.value() + "." + std::string(to_string(plane));

  if (source_feature.empty()) {
    return Result<SemanticSeatingPlaneReference>::failure(Error::validation(
        object_id, "semantic seating plane source feature id must not be empty"));
  }

  return Result<SemanticSeatingPlaneReference>::success(
      SemanticSeatingPlaneReference(std::move(source_feature), plane));
}

const FeatureId& SemanticSeatingPlaneReference::source_feature() const noexcept {
  return source_feature_;
}

SemanticSeatingPlane SemanticSeatingPlaneReference::plane() const noexcept {
  return plane_;
}

std::string SemanticSeatingPlaneReference::node_id() const {
  return source_feature_.value() + "." + std::string(to_string(plane_));
}

SemanticSeatingPlaneReference::SemanticSeatingPlaneReference(FeatureId source_feature,
                                                             SemanticSeatingPlane plane)
    : source_feature_(std::move(source_feature)), plane_(plane) {}

Result<SemanticEdgeReference> SemanticEdgeReference::create(FeatureId source_feature,
                                                             SemanticEdge edge) {
  const auto object_id = source_feature.empty()
                             ? std::string("semantic_edge")
                             : source_feature.value() + ".edge." + std::string(to_string(edge));

  if (source_feature.empty()) {
    return Result<SemanticEdgeReference>::failure(
        Error::validation(object_id, "semantic edge source feature id must not be empty"));
  }

  return Result<SemanticEdgeReference>::success(
      SemanticEdgeReference(std::move(source_feature), edge));
}

const FeatureId& SemanticEdgeReference::source_feature() const noexcept {
  return source_feature_;
}

SemanticEdge SemanticEdgeReference::edge() const noexcept {
  return edge_;
}

std::string SemanticEdgeReference::node_id() const {
  return source_feature_.value() + ".edge." + std::string(to_string(edge_));
}

SemanticEdgeReference::SemanticEdgeReference(FeatureId source_feature, SemanticEdge edge)
    : source_feature_(std::move(source_feature)), edge_(edge) {}

Result<SemanticVertexReference> SemanticVertexReference::create(FeatureId source_feature,
                                                                 SemanticVertex vertex) {
  const auto object_id = source_feature.empty()
                             ? std::string("semantic_vertex")
                             : source_feature.value() + ".vertex." + std::string(to_string(vertex));

  if (source_feature.empty()) {
    return Result<SemanticVertexReference>::failure(
        Error::validation(object_id, "semantic vertex source feature id must not be empty"));
  }

  return Result<SemanticVertexReference>::success(
      SemanticVertexReference(std::move(source_feature), vertex));
}

const FeatureId& SemanticVertexReference::source_feature() const noexcept {
  return source_feature_;
}

SemanticVertex SemanticVertexReference::vertex() const noexcept {
  return vertex_;
}

std::string SemanticVertexReference::node_id() const {
  return source_feature_.value() + ".vertex." + std::string(to_string(vertex_));
}

SemanticVertexReference::SemanticVertexReference(FeatureId source_feature, SemanticVertex vertex)
    : source_feature_(std::move(source_feature)), vertex_(vertex) {}

Result<DatumPlane> DatumPlane::xy(DatumPlaneId id, std::string name) {
  const auto object_id = id.empty() ? std::string("datum_plane") : id.value();

  if (id.empty()) {
    return Result<DatumPlane>::failure(
        Error::validation(object_id, "datum plane id must not be empty"));
  }

  if (name.empty()) {
    return Result<DatumPlane>::failure(
        Error::validation(object_id, "datum plane name must not be empty"));
  }

  return Result<DatumPlane>::success(DatumPlane(std::move(id), std::move(name),
                                                 Point3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
                                                 Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0}));
}

const DatumPlaneId& DatumPlane::id() const noexcept {
  return id_;
}

const std::string& DatumPlane::name() const noexcept {
  return name_;
}

Point3 DatumPlane::origin() const noexcept {
  return origin_;
}

Vector3 DatumPlane::x_axis() const noexcept {
  return x_axis_;
}

Vector3 DatumPlane::y_axis() const noexcept {
  return y_axis_;
}

Vector3 DatumPlane::normal() const noexcept {
  return normal_;
}

DatumPlane::DatumPlane(DatumPlaneId id, std::string name, Point3 origin, Vector3 x_axis,
                       Vector3 y_axis, Vector3 normal)
    : id_(std::move(id)), name_(std::move(name)), origin_(origin), x_axis_(x_axis), y_axis_(y_axis),
      normal_(normal) {}

Result<DerivedWorkplane>
DerivedWorkplane::create_on_feature_face(DatumPlaneId id, std::string name,
                                         SemanticFaceReference face_reference) {
  const auto object_id = id.empty() ? std::string("derived_workplane") : id.value();

  if (id.empty()) {
    return Result<DerivedWorkplane>::failure(
        Error::validation(object_id, "derived workplane id must not be empty"));
  }

  if (name.empty()) {
    return Result<DerivedWorkplane>::failure(
        Error::validation(object_id, "derived workplane name must not be empty"));
  }

  return Result<DerivedWorkplane>::success(
      DerivedWorkplane(std::move(id), std::move(name), std::move(face_reference)));
}

const DatumPlaneId& DerivedWorkplane::id() const noexcept {
  return id_;
}

const std::string& DerivedWorkplane::name() const noexcept {
  return name_;
}

const SemanticFaceReference& DerivedWorkplane::face_reference() const noexcept {
  return face_reference_;
}

DerivedWorkplane::DerivedWorkplane(DatumPlaneId id, std::string name,
                                   SemanticFaceReference face_reference)
    : id_(std::move(id)), name_(std::move(name)), face_reference_(std::move(face_reference)) {}

} // namespace blcad
