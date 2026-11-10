#pragma once

#include "hole_feature_fixture.hpp"

namespace blcad::test::hole_pattern_fixture {
using history_fixture::accept;
using history_fixture::checked;
inline AxisReference axis(const char* name) {
  return checked(
      AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId(name)));
}
inline PatternSourceReference source(const char* name = "holes") {
  return checked(PatternSourceReference::feature(FeatureId(name)));
}
inline FeatureBodyResultContext context() {
  return checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                  BodyId("body.base"), std::nullopt));
}
inline PatternPlacementOptions options(bool second = true) {
  PatternPlacementOptions value;
  if (second)
    value.second_direction = LinearPatternDirection{
        axis("axis.y"), ParameterId("rows"), LinearPatternExtentMode::Spacing,
        ParameterId("spacing.y"), PatternDirectionSign::Positive};
  return value;
}
inline LinearPatternFeature linear(PatternPlacementOptions value = options()) {
  return checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Hole pattern", {source()}, axis("axis.x"), ParameterId("count"),
      LinearPatternExtentMode::Spacing, ParameterId("spacing.x"), PatternDirectionSign::Positive,
      context(), std::move(value)));
}
inline CircularPatternFeature circular(double angle = 360,
                                       PatternPlacementOptions value = options(false)) {
  return checked(CircularPatternFeature::create(FeatureId("pattern"), "Hole circle", {source()},
                                                axis("axis"), ParameterId("count"), angle, true,
                                                context(), std::move(value)));
}
inline PartDocument part(bool multiple = false, bool seat = true) {
  auto p = history_fixture::native_base();
  (void)checked(p.set_parameter_value(ParameterId("width"), checked(Quantity::length_mm(80))));
  (void)checked(p.set_parameter_value(ParameterId("height"), checked(Quantity::length_mm(60))));
  for (const auto& [name, value] : {std::pair{"diameter", 3.0},
                                    {"spacing.x", 12.0},
                                    {"spacing.y", 15.0},
                                    {"drill.depth", 6.0},
                                    {"seat.diameter", 5.0},
                                    {"seat.depth", 1.0}})
    accept(p.add_parameter(checked(
        Parameter::create_length(ParameterId(name), name, checked(Quantity::length_mm(value))))));
  accept(p.add_parameter(
      checked(Parameter::create_count(ParameterId("rows"), "Rows", checked(Quantity::count(2))))));
  for (const auto& [name, direction] :
       {std::pair{"axis.x", Vector3{1, 0, 0}}, {"axis.y", Vector3{0, 1, 0}}})
    accept(p.add_datum_axis(
        checked(DatumAxis::create_explicit(DatumAxisId(name), name, {}, direction))));
  const auto support = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  std::vector<HolePlacement> placements{{HolePlacementId("first"), Point2{-20, -15}}};
  if (multiple)
    placements.push_back({HolePlacementId("other"), Point2{-20, -6}});
  HoleManufacturing manufacturing;
  if (seat)
    manufacturing = HoleCounterbore{ParameterId("seat.diameter"), ParameterId("seat.depth")};
  accept(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("holes"), "Holes", HoleKind::Plain, BodyId("body.base"), support,
      std::move(placements), ParameterId("diameter"), HoleBlindExtent{ParameterId("drill.depth")},
      HoleDirection::OppositeSupportNormal, HoleFlatTip{}, std::move(manufacturing)))));
  return p;
}
} // namespace blcad::test::hole_pattern_fixture
