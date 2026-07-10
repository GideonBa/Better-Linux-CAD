#pragma once

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class ProjectedSketchPointSource { ConstructionPoint, SemanticVertex };
[[nodiscard]] std::string_view to_string(ProjectedSketchPointSource source) noexcept;

enum class ProjectedSketchLineSource { ConstructionLine, SemanticEdge };
[[nodiscard]] std::string_view to_string(ProjectedSketchLineSource source) noexcept;

enum class SketchReferenceTargetKind { LineSegment, LineSegmentStart, LineSegmentEnd, ProjectedPoint, ProjectedLine };
[[nodiscard]] std::string_view to_string(SketchReferenceTargetKind kind) noexcept;

enum class SketchConstraintKind { CoincidentToProjectedPoint, ParallelToProjectedLine, CollinearWithProjectedLine };
[[nodiscard]] std::string_view to_string(SketchConstraintKind kind) noexcept;

enum class SketchGeometricConstraintKind { Fixed, Horizontal, Vertical, Parallel, Perpendicular, EqualLength };
[[nodiscard]] std::string_view to_string(SketchGeometricConstraintKind kind) noexcept;

enum class SketchDrivingDimensionKind { HorizontalDistance, VerticalDistance, AlignedDistance, PointToPointDistance };
[[nodiscard]] std::string_view to_string(SketchDrivingDimensionKind kind) noexcept;

enum class SketchTrimExtendOperationKind { Trim, Extend };
[[nodiscard]] std::string_view to_string(SketchTrimExtendOperationKind kind) noexcept;

enum class SketchTangentContinuityKind { Tangent };
[[nodiscard]] std::string_view to_string(SketchTangentContinuityKind kind) noexcept;

class SketchReferenceTarget {
public:
  [[nodiscard]] static Result<SketchReferenceTarget> create_line_segment(SketchEntityId entity);
  [[nodiscard]] static Result<SketchReferenceTarget> create_line_segment_start(SketchEntityId entity);
  [[nodiscard]] static Result<SketchReferenceTarget> create_line_segment_end(SketchEntityId entity);
  [[nodiscard]] static Result<SketchReferenceTarget> create_projected_point(SketchEntityId entity);
  [[nodiscard]] static Result<SketchReferenceTarget> create_projected_line(SketchEntityId entity);
  [[nodiscard]] SketchReferenceTargetKind kind() const noexcept;
  [[nodiscard]] const SketchEntityId& entity() const noexcept;
  SketchReferenceTarget(SketchReferenceTargetKind kind, SketchEntityId entity);
private:
  SketchReferenceTargetKind kind_;
  SketchEntityId entity_;
};

class SketchConstraint {
public:
  [[nodiscard]] static Result<SketchConstraint> create_coincident_to_projected_point(SketchConstraintId id, SketchReferenceTarget constrained_point, SketchReferenceTarget projected_point);
  [[nodiscard]] static Result<SketchConstraint> create_parallel_to_projected_line(SketchConstraintId id, SketchReferenceTarget constrained_line, SketchReferenceTarget projected_line);
  [[nodiscard]] static Result<SketchConstraint> create_collinear_with_projected_line(SketchConstraintId id, SketchReferenceTarget constrained_line, SketchReferenceTarget projected_line);
  [[nodiscard]] const SketchConstraintId& id() const noexcept;
  [[nodiscard]] SketchConstraintKind kind() const noexcept;
  [[nodiscard]] const SketchReferenceTarget& constrained_target() const noexcept;
  [[nodiscard]] const SketchReferenceTarget& reference_target() const noexcept;
private:
  SketchConstraint(SketchConstraintId id, SketchConstraintKind kind, SketchReferenceTarget constrained_target, SketchReferenceTarget reference_target);
  SketchConstraintId id_;
  SketchConstraintKind kind_;
  SketchReferenceTarget constrained_target_;
  SketchReferenceTarget reference_target_;
};

