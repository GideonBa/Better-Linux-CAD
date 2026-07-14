#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/sketch.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace blcad {

enum class SketchCoordinate3DSource { Explicit, Parameter };

class SketchCoordinate3D {
public:
  [[nodiscard]] static Result<SketchCoordinate3D> create_explicit(Quantity coordinate);
  [[nodiscard]] static Result<SketchCoordinate3D> create_parameter(ParameterId parameter);

  [[nodiscard]] SketchCoordinate3DSource source() const noexcept;
  [[nodiscard]] const std::optional<Quantity>& explicit_coordinate() const noexcept;
  [[nodiscard]] const std::optional<ParameterId>& parameter() const noexcept;

private:
  SketchCoordinate3D(SketchCoordinate3DSource source, std::optional<Quantity> explicit_coordinate,
                     std::optional<ParameterId> parameter);

  SketchCoordinate3DSource source_;
  std::optional<Quantity> explicit_coordinate_;
  std::optional<ParameterId> parameter_;
};

class SketchPoint3D {
public:
  [[nodiscard]] static Result<SketchPoint3D> create(SketchEntityId id, SketchCoordinate3D x,
                                                    SketchCoordinate3D y, SketchCoordinate3D z);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchCoordinate3D& x() const noexcept;
  [[nodiscard]] const SketchCoordinate3D& y() const noexcept;
  [[nodiscard]] const SketchCoordinate3D& z() const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;

private:
  SketchPoint3D(SketchEntityId id, SketchCoordinate3D x, SketchCoordinate3D y,
                SketchCoordinate3D z);

  SketchEntityId id_;
  SketchCoordinate3D x_;
  SketchCoordinate3D y_;
  SketchCoordinate3D z_;
};

class SketchLine3D {
public:
  [[nodiscard]] static Result<SketchLine3D> create(SketchEntityId id, SketchEntityId start_point,
                                                   SketchEntityId end_point);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchEntityId& start_point() const noexcept;
  [[nodiscard]] const SketchEntityId& end_point() const noexcept;

private:
  SketchLine3D(SketchEntityId id, SketchEntityId start_point, SketchEntityId end_point);

  SketchEntityId id_;
  SketchEntityId start_point_;
  SketchEntityId end_point_;
};

class SketchPolyline3D {
public:
  [[nodiscard]] static Result<SketchPolyline3D>
  create(SketchEntityId id, std::vector<SketchEntityId> ordered_vertices);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const std::vector<SketchEntityId>& ordered_vertices() const noexcept;

private:
  SketchPolyline3D(SketchEntityId id, std::vector<SketchEntityId> ordered_vertices);

  SketchEntityId id_;
  std::vector<SketchEntityId> ordered_vertices_;
};

enum class SketchPointReference3DSource { LocalPoint, ConstructionPoint, PlanarSketchPoint };

class SketchPointReference3D {
public:
  [[nodiscard]] static Result<SketchPointReference3D> create_local_point(SketchEntityId point);
  [[nodiscard]] static Result<SketchPointReference3D>
  create_construction_point(ConstructionPointId point);
  [[nodiscard]] static Result<SketchPointReference3D>
  create_planar_sketch_point(SketchId sketch, SketchReferenceTarget point);

  [[nodiscard]] SketchPointReference3DSource source() const noexcept;
  [[nodiscard]] const std::optional<SketchEntityId>& local_point() const noexcept;
  [[nodiscard]] const std::optional<ConstructionPointId>& construction_point() const noexcept;
  [[nodiscard]] const std::optional<SketchId>& planar_sketch() const noexcept;
  [[nodiscard]] const std::optional<SketchReferenceTarget>& planar_point() const noexcept;
  [[nodiscard]] std::optional<std::string> referenced_node_id() const;
  [[nodiscard]] bool same_source(const SketchPointReference3D& other) const noexcept;

private:
  SketchPointReference3D(SketchPointReference3DSource source,
                         std::optional<SketchEntityId> local_point,
                         std::optional<ConstructionPointId> construction_point,
                         std::optional<SketchId> planar_sketch,
                         std::optional<SketchReferenceTarget> planar_point);

  SketchPointReference3DSource source_;
  std::optional<SketchEntityId> local_point_;
  std::optional<ConstructionPointId> construction_point_;
  std::optional<SketchId> planar_sketch_;
  std::optional<SketchReferenceTarget> planar_point_;
};

