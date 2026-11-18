#pragma once
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/sketch_driven_guides.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
namespace blcad::test::direct_integrated {
namespace f = history_fixture;
using namespace geometry;
inline void compute(const PartDocument& p, ShapeCache& c, StepImportTopologyContext* context) {
  (void)f::checked(GeometryRecomputeExecutor(context).execute_document(p, c));
}
inline void length_parameter(PartDocument& p, const char* id, double value) {
  f::accept(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
}
inline ResultFaceReference plane(const PartDocument& p, ShapeCache& c, BodyId body, Point3 at,
                                 Vector3 normal, StepImportTopologyContext* context) {
  compute(p, c, context);
  const auto choices = f::checked(ResultFaceSupportGeometry::selectable_faces(p, c, body, context));
  for (const auto& choice : choices) {
    auto resolved =
        ResultFaceSupportGeometry::resolve_current(p, c, body, choice.reference.origins(), context);
    if (!resolved)
      continue;
    const auto& frame = resolved.value().plane;
    if (std::abs(dot(frame.normal, normal)) < 1 - 1e-8)
      continue;
    const Vector3 delta{at.x - frame.origin.x, at.y - frame.origin.y, at.z - frame.origin.z};
    if (std::abs(dot(delta, frame.normal)) > 1e-6)
      continue;
    if (!ClosedProfileAdapter{}.validate_planar_circle_footprint(choice.shape, at, normal, .001))
      continue;
    return choice.reference;
  }
  throw std::runtime_error("Advanced Part fixture requires a unique physical support plane");
}
inline FaceReference face(ResultFaceReference ref,
                          PartFeatureInputRole role = PartFeatureInputRole::FilletSupportFace) {
  return f::checked(FaceReference::create_result(role, std::move(ref)));
}
inline DatumPlaneId workplane(PartDocument& p, ShapeCache& c, BodyId body, const char* id,
                              Point3 at, Vector3 normal, StepImportTopologyContext* context) {
  auto reference = plane(p, c, body, at, normal, context);
  const DatumPlaneId key(id);
  f::accept(
      p.add_result_face_workplane(f::checked(ResultFaceWorkplane::create(key, id, reference))));
  compute(p, c, context);
  return key;
}
inline PartDocument build(PartDocument p, StepImportTopologyContext* context) {
  const BodyId body = p.imported_body_features().empty()
                          ? BodyId("body.base")
                          : p.imported_body_features().front().definition().outputs().front().body;
  auto c = f::checked(ShapeCache::create(ShapeCacheId("advanced.part.fixture")));
  length_parameter(p, "direct.distance", 1);
  compute(p, c, context);
  const auto choices = f::checked(SketchDrivenGuideQuery::faces(p, c, body, context));
  auto bottom = std::find_if(choices.begin(), choices.end(), [](const auto& t) {
    return t.mapping == EmbossMapping::Planar && t.normal.z < -.9 && std::abs(t.anchor.z) < 1e-6;
  });
  if (bottom == choices.end())
    throw std::runtime_error("Advanced fixture requires original bottom z=0");
  f::accept(p.add_direct_edit_feature(f::checked(DirectEditFeature::create(
      FeatureId("direct"), "Housing underside", body, c.find_body_result(body)->source_feature_id,
      {{std::get<FaceReference>(bottom->selection.reference), DirectEditFaceCapability::Planar}},
      DirectFaceOffset{ParameterId("direct.distance")}))));
  compute(p, c, context);
  for (const auto& [id, value] : {std::pair{"boss.width", 2.},
                                  {"boss.height", 2.},
                                  {"boss.depth", 3.},
                                  {"boss.hole.d", .2},
                                  {"rib.t", 1.},
                                  {"rib.l", 12.},
                                  {"rib.hole.d", .2},
                                  {"r0", .2},
                                  {"r1", .3},
                                  {"label.w", 2.},
                                  {"label.h", 1.},
                                  {"label.d", .5},
                                  {"pattern.spacing", 40.}})
    length_parameter(p, id, value);
  const auto support = workplane(p, c, body, "boss.plane", {5, -2, 8}, {0, 0, 1}, context);
  const auto frame = f::checked(WorkplaneResolver{}.resolve(p, support, c));
  const Vector3 center{5 - frame.origin.x, -2 - frame.origin.y, 8 - frame.origin.z};
  auto boss = f::checked(Sketch::create(SketchId("boss.sketch"), "Boss on edited import", support));
  f::accept(boss.add_profile(f::checked(RectangleProfile::create(
      ProfileId("boss.profile"), ParameterId("boss.width"), ParameterId("boss.height"),
      {dot(center, frame.x_axis), dot(center, frame.y_axis)}))));
  f::accept(p.add_sketch(boss));
  auto extrusion = f::checked(Feature::create_additive_extrude(
      FeatureId("boss"), "Supported boss", boss.id(), ParameterId("boss.depth")));
  f::accept(p.add_feature(f::checked(extrusion.with_body_result_context(f::checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, body, std::nullopt))))));
  compute(p, c, context);
  length_parameter(p, "boss.thread.major", .25);
  length_parameter(p, "boss.thread.pitch", .05);
  length_parameter(p, "boss.thread.length", 2);
  f::accept(p.add_parameter(
      f::checked(Parameter::create_angle(ParameterId("boss.thread.angle"), "Thread included angle",
                                         f::checked(Quantity::angle_deg(60))))));
  auto thread = f::checked(ThreadDefinition::create(
      ThreadParameterProfile{"Synthetic", "Quarter-mm", "Authored",
                             ParameterId("boss.thread.major"), ParameterId("boss.hole.d"),
                             ParameterId("boss.thread.pitch"), ParameterId("boss.thread.angle"),
                             std::nullopt},
      ParameterId("boss.thread.length")));
  const auto hole_plane = workplane(p, c, body, "boss.hole.plane", {5, -2, 11}, {0, 0, 1}, context);
  const auto hole_frame = f::checked(WorkplaneResolver{}.resolve(p, hole_plane, c));
  const Vector3 hole_center{5 - hole_frame.origin.x, -2 - hole_frame.origin.y,
                            11 - hole_frame.origin.z};
  f::accept(p.add_hole_feature(f::checked(HoleFeature::create(
      FeatureId("boss.hole"), "Boss Hole", HoleKind::Plain, body,
      f::checked(
          PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane, hole_plane)),
      {{HolePlacementId("center"),
        Point2{dot(hole_center, hole_frame.x_axis), dot(hole_center, hole_frame.y_axis)}}},
      ParameterId("boss.hole.d"), HoleThroughAllExtent{}, HoleDirection::OppositeSupportNormal,
      HoleFlatTip{}, {}, thread))));
  auto rib =
      f::checked(Sketch::create(SketchId("rib.sketch"), "Cross Web", DatumPlaneId("datum.xy")));
  f::accept(
      rib.add_entity(f::checked(LineSegment::create(SketchEntityId("line"), {-8, 0}, {8, 0}))));
  f::accept(
      rib.add_entity(f::checked(LineSegment::create(SketchEntityId("cross"), {0, -4}, {0, 4}))));
  f::accept(p.add_sketch(rib));
  RibWebIntent web{RibWebKind::Web,
                   {rib.id(), {SketchEntityId("line"), SketchEntityId("cross")}, std::nullopt},
                   RibExtrusionPlane::PerpendicularToSketch,
                   {0, 0, 1},
                   RibThicknessSide::Symmetric,
                   ParameterId("rib.t"),
                   std::nullopt,
                   std::nullopt,
                   RibDistanceExtent{ParameterId("rib.l")},
                   RibCrossingPolicy::Union};
  f::accept(p.add_sketch_driven_feature(
      f::checked(SketchDrivenFeature::create_rib_web(FeatureId("web"), "Cross Web", body, web))));
  compute(p, c, context);
  const auto side_plane = workplane(p, c, body, "rib.hole.plane", {-4, .5, 10}, {0, 1, 0}, context);
  const auto side_frame = f::checked(WorkplaneResolver{}.resolve(p, side_plane, c));
  const Vector3 side_center{-4 - side_frame.origin.x, .5 - side_frame.origin.y,
                            10 - side_frame.origin.z};
  f::accept(p.add_hole_feature(f::checked(HoleFeature::create(
      FeatureId("rib.hole"), "Web Hole", HoleKind::Plain, body,
      f::checked(
          PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane, side_plane)),
      {{HolePlacementId("center"),
        Point2{dot(side_center, side_frame.x_axis), dot(side_center, side_frame.y_axis)}}},
      ParameterId("rib.hole.d"), HoleThroughAllExtent{}, HoleDirection::OppositeSupportNormal,
      HoleFlatTip{}))));
  compute(p, c, context);
  FullRoundIntent full{
      {"first", "First", {face(plane(p, c, body, {-4, .5, 11}, {0, 1, 0}, context))}},
      {"second", "Second", {face(plane(p, c, body, {-4, -.5, 11}, {0, 1, 0}, context))}},
      {"middle",
       "Arm top",
       {face(plane(p, c, body, {-4, 0, 12}, {0, 0, 1}, context),
             PartFeatureInputRole::FilletMiddleFace)}},
      FilletContinuity::Curvature,
      {}};
  f::accept(p.add_fillet_feature(
      f::checked(FilletFeature::create_full_round(FeatureId("full"), "Full Round", body, full))));
  compute(p, c, context);
  const auto edges = f::checked(
      FinishingSelectionBuilder::build(p, c, body, PartFeatureInputRole::FilletEdge, context));
  auto edge = std::find_if(edges.begin(), edges.end(), [](const auto& e) {
    return std::abs(e.anchor.x) < 1e-6 && std::abs(e.anchor.y - 5) < 1e-6 &&
           std::abs(e.anchor.z - 8) < 1e-6;
  });
  if (edge == edges.end())
    throw std::runtime_error("Advanced fixture requires untouched top/front edge");
  VariableFilletIntent law{{{"edge",
                             "Outer edge",
                             {{std::get<EdgeReference>(edge->reference), false}},
                             FilletPropagation::Explicit,
                             FilletRadiusInterpolation::Linear,
                             {{"start", FilletStationLocation::Start, 0, ParameterId("r0")},
                              {"end", FilletStationLocation::End, 1, ParameterId("r1")}}}}};
  f::accept(p.add_fillet_feature(f::checked(
      FilletFeature::create_variable(FeatureId("variable"), "Variable Round", body, law))));
  compute(p, c, context);
  auto label =
      f::checked(Sketch::create(SketchId("label.sketch"), "Label", DatumPlaneId("datum.xy")));
  f::accept(label.add_profile(f::checked(RectangleProfile::create(
      ProfileId("label.profile"), ParameterId("label.w"), ParameterId("label.h"), {6, 3}))));
  f::accept(p.add_sketch(label));
  EmbossIntent emboss{
      f::checked(ProfileRegionReference::create(label.id(), ProfileId("label.profile"))),
      {face(plane(p, c, body, {6, 3, 8}, {0, 0, 1}, context),
            PartFeatureInputRole::SurfaceSourceFace)},
      EmbossOperation::Raise,
      ParameterId("label.d"),
      EmbossMapping::Planar,
      {},
      0,
      std::nullopt};
  f::accept(p.add_sketch_driven_feature(f::checked(SketchDrivenFeature::create(
      FeatureId("emboss"), "Emboss", body, SketchDrivenDefinition{emboss}))));
  compute(p, c, context);
  f::accept(p.add_parameter(f::checked(Parameter::create_count(
      ParameterId("pattern.count"), "Pattern count", f::checked(Quantity::count(2))))));
  f::accept(p.add_datum_axis(f::checked(
      DatumAxis::create_explicit(DatumAxisId("pattern.axis"), "Pattern axis", {}, {1, 0, 0}))));
  f::accept(p.add_body(
      f::checked(Body::create(BodyId("body.pattern"), "Pattern result", BodyKind::Solid))));
  f::accept(p.add_linear_pattern_feature(f::checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Body Pattern", {f::checked(PatternSourceReference::body(body))},
      f::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                  DatumAxisId("pattern.axis"))),
      ParameterId("pattern.count"), LinearPatternExtentMode::Spacing,
      ParameterId("pattern.spacing"), PatternDirectionSign::Positive,
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("body.pattern")))))));
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("pattern.place"), BodyId("body.pattern"), BodyTransformCoordinateSpace::World,
      std::nullopt, {0, 0, 20}, false, false))));
  compute(p, c, context);
  length_parameter(p, "skin.distance", 1);
  length_parameter(p, "skin.thickness", .5);
  length_parameter(p, "skin.tolerance", 1e-6);
  const auto bottom_face = face(plane(p, c, body, {6, 3, -1}, {0, 0, -1}, context),
                                PartFeatureInputRole::SurfaceSourceFace);
  const auto skin = BodyId("body.skin");
  f::accept(p.add_body(f::checked(Body::create(skin, "Offset underside", BodyKind::Surface))));
  f::accept(p.add_surface_feature(f::checked(SurfaceOffsetFeature::create_offset(
      FeatureId("skin.offset"), "Offset edited underside",
      f::checked(SurfaceReference::create_faces(body, {bottom_face})),
      {ParameterId("skin.distance"), ParameterId("skin.tolerance"), SurfaceOffsetSide::AlongNormal,
       SurfaceOffsetApproximation::ExactAnalytic, std::nullopt},
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  skin))))));
  compute(p, c, context);
  const auto thick = BodyId("body.thick");
  f::accept(p.add_body(f::checked(Body::create(thick, "Thickened underside", BodyKind::Solid))));
  f::accept(p.add_surface_feature(f::checked(SurfaceOffsetFeature::create_thicken(
      FeatureId("skin.thicken"), "Thicken offset underside",
      f::checked(SurfaceReference::create_body(skin)),
      {ParameterId("skin.thickness"), ParameterId("skin.tolerance"), SurfaceOffsetSide::AlongNormal,
       SurfaceOffsetApproximation::ExactAnalytic, std::nullopt},
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  thick))))));
  engineering_fixture::annotate(p);
  compute(p, c, context);
  return p;
}
} // namespace blcad::test::direct_integrated
