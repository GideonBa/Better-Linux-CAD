#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <string>
#include <string_view>

namespace blcad {

enum class SemanticFace {
  Top,
  Bottom,
  Right,
  Left,
  Front,
  Back,
};

[[nodiscard]] std::string_view to_string(SemanticFace face) noexcept;

enum class SemanticEdge {
  TopFront,
  TopBack,
  TopRight,
  TopLeft,
  BottomFront,
  BottomBack,
  BottomRight,
  BottomLeft,
  FrontRight,
  FrontLeft,
  BackRight,
  BackLeft,
};

[[nodiscard]] std::string_view to_string(SemanticEdge edge) noexcept;

enum class SemanticVertex {
  TopFrontRight,
  TopFrontLeft,
  TopBackRight,
  TopBackLeft,
  BottomFrontRight,
  BottomFrontLeft,
  BottomBackRight,
  BottomBackLeft,
};

[[nodiscard]] std::string_view to_string(SemanticVertex vertex) noexcept;

// The first semantic axis family exposes one primary generated axis per
// supported feature. Additional feature-specific axis identities can extend
// this enum without exposing transient kernel topology ids.
enum class SemanticAxis {
  Primary,
};

[[nodiscard]] std::string_view to_string(SemanticAxis axis) noexcept;

// The first semantic seating-plane family exposes the oriented source seat of
// one supported circular feature. It is constructive feature intent, not an
// OCCT face id.
enum class SemanticSeatingPlane {
  Primary,
};

[[nodiscard]] std::string_view to_string(SemanticSeatingPlane plane) noexcept;

class SemanticFaceReference {
public:
  [[nodiscard]] static Result<SemanticFaceReference> create(FeatureId source_feature,
                                                            SemanticFace face);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] SemanticFace face() const noexcept;

private:
  SemanticFaceReference(FeatureId source_feature, SemanticFace face);

  FeatureId source_feature_;
  SemanticFace face_;
};

class SemanticAxisReference {
public:
  [[nodiscard]] static Result<SemanticAxisReference>
  create(FeatureId source_feature, SemanticAxis axis = SemanticAxis::Primary);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] SemanticAxis axis() const noexcept;
  [[nodiscard]] std::string node_id() const;

private:
  SemanticAxisReference(FeatureId source_feature, SemanticAxis axis);

  FeatureId source_feature_;
  SemanticAxis axis_;
};

class SemanticSeatingPlaneReference {
public:
  [[nodiscard]] static Result<SemanticSeatingPlaneReference>
  create(FeatureId source_feature,
         SemanticSeatingPlane plane = SemanticSeatingPlane::Primary);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] SemanticSeatingPlane plane() const noexcept;
  [[nodiscard]] std::string node_id() const;

private:
  SemanticSeatingPlaneReference(FeatureId source_feature, SemanticSeatingPlane plane);

  FeatureId source_feature_;
  SemanticSeatingPlane plane_;
};

class SemanticEdgeReference {
public:
  [[nodiscard]] static Result<SemanticEdgeReference> create(FeatureId source_feature,
                                                            SemanticEdge edge);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] SemanticEdge edge() const noexcept;
  [[nodiscard]] std::string node_id() const;

private:
  SemanticEdgeReference(FeatureId source_feature, SemanticEdge edge);

  FeatureId source_feature_;
  SemanticEdge edge_;
};

class SemanticVertexReference {
public:
  [[nodiscard]] static Result<SemanticVertexReference> create(FeatureId source_feature,
                                                              SemanticVertex vertex);

  [[nodiscard]] const FeatureId& source_feature() const noexcept;
  [[nodiscard]] SemanticVertex vertex() const noexcept;
  [[nodiscard]] std::string node_id() const;

private:
  SemanticVertexReference(FeatureId source_feature, SemanticVertex vertex);

  FeatureId source_feature_;
  SemanticVertex vertex_;
};

class DatumPlane {
public:
  [[nodiscard]] static Result<DatumPlane> xy(DatumPlaneId id = DatumPlaneId("datum.xy"),
                                             std::string name = "XY");

  [[nodiscard]] const DatumPlaneId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] Point3 origin() const noexcept;
  [[nodiscard]] Vector3 x_axis() const noexcept;
  [[nodiscard]] Vector3 y_axis() const noexcept;
  [[nodiscard]] Vector3 normal() const noexcept;

private:
  DatumPlane(DatumPlaneId id, std::string name, Point3 origin, Vector3 x_axis, Vector3 y_axis,
             Vector3 normal);

  DatumPlaneId id_;
  std::string name_;
  Point3 origin_;
  Vector3 x_axis_;
  Vector3 y_axis_;
  Vector3 normal_;
};

class DerivedWorkplane {
public:
  [[nodiscard]] static Result<DerivedWorkplane>
  create_on_feature_face(DatumPlaneId id, std::string name, SemanticFaceReference face_reference);

  [[nodiscard]] const DatumPlaneId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const SemanticFaceReference& face_reference() const noexcept;

private:
  DerivedWorkplane(DatumPlaneId id, std::string name, SemanticFaceReference face_reference);

  DatumPlaneId id_;
  std::string name_;
  SemanticFaceReference face_reference_;
};

} // namespace blcad