class SketchArc3D {
public:
  [[nodiscard]] static Result<SketchArc3D> create_three_point(SketchEntityId id,
                                                              SketchPointReference3D start,
                                                              SketchPointReference3D intermediate,
                                                              SketchPointReference3D end);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchPointReference3D& start() const noexcept;
  [[nodiscard]] const SketchPointReference3D& intermediate() const noexcept;
  [[nodiscard]] const SketchPointReference3D& end() const noexcept;
  [[nodiscard]] std::vector<SketchPointReference3D> point_references() const;

private:
  SketchArc3D(SketchEntityId id, SketchPointReference3D start, SketchPointReference3D intermediate,
              SketchPointReference3D end);

  SketchEntityId id_;
  SketchPointReference3D start_;
  SketchPointReference3D intermediate_;
  SketchPointReference3D end_;
};

enum class SketchSpline3DRepresentation { FitPoints, ControlPoints };
enum class SketchSpline3DContinuity { Positional, Tangent, Curvature };

class SketchSpline3D {
public:
  [[nodiscard]] static Result<SketchSpline3D>
  create(SketchEntityId id, SketchSpline3DRepresentation representation,
         std::vector<SketchPointReference3D> ordered_points, std::size_t degree,
         SketchSpline3DContinuity continuity);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] SketchSpline3DRepresentation representation() const noexcept;
  [[nodiscard]] const std::vector<SketchPointReference3D>& ordered_points() const noexcept;
  [[nodiscard]] std::size_t degree() const noexcept;
  [[nodiscard]] SketchSpline3DContinuity continuity() const noexcept;

private:
  SketchSpline3D(SketchEntityId id, SketchSpline3DRepresentation representation,
                 std::vector<SketchPointReference3D> ordered_points, std::size_t degree,
                 SketchSpline3DContinuity continuity);

  SketchEntityId id_;
  SketchSpline3DRepresentation representation_;
  std::vector<SketchPointReference3D> ordered_points_;
  std::size_t degree_;
  SketchSpline3DContinuity continuity_;
};

enum class SketchHelixAxis3DSource { DatumAxis, ConstructionLine };

class SketchHelixAxis3D {
public:
  [[nodiscard]] static Result<SketchHelixAxis3D> create_datum_axis(DatumAxisId axis);
  [[nodiscard]] static Result<SketchHelixAxis3D> create_construction_line(ConstructionLineId line);

  [[nodiscard]] SketchHelixAxis3DSource source() const noexcept;
  [[nodiscard]] const std::optional<DatumAxisId>& datum_axis() const noexcept;
  [[nodiscard]] const std::optional<ConstructionLineId>& construction_line() const noexcept;
  [[nodiscard]] std::string referenced_node_id() const;

private:
  SketchHelixAxis3D(SketchHelixAxis3DSource source, std::optional<DatumAxisId> datum_axis,
                    std::optional<ConstructionLineId> construction_line);

  SketchHelixAxis3DSource source_;
  std::optional<DatumAxisId> datum_axis_;
  std::optional<ConstructionLineId> construction_line_;
};

enum class SketchHelix3DHandedness { RightHanded, LeftHanded };

class SketchHelix3D {
public:
  [[nodiscard]] static Result<SketchHelix3D> create(SketchEntityId id, SketchHelixAxis3D axis,
                                                    ParameterId radius_parameter,
                                                    ParameterId pitch_parameter,
                                                    ParameterId turns_parameter,
                                                    SketchHelix3DHandedness handedness);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchHelixAxis3D& axis() const noexcept;
  [[nodiscard]] const ParameterId& radius_parameter() const noexcept;
  [[nodiscard]] const ParameterId& pitch_parameter() const noexcept;
  [[nodiscard]] const ParameterId& turns_parameter() const noexcept;
  [[nodiscard]] SketchHelix3DHandedness handedness() const noexcept;

private:
  SketchHelix3D(SketchEntityId id, SketchHelixAxis3D axis, ParameterId radius_parameter,
                ParameterId pitch_parameter, ParameterId turns_parameter,
                SketchHelix3DHandedness handedness);