class SketchGeometricConstraint {
public:
  [[nodiscard]] static Result<SketchGeometricConstraint> create_fixed(SketchConstraintId id, SketchReferenceTarget target);
  [[nodiscard]] static Result<SketchGeometricConstraint> create_horizontal(SketchConstraintId id, SketchReferenceTarget line);
  [[nodiscard]] static Result<SketchGeometricConstraint> create_vertical(SketchConstraintId id, SketchReferenceTarget line);
  [[nodiscard]] static Result<SketchGeometricConstraint> create_parallel(SketchConstraintId id, SketchReferenceTarget first_line, SketchReferenceTarget second_line);
  [[nodiscard]] static Result<SketchGeometricConstraint> create_perpendicular(SketchConstraintId id, SketchReferenceTarget first_line, SketchReferenceTarget second_line);
  [[nodiscard]] static Result<SketchGeometricConstraint> create_equal_length(SketchConstraintId id, SketchReferenceTarget first_line, SketchReferenceTarget second_line);
  [[nodiscard]] const SketchConstraintId& id() const noexcept;
  [[nodiscard]] SketchGeometricConstraintKind kind() const noexcept;
  [[nodiscard]] const SketchReferenceTarget& first_target() const noexcept;
  [[nodiscard]] const std::optional<SketchReferenceTarget>& second_target() const noexcept;
private:
  SketchGeometricConstraint(SketchConstraintId id, SketchGeometricConstraintKind kind, SketchReferenceTarget first_target, std::optional<SketchReferenceTarget> second_target);
  SketchConstraintId id_;
  SketchGeometricConstraintKind kind_;
  SketchReferenceTarget first_target_;
  std::optional<SketchReferenceTarget> second_target_;
};

class SketchDrivingDimension {
public:
  [[nodiscard]] static Result<SketchDrivingDimension> create_horizontal_distance(SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point, ParameterId parameter);
  [[nodiscard]] static Result<SketchDrivingDimension> create_vertical_distance(SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point, ParameterId parameter);
  [[nodiscard]] static Result<SketchDrivingDimension> create_aligned_distance(SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point, ParameterId parameter);
  [[nodiscard]] static Result<SketchDrivingDimension> create_point_to_point_distance(SketchDimensionId id, SketchReferenceTarget first_point, SketchReferenceTarget second_point, ParameterId parameter);
  [[nodiscard]] const SketchDimensionId& id() const noexcept;
  [[nodiscard]] SketchDrivingDimensionKind kind() const noexcept;
  [[nodiscard]] const SketchReferenceTarget& first_target() const noexcept;
  [[nodiscard]] const SketchReferenceTarget& second_target() const noexcept;
  [[nodiscard]] const ParameterId& parameter() const noexcept;
private:
  SketchDrivingDimension(SketchDimensionId id, SketchDrivingDimensionKind kind, SketchReferenceTarget first_target, SketchReferenceTarget second_target, ParameterId parameter);
  SketchDimensionId id_;
  SketchDrivingDimensionKind kind_;
  SketchReferenceTarget first_target_;
  SketchReferenceTarget second_target_;
  ParameterId parameter_;
};

class ProjectedSketchPoint {
public:
  [[nodiscard]] static Result<ProjectedSketchPoint> create_from_construction_point(SketchEntityId id, ConstructionPointId point);
  [[nodiscard]] static Result<ProjectedSketchPoint> create_from_semantic_vertex(SketchEntityId id, SemanticVertexReference vertex);
  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] ProjectedSketchPointSource source() const noexcept;
  [[nodiscard]] const ConstructionPointId& construction_point() const noexcept;
  [[nodiscard]] const std::optional<SemanticVertexReference>& semantic_vertex() const noexcept;
  [[nodiscard]] std::string referenced_node_id() const;
private:
  ProjectedSketchPoint(SketchEntityId id, ProjectedSketchPointSource source, ConstructionPointId construction_point, std::optional<SemanticVertexReference> semantic_vertex);
  SketchEntityId id_;
  ProjectedSketchPointSource source_;
  ConstructionPointId construction_point_;
  std::optional<SemanticVertexReference> semantic_vertex_;
};

