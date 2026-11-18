#pragma once

#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/core/hole_instance_reference.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/sketch_driven_guides.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "engineering_resource_fixture.hpp"
#include "part_history_fixture.hpp"
#include "rotor_performance_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace blcad::test::advanced_rotor {
namespace f = history_fixture;
using namespace geometry;
inline BodyId rotor_body() {
  return BodyId("body.rotor");
}
inline BodyId blade_body() {
  return BodyId("body.blade");
}
inline ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("advanced.rotor")));
}
inline void execute(const PartDocument& p, ShapeCache& c) {
  (void)f::checked(GeometryRecomputeExecutor{}.execute_document(p, c));
}
inline void length(PartDocument& p, const char* id, double value) {
  (void)f::checked(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
}
// This local fixture refinement leaves rotor_performance_fixture.hpp untouched.
// Parallel 10 mm root necks give the real variable root round a closed-form
// quarter-circle volume. The translated circular sides are tangent to the neck and 4 mm apart.
inline void root_neck(PartDocument& p) {
  length(p, "neck.length", 10);
  length(p, "blade.width", 4);
  const auto* old = p.find_sketch(SketchId("sketch.blade"));
  auto sketch = f::checked(Sketch::create(old->id(), old->name(), old->workplane()));
  (void)f::checked(sketch.add_profile(
      f::checked(RectangleProfile::create(ProfileId("profile.blade"), ParameterId("neck.length"),
                                          ParameterId("blade.width"), {45, 0}))));
  (void)f::checked(p.update_sketch(std::move(sketch)));
}
inline void curved_tip(PartDocument& p) {
  auto sketch = f::checked(
      Sketch::create(SketchId("sketch.blade.curve"), "Arc blade tip", DatumPlaneId("datum.xy")));
  const double radius = 449. / 7, center_y = 435. / 7;
  const double middle_y = center_y - std::sqrt(radius * radius - 400);
  const auto lower = f::checked(ArcSegment::create_three_point(SketchEntityId("blade.lower"),
                                                               {50, -2}, {70, middle_y}, {90, 12}));
  const auto tip = f::checked(LineSegment::create(SketchEntityId("blade.tip"), {90, 12}, {90, 16}));
  const auto upper = f::checked(ArcSegment::create_three_point(
      SketchEntityId("blade.upper"), {90, 16}, {70, middle_y + 4}, {50, 2}));
  const auto root =
      f::checked(LineSegment::create(SketchEntityId("blade.curve.root"), {50, 2}, {50, -2}));
  (void)f::checked(sketch.add_entity(lower));
  (void)f::checked(sketch.add_entity(tip));
  (void)f::checked(sketch.add_entity(upper));
  (void)f::checked(sketch.add_entity(root));
  (void)f::checked(sketch.add_profile(f::checked(ArcClosedProfile::create(
      ProfileId("profile.blade.curve"), {lower.id(), tip.id(), upper.id(), root.id()}))));
  (void)f::checked(p.add_sketch(sketch));
  auto feature = f::checked(Feature::create_additive_extrude(
      FeatureId("feature.blade.curve"), "Arc blade tip", sketch.id(), ParameterId("blade.height")));
  feature = f::checked(feature.with_profile(ProfileId("profile.blade.curve")));
  feature = f::checked(feature.with_body_result_context(f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::Join, blade_body(), std::nullopt))));
  (void)f::checked(p.add_feature(feature));
}
inline ResultFaceReference top(const PartDocument& p, const ShapeCache& c, Point3 station) {
  const auto candidates =
      f::checked(ResultFaceSupportGeometry::selectable_faces(p, c, rotor_body()));
  for (const auto& candidate : candidates) {
    const auto resolved = ResultFaceSupportGeometry::resolve_current(p, c, rotor_body(),
                                                                     candidate.reference.origins());
    if (!resolved || std::abs(resolved.value().plane.normal.z) < .99)
      continue;
    const auto& plane = resolved.value().plane;
    const Vector3 d{station.x - plane.origin.x, station.y - plane.origin.y,
                    station.z - plane.origin.z};
    if (std::abs(dot(d, plane.normal)) > 1e-6 ||
        !ClosedProfileAdapter{}.validate_planar_circle_footprint(candidate.shape, station,
                                                                 {0, 0, 1}, .01))
      continue;
    return candidate.reference;
  }
  throw std::runtime_error("Advanced rotor requires the actual trimmed hub top");
}
inline FaceReference face(ResultFaceReference ref) {
  return f::checked(
      FaceReference::create_result(PartFeatureInputRole::SurfaceSourceFace, std::move(ref)));
}
inline EmbossIntent identifier(const PartDocument& p, const ShapeCache& c, bool raised) {
  const auto font = bundled_outline_font("Barlow");
  if (!font)
    throw std::runtime_error("Advanced rotor requires the pinned bundled Barlow outline");
  const auto text = f::checked(SketchText::create(
      SketchTextId(raised ? "identifier.raised" : "identifier.engraved"),
      SketchId("identifier.sketch"), raised ? "R" : "${n}", "Barlow",
      ParameterId("identifier.height"), raised ? Point2{-6, -2} : Point2{3, -2}, std::nullopt,
      raised ? std::vector<SketchTextBinding>{}
             : std::vector<SketchTextBinding>{{"n", ParameterId("blade.count"), 0}}));
  return {EmbossTextSource{text, *font},
          {face(top(p, c, raised ? Point3{-5, 0, 6} : Point3{4, 0, 6}))},
          raised ? EmbossOperation::Raise : EmbossOperation::Recess,
          ParameterId(raised ? "identifier.raise.depth" : "identifier.recess.depth"),
          EmbossMapping::Planar,
          {},
          0,
          {}};
}
inline PartDocument build(int count = 5) {
  auto p = rotor_performance_part(count);
  const auto pattern = *p.find_circular_pattern_feature(FeatureId("feature.blades"));
  (void)f::checked(p.remove_circular_pattern_feature(pattern.id()));
  root_neck(p);
  PartFeatureHistoryIntent order;
  for (const auto& row : f::checked(p.executable_history()))
    order.order.push_back(row.node_id);
  (void)f::checked(p.apply_history_intent(order));
  for (const auto& [id, value] : {std::pair{"direct.depth", 1.},
                                  {"root.r0", .2},
                                  {"root.r1", .3},
                                  {"mount.minor", 1.6},
                                  {"mount.major", 2.},
                                  {"mount.pitch", .4},
                                  {"mount.thread.length", 3.},
                                  {"identifier.height", 4.},
                                  {"identifier.raise.depth", .3},
                                  {"identifier.recess.depth", .2}})
    length(p, id, value);
  (void)f::checked(p.add_parameter(f::checked(Parameter::create_angle(
      ParameterId("mount.angle"), "Thread angle", f::checked(Quantity::angle_deg(60))))));
  (void)f::checked(p.add_parameter(f::checked(Parameter::create_count(
      ParameterId("mount.count"), "Mounting count", f::checked(Quantity::count(3))))));
  auto c = cache();
  execute(p, c);
  const auto choices = f::checked(SketchDrivenGuideQuery::faces(p, c, rotor_body()));
  const auto bottom = std::find_if(choices.begin(), choices.end(), [](const auto& target) {
    return target.mapping == EmbossMapping::Planar && target.normal.z < -.99 &&
           std::abs(target.anchor.z) < 1e-6;
  });
  if (bottom == choices.end())
    throw std::runtime_error("Advanced rotor requires the actual revolved hub bottom");
  (void)f::checked(p.add_direct_edit_feature(f::checked(DirectEditFeature::create(
      FeatureId("hub.direct"), "Hub underside", rotor_body(), FeatureId("feature.hub"),
      {{std::get<FaceReference>(bottom->selection.reference), DirectEditFaceCapability::Planar}},
      DirectFaceOffset{ParameterId("direct.depth")}))));
  execute(p, c);
  (void)f::checked(p.set_body_visibility(blade_body(), BodyVisibility::Visible));
  execute(p, c);
  const auto edges = f::checked(
      FinishingSelectionBuilder::build(p, c, blade_body(), PartFeatureInputRole::FilletEdge));
  const auto root = std::find_if(edges.begin(), edges.end(), [](const auto& target) {
    const auto* ref = std::get_if<EdgeReference>(&target.reference);
    return ref && std::abs(target.anchor.x - 40) < 1e-6 && std::abs(target.anchor.y) < 1e-6 &&
           std::abs(target.anchor.z - 10) < 1e-6;
  });
  if (root == edges.end())
    throw std::runtime_error("Advanced rotor requires the actual straight root/top edge");
  const auto& root_ref = std::get<EdgeReference>(root->reference);
  bool reverse = false;
  if (const auto* semantic = std::get_if<SemanticEdgeReference>(&root_ref.source())) {
    const auto endpoints = f::checked(SemanticReferenceEvaluator{}.resolve_edge(p, *semantic, c));
    reverse = endpoints.start.y > endpoints.end.y;
  }
  VariableFilletIntent law{{{"root",
                             "Blade root",
                             {{root_ref, reverse}},
                             FilletPropagation::Explicit,
                             FilletRadiusInterpolation::Linear,
                             {{"start", FilletStationLocation::Start, 0, ParameterId("root.r0")},
                              {"end", FilletStationLocation::End, 1, ParameterId("root.r1")}}}}};
  (void)f::checked(p.add_fillet_feature(f::checked(FilletFeature::create_variable(
      FeatureId("blade.root.round"), "Variable blade root", blade_body(), law))));
  (void)f::checked(p.set_body_visibility(blade_body(), BodyVisibility::Hidden));
  execute(p, c);
  curved_tip(p);
  execute(p, c);
  const auto support = top(p, c, {15, 0, 6});
  (void)f::checked(p.add_result_face_workplane(f::checked(ResultFaceWorkplane::create(
      DatumPlaneId("mount.plane"), "Actual hub mount support", support))));
  execute(p, c);
  const auto frame = f::checked(WorkplaneResolver{}.resolve(p, DatumPlaneId("mount.plane"), c));
  const Vector3 d{15 - frame.origin.x, -frame.origin.y, 6 - frame.origin.z};
  const auto thread =
      f::checked(ThreadDefinition::create(ThreadParameterProfile{"Synthetic",
                                                                 "M2 fixture",
                                                                 "Authored",
                                                                 ParameterId("mount.major"),
                                                                 ParameterId("mount.minor"),
                                                                 ParameterId("mount.pitch"),
                                                                 ParameterId("mount.angle"),
                                                                 {}},
                                          ParameterId("mount.thread.length")));
  (void)f::checked(p.add_hole_feature(f::checked(HoleFeature::create(
      FeatureId("mount.hole"), "Threaded hub mount", HoleKind::Plain, rotor_body(),
      f::checked(PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                    DatumPlaneId("mount.plane"))),
      {{HolePlacementId("seed"), Point2{dot(d, frame.x_axis), dot(d, frame.y_axis)}}},
      ParameterId("mount.minor"), HoleThroughAllExtent{}, HoleDirection::OppositeSupportNormal,
      HoleFlatTip{}, {}, thread))));
  const auto cut = f::checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, rotor_body(), std::nullopt));
  (void)f::checked(p.add_circular_pattern_feature(f::checked(CircularPatternFeature::create(
      FeatureId("mount.pattern"), "Threaded mounting circle",
      {f::checked(PatternSourceReference::feature(FeatureId("mount.hole")))},
      f::checked(AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                  DatumAxisId("axis.z"))),
      ParameterId("mount.count"), 360, true, cut, PatternPlacementOptions{}))));
  execute(p, c);
  (void)f::checked(p.add_sketch(f::checked(Sketch::create(
      SketchId("identifier.sketch"), "Rotor identifier", DatumPlaneId("datum.xy")))));
  execute(p, c);
  for (const bool raised : {true, false}) {
    (void)f::checked(p.add_sketch_driven_feature(f::checked(SketchDrivenFeature::create(
        FeatureId(raised ? "identifier.raise" : "identifier.engrave"),
        raised ? "Raised rotor R" : "Engraved blade count", rotor_body(),
        SketchDrivenDefinition{identifier(p, c, raised)}))));
    execute(p, c);
  }
  (void)f::checked(p.add_circular_pattern_feature(pattern));
  execute(p, c);
  engineering_fixture::annotate(p);
  return p;
}
} // namespace blcad::test::advanced_rotor
