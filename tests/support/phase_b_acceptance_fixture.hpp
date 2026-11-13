#pragma once

#include "hole_pattern_fixture.hpp"
#include "thread_feature_fixture.hpp"

#include <numbers>

namespace blcad::test::phase_b_fixture {
using history_fixture::checked;
inline PartDocument stock(bool migrated) {
  auto part = hole_pattern_fixture::part(false, false);
  (void)checked(part.remove_hole_feature(FeatureId("holes")));
  for (const auto& [id, value] : {std::pair{"legacy.diameter", 2.},
                                  {"unrelated.width", 4.},
                                  {"unrelated.height", 6.},
                                  {"unrelated.depth", 2.},
                                  {"fillet.radius", .5},
                                  {"chamfer.distance", .25},
                                  {"thread.major", 5.},
                                  {"thread.pitch", 1.5},
                                  {"thread.start", 1.},
                                  {"thread.length", 3.}})
    (void)checked(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  (void)checked(part.add_parameter(checked(Parameter::create_angle(
      ParameterId("thread.angle"), "Thread angle", checked(Quantity::angle_deg(60))))));
  if (migrated) {
    auto sketch = checked(
        Sketch::create(SketchId("legacy.profile"), "Legacy circle", DatumPlaneId("datum.xy")));
    (void)checked(sketch.add_profile(checked(CircleProfile::create(
        ProfileId("legacy.circle"), ParameterId("legacy.diameter"), {30, 20}))));
    (void)checked(part.add_sketch(std::move(sketch)));
    const auto cut = checked(Feature::create_subtractive_extrude(
        FeatureId("legacy"), "Legacy cut", SketchId("legacy.profile"), FeatureId("base")));
    (void)checked(
        part.add_feature(checked(cut.with_body_result_context(hole_pattern_fixture::context()))));
  }
  auto unrelated = checked(
      Sketch::create(SketchId("unrelated.profile"), "Unrelated solid", DatumPlaneId("datum.xy")));
  (void)checked(unrelated.add_profile(checked(
      RectangleProfile::create(ProfileId("unrelated.rectangle"), ParameterId("unrelated.width"),
                               ParameterId("unrelated.height"), {60, 0}))));
  (void)checked(part.add_sketch(std::move(unrelated)));
  const auto context = history_fixture::new_body(part, "unrelated");
  const auto creator = checked(Feature::create_additive_extrude(
      FeatureId("unrelated"), "Unrelated creator", SketchId("unrelated.profile"),
      ParameterId("unrelated.depth")));
  (void)checked(part.add_feature(checked(creator.with_body_result_context(context))));
  (void)checked(part.set_body_visibility(BodyId("body.unrelated"), BodyVisibility::Hidden));
  engineering_fixture::annotate(part);
  if (migrated) {
    const auto bytes = checked(serialize_part_document_to_json(part));
    if (nlohmann::json::parse(bytes).at("version") != 3)
      throw std::runtime_error("migration input must retain the pre-Hole V3 envelope");
    part = checked(deserialize_part_document_from_json(bytes));
  }
  return part;
}
inline PartDocument complete(bool migrated) {
  auto part = stock(migrated);
  const auto thread =
      checked(ThreadDefinition::create(ThreadParameterProfile{"Synthetic",
                                                              "Authored 3/5",
                                                              "Authored tolerance",
                                                              ParameterId("thread.major"),
                                                              ParameterId("diameter"),
                                                              ParameterId("thread.pitch"),
                                                              ParameterId("thread.angle"),
                                                              {}},
                                       ParameterId("thread.length"), ParameterId("thread.start")));
  (void)checked(part.add_hole_feature(checked(HoleFeature::create(
      FeatureId("holes"), "Threaded Counterbores", HoleKind::Plain, BodyId("body.base"),
      checked(PlaneReference::create_semantic_face(
          PartFeatureInputRole::HoleSupportPlane,
          checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top)))),
      {{HolePlacementId("first"), Point2{-20, -15}}, {HolePlacementId("other"), Point2{-20, -6}}},
      ParameterId("diameter"), HoleThroughAllExtent{}, HoleDirection::OppositeSupportNormal,
      HoleFlatTip{}, HoleCounterbore{ParameterId("seat.diameter"), ParameterId("seat.depth")},
      thread))));
  auto options = hole_pattern_fixture::options();
  options.skipped = {{2, 1}};
  (void)checked(part.add_linear_pattern_feature(hole_pattern_fixture::linear(options)));
  (void)checked(part.add_fillet_feature(checked(FilletFeature::create(
      FeatureId("fillet"), "Outer Fillet", BodyId("body.base"),
      {history_fixture::edge(PartFeatureInputRole::FilletEdge)}, ParameterId("fillet.radius")))));
  const auto opposite = checked(EdgeReference::create_linear(
      PartFeatureInputRole::ChamferEdge,
      checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::BottomBack))));
  (void)checked(part.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
      FeatureId("chamfer"), "Separate outer Chamfer", BodyId("body.base"), {opposite},
      ParameterId("chamfer.distance")))));
  return part;
}
inline double base_volume(unsigned holes, bool migrated, double diameter = 3) {
  const double finishing = 80 * (.25 * (1 - std::numbers::pi / 4) + .0625 / 2);
  const double bore = diameter * diameter / 4;
  return 38400 - finishing - (migrated ? 8 * std::numbers::pi : 0) -
         holes * std::numbers::pi * (8 * bore + 6.25 - bore);
}
} // namespace blcad::test::phase_b_fixture
