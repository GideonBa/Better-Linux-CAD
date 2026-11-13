#pragma once
#include "blcad/core/id.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/path_curve.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch_3d_reference.hpp"
#include "blcad/core/spatial.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace blcad {
class PartDocument;
class Sketch3D;
enum class SpatialCurveLocation { Whole, Start, End };
struct SpatialLocalCurve {
  SketchEntityId entity;
  SpatialCurveLocation location;
};
struct SpatialExternalCurve {
  PathSegmentReference reference;
  SpatialCurveLocation location;
};
using SpatialConstraintTargetSource =
    std::variant<SketchPointReference3D, SpatialLocalCurve, SpatialExternalCurve, AxisReference,
                 PlaneReference, FaceReference>;
enum class SpatialTargetKind { Point, Curve, Axis, Plane, Face };
class SpatialConstraintTarget {
public:
  [[nodiscard]] static Result<SpatialConstraintTarget> point(SketchPointReference3D reference);
  [[nodiscard]] static Result<SpatialConstraintTarget> local_point(SketchEntityId point);
  [[nodiscard]] static Result<SpatialConstraintTarget>
  curve(SketchEntityId entity, SpatialCurveLocation location = SpatialCurveLocation::Whole);
  [[nodiscard]] static Result<SpatialConstraintTarget>
  external_curve(PathSegmentReference curve,
                 SpatialCurveLocation location = SpatialCurveLocation::Whole);
  [[nodiscard]] static Result<SpatialConstraintTarget> axis(AxisReference reference);
  [[nodiscard]] static Result<SpatialConstraintTarget> plane(PlaneReference reference);
  [[nodiscard]] static Result<SpatialConstraintTarget> face(FaceReference reference);
  [[nodiscard]] SpatialTargetKind kind() const noexcept;
  [[nodiscard]] const SpatialConstraintTargetSource& source() const noexcept;
  [[nodiscard]] SpatialCurveLocation curve_location() const noexcept;
  [[nodiscard]] std::optional<std::string> referenced_node_id() const;
  [[nodiscard]] std::optional<SketchEntityId> local_entity() const;

private:
  explicit SpatialConstraintTarget(SpatialConstraintTargetSource source);
  SpatialConstraintTargetSource source_;
};
enum class SpatialConstraintKind {
  Coincident,
  Fixed,
  AxisParallel,
  PlaneParallel,
  PointOnLine,
  PointOnPlane,
  PointOnCurve,
  PointOnFace,
  Parallel,
  Perpendicular,
  Tangent,
  Smooth,
  Distance,
  Angle,
  Radius,
  Length
};
enum class SpatialAngleConvention { Unsigned, OrientedAboutAxis };
class SpatialDimensionValue {
public:
  [[nodiscard]] static Result<SpatialDimensionValue> literal(Quantity quantity);
  [[nodiscard]] static Result<SpatialDimensionValue> parameter(ParameterId parameter);
  [[nodiscard]] const std::variant<Quantity, ParameterId>& source() const noexcept;
  [[nodiscard]] Result<Quantity> resolve(const PartDocument&) const;

private:
  explicit SpatialDimensionValue(std::variant<Quantity, ParameterId> source);
  std::variant<Quantity, ParameterId> source_;
};
class SpatialConstraint {
public:
  [[nodiscard]] static Result<SpatialConstraint>
  create(SketchConstraintId id, SpatialConstraintKind kind,
         std::vector<SpatialConstraintTarget> targets,
         std::optional<SpatialDimensionValue> value = std::nullopt,
         std::optional<Point3> fixed_position = std::nullopt,
         SpatialAngleConvention angle = SpatialAngleConvention::Unsigned,
         std::optional<double> curve_parameter_seed = std::nullopt);
  [[nodiscard]] const SketchConstraintId& id() const noexcept;
  [[nodiscard]] SpatialConstraintKind kind() const noexcept;
  [[nodiscard]] const std::vector<SpatialConstraintTarget>& targets() const noexcept;
  [[nodiscard]] const std::optional<SpatialDimensionValue>& value() const noexcept;
  [[nodiscard]] const std::optional<Point3>& fixed_position() const noexcept;
  [[nodiscard]] SpatialAngleConvention angle_convention() const noexcept;
  [[nodiscard]] const std::optional<double>& curve_parameter_seed() const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::vector<std::string> referenced_node_ids() const;

private:
  SpatialConstraint(SketchConstraintId, SpatialConstraintKind, std::vector<SpatialConstraintTarget>,
                    std::optional<SpatialDimensionValue>, std::optional<Point3>,
                    SpatialAngleConvention, std::optional<double>);
  SketchConstraintId id_;
  SpatialConstraintKind kind_;
  std::vector<SpatialConstraintTarget> targets_;
  std::optional<SpatialDimensionValue> value_;
  std::optional<Point3> fixed_position_;
  SpatialAngleConvention angle_;
  std::optional<double> curve_parameter_seed_;
};
// Structural capability validation. Differential/projection capabilities are derived at solve time.
[[nodiscard]] Result<std::size_t> validate_spatial_constraint(const Sketch3D&,
                                                              const SpatialConstraint&);
[[nodiscard]] Result<std::size_t> validate_spatial_constraints(const PartDocument&,
                                                               const Sketch3D&);
[[nodiscard]] std::string_view to_string(SpatialConstraintKind) noexcept;
[[nodiscard]] std::string_view to_string(SpatialCurveLocation) noexcept;
[[nodiscard]] std::string_view to_string(SpatialAngleConvention) noexcept;
// Explicit coordinates and entity-owned positive Length scalars only; immutable parameter bindings
// are separately identified. Auxiliary curve parameters follow constraints sorted by stable id.
enum class SpatialVariableKind { PointX, PointY, PointZ, HelixRadius, HelixPitch, CurveParameter };
struct SpatialVariable {
  SpatialVariableKind kind;
  std::string owner_id;
  double initial_value;
};
[[nodiscard]] Result<std::vector<SpatialVariable>> spatial_variable_layout(const PartDocument&,
                                                                           const Sketch3D&);
} // namespace blcad
