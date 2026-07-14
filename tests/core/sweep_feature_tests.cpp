#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;

namespace {
Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto result = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(result);
  return result.value();
}

Parameter angle_parameter(const char* id, double value) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  auto result = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(result);
  return result.value();
}

Body body(const char* id, BodyKind kind) {
  auto result = Body::create(BodyId(id), id, kind);
  REQUIRE(result);
  return result.value();
}

FeatureBodyResultContext new_body(const char* id) {
  auto result =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id));
  REQUIRE(result);
  return result.value();
}

FeatureBodyResultContext cut_body(const char* id) {
  auto result =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId(id), std::nullopt);
  REQUIRE(result);
  return result.value();
}

PathCurve path(const char* id, const char* entity) {
  auto segment = PathSegmentReference::create_planar(
      SketchId("sketch.paths"), SketchEntityId(entity), PlanarPathCurveKind::Line);
  REQUIRE(segment);
  auto result = PathCurve::create(PathCurveId(id), id, {segment.value()}, PathClosure::Open,
                                  PathOrientationRule::MinimumTwist);
  REQUIRE(result);
  return result.value();
}

PartDocument sweep_document() {
  auto document = PartDocument::create(DocumentId("part.sweep"), "Sweep");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.value().add_parameter(length_parameter("profile.width", 4.0)));
  REQUIRE(document.value().add_parameter(length_parameter("profile.height", 2.0)));
  REQUIRE(document.value().add_parameter(angle_parameter("sweep.twist", 30.0)));

  auto profile = Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy"));
  REQUIRE(profile);
  auto rectangle = RectangleProfile::create(
      ProfileId("profile.closed"), ParameterId("profile.width"), ParameterId("profile.height"));
  REQUIRE(rectangle);
  REQUIRE(profile.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(profile.value()));

  auto paths = Sketch::create(SketchId("sketch.paths"), "Paths", DatumPlaneId("datum.xy"));
  REQUIRE(paths);
  auto trajectory = LineSegment::create(SketchEntityId("line.trajectory"), {0.0, 0.0}, {8.0, 0.0});
  auto open_profile =
      LineSegment::create(SketchEntityId("line.open-profile"), {0.0, 0.0}, {0.0, 2.0});
  REQUIRE(trajectory);
  REQUIRE(open_profile);
  REQUIRE(paths.value().add_entity(trajectory.value()));
  REQUIRE(paths.value().add_entity(open_profile.value()));
  REQUIRE(document.value().add_sketch(paths.value()));
  REQUIRE(document.value().add_path_curve(path("path.trajectory", "line.trajectory")));
  REQUIRE(document.value().add_path_curve(path("path.open-profile", "line.open-profile")));

  REQUIRE(document.value().add_body(body("body.sweep", BodyKind::Solid)));
  REQUIRE(document.value().add_body(body("body.cut", BodyKind::Solid)));
  REQUIRE(document.value().add_body(body("body.surface", BodyKind::Surface)));
  REQUIRE(document.value().add_body(body("body.wrong", BodyKind::Solid)));
  return document.value();
}

SweepProfileReference closed_profile() {
  auto profile = ProfileRegionReference::create(
      SketchId("sketch.profile"), ProfileId("profile.closed"), PartFeatureInputRole::SweepProfile);
  REQUIRE(profile);
  auto result = SweepProfileReference::create_closed_region(profile.value());
  REQUIRE(result);
  return result.value();
}

SweepProfileReference open_profile() {
  auto result = SweepProfileReference::create_open_path(PathCurveId("path.open-profile"));
  REQUIRE(result);
  return result.value();
}
} // namespace

TEST_CASE("Block 80 freezes solid cut and surface sweep intent", "[core][sweep-feature]") {
  auto solid = SweepFeature::create_sweep(FeatureId("sweep.solid"), "Solid", closed_profile(),
                                          PathCurveId("path.trajectory"), new_body("body.sweep"));
  REQUIRE(solid);
  CHECK(solid.value().kind() == SweepFeatureKind::Sweep);
  CHECK(solid.value().profile().kind() == SweepProfileKind::ClosedRegion);
  CHECK_FALSE(solid.value().orientation_override().has_value());

  auto cut = SweepFeature::create_sweep_cut(FeatureId("sweep.cut"), "Cut", closed_profile(),
                                            PathCurveId("path.trajectory"), cut_body("body.cut"),
                                            PathOrientationRule::ProfileNormal, std::nullopt,
                                            ParameterId("sweep.twist"));
  REQUIRE(cut);
  CHECK(cut.value().kind() == SweepFeatureKind::SweepCut);
  CHECK(cut.value().twist_parameter() == ParameterId("sweep.twist"));

  auto surface = SweepFeature::create_sweep_surface(
      FeatureId("sweep.surface"), "Surface", open_profile(), PathCurveId("path.trajectory"),
      new_body("body.surface"), PathOrientationRule::FixedUpVector, Vector3{0.0, 0.0, 1.0});
  REQUIRE(surface);
  CHECK(surface.value().kind() == SweepFeatureKind::SweepSurface);
  CHECK(surface.value().fixed_up_vector_override()->z == 1.0);
}

