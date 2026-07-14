#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/body_boolean_feature.hpp"
#include "blcad/core/body_transform.hpp"
#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/datum_axis.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/dependency_graph.hpp"
#include "blcad/core/draft_feature.hpp"
#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/invalidation_state.hpp"
#include "blcad/core/loft_feature.hpp"
#include "blcad/core/mirror_feature.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_pattern_feature.hpp"
#include "blcad/core/path_curve.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/reference_recovery.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/revolve_feature.hpp"
#include "blcad/core/shell_feature.hpp"
#include "blcad/core/sketch.hpp"
#include "blcad/core/sketch_3d.hpp"
#include "blcad/core/sketch_ownership.hpp"
#include "blcad/core/sweep_feature.hpp"
#include "blcad/core/surface_feature.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

class PartDocument {
public:
  [[nodiscard]] static Result<PartDocument> create(DocumentId id, std::string name);

  [[nodiscard]] Result<std::size_t> add_parameter(Parameter parameter);
  // Adds a computed parameter whose value derives from a unit-aware formula
  // over already-existing parameters of this document. Evaluation happens
  // here; dependency edges run from every referenced parameter to this one.
  [[nodiscard]] Result<std::size_t> add_expression_parameter(ParameterId id, std::string name,
                                                             ParameterType type,
                                                             std::string formula);
  [[nodiscard]] Result<std::size_t> add_datum_plane(DatumPlane datum_plane);
  [[nodiscard]] Result<std::size_t> add_datum_axis(DatumAxis datum_axis);
  [[nodiscard]] Result<std::size_t> add_construction_point(ConstructionPoint point);
  [[nodiscard]] Result<std::size_t> add_construction_line(ConstructionLine line);
  [[nodiscard]] Result<std::size_t> add_construction_plane(ConstructionPlane plane);
  [[nodiscard]] Result<std::size_t> add_derived_workplane(DerivedWorkplane workplane);
  [[nodiscard]] Result<std::size_t> add_sketch(Sketch sketch);
  [[nodiscard]] Result<std::size_t> add_sketch_3d(Sketch3D sketch);
  [[nodiscard]] Result<std::size_t> remove_sketch_3d(Sketch3DId id);
  [[nodiscard]] Result<std::size_t> add_path_curve(PathCurve path_curve);
  [[nodiscard]] Result<std::size_t> remove_path_curve(PathCurveId id);
  [[nodiscard]] Result<std::size_t> add_feature(Feature feature);
  [[nodiscard]] Result<std::size_t> add_revolve_feature(RevolveFeature feature);
  [[nodiscard]] Result<std::size_t> add_sweep_feature(SweepFeature feature);
  [[nodiscard]] Result<std::size_t> add_loft_feature(LoftFeature feature);
  [[nodiscard]] Result<std::size_t> add_surface_feature(SurfaceFeature feature);
  [[nodiscard]] Result<std::size_t> remove_surface_feature(FeatureId id);
  [[nodiscard]] Result<std::size_t> add_linear_pattern_feature(LinearPatternFeature feature);
  [[nodiscard]] Result<std::size_t> add_circular_pattern_feature(CircularPatternFeature feature);
  [[nodiscard]] Result<std::size_t> add_mirror_feature(MirrorFeature feature);
  [[nodiscard]] Result<std::size_t> add_fillet_feature(FilletFeature feature);
  [[nodiscard]] Result<std::size_t> add_chamfer_feature(ChamferFeature feature);
  [[nodiscard]] Result<std::size_t> add_shell_feature(ShellFeature feature);
  [[nodiscard]] Result<std::size_t> add_draft_feature(DraftFeature feature);
  [[nodiscard]] Result<std::size_t> add_body_boolean_feature(BodyBooleanFeature feature);
  [[nodiscard]] Result<std::size_t> add_sketch_ownership(SketchOwnership ownership);
  [[nodiscard]] Result<std::size_t> add_body_transform(BodyTransform transform);
  [[nodiscard]] Result<std::size_t> add_body(Body body);
  [[nodiscard]] Result<std::size_t> remove_body(BodyId id);
  [[nodiscard]] Result<std::size_t> set_body_visibility(BodyId id, BodyVisibility visibility);
  [[nodiscard]] Result<std::size_t> add_reference_status(ReferenceStatusRecord status);
  [[nodiscard]] Result<std::size_t> add_reference_remap(ReferenceRemapRecord remap);
  [[nodiscard]] Result<std::size_t>
  add_sketch_origin_override(SketchOriginOverrideRecord origin_override);
  [[nodiscard]] Result<std::vector<std::string>> mark_parameter_changed(ParameterId id);
  [[nodiscard]] Result<std::vector<std::string>> mark_feature_changed(FeatureId id);
  [[nodiscard]] Result<std::vector<std::string>> mark_body_changed(BodyId id);
  [[nodiscard]] Result<std::vector<std::string>> mark_body_transform_changed(BodyTransformId id);
  // Sets a parameter value, validates it, and marks the parameter and its
  // dependents as changed. Affected expression parameters are re-evaluated in
  // topological order before the affected graph nodes are returned. Direct
  // writes to expression-driven parameters are rejected.
  [[nodiscard]] Result<std::vector<std::string>> set_parameter_value(ParameterId id,
                                                                     Quantity value);
  // Rewrites the formula of an existing expression parameter: the new formula
  // is evaluated, the parameter's old input edges are replaced by the new
  // ones, cycles are rejected, and affected expression parameters are
  // re-evaluated in topological order. Returns the affected graph nodes.
  [[nodiscard]] Result<std::vector<std::string>> set_parameter_formula(ParameterId id,
                                                                       std::string formula);
  [[nodiscard]] Result<RecomputePlan> create_recompute_plan() const;
  void mark_all_clean() noexcept;

