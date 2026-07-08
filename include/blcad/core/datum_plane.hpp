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
};

[[nodiscard]] std::string_view to_string(SemanticFace face) noexcept;

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
