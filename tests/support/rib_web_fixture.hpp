#pragma once
#include "blcad/core/sketch_driven_intent.hpp"
#include "part_history_fixture.hpp"
namespace blcad::test::rib_fixture {
inline PartDocument fixture(std::vector<LineSegment> lines = {}) {
  auto p = history_fixture::native_base();
  auto sketch = history_fixture::checked(
      Sketch::create(SketchId("rib.sketch"), "Skeleton", DatumPlaneId("datum.xy")));
  if (lines.empty())
    lines.push_back(
        history_fixture::checked(LineSegment::create(SketchEntityId("line"), {-8, 0}, {8, 0})));
  for (auto line : lines)
    history_fixture::accept(sketch.add_entity(line));
  history_fixture::accept(p.add_sketch(sketch));
  for (const auto& [id, value] : {std::pair{"rib.t", 1.0}, {"rib.l", 12.0}, {"rib.second", 2.0}})
    history_fixture::accept(p.add_parameter(history_fixture::checked(Parameter::create_length(
        ParameterId(id), id, history_fixture::checked(Quantity::length_mm(value))))));
  history_fixture::accept(p.add_parameter(history_fixture::checked(Parameter::create_angle(
      ParameterId("rib.draft"), "Draft", history_fixture::checked(Quantity::angle_deg(5))))));
  return p;
}
inline RibWebIntent intent() {
  return {RibWebKind::Rib,
          {SketchId("rib.sketch"), {SketchEntityId("line")}, std::nullopt},
          RibExtrusionPlane::PerpendicularToSketch,
          {0, 0, 1},
          RibThicknessSide::Symmetric,
          ParameterId("rib.t"),
          std::nullopt,
          std::nullopt,
          RibDistanceExtent{ParameterId("rib.l")},
          RibCrossingPolicy::Union};
}
inline void add(PartDocument& p, RibWebIntent r = intent()) {
  history_fixture::accept(
      p.add_sketch_driven_feature(history_fixture::checked(SketchDrivenFeature::create_rib_web(
          FeatureId("rib"), "Rib", BodyId("body.base"), std::move(r)))));
}
} // namespace blcad::test::rib_fixture