  [[nodiscard]] const DocumentId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const std::vector<Parameter>& parameters() const noexcept;
  [[nodiscard]] const std::vector<DatumPlane>& datum_planes() const noexcept;
  [[nodiscard]] const std::vector<DatumAxis>& datum_axes() const noexcept;
  [[nodiscard]] const std::vector<ConstructionPoint>& construction_points() const noexcept;
  [[nodiscard]] const std::vector<ConstructionLine>& construction_lines() const noexcept;
  [[nodiscard]] const std::vector<ConstructionPlane>& construction_planes() const noexcept;
  [[nodiscard]] const std::vector<DerivedWorkplane>& derived_workplanes() const noexcept;
  [[nodiscard]] const std::vector<Sketch>& sketches() const noexcept;
  [[nodiscard]] const std::vector<Sketch3D>& sketches_3d() const noexcept;
  [[nodiscard]] const std::vector<PathCurve>& path_curves() const noexcept;
  [[nodiscard]] const std::vector<Feature>& features() const noexcept;
  [[nodiscard]] const std::vector<RevolveFeature>& revolve_features() const noexcept;
  [[nodiscard]] const std::vector<SweepFeature>& sweep_features() const noexcept;
  [[nodiscard]] const std::vector<LoftFeature>& loft_features() const noexcept;
  [[nodiscard]] const std::vector<SurfaceFeature>& surface_features() const noexcept;
  [[nodiscard]] const std::vector<LinearPatternFeature>& linear_pattern_features() const noexcept;
  [[nodiscard]] const std::vector<CircularPatternFeature>&
  circular_pattern_features() const noexcept;
  [[nodiscard]] const std::vector<MirrorFeature>& mirror_features() const noexcept;
  [[nodiscard]] const std::vector<FilletFeature>& fillet_features() const noexcept;
  [[nodiscard]] const std::vector<ChamferFeature>& chamfer_features() const noexcept;
  [[nodiscard]] const std::vector<ShellFeature>& shell_features() const noexcept;
  [[nodiscard]] const std::vector<DraftFeature>& draft_features() const noexcept;
  [[nodiscard]] const std::vector<BodyBooleanFeature>& body_boolean_features() const noexcept;
  [[nodiscard]] const std::vector<SketchOwnership>& sketch_ownerships() const noexcept;
  [[nodiscard]] const std::vector<BodyTransform>& body_transforms() const noexcept;
  // Body order is canonical lexicographic BodyId order, independent of insertion order.
  [[nodiscard]] const std::vector<Body>& bodies() const noexcept;
  [[nodiscard]] const std::vector<ReferenceStatusRecord>& reference_statuses() const noexcept;
  [[nodiscard]] const std::vector<ReferenceRemapRecord>& reference_remaps() const noexcept;
  [[nodiscard]] const std::vector<SketchOriginOverrideRecord>&
  sketch_origin_overrides() const noexcept;
  [[nodiscard]] const DependencyGraph& dependency_graph() const noexcept;
  [[nodiscard]] const InvalidationState& invalidation_state() const noexcept;
  [[nodiscard]] std::size_t parameter_count() const noexcept;
  [[nodiscard]] std::size_t datum_plane_count() const noexcept;
  [[nodiscard]] std::size_t datum_axis_count() const noexcept;
  [[nodiscard]] std::size_t construction_point_count() const noexcept;
  [[nodiscard]] std::size_t construction_line_count() const noexcept;
  [[nodiscard]] std::size_t construction_plane_count() const noexcept;
  [[nodiscard]] std::size_t derived_workplane_count() const noexcept;
  [[nodiscard]] std::size_t sketch_count() const noexcept;
  [[nodiscard]] std::size_t sketch_3d_count() const noexcept;
  [[nodiscard]] std::size_t path_curve_count() const noexcept;
  [[nodiscard]] std::size_t feature_count() const noexcept;
  [[nodiscard]] std::size_t revolve_feature_count() const noexcept;
  [[nodiscard]] std::size_t sweep_feature_count() const noexcept;
  [[nodiscard]] std::size_t loft_feature_count() const noexcept;
  [[nodiscard]] std::size_t surface_feature_count() const noexcept;
  [[nodiscard]] std::size_t linear_pattern_feature_count() const noexcept;
  [[nodiscard]] std::size_t circular_pattern_feature_count() const noexcept;
  [[nodiscard]] std::size_t mirror_feature_count() const noexcept;
  [[nodiscard]] std::size_t fillet_feature_count() const noexcept;
  [[nodiscard]] std::size_t chamfer_feature_count() const noexcept;
  [[nodiscard]] std::size_t shell_feature_count() const noexcept;
  [[nodiscard]] std::size_t draft_feature_count() const noexcept;
  [[nodiscard]] std::size_t body_boolean_feature_count() const noexcept;
  [[nodiscard]] std::size_t sketch_ownership_count() const noexcept;
  [[nodiscard]] std::size_t body_transform_count() const noexcept;
  [[nodiscard]] std::size_t body_count() const noexcept;
  [[nodiscard]] std::size_t reference_status_count() const noexcept;
  [[nodiscard]] std::size_t reference_remap_count() const noexcept;
  [[nodiscard]] std::size_t sketch_origin_override_count() const noexcept;

