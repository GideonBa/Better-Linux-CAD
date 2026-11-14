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
class PartDocument;

enum class PathSegmentSourceKind {
  PlanarSketchCurve,
  ConstructionLine,
  Sketch3DCurve,
  SemanticGeneratedEdge
};
enum class PlanarPathCurveKind {
  Line,
  Arc,
  Spline,
  ProjectedLine,
  ReferenceGeneratedLine,
  EquationCurve
};
enum class Sketch3DPathCurveKind { Line, Polyline, Arc, Spline, Helix, EquationCurve };

class PathSegmentReference {
public:
  [[nodiscard]] static Result<PathSegmentReference> create_planar(SketchId sketch,
                                                                  SketchEntityId entity,
                                                                  PlanarPathCurveKind curve_kind,
                                                                  bool reversed = false);
  [[nodiscard]] static Result<PathSegmentReference>
  create_construction_line(ConstructionLineId line, bool reversed = false);
  [[nodiscard]] static Result<PathSegmentReference>
  create_sketch_3d(Sketch3DId sketch, SketchEntityId entity, Sketch3DPathCurveKind curve_kind,
                   bool reversed = false);
  [[nodiscard]] static Result<PathSegmentReference> create_semantic_edge(SemanticEdgeReference edge,
                                                                         bool reversed = false);

  [[nodiscard]] PathSegmentSourceKind source_kind() const noexcept;
  [[nodiscard]] const std::optional<SketchId>& planar_sketch() const noexcept;
  [[nodiscard]] const std::optional<Sketch3DId>& sketch_3d() const noexcept;
  [[nodiscard]] const std::optional<SketchEntityId>& entity() const noexcept;
  [[nodiscard]] const std::optional<ConstructionLineId>& construction_line() const noexcept;
  [[nodiscard]] const std::optional<SemanticEdgeReference>& semantic_edge() const noexcept;
  [[nodiscard]] const std::optional<PlanarPathCurveKind>& planar_curve_kind() const noexcept;
  [[nodiscard]] const std::optional<Sketch3DPathCurveKind>& sketch_3d_curve_kind() const noexcept;
  [[nodiscard]] bool reversed() const noexcept;
  [[nodiscard]] std::string source_node_id() const;
  [[nodiscard]] std::string stable_key() const;

private:
  PathSegmentReference(PathSegmentSourceKind source_kind, std::optional<SketchId> planar_sketch,
                       std::optional<Sketch3DId> sketch_3d, std::optional<SketchEntityId> entity,
                       std::optional<ConstructionLineId> construction_line,
                       std::optional<SemanticEdgeReference> semantic_edge,
                       std::optional<PlanarPathCurveKind> planar_curve_kind,
                       std::optional<Sketch3DPathCurveKind> sketch_3d_curve_kind, bool reversed);

  PathSegmentSourceKind source_kind_;
  std::optional<SketchId> planar_sketch_;
  std::optional<Sketch3DId> sketch_3d_;
  std::optional<SketchEntityId> entity_;
  std::optional<ConstructionLineId> construction_line_;
  std::optional<SemanticEdgeReference> semantic_edge_;
  std::optional<PlanarPathCurveKind> planar_curve_kind_;
  std::optional<Sketch3DPathCurveKind> sketch_3d_curve_kind_;
  bool reversed_;
};

[[nodiscard]] Result<std::size_t> validate_path_segment_source(const PartDocument&,
                                                               const PathSegmentReference&);

enum class PathClosure { Open, Closed };
enum class PathOrientationRule { ProfileNormal, MinimumTwist, FixedUpVector };
enum class PathContinuityHint { C0, G1, G2 };

[[nodiscard]] std::string_view to_string(PathSegmentSourceKind value) noexcept;
[[nodiscard]] std::string_view to_string(PlanarPathCurveKind value) noexcept;
[[nodiscard]] std::string_view to_string(Sketch3DPathCurveKind value) noexcept;
[[nodiscard]] std::string_view to_string(PathClosure value) noexcept;
[[nodiscard]] std::string_view to_string(PathOrientationRule value) noexcept;
[[nodiscard]] std::string_view to_string(PathContinuityHint value) noexcept;

enum class PathGeometryPolicyKind { LegacyFixed, CurvePreserving };

// Modeling intent only. Connection, solver and display tolerances keep their own contracts.
class PathExecutionPolicy {
public:
  [[nodiscard]] static PathExecutionPolicy legacy() noexcept;
  [[nodiscard]] static Result<PathExecutionPolicy> curve_preserving(double tolerance_mm);
  [[nodiscard]] PathGeometryPolicyKind kind() const noexcept {
    return kind_;
  }
  [[nodiscard]] const std::optional<double>& tolerance_mm() const noexcept {
    return tolerance_;
  }
  bool operator==(const PathExecutionPolicy&) const = default;

private:
  PathExecutionPolicy(PathGeometryPolicyKind kind, std::optional<double> tolerance)
      : kind_(kind), tolerance_(tolerance) {}
  PathGeometryPolicyKind kind_;
  std::optional<double> tolerance_;
};

class PathCurve {
public:
  [[nodiscard]] static Result<PathCurve>
  create(PathCurveId id, std::string name, std::vector<PathSegmentReference> segments,
         PathClosure closure, PathOrientationRule orientation_rule,
         std::optional<Vector3> fixed_up_vector = std::nullopt,
         std::optional<PathContinuityHint> continuity_hint = std::nullopt,
         double connection_tolerance_mm = 1.0e-7,
         PathExecutionPolicy execution_policy = PathExecutionPolicy::legacy());

  [[nodiscard]] const PathCurveId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<PathSegmentReference>& segments() const noexcept;
  [[nodiscard]] PathClosure closure() const noexcept;
  [[nodiscard]] PathOrientationRule orientation_rule() const noexcept;
  [[nodiscard]] const std::optional<Vector3>& fixed_up_vector() const noexcept;
  [[nodiscard]] const std::optional<PathContinuityHint>& continuity_hint() const noexcept;
  [[nodiscard]] double connection_tolerance_mm() const noexcept;
  [[nodiscard]] const PathExecutionPolicy& execution_policy() const noexcept;
  [[nodiscard]] Result<PathCurve> with_execution_policy(PathExecutionPolicy) const;

private:
  PathCurve(PathCurveId id, std::string name, std::vector<PathSegmentReference> segments,
            PathClosure closure, PathOrientationRule orientation_rule,
            std::optional<Vector3> fixed_up_vector,
            std::optional<PathContinuityHint> continuity_hint, double connection_tolerance_mm,
            PathExecutionPolicy execution_policy);

  PathCurveId id_;
  std::string name_;
  std::vector<PathSegmentReference> segments_;
  PathClosure closure_;
  PathOrientationRule orientation_rule_;
  std::optional<Vector3> fixed_up_vector_;
  std::optional<PathContinuityHint> continuity_hint_;
  double connection_tolerance_mm_;
  PathExecutionPolicy execution_policy_;
};

} // namespace blcad
