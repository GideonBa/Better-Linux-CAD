#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class ConstructionRelationType {
  PlaneOffsetFromPlane,
  LineThroughTwoPoints,
  PlaneThroughThreePoints,
  PointOnPlane,
  PointOnLine,
  PointOnGeneratedEdge,
  PointOnGeneratedVertex,
  LineOnPlane,
  PlaneParallelToPlaneThroughPoint,
  LineParallelToLineThroughPoint,
  LineParallelToGeneratedEdgeThroughPoint,
};

[[nodiscard]] std::string_view to_string(ConstructionRelationType type) noexcept;

class ConstructionRelation {
public:
  [[nodiscard]] static Result<ConstructionRelation>
  create_plane_offset_from_plane(ConstructionRelationId id, DatumPlaneId source_plane,
                                 ParameterId offset_parameter);

  [[nodiscard]] static Result<ConstructionRelation>
  create_line_through_two_points(ConstructionRelationId id, ConstructionPointId first_point,
                                 ConstructionPointId second_point);

  [[nodiscard]] static Result<ConstructionRelation>
  create_plane_through_three_points(ConstructionRelationId id, ConstructionPointId first_point,
                                    ConstructionPointId second_point, ConstructionPointId third_point);

  [[nodiscard]] static Result<ConstructionRelation>
  create_point_on_plane(ConstructionRelationId id, ConstructionPointId point,
                        DatumPlaneId source_plane);

  [[nodiscard]] static Result<ConstructionRelation>
  create_point_on_line(ConstructionRelationId id, ConstructionPointId point,
                       ConstructionLineId source_line);

  [[nodiscard]] static Result<ConstructionRelation>
  create_point_on_generated_edge(ConstructionRelationId id, ConstructionPointId point,
                                 SemanticEdgeReference generated_edge);

  [[nodiscard]] static Result<ConstructionRelation>
  create_point_on_generated_vertex(ConstructionRelationId id, ConstructionPointId point,
                                   SemanticVertexReference generated_vertex);

  [[nodiscard]] static Result<ConstructionRelation>
  create_line_on_plane(ConstructionRelationId id, ConstructionLineId source_line,
                       DatumPlaneId source_plane);

  [[nodiscard]] static Result<ConstructionRelation>
  create_plane_parallel_to_plane_through_point(ConstructionRelationId id, DatumPlaneId source_plane,
                                               ConstructionPointId through_point);

  [[nodiscard]] static Result<ConstructionRelation>
  create_line_parallel_to_line_through_point(ConstructionRelationId id,
                                             ConstructionLineId source_line,
                                             ConstructionPointId through_point);

  [[nodiscard]] static Result<ConstructionRelation>
  create_line_parallel_to_generated_edge_through_point(ConstructionRelationId id,
                                                       SemanticEdgeReference generated_edge,
                                                       ConstructionPointId through_point);

  [[nodiscard]] const ConstructionRelationId& id() const noexcept;
  [[nodiscard]] ConstructionRelationType type() const noexcept;
  [[nodiscard]] const DatumPlaneId& source_plane() const noexcept;
  [[nodiscard]] const ParameterId& offset_parameter() const noexcept;
  [[nodiscard]] const ConstructionPointId& first_point() const noexcept;
  [[nodiscard]] const ConstructionPointId& second_point() const noexcept;
  [[nodiscard]] const ConstructionPointId& third_point() const noexcept;
  [[nodiscard]] const ConstructionLineId& source_line() const noexcept;
  [[nodiscard]] const std::optional<SemanticEdgeReference>& generated_edge() const noexcept;
  [[nodiscard]] const std::optional<SemanticVertexReference>& generated_vertex() const noexcept;
  [[nodiscard]] std::vector<std::string> referenced_node_ids() const;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;

private:
  ConstructionRelation(ConstructionRelationId id, ConstructionRelationType type,
                       DatumPlaneId source_plane, ParameterId offset_parameter,
                       ConstructionPointId first_point, ConstructionPointId second_point,
                       ConstructionPointId third_point, ConstructionLineId source_line,
                       std::optional<SemanticEdgeReference> generated_edge,
                       std::optional<SemanticVertexReference> generated_vertex);

  ConstructionRelationId id_;
  ConstructionRelationType type_;
  DatumPlaneId source_plane_;
  ParameterId offset_parameter_;
  ConstructionPointId first_point_;
  ConstructionPointId second_point_;
  ConstructionPointId third_point_;
  ConstructionLineId source_line_;
  std::optional<SemanticEdgeReference> generated_edge_;
  std::optional<SemanticVertexReference> generated_vertex_;
};

enum class ConstructionPointKind {
  Explicit,
  OnGeneratedEdge,
  OnGeneratedVertex,
};

[[nodiscard]] std::string_view to_string(ConstructionPointKind kind) noexcept;

enum class ConstructionLineKind {
  Explicit,
  ThroughTwoPoints,
  ParallelToLineThroughPoint,
  ParallelToGeneratedEdgeThroughPoint,
};

[[nodiscard]] std::string_view to_string(ConstructionLineKind kind) noexcept;