  [[nodiscard]] const Parameter* find_parameter(ParameterId id) const noexcept;
  [[nodiscard]] const Parameter* find_parameter(std::string_view name) const noexcept;
  [[nodiscard]] const DatumPlane* find_datum_plane(DatumPlaneId id) const noexcept;
  [[nodiscard]] const DatumAxis* find_datum_axis(DatumAxisId id) const noexcept;
  [[nodiscard]] const ConstructionPoint*
  find_construction_point(ConstructionPointId id) const noexcept;
  [[nodiscard]] const ConstructionLine*
  find_construction_line(ConstructionLineId id) const noexcept;
  [[nodiscard]] const ConstructionPlane*
  find_construction_plane(ConstructionPlaneId id) const noexcept;
  [[nodiscard]] const DerivedWorkplane* find_derived_workplane(DatumPlaneId id) const noexcept;
  [[nodiscard]] const Sketch* find_sketch(SketchId id) const noexcept;
  [[nodiscard]] const Sketch3D* find_sketch_3d(Sketch3DId id) const noexcept;
  [[nodiscard]] const PathCurve* find_path_curve(PathCurveId id) const noexcept;
  [[nodiscard]] const Feature* find_feature(FeatureId id) const noexcept;
  [[nodiscard]] const RevolveFeature* find_revolve_feature(FeatureId id) const noexcept;
  [[nodiscard]] const SweepFeature* find_sweep_feature(FeatureId id) const noexcept;
  [[nodiscard]] const LoftFeature* find_loft_feature(FeatureId id) const noexcept;
  [[nodiscard]] const SurfaceFeature* find_surface_feature(FeatureId id) const noexcept;
  [[nodiscard]] const LinearPatternFeature*
  find_linear_pattern_feature(FeatureId id) const noexcept;
  [[nodiscard]] const CircularPatternFeature*
  find_circular_pattern_feature(FeatureId id) const noexcept;
  [[nodiscard]] const MirrorFeature* find_mirror_feature(FeatureId id) const noexcept;
  [[nodiscard]] const FilletFeature* find_fillet_feature(FeatureId id) const noexcept;
  [[nodiscard]] const ChamferFeature* find_chamfer_feature(FeatureId id) const noexcept;
  [[nodiscard]] const ShellFeature* find_shell_feature(FeatureId id) const noexcept;
  [[nodiscard]] const DraftFeature* find_draft_feature(FeatureId id) const noexcept;
  [[nodiscard]] const BodyBooleanFeature* find_body_boolean_feature(FeatureId id) const noexcept;
  [[nodiscard]] const SketchOwnership* find_sketch_ownership(SketchId id) const noexcept;
  [[nodiscard]] const BodyTransform* find_body_transform(BodyTransformId id) const noexcept;
  [[nodiscard]] const Body* find_body(BodyId id) const noexcept;
  [[nodiscard]] const ReferenceStatusRecord*
  find_reference_status(ReferenceStatusId id) const noexcept;
  [[nodiscard]] const ReferenceRemapRecord*
  find_reference_remap(ReferenceRemapId id) const noexcept;
  [[nodiscard]] const SketchOriginOverrideRecord*
  find_sketch_origin_override(SketchId id) const noexcept;
  [[nodiscard]] bool has_workplane_id(const DatumPlaneId& id) const noexcept;

private:
  PartDocument(DocumentId id, std::string name);