class ProjectedSketchLine {
public:
  [[nodiscard]] static Result<ProjectedSketchLine> create_from_construction_line(SketchEntityId id, ConstructionLineId line);
  [[nodiscard]] static Result<ProjectedSketchLine> create_from_semantic_edge(SketchEntityId id, SemanticEdgeReference edge);
  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] ProjectedSketchLineSource source() const noexcept;
  [[nodiscard]] const ConstructionLineId& construction_line() const noexcept;
  [[nodiscard]] const std::optional<SemanticEdgeReference>& semantic_edge() const noexcept;
  [[nodiscard]] std::string referenced_node_id() const;
private:
  ProjectedSketchLine(SketchEntityId id, ProjectedSketchLineSource source, ConstructionLineId construction_line, std::optional<SemanticEdgeReference> semantic_edge);
  SketchEntityId id_;
  ProjectedSketchLineSource source_;
  ConstructionLineId construction_line_;
  std::optional<SemanticEdgeReference> semantic_edge_;
};

class ReferenceGeneratedLine {
public:
  [[nodiscard]] static Result<ReferenceGeneratedLine> create_from_projected_point_constraints(SketchEntityId id, SketchConstraintId start_constraint, SketchConstraintId end_constraint);
  [[nodiscard]] static Result<ReferenceGeneratedLine> create_with_projected_line_direction(SketchEntityId id, SketchConstraintId start_constraint, SketchConstraintId end_constraint, SketchConstraintId direction_constraint);
  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchConstraintId& start_constraint() const noexcept;
  [[nodiscard]] const SketchConstraintId& end_constraint() const noexcept;
  [[nodiscard]] const std::optional<SketchConstraintId>& direction_constraint() const noexcept;
private:
  ReferenceGeneratedLine(SketchEntityId id, SketchConstraintId start_constraint, SketchConstraintId end_constraint, std::optional<SketchConstraintId> direction_constraint);
  SketchEntityId id_;
  SketchConstraintId start_constraint_;
  SketchConstraintId end_constraint_;
  std::optional<SketchConstraintId> direction_constraint_;
};

class LineSegment {
public:
  [[nodiscard]] static Result<LineSegment> create(SketchEntityId id, Point2 start, Point2 end);
  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] Point2 start() const noexcept;
  [[nodiscard]] Point2 end() const noexcept;
private:
  LineSegment(SketchEntityId id, Point2 start, Point2 end);
  SketchEntityId id_;
  Point2 start_;
  Point2 end_;
};

class ArcSegment {
public:
  [[nodiscard]] static Result<ArcSegment> create_three_point(SketchEntityId id, Point2 start, Point2 mid, Point2 end);
  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] Point2 start() const noexcept;
  [[nodiscard]] Point2 mid() const noexcept;
  [[nodiscard]] Point2 end() const noexcept;
private:
  ArcSegment(SketchEntityId id, Point2 start, Point2 mid, Point2 end);
  SketchEntityId id_;
  Point2 start_;
  Point2 mid_;
  Point2 end_;
};

class SplineSegment {
public:
  [[nodiscard]] static Result<SplineSegment> create_cubic_bezier(SketchEntityId id, Point2 start, Point2 control1, Point2 control2, Point2 end);
  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] Point2 start() const noexcept;
  [[nodiscard]] Point2 control1() const noexcept;
  [[nodiscard]] Point2 control2() const noexcept;
  [[nodiscard]] Point2 end() const noexcept;
private:
  SplineSegment(SketchEntityId id, Point2 start, Point2 control1, Point2 control2, Point2 end);
  SketchEntityId id_;
  Point2 start_;
  Point2 control1_;
  Point2 control2_;
  Point2 end_;
};