enum class ConstructionPlaneKind {
  Explicit,
  OffsetFromPlane,
  ThroughThreePoints,
  ParallelToPlaneThroughPoint,
};

[[nodiscard]] std::string_view to_string(ConstructionPlaneKind kind) noexcept;

class ConstructionPoint {
public:
  [[nodiscard]] static Result<ConstructionPoint>
  create_explicit(ConstructionPointId id, std::string name, Point3 position,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] static Result<ConstructionPoint>
  create_on_generated_edge(ConstructionPointId id, std::string name, ConstructionRelation relation);

  [[nodiscard]] static Result<ConstructionPoint>
  create_on_generated_vertex(ConstructionPointId id, std::string name, ConstructionRelation relation);

  [[nodiscard]] const ConstructionPointId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] ConstructionPointKind kind() const noexcept;
  [[nodiscard]] Point3 position() const noexcept;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;
  [[nodiscard]] const std::optional<ConstructionRelation>& relation() const noexcept;

private:
  ConstructionPoint(ConstructionPointId id, std::string name, ConstructionPointKind kind,
                    Point3 position, std::vector<ParameterId> parameter_dependencies,
                    std::optional<ConstructionRelation> relation);

  ConstructionPointId id_;
  std::string name_;
  ConstructionPointKind kind_;
  Point3 position_;
  std::vector<ParameterId> parameter_dependencies_;
  std::optional<ConstructionRelation> relation_;
};

class ConstructionLine {
public:
  [[nodiscard]] static Result<ConstructionLine>
  create_explicit(ConstructionLineId id, std::string name, Point3 point, Vector3 direction,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] static Result<ConstructionLine>
  create_through_two_points(ConstructionLineId id, std::string name,
                            ConstructionRelation relation);

  [[nodiscard]] static Result<ConstructionLine>
  create_parallel_to_line_through_point(ConstructionLineId id, std::string name,
                                        ConstructionRelation relation);

  [[nodiscard]] static Result<ConstructionLine>
  create_parallel_to_generated_edge_through_point(ConstructionLineId id, std::string name,
                                                  ConstructionRelation relation);

  [[nodiscard]] const ConstructionLineId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] ConstructionLineKind kind() const noexcept;
  [[nodiscard]] Point3 point() const noexcept;
  [[nodiscard]] Vector3 direction() const noexcept;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;
  [[nodiscard]] const std::optional<ConstructionRelation>& relation() const noexcept;

private:
  ConstructionLine(ConstructionLineId id, std::string name, ConstructionLineKind kind, Point3 point,
                   Vector3 direction, std::vector<ParameterId> parameter_dependencies,
                   std::optional<ConstructionRelation> relation);

  ConstructionLineId id_;
  std::string name_;
  ConstructionLineKind kind_;
  Point3 point_;
  Vector3 direction_;
  std::vector<ParameterId> parameter_dependencies_;
  std::optional<ConstructionRelation> relation_;
};

class ConstructionPlane {
public:
  [[nodiscard]] static Result<ConstructionPlane>
  create_explicit(ConstructionPlaneId id, std::string name, Point3 origin, Vector3 x_axis,
                  Vector3 y_axis, Vector3 normal,
                  std::vector<ParameterId> parameter_dependencies = {});

  [[nodiscard]] static Result<ConstructionPlane>
  create_offset_from_plane(ConstructionPlaneId id, std::string name, ConstructionRelation relation);

  [[nodiscard]] static Result<ConstructionPlane>
  create_through_three_points(ConstructionPlaneId id, std::string name,
                              ConstructionRelation relation);

  [[nodiscard]] static Result<ConstructionPlane>
  create_parallel_to_plane_through_point(ConstructionPlaneId id, std::string name,
                                         ConstructionRelation relation);

  [[nodiscard]] const ConstructionPlaneId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] ConstructionPlaneKind kind() const noexcept;
  [[nodiscard]] Point3 origin() const noexcept;
  [[nodiscard]] Vector3 x_axis() const noexcept;
  [[nodiscard]] Vector3 y_axis() const noexcept;
  [[nodiscard]] Vector3 normal() const noexcept;
  [[nodiscard]] DatumPlaneId workplane_id() const;
  [[nodiscard]] const std::vector<ParameterId>& parameter_dependencies() const noexcept;
  [[nodiscard]] const std::optional<ConstructionRelation>& relation() const noexcept;

private:
  ConstructionPlane(ConstructionPlaneId id, std::string name, ConstructionPlaneKind kind,
                    Point3 origin, Vector3 x_axis, Vector3 y_axis, Vector3 normal,
                    std::vector<ParameterId> parameter_dependencies,
                    std::optional<ConstructionRelation> relation);

  ConstructionPlaneId id_;
  std::string name_;
  ConstructionPlaneKind kind_;
  Point3 origin_;
  Vector3 x_axis_;
  Vector3 y_axis_;
  Vector3 normal_;
  std::vector<ParameterId> parameter_dependencies_;
  std::optional<ConstructionRelation> relation_;
};

} // namespace blcad
