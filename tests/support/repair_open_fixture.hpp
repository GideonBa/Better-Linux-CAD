#pragma once
#include "reference_repair_fixture.hpp"

namespace blcad::test {
// Core-valid intent whose plane-driven consumers fail initial full recompute.
inline PartDocument repair_open_fixture() {
  auto part = reference_repair_fixture(true, true);
  REQUIRE(part.add_derived_workplane(reference_checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("plane.second"), "Second plane",
      reference_checked(
          SemanticFaceReference::create(FeatureId("feature.old"), SemanticFace::Top))))));
  for (const std::string suffix : {"one", "two"}) {
    auto sketch =
        reference_checked(Sketch::create(SketchId("face." + suffix), "Face " + suffix,
                                         DatumPlaneId(suffix == "one" ? "plane" : "plane.second")));
    REQUIRE(sketch.add_profile(reference_checked(RectangleProfile::create(
        ProfileId("face.profile." + suffix), ParameterId("width"), ParameterId("height")))));
    REQUIRE(part.add_sketch(sketch));
    REQUIRE(part.add_feature(reference_checked(Feature::create_additive_extrude(
        FeatureId("face.solid." + suffix), "Solid " + suffix, sketch.id(), ParameterId("depth")))));
  }
  return part;
}
inline Result<std::size_t> repair_open_planes(PartDocument& part, bool complete = true) {
  for (const std::string& id : complete ? std::vector<std::string>{"plane", "plane.second"}
                                        : std::vector<std::string>{"plane"}) {
    const auto* plane = part.find_derived_workplane(DatumPlaneId(id));
    if (!plane)
      return Result<std::size_t>::failure(Error::validation(id, "plane missing"));
    auto changed =
        part.update_derived_workplane(reference_checked(DerivedWorkplane::create_on_feature_face(
            plane->id(), plane->name(),
            reference_checked(
                SemanticFaceReference::create(FeatureId("feature.new"), SemanticFace::Top)))));
    if (!changed)
      return changed;
  }
  return Result<std::size_t>::success(complete ? 2 : 1);
}
} // namespace blcad::test
