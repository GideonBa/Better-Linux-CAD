#pragma once

#include "blcad/core/part_document.hpp"

#include <stdexcept>
#include <utility>

namespace blcad::test::history_fixture {
template <typename T> T checked(Result<T> result) {
  if (!result)
    throw std::runtime_error(result.error().object_id() + ": " + result.error().message());
  return std::move(result.value());
}
inline void accept(Result<std::size_t> result) {
  (void)checked(std::move(result));
}

inline PartDocument imported(bool reference = false) {
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "assets/history-supplier.step",
      std::string(64, '1'), reference ? StepImportMode::Reference : StepImportMode::EditableBody));
  ImportedTopologyDescriptor descriptor;
  descriptor.kind = ImportedTopologyKind::Body;
  descriptor.geometry = ImportedGeometryClass::Opaque;
  descriptor.body_kind = BodyKind::Solid;
  descriptor.bounds_mm = {0, 0, 0, 10, 10, 10};
  descriptor.centroid = {5, 5, 5};
  descriptor.measure = 1000;
  descriptor.geometry_key = std::string(64, 'a');
  const auto catalog = checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), source.id(), source.sha256_digest(),
      std::string(64, '2'), {{ImportedTopologyId("solid"), std::nullopt, descriptor}}));
  const auto definition = checked(ImportedPartDefinition::create(
      source.id(), catalog.id(),
      {{ImportedTopologyId("solid"), BodyId("imported.body"), BodyKind::Solid}}));
  if (reference)
    return checked(PartDocument::create_reference(DocumentId("history.reference"), "Reference",
                                                  source, catalog, definition));
  const auto feature =
      checked(ImportedBodyFeature::create(FeatureId("imported"), "Imported", definition));
  return checked(PartDocument::create_editable_import(DocumentId("history"), "History", source,
                                                      catalog, feature));
}
inline FeatureBodyResultContext new_body(PartDocument& part, const std::string& suffix,
                                         BodyKind kind = BodyKind::Solid) {
  const BodyId id("body." + suffix);
  accept(part.add_body(checked(Body::create(id, suffix, kind))));
  return checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, id));
}
inline void rectangle(PartDocument& part, const std::string& id, const char* plane = "datum.xy") {
  auto sketch = checked(Sketch::create(SketchId(id), id, DatumPlaneId(plane)));
  accept(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId(id + ".profile"), ParameterId("width"), ParameterId("height")))));
  accept(part.add_sketch(std::move(sketch)));
}
inline ProfileRegionReference profile(PartFeatureInputRole role, const std::string& id) {
  return checked(ProfileRegionReference::create(SketchId(id), ProfileId(id + ".profile"), role));
}
inline AxisReference axis(PartFeatureInputRole role) {
  return checked(AxisReference::create_datum_axis(role, DatumAxisId("axis")));
}
inline PlaneReference plane(PartFeatureInputRole role) {
  return checked(PlaneReference::create_datum_plane(role, DatumPlaneId("datum.xy")));
}
inline EdgeReference edge(PartFeatureInputRole role) {
  return checked(EdgeReference::create_linear(
      role, checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront))));
}
inline FaceReference face(PartFeatureInputRole role) {
  return checked(FaceReference::create_planar(
      role, checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
}
inline PartDocument base(bool authored = false) {
  auto part = imported();
  if (authored)
    accept(part.apply_history_intent({{"imported"}, {}}));
  for (const auto& [id, value] :
       {std::pair{"width", 20.0}, {"height", 10.0}, {"depth", 8.0}, {"radius", 1.0}})
    accept(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  accept(part.add_parameter(checked(
      Parameter::create_count(ParameterId("count"), "Count", checked(Quantity::count(3))))));
  accept(part.add_parameter(checked(
      Parameter::create_angle(ParameterId("angle"), "Angle", checked(Quantity::angle_deg(2))))));
  accept(part.add_datum_axis(
      checked(DatumAxis::create_explicit(DatumAxisId("axis"), "Axis", {}, {0, 0, 1}))));
  rectangle(part, "profile");
  const auto context = new_body(part, "base");
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("base"), "Base", SketchId("profile"), ParameterId("depth")));
  accept(part.add_feature(checked(feature.with_body_result_context(context))));
  return part;
}
inline PartDocument native_base() {
  auto part = base();
  accept(part.remove_imported_body_feature(FeatureId("imported")));
  accept(part.remove_body(BodyId("imported.body")));
  accept(part.remove_imported_topology_catalog(ImportedTopologyCatalogId("catalog")));
  accept(part.remove_step_import_source(StepImportSourceId("supplier")));
  return part;
}
inline void path(PartDocument& part, const char* id, double y) {
  auto sketch =
      checked(Sketch::create(SketchId(std::string(id) + ".sketch"), id, DatumPlaneId("datum.xy")));
  accept(sketch.add_entity(checked(LineSegment::create(SketchEntityId("line"), {0, y}, {10, y}))));
  const auto segment = checked(PathSegmentReference::create_planar(
      sketch.id(), SketchEntityId("line"), PlanarPathCurveKind::Line));
  accept(part.add_sketch(std::move(sketch)));
  accept(part.add_path_curve(checked(PathCurve::create(
      PathCurveId(id), id, {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist))));
}
// Core intent coverage only: geometric feasibility of this synthetic all-family
// catalog is not asserted. Existing Geometry fixtures supply physical replay proof.
inline PartDocument all_families(bool authored = false) {
  auto part = base(authored);
  accept(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("translate"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {10, 0, 0}, false, false))));
  accept(part.add_revolve_feature(checked(RevolveFeature::create_revolve(
      FeatureId("revolve"), "Revolve", profile(PartFeatureInputRole::RevolveProfile, "profile"),
      axis(PartFeatureInputRole::RevolveAxis), RevolveAngleExtent::full(),
      new_body(part, "revolve")))));
  path(part, "path.one", 0);
  path(part, "path.two", 10);
  accept(part.add_sweep_feature(checked(
      SweepFeature::create_sweep(FeatureId("sweep"), "Sweep",
                                 checked(SweepProfileReference::create_closed_region(
                                     profile(PartFeatureInputRole::SweepProfile, "profile"))),
                                 PathCurveId("path.one"), new_body(part, "sweep")))));
  rectangle(part, "section", "datum.xz");
  accept(part.add_loft_feature(checked(
      LoftFeature::create_loft(FeatureId("loft"), "Loft",
                               {checked(ProfileSectionReference::create_closed_region(
                                    profile(PartFeatureInputRole::LoftSection, "profile"))),
                                checked(ProfileSectionReference::create_closed_region(
                                    profile(PartFeatureInputRole::LoftSection, "section")))},
                               new_body(part, "loft")))));
  (void)new_body(part, "surface", BodyKind::Surface);
  accept(part.add_surface_feature(checked(FillSurfaceFeature::create(
      FeatureId("surface"), "Fill",
      {checked(BoundaryCurveReference::create_path_curve(PathCurveId("path.one"))),
       checked(BoundaryCurveReference::create_path_curve(PathCurveId("path.two")))},
      BodyId("body.surface")))));
  const auto source = checked(PatternSourceReference::feature(FeatureId("base")));
  accept(part.add_linear_pattern_feature(checked(LinearPatternFeature::create(
      FeatureId("linear"), "Linear", {source}, axis(PartFeatureInputRole::PatternAxis),
      ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("depth"),
      PatternDirectionSign::Positive, new_body(part, "linear")))));
  accept(part.add_circular_pattern_feature(checked(CircularPatternFeature::create(
      FeatureId("circular"), "Circular", {source}, axis(PartFeatureInputRole::PatternAxis),
      ParameterId("count"), 360, true, new_body(part, "circular")))));
  accept(part.add_mirror_feature(checked(MirrorFeature::create(
      FeatureId("mirror"), "Mirror", {checked(MirrorSourceReference::feature(FeatureId("base")))},
      plane(PartFeatureInputRole::MirrorPlane), new_body(part, "mirror")))));
  accept(part.add_fillet_feature(checked(
      FilletFeature::create(FeatureId("fillet"), "Fillet", BodyId("body.base"),
                            {edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
  accept(part.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
      FeatureId("chamfer"), "Chamfer", BodyId("body.base"),
      {edge(PartFeatureInputRole::ChamferEdge)}, ParameterId("radius")))));
  accept(part.add_shell_feature(
      checked(ShellFeature::create(FeatureId("shell"), "Shell", BodyId("body.base"),
                                   {face(PartFeatureInputRole::ShellRemovalFace)},
                                   ParameterId("radius"), ShellDirection::Inward))));
  accept(part.add_draft_feature(checked(DraftFeature::create(
      FeatureId("draft"), "Draft", BodyId("body.base"), {face(PartFeatureInputRole::DraftFace)},
      axis(PartFeatureInputRole::DraftPullDirection),
      plane(PartFeatureInputRole::DraftNeutralPlane), ParameterId("angle")))));
  accept(part.add_body_boolean_feature(checked(BodyBooleanFeature::create(
      FeatureId("boolean"), BodyBooleanOperation::Add, BodyId("body.base"), {BodyId("body.mirror")},
      BodyBooleanResultMode::ModifyTarget, std::nullopt, false))));
  return part;
}

inline PartDocument mixed_chain() {
  auto part = base();
  auto tool = checked(Feature::create_additive_extrude(FeatureId("tool"), "Tool",
                                                       SketchId("profile"), ParameterId("depth")));
  accept(part.add_feature(checked(tool.with_body_result_context(new_body(part, "tool")))));
  accept(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("before"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {10, 0, 0}, false, false))));
  accept(part.add_body_boolean_feature(checked(BodyBooleanFeature::create(
      FeatureId("boolean"), BodyBooleanOperation::Subtract, BodyId("body.base"),
      {BodyId("body.tool")}, BodyBooleanResultMode::ModifyTarget, std::nullopt, true))));
  accept(part.add_body_transform(checked(BodyTransform::create_uniform_scale(
      BodyTransformId("after"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, 1.5, {}, false, false))));
  accept(part.add_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("consumer.plane"), "Consumer",
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))))));
  rectangle(part, "consumer.sketch", "consumer.plane");
  return part;
}
} // namespace blcad::test::history_fixture