  SketchEntityId id_;
  SketchHelixAxis3D axis_;
  ParameterId radius_parameter_;
  ParameterId pitch_parameter_;
  ParameterId turns_parameter_;
  SketchHelix3DHandedness handedness_;
};

enum class SketchGuideCurve3DRole { General, SweepPath, LoftGuide, Centerline };

class SketchGuideCurve3D {
public:
  [[nodiscard]] static Result<SketchGuideCurve3D> create(SketchEntityId id,
                                                         SketchEntityId source_curve,
                                                         SketchGuideCurve3DRole role,
                                                         std::string label = {});

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] const SketchEntityId& source_curve() const noexcept;
  [[nodiscard]] SketchGuideCurve3DRole role() const noexcept;
  [[nodiscard]] const std::string& label() const noexcept;

private:
  SketchGuideCurve3D(SketchEntityId id, SketchEntityId source_curve, SketchGuideCurve3DRole role,
                     std::string label);

  SketchEntityId id_;
  SketchEntityId source_curve_;
  SketchGuideCurve3DRole role_;
  std::string label_;
};

class Sketch3D {
public:
  [[nodiscard]] static Result<Sketch3D> create(Sketch3DId id, std::string name);

  [[nodiscard]] Result<std::size_t> add_point(SketchPoint3D point);
  [[nodiscard]] Result<std::size_t> add_line(SketchLine3D line);
  [[nodiscard]] Result<std::size_t> add_polyline(SketchPolyline3D polyline);
  [[nodiscard]] Result<std::size_t> add_arc(SketchArc3D arc);
  [[nodiscard]] Result<std::size_t> add_spline(SketchSpline3D spline);
  [[nodiscard]] Result<std::size_t> add_helix(SketchHelix3D helix);
  [[nodiscard]] Result<std::size_t> add_guide_curve(SketchGuideCurve3D guide_curve);
  [[nodiscard]] Result<std::size_t> remove_point(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_line(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_polyline(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_arc(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_spline(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_helix(SketchEntityId id);
  [[nodiscard]] Result<std::size_t> remove_guide_curve(SketchEntityId id);

  [[nodiscard]] const Sketch3DId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<SketchPoint3D>& points() const noexcept;
  [[nodiscard]] const std::vector<SketchLine3D>& lines() const noexcept;
  [[nodiscard]] const std::vector<SketchPolyline3D>& polylines() const noexcept;
  [[nodiscard]] const std::vector<SketchArc3D>& arcs() const noexcept;
  [[nodiscard]] const std::vector<SketchSpline3D>& splines() const noexcept;
  [[nodiscard]] const std::vector<SketchHelix3D>& helices() const noexcept;
  [[nodiscard]] const std::vector<SketchGuideCurve3D>& guide_curves() const noexcept;
  [[nodiscard]] const SketchPoint3D* find_point(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchLine3D* find_line(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchPolyline3D* find_polyline(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchArc3D* find_arc(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchSpline3D* find_spline(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchHelix3D* find_helix(SketchEntityId id) const noexcept;
  [[nodiscard]] const SketchGuideCurve3D* find_guide_curve(SketchEntityId id) const noexcept;
  [[nodiscard]] std::vector<ParameterId> parameter_dependencies() const;
  [[nodiscard]] std::vector<std::string> referenced_node_ids() const;
  [[nodiscard]] std::size_t entity_count() const noexcept;

private:
  Sketch3D(Sketch3DId id, std::string name);
  [[nodiscard]] bool has_entity_id(const SketchEntityId& id) const noexcept;
  [[nodiscard]] bool has_curve_id(const SketchEntityId& id) const noexcept;
  [[nodiscard]] Result<std::size_t>
  validate_point_references(const std::vector<SketchPointReference3D>& references,
                            const SketchEntityId& owner) const;
  [[nodiscard]] bool guide_references(const SketchEntityId& id) const noexcept;

  Sketch3DId id_;
  std::string name_;
  std::vector<SketchPoint3D> points_;
  std::vector<SketchLine3D> lines_;
  std::vector<SketchPolyline3D> polylines_;
  std::vector<SketchArc3D> arcs_;
  std::vector<SketchSpline3D> splines_;
  std::vector<SketchHelix3D> helices_;
  std::vector<SketchGuideCurve3D> guide_curves_;
};

} // namespace blcad