class SketchTrimExtendOperation {
public:
  [[nodiscard]] static Result<SketchTrimExtendOperation> create_trim(SketchTrimOperationId id, SketchEntityId target_entity, Point2 replacement_endpoint);
  [[nodiscard]] static Result<SketchTrimExtendOperation> create_extend(SketchTrimOperationId id, SketchEntityId target_entity, Point2 replacement_endpoint);
  [[nodiscard]] const SketchTrimOperationId& id() const noexcept;
  [[nodiscard]] SketchTrimExtendOperationKind kind() const noexcept;
  [[nodiscard]] const SketchEntityId& target_entity() const noexcept;
  [[nodiscard]] Point2 replacement_endpoint() const noexcept;
private:
  SketchTrimExtendOperation(SketchTrimOperationId id, SketchTrimExtendOperationKind kind, SketchEntityId target_entity, Point2 replacement_endpoint);
  SketchTrimOperationId id_;
  SketchTrimExtendOperationKind kind_;
  SketchEntityId target_entity_;
  Point2 replacement_endpoint_;
};

class SketchTangentContinuity {
public:
  [[nodiscard]] static Result<SketchTangentContinuity> create_tangent(SketchConstraintId id, SketchEntityId first_entity, SketchEntityId second_entity);
  [[nodiscard]] const SketchConstraintId& id() const noexcept;
  [[nodiscard]] SketchTangentContinuityKind kind() const noexcept;
  [[nodiscard]] const SketchEntityId& first_entity() const noexcept;
  [[nodiscard]] const SketchEntityId& second_entity() const noexcept;
private:
  SketchTangentContinuity(SketchConstraintId id, SketchTangentContinuityKind kind, SketchEntityId first_entity, SketchEntityId second_entity);
  SketchConstraintId id_;
  SketchTangentContinuityKind kind_;
  SketchEntityId first_entity_;
  SketchEntityId second_entity_;
};

class RectangleProfile { public: [[nodiscard]] static Result<RectangleProfile> create(ProfileId id, ParameterId width_parameter, ParameterId height_parameter, Point2 center = {}); [[nodiscard]] const ProfileId& id() const noexcept; [[nodiscard]] Point2 center() const noexcept; [[nodiscard]] const ParameterId& width_parameter() const noexcept; [[nodiscard]] const ParameterId& height_parameter() const noexcept; private: RectangleProfile(ProfileId id, ParameterId width_parameter, ParameterId height_parameter, Point2 center); ProfileId id_; ParameterId width_parameter_; ParameterId height_parameter_; Point2 center_; };
class CircleProfile { public: [[nodiscard]] static Result<CircleProfile> create(ProfileId id, ParameterId diameter_parameter, Point2 center = {}); [[nodiscard]] const ProfileId& id() const noexcept; [[nodiscard]] Point2 center() const noexcept; [[nodiscard]] const ParameterId& diameter_parameter() const noexcept; private: CircleProfile(ProfileId id, ParameterId diameter_parameter, Point2 center); ProfileId id_; ParameterId diameter_parameter_; Point2 center_; };
class ClosedProfile { public: [[nodiscard]] static Result<ClosedProfile> create(ProfileId id, std::vector<SketchEntityId> line_segments); [[nodiscard]] const ProfileId& id() const noexcept; [[nodiscard]] const std::vector<SketchEntityId>& line_segments() const noexcept; private: ClosedProfile(ProfileId id, std::vector<SketchEntityId> line_segments); ProfileId id_; std::vector<SketchEntityId> line_segments_; };
class ArcClosedProfile { public: [[nodiscard]] static Result<ArcClosedProfile> create(ProfileId id, std::vector<SketchEntityId> curve_segments); [[nodiscard]] const ProfileId& id() const noexcept; [[nodiscard]] const std::vector<SketchEntityId>& curve_segments() const noexcept; private: ArcClosedProfile(ProfileId id, std::vector<SketchEntityId> curve_segments); ProfileId id_; std::vector<SketchEntityId> curve_segments_; };
class CompositeClosedProfile { public: [[nodiscard]] static Result<CompositeClosedProfile> create(ProfileId id, std::vector<SketchEntityId> outer_contour, std::vector<std::vector<SketchEntityId>> inner_contours); [[nodiscard]] const ProfileId& id() const noexcept; [[nodiscard]] const std::vector<SketchEntityId>& outer_contour() const noexcept; [[nodiscard]] const std::vector<std::vector<SketchEntityId>>& inner_contours() const noexcept; private: CompositeClosedProfile(ProfileId id, std::vector<SketchEntityId> outer_contour, std::vector<std::vector<SketchEntityId>> inner_contours); ProfileId id_; std::vector<SketchEntityId> outer_contour_; std::vector<std::vector<SketchEntityId>> inner_contours_; };

