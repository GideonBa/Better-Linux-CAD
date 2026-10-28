#pragma once

#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

namespace blcad::test {

// One visible, connected curved solid: a revolved hub with repeated spline
// blades. The hidden source blade remains an editable upstream dependency.
inline PartDocument rotor_performance_part(int blades, const char* hub_feature = "feature.hub") {
  auto created = PartDocument::create(DocumentId("part.performance.rotor"), "Curved rotor");
  REQUIRE(created);
  auto part = created.value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(part.add_datum_plane(DatumPlane::xz().value()));
  const auto length = [&](const char* id, double value) {
    auto quantity = Quantity::length_mm(value, id);
    REQUIRE(quantity);
    auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
    REQUIRE(parameter);
    REQUIRE(part.add_parameter(parameter.value()));
  };
  length("hub.radius", 45);
  length("hub.height", 6);
  length("blade.height", 10);
  length("fillet.radius", 0.2);
  auto count = Quantity::count(blades, "blade.count");
  REQUIRE(count);
  REQUIRE(part.add_parameter(
      Parameter::create_count(ParameterId("blade.count"), "blade.count", count.value()).value()));
  REQUIRE(part.add_datum_axis(
      DatumAxis::create_explicit(DatumAxisId("axis.z"), "Rotor axis", {}, {0, 0, 1}).value()));
  REQUIRE(part.add_body(Body::create(BodyId("body.rotor"), "Rotor", BodyKind::Solid).value()));
  REQUIRE(part.add_body(
      Body::create(BodyId("body.blade"), "Source blade", BodyKind::Solid, BodyVisibility::Hidden)
          .value()));
  const auto new_body = [](const char* id) {
    return FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                            BodyId(id))
        .value();
  };
  auto hub = Sketch::create(SketchId("sketch.hub"), "Hub", DatumPlaneId("datum.xz")).value();
  REQUIRE(
      hub.add_profile(RectangleProfile::create(ProfileId("profile.hub"), ParameterId("hub.radius"),
                                               ParameterId("hub.height"), {22.5, 3})
                          .value()));
  REQUIRE(part.add_sketch(hub));
  auto profile = ProfileRegionReference::create(hub.id(), ProfileId("profile.hub"),
                                                PartFeatureInputRole::RevolveProfile)
                     .value();
  auto revolve_axis =
      AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis, DatumAxisId("axis.z"))
          .value();
  REQUIRE(part.add_revolve_feature(
      RevolveFeature::create_revolve(FeatureId(hub_feature), "Revolved hub", profile, revolve_axis,
                                     RevolveAngleExtent::full(), new_body("body.rotor"))
          .value()));
  auto blade =
      Sketch::create(SketchId("sketch.blade"), "Curved blade", DatumPlaneId("datum.xy")).value();
  const auto lower = SplineSegment::create_cubic_bezier(SketchEntityId("blade.lower"), {40, -2},
                                                        {55, -2}, {70, 12}, {90, 12})
                         .value();
  const auto tip = LineSegment::create(SketchEntityId("blade.tip"), {90, 12}, {90, 16}).value();
  const auto upper = SplineSegment::create_cubic_bezier(SketchEntityId("blade.upper"), {90, 16},
                                                        {70, 16}, {55, 2}, {40, 2})
                         .value();
  const auto root = LineSegment::create(SketchEntityId("blade.root"), {40, 2}, {40, -2}).value();
  REQUIRE(blade.add_entity(lower));
  REQUIRE(blade.add_entity(tip));
  REQUIRE(blade.add_entity(upper));
  REQUIRE(blade.add_entity(root));
  REQUIRE(blade.add_profile(ArcClosedProfile::create(ProfileId("profile.blade"),
                                                     {lower.id(), tip.id(), upper.id(), root.id()})
                                .value()));
  REQUIRE(part.add_sketch(blade));
  auto extrusion = Feature::create_additive_extrude(FeatureId("feature.blade"), "Source blade",
                                                    blade.id(), ParameterId("blade.height"))
                       .value()
                       .with_profile(ProfileId("profile.blade"))
                       .value()
                       .with_body_result_context(new_body("body.blade"));
  REQUIRE(extrusion);
  REQUIRE(part.add_feature(extrusion.value()));
  auto pattern_axis =
      AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis.z"))
          .value();
  auto join = FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, BodyId("body.rotor"),
                                               std::nullopt)
                  .value();
  REQUIRE(part.add_circular_pattern_feature(
      CircularPatternFeature::create(FeatureId("feature.blades"), "Repeated curved blades",
                                     {PatternSourceReference::body(BodyId("body.blade")).value()},
                                     pattern_axis, ParameterId("blade.count"), 360, true, join)
          .value()));
  return part;
}

// Serial alignment operations create a real deep producer chain. Paired
// translations preserve the tip coordinates used by independent pick oracles.
inline PartDocument rotor_performance_sequence_part(int blades) {
  auto part = rotor_performance_part(blades);
  const int depth = blades <= 5 ? 4 : blades <= 20 ? 12 : 24;
  for (int index = 0; index < depth; ++index) {
    auto transform = BodyTransform::create_translate(
        BodyTransformId("align." + std::to_string(index)), BodyId("body.rotor"),
        BodyTransformCoordinateSpace::World, std::nullopt, {0, 0, index % 2 == 0 ? 0.05 : -0.05},
        false, false);
    REQUIRE(transform);
    REQUIRE(part.add_body_transform(transform.value()));
  }
  REQUIRE(part.add_parameter(
      Parameter::create_length(ParameterId("blade.tip.dimension"), "Blade tip",
                               Quantity::length_mm(4, "blade.tip.dimension").value())
          .value()));
  return part;
}

} // namespace blcad::test
