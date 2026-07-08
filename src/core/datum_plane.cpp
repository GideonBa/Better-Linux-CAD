#include "blcad/core/datum_plane.hpp"

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
  }

  return "top";
}

Result<SemanticFaceReference> SemanticFaceReference::create(FeatureId source_feature,
                                                            SemanticFace face) {
  const auto object_id = source_feature.empty() ? std::string("semantic_face")
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

Result<DerivedWorkplane> DerivedWorkplane::create_on_feature_face(
    DatumPlaneId id, std::string name, SemanticFaceReference face_reference) {
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
