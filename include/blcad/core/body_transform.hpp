#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace blcad {

enum class BodyTransformKind { Translate, Rotate, UniformScale };
enum class BodyTransformCoordinateSpace { World, BodyLocal, SketchLocal, ConstructionReference };
enum class BodyTransformRotationAxisKind { Explicit, DatumAxis, ConstructionLine, SemanticEdge };

[[nodiscard]] std::string_view to_string(BodyTransformKind kind) noexcept;
[[nodiscard]] std::string_view to_string(BodyTransformCoordinateSpace space) noexcept;
[[nodiscard]] std::string_view to_string(BodyTransformRotationAxisKind kind) noexcept;

class BodyTransformRotationAxis {
public:
  [[nodiscard]] static Result<BodyTransformRotationAxis> create_explicit(Point3 origin,
                                                                         Vector3 direction);
  [[nodiscard]] static Result<BodyTransformRotationAxis> create_datum_axis(DatumAxisId axis);
  [[nodiscard]] static Result<BodyTransformRotationAxis>
  create_construction_line(ConstructionLineId line);
  [[nodiscard]] static Result<BodyTransformRotationAxis>
  create_semantic_edge(SemanticEdgeReference edge);

  [[nodiscard]] BodyTransformRotationAxisKind kind() const noexcept;
  [[nodiscard]] Point3 explicit_origin() const noexcept;
  [[nodiscard]] Vector3 explicit_direction() const noexcept;
  [[nodiscard]] const DatumAxisId& datum_axis() const noexcept;
  [[nodiscard]] const ConstructionLineId& construction_line() const noexcept;
  [[nodiscard]] const std::optional<SemanticEdgeReference>& semantic_edge() const noexcept;

private:
  BodyTransformRotationAxis(BodyTransformRotationAxisKind kind, Point3 explicit_origin,
                            Vector3 explicit_direction, DatumAxisId datum_axis,
                            ConstructionLineId construction_line,
                            std::optional<SemanticEdgeReference> semantic_edge);

  BodyTransformRotationAxisKind kind_ = BodyTransformRotationAxisKind::Explicit;
  Point3 explicit_origin_;
  Vector3 explicit_direction_;
  DatumAxisId datum_axis_;
  ConstructionLineId construction_line_;
  std::optional<SemanticEdgeReference> semantic_edge_;
};

class BodyTransform {
public:
  [[nodiscard]] static Result<BodyTransform>
  create_translate(BodyTransformId id, BodyId body, BodyTransformCoordinateSpace coordinate_space,
                   std::optional<std::string> coordinate_reference, Vector3 translation_mm,
                   bool apply_to_owned_sketches, bool apply_to_owned_construction_geometry);

  [[nodiscard]] static Result<BodyTransform>
  create_rotate(BodyTransformId id, BodyId body, BodyTransformCoordinateSpace coordinate_space,
                std::optional<std::string> coordinate_reference,
                BodyTransformRotationAxis rotation_axis, double angle_deg,
                bool apply_to_owned_sketches, bool apply_to_owned_construction_geometry);

  [[nodiscard]] static Result<BodyTransform>
  create_uniform_scale(BodyTransformId id, BodyId body,
                       BodyTransformCoordinateSpace coordinate_space,
                       std::optional<std::string> coordinate_reference, double scale_factor,
                       Point3 center, bool apply_to_owned_sketches,
                       bool apply_to_owned_construction_geometry);

  [[nodiscard]] const BodyTransformId& id() const noexcept;
  [[nodiscard]] const BodyId& body() const noexcept;
  [[nodiscard]] BodyTransformKind kind() const noexcept;
  [[nodiscard]] BodyTransformCoordinateSpace coordinate_space() const noexcept;
  [[nodiscard]] const std::optional<std::string>& coordinate_reference() const noexcept;
  [[nodiscard]] Vector3 translation_mm() const noexcept;
  [[nodiscard]] const std::optional<BodyTransformRotationAxis>& rotation_axis() const noexcept;
  [[nodiscard]] double angle_deg() const noexcept;
  [[nodiscard]] double scale_factor() const noexcept;
  [[nodiscard]] Point3 scale_center() const noexcept;
  [[nodiscard]] bool apply_to_owned_sketches() const noexcept;
  [[nodiscard]] bool apply_to_owned_construction_geometry() const noexcept;

private:
  BodyTransform(BodyTransformId id, BodyId body, BodyTransformKind kind,
                BodyTransformCoordinateSpace coordinate_space,
                std::optional<std::string> coordinate_reference, Vector3 translation_mm,
                std::optional<BodyTransformRotationAxis> rotation_axis, double angle_deg,
                double scale_factor, Point3 scale_center, bool apply_to_owned_sketches,
                bool apply_to_owned_construction_geometry);

  BodyTransformId id_;
  BodyId body_;
  BodyTransformKind kind_ = BodyTransformKind::Translate;
  BodyTransformCoordinateSpace coordinate_space_ = BodyTransformCoordinateSpace::World;
  std::optional<std::string> coordinate_reference_;
  Vector3 translation_mm_;
  std::optional<BodyTransformRotationAxis> rotation_axis_;
  double angle_deg_ = 0.0;
  double scale_factor_ = 1.0;
  Point3 scale_center_;
  bool apply_to_owned_sketches_ = false;
  bool apply_to_owned_construction_geometry_ = false;
};

} // namespace blcad