  [[nodiscard]] Result<std::size_t>
  reevaluate_affected_expressions(const std::vector<std::string>& affected_nodes);
  [[nodiscard]] bool has_parameter_id(const ParameterId& id) const noexcept;
  [[nodiscard]] bool has_parameter_name(std::string_view name) const noexcept;
  [[nodiscard]] bool has_datum_plane_id(const DatumPlaneId& id) const noexcept;
  [[nodiscard]] bool has_datum_axis_id(const DatumAxisId& id) const noexcept;
  [[nodiscard]] bool has_construction_point_id(const ConstructionPointId& id) const noexcept;
  [[nodiscard]] bool has_construction_line_id(const ConstructionLineId& id) const noexcept;
  [[nodiscard]] bool has_construction_plane_id(const ConstructionPlaneId& id) const noexcept;
  [[nodiscard]] bool has_derived_workplane_id(const DatumPlaneId& id) const noexcept;
  [[nodiscard]] bool has_sketch_id(const SketchId& id) const noexcept;
  [[nodiscard]] bool has_sketch_3d_id(const Sketch3DId& id) const noexcept;
  [[nodiscard]] bool has_path_curve_id(const PathCurveId& id) const noexcept;
  [[nodiscard]] bool has_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_revolve_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_sweep_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_loft_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_surface_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_linear_pattern_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_circular_pattern_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_mirror_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_fillet_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_chamfer_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_shell_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_draft_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_body_boolean_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_sketch_ownership_id(const SketchId& id) const noexcept;
  [[nodiscard]] bool has_body_transform_id(const BodyTransformId& id) const noexcept;
  [[nodiscard]] bool has_any_feature_id(const FeatureId& id) const noexcept;
  [[nodiscard]] bool has_body_id(const BodyId& id) const noexcept;
  [[nodiscard]] bool has_reference_status_id(const ReferenceStatusId& id) const noexcept;
  [[nodiscard]] bool has_reference_remap_id(const ReferenceRemapId& id) const noexcept;
  [[nodiscard]] bool has_sketch_origin_override_id(const SketchId& id) const noexcept;
  [[nodiscard]] Result<std::size_t>
  add_edge_treatment_dependencies(const FeatureId& id, const BodyId& target_body,
                                  const std::vector<EdgeReference>& edges,
                                  const std::vector<ParameterId>& parameters);
  [[nodiscard]] Result<std::size_t> add_shell_dependencies(const ShellFeature& feature);
  [[nodiscard]] Result<std::size_t> add_draft_dependencies(const DraftFeature& feature);

  DocumentId id_;
  std::string name_;
  std::vector<Parameter> parameters_;
  std::vector<DatumPlane> datum_planes_;
  std::vector<DatumAxis> datum_axes_;
  std::vector<ConstructionPoint> construction_points_;
  std::vector<ConstructionLine> construction_lines_;
  std::vector<ConstructionPlane> construction_planes_;
  std::vector<DerivedWorkplane> derived_workplanes_;
  std::vector<Sketch> sketches_;
  std::vector<Sketch3D> sketches_3d_;
  std::vector<PathCurve> path_curves_;
  std::vector<Feature> features_;
  std::vector<RevolveFeature> revolve_features_;
  std::vector<SweepFeature> sweep_features_;
  std::vector<LoftFeature> loft_features_;
  std::vector<SurfaceFeature> surface_features_;
  std::vector<LinearPatternFeature> linear_pattern_features_;
  std::vector<CircularPatternFeature> circular_pattern_features_;
  std::vector<MirrorFeature> mirror_features_;
  std::vector<FilletFeature> fillet_features_;
  std::vector<ChamferFeature> chamfer_features_;
  std::vector<ShellFeature> shell_features_;
  std::vector<DraftFeature> draft_features_;
  std::vector<BodyBooleanFeature> body_boolean_features_;
  std::vector<SketchOwnership> sketch_ownerships_;
  std::vector<BodyTransform> body_transforms_;
  std::vector<Body> bodies_;
  std::vector<ReferenceStatusRecord> reference_statuses_;
  std::vector<ReferenceRemapRecord> reference_remaps_;
  std::vector<SketchOriginOverrideRecord> sketch_origin_overrides_;
  DependencyGraph dependency_graph_;
  InvalidationState invalidation_state_;
};

} // namespace blcad