class Sketch {
public:
  [[nodiscard]] static Result<Sketch> create(SketchId id, std::string name, DatumPlaneId workplane);
  [[nodiscard]] Result<std::size_t> add_entity(LineSegment line_segment);
  [[nodiscard]] Result<std::size_t> add_entity(ArcSegment arc_segment);
  [[nodiscard]] Result<std::size_t> add_entity(SplineSegment spline_segment);
  [[nodiscard]] Result<std::size_t> add_reference(ProjectedSketchPoint point_reference);
  [[nodiscard]] Result<std::size_t> add_reference(ProjectedSketchLine line_reference);
  [[nodiscard]] Result<std::size_t> add_reference(ReferenceGeneratedLine line_reference);
  [[nodiscard]] Result<std::size_t> add_constraint(SketchConstraint constraint);
  [[nodiscard]] Result<std::size_t> add_constraint(SketchGeometricConstraint constraint);
  [[nodiscard]] Result<std::size_t> add_dimension(SketchDrivingDimension dimension);
  [[nodiscard]] Result<std::size_t> add_trim_extend_operation(SketchTrimExtendOperation operation);
  [[nodiscard]] Result<std::size_t> add_tangent_continuity(SketchTangentContinuity continuity);
  [[nodiscard]] Result<std::size_t> add_profile(RectangleProfile profile);
  [[nodiscard]] Result<std::size_t> add_profile(CircleProfile profile);
  [[nodiscard]] Result<std::size_t> add_profile(ClosedProfile profile);
  [[nodiscard]] Result<std::size_t> add_profile(ArcClosedProfile profile);
  [[nodiscard]] Result<std::size_t> add_profile(CompositeClosedProfile profile);
  [[nodiscard]] const SketchId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const DatumPlaneId& workplane() const noexcept;
  [[nodiscard]] const std::vector<LineSegment>& line_segments() const noexcept;
  [[nodiscard]] const std::vector<ArcSegment>& arc_segments() const noexcept;
  [[nodiscard]] const std::vector<SplineSegment>& spline_segments() const noexcept;
  [[nodiscard]] const std::vector<ProjectedSketchPoint>& projected_points() const noexcept;
  [[nodiscard]] const std::vector<ProjectedSketchLine>& projected_lines() const noexcept;
  [[nodiscard]] const std::vector<ReferenceGeneratedLine>& reference_generated_lines() const noexcept;
  [[nodiscard]] const std::vector<SketchConstraint>& constraints() const noexcept;
  [[nodiscard]] const std::vector<SketchGeometricConstraint>& geometric_constraints() const noexcept;
  [[nodiscard]] const std::vector<SketchDrivingDimension>& driving_dimensions() const noexcept;
  [[nodiscard]] const std::vector<SketchTrimExtendOperation>& trim_extend_operations() const noexcept;
  [[nodiscard]] const std::vector<SketchTangentContinuity>& tangent_continuities() const noexcept;
  [[nodiscard]] const std::vector<RectangleProfile>& rectangle_profiles() const noexcept;
  [[nodiscard]] const std::vector<CircleProfile>& circle_profiles() const noexcept;
  [[nodiscard]] const std::vector<ClosedProfile>& closed_profiles() const noexcept;
  [[nodiscard]] const std::vector<ArcClosedProfile>& arc_closed_profiles() const noexcept;
  [[nodiscard]] const std::vector<CompositeClosedProfile>& composite_closed_profiles() const noexcept;
  [[nodiscard]] std::size_t profile_count() const noexcept;
  [[nodiscard]] const LineSegment* find_line_segment(SketchEntityId id) const noexcept;
  [[nodiscard]] const ArcSegment* find_arc_segment(SketchEntityId id) const noexcept;
  [[nodiscard]] const SplineSegment* find_spline_segment(SketchEntityId id) const noexcept;
  [[nodiscard]] const ProjectedSketchPoint* find_projected_point(SketchEntityId id) const noexcept;
  [[nodiscard]] const ProjectedSketchLine* find_projected_line(SketchEntityId id) const noexcept;
  [[nodiscard]] const ReferenceGeneratedLine* find_reference_generated_line(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchConstraint* find_constraint(SketchConstraintId id) const noexcept;
  [[nodiscard]] const SketchGeometricConstraint* find_geometric_constraint(SketchConstraintId id) const noexcept;
  [[nodiscard]] const SketchDrivingDimension* find_driving_dimension(SketchDimensionId id) const noexcept;
  [[nodiscard]] const SketchTrimExtendOperation* find_trim_extend_operation(SketchTrimOperationId id) const noexcept;
  [[nodiscard]] const SketchTangentContinuity* find_tangent_continuity(SketchConstraintId id) const noexcept;
  [[nodiscard]] const RectangleProfile* find_rectangle_profile(ProfileId id) const noexcept;
  [[nodiscard]] const CircleProfile* find_circle_profile(ProfileId id) const noexcept;
  [[nodiscard]] const ClosedProfile* find_closed_profile(ProfileId id) const noexcept;
  [[nodiscard]] const ArcClosedProfile* find_arc_closed_profile(ProfileId id) const noexcept;
  [[nodiscard]] const CompositeClosedProfile* find_composite_closed_profile(ProfileId id) const noexcept;
  [[nodiscard]] Result<std::vector<Point2>> closed_profile_vertices(const ClosedProfile& profile) const;
  [[nodiscard]] Result<std::vector<Point2>> arc_closed_profile_vertices(const ArcClosedProfile& profile) const;
private:
  Sketch(SketchId id, std::string name, DatumPlaneId workplane);
  [[nodiscard]] bool has_entity_id(const SketchEntityId& id) const noexcept;
  [[nodiscard]] bool has_constraint_id(const SketchConstraintId& id) const noexcept;
  [[nodiscard]] bool has_dimension_id(const SketchDimensionId& id) const noexcept;
  [[nodiscard]] bool has_profile_id(const ProfileId& id) const noexcept;
  [[nodiscard]] Result<std::size_t> validate_constraint_target(const SketchReferenceTarget& target, const std::string& object_id) const;
  [[nodiscard]] Result<std::size_t> validate_point_target(const SketchReferenceTarget& target, const std::string& object_id) const;
  [[nodiscard]] Result<std::size_t> validate_explicit_line_target(const SketchReferenceTarget& target, const std::string& object_id) const;
  [[nodiscard]] Result<std::vector<Point2>> validate_closed_profile(const ClosedProfile& profile) const;
  [[nodiscard]] Result<std::vector<Point2>> validate_arc_closed_profile(const ArcClosedProfile& profile) const;
  [[nodiscard]] Result<std::size_t> validate_composite_closed_profile(const CompositeClosedProfile& profile) const;
  SketchId id_; std::string name_; DatumPlaneId workplane_;
  std::vector<LineSegment> line_segments_; std::vector<ArcSegment> arc_segments_; std::vector<SplineSegment> spline_segments_;
  std::vector<ProjectedSketchPoint> projected_points_; std::vector<ProjectedSketchLine> projected_lines_; std::vector<ReferenceGeneratedLine> reference_generated_lines_;
  std::vector<SketchConstraint> constraints_; std::vector<SketchGeometricConstraint> geometric_constraints_; std::vector<SketchDrivingDimension> driving_dimensions_;
  std::vector<SketchTrimExtendOperation> trim_extend_operations_; std::vector<SketchTangentContinuity> tangent_continuities_;
  std::vector<RectangleProfile> rectangle_profiles_; std::vector<CircleProfile> circle_profiles_; std::vector<ClosedProfile> closed_profiles_; std::vector<ArcClosedProfile> arc_closed_profiles_; std::vector<CompositeClosedProfile> composite_closed_profiles_;
};

} // namespace blcad