TEST_CASE("Block 80 integrates sweep sources bodies and twist into the dependency graph",
          "[core][sweep-feature]") {
  auto document = sweep_document();
  auto feature = SweepFeature::create_sweep_cut(
      FeatureId("sweep.cut"), "Cut", closed_profile(), PathCurveId("path.trajectory"),
      cut_body("body.cut"), PathOrientationRule::MinimumTwist, std::nullopt,
      ParameterId("sweep.twist"));
  REQUIRE(feature);
  REQUIRE(document.add_sweep_feature(feature.value()));
  CHECK(document.sweep_feature_count() == 1U);
  CHECK(document.dependency_graph().has_dependency("sketch.profile", "sweep.cut"));
  CHECK(document.dependency_graph().has_dependency("path.trajectory", "sweep.cut"));
  CHECK(document.dependency_graph().has_dependency("sweep.twist", "sweep.cut"));
  CHECK(document.remove_path_curve(PathCurveId("path.trajectory")).has_error());
  CHECK(document.remove_body(BodyId("body.cut")).has_error());
  auto affected = document.mark_parameter_changed(ParameterId("sweep.twist"));
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "sweep.cut") !=
        affected.value().end());
}

TEST_CASE("Block 80 roundtrips all sweep families and remains additive", "[core][sweep-feature]") {
  auto document = sweep_document();
  auto solid = SweepFeature::create_sweep(FeatureId("sweep.solid"), "Solid", closed_profile(),
                                          PathCurveId("path.trajectory"), new_body("body.sweep"));
  auto cut = SweepFeature::create_sweep_cut(FeatureId("sweep.cut"), "Cut", closed_profile(),
                                            PathCurveId("path.trajectory"), cut_body("body.cut"),
                                            PathOrientationRule::ProfileNormal, std::nullopt,
                                            ParameterId("sweep.twist"));
  auto surface = SweepFeature::create_sweep_surface(
      FeatureId("sweep.surface"), "Surface", open_profile(), PathCurveId("path.trajectory"),
      new_body("body.surface"), PathOrientationRule::FixedUpVector, Vector3{0.0, 0.0, 1.0});
  REQUIRE(solid);
  REQUIRE(cut);
  REQUIRE(surface);
  REQUIRE(document.add_sweep_feature(solid.value()));
  REQUIRE(document.add_sweep_feature(cut.value()));
  REQUIRE(document.add_sweep_feature(surface.value()));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().sweep_feature_count() == 3U);
  CHECK(restored.value().find_sweep_feature(FeatureId("sweep.surface"))->profile().kind() ==
        SweepProfileKind::OpenPath);
  CHECK(restored.value().find_sweep_feature(FeatureId("sweep.cut"))->twist_parameter() ==
        ParameterId("sweep.twist"));

  json historical = json::parse(serialized.value());
  historical.erase("sweep_features");
  auto old = deserialize_part_document_from_json(historical.dump());
  REQUIRE(old);
  CHECK(old.value().sweep_feature_count() == 0U);
}

TEST_CASE("Block 80 rejects mismatched profiles operations bodies and malformed JSON",
          "[core][sweep-feature]") {
  CHECK(SweepFeature::create_sweep_surface(FeatureId("bad.surface"), "Bad", closed_profile(),
                                           PathCurveId("path.trajectory"), new_body("body.surface"))
            .has_error());
  CHECK(SweepFeature::create_sweep_cut(FeatureId("bad.cut"), "Bad", closed_profile(),
                                       PathCurveId("path.trajectory"), new_body("body.sweep"))
            .has_error());
  CHECK(SweepFeature::create_sweep(FeatureId("bad.up"), "Bad", closed_profile(),
                                   PathCurveId("path.trajectory"), new_body("body.sweep"),
                                   PathOrientationRule::FixedUpVector)
            .has_error());

  auto document = sweep_document();
  auto wrong_body =
      SweepFeature::create_sweep_surface(FeatureId("bad.body"), "Bad", open_profile(),
                                         PathCurveId("path.trajectory"), new_body("body.wrong"));
  REQUIRE(wrong_body);
  CHECK(document.add_sweep_feature(wrong_body.value()).has_error());

  auto valid = SweepFeature::create_sweep(FeatureId("sweep.solid"), "Solid", closed_profile(),
                                          PathCurveId("path.trajectory"), new_body("body.sweep"));
  REQUIRE(valid);
  REQUIRE(document.add_sweep_feature(valid.value()));
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  json malformed = json::parse(serialized.value());
  malformed["sweep_features"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
}
