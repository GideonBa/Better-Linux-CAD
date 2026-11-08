#include "../support/hole_feature_fixture.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <BRepClass3d_SolidClassifier.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <limits>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace fixture = blcad::test::hole_fixture;
namespace history = blcad::test::history_fixture;
using fixture::checked;
namespace {
ShapeCache evaluate(const PartDocument& p) {
  auto cache = checked(ShapeCache::create(ShapeCacheId("hole.physical")));
  (void)checked(GeometryRecomputeExecutor{}.execute_document(p, cache));
  return cache;
}
double volume(const ShapeCache& cache) {
  return RectangleExtrusionAdapter{}
      .summarize(*cache.find_body_shape(BodyId("body.base")))
      .volume_mm3;
}
bool material_at(const ShapeCache& cache, Point3 point) {
  const auto* shape = OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")));
  BRepClass3d_SolidClassifier classifier(*shape, gp_Pnt(point.x, point.y, point.z), 1e-7);
  return classifier.State() == TopAbs_IN;
}
void dimension(PartDocument& p, const char* id, double value) {
  fixture::accept(p.add_parameter(
      checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
}
PlaneReference construction_support(PartDocument& p, Point3 origin = {0, 0, 8},
                                    Vector3 x = {1, 0, 0}, Vector3 y = {0, 1, 0},
                                    Vector3 normal = {0, 0, 1}) {
  fixture::accept(p.add_construction_plane(checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("hole.support"), "Hole support", origin, x, y, normal))));
  return checked(PlaneReference::create_construction_plane(PartFeatureInputRole::HoleSupportPlane,
                                                           ConstructionPlaneId("hole.support")));
}
} // namespace

TEST_CASE("Straight semantic Hole cuts each rectangular support in the authored forward direction",
          "[geometry][parity-b153][hole-straight-cutting]") {
  const auto face = GENERATE(SemanticFace::Top, SemanticFace::Bottom, SemanticFace::Left,
                             SemanticFace::Right, SemanticFace::Front, SemanticFace::Back);
  auto p = history::native_base();
  const auto support = checked(PlaneReference::create_semantic_face(
      PartFeatureInputRole::HoleSupportPlane,
      checked(SemanticFaceReference::create(FeatureId("base"), face))));
  REQUIRE(p.add_hole_feature(checked(
      HoleFeature::create(FeatureId("hole"), "Hole", HoleKind::Plain, BodyId("body.base"), support,
                          {{HolePlacementId("center"), Point2{}}}, ParameterId("radius")))));
  auto cache = evaluate(p);
  const double length = face == SemanticFace::Top || face == SemanticFace::Bottom   ? 8.
                        : face == SemanticFace::Left || face == SemanticFace::Right ? 20.
                                                                                    : 10.;
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * length).margin(1e-6));
  REQUIRE(cache.validate_complete_geometry_result(p));
  CHECK(cache.find_body_result(BodyId("body.base"))->source_feature_id == FeatureId("hole"));
  const auto encoded = checked(serialize_part_document_to_json(p));
  CHECK(volume(evaluate(checked(deserialize_part_document_from_json(encoded)))) ==
        Catch::Approx(volume(cache)).margin(1e-6));
}

TEST_CASE("Blind semantic Hole separates cylinder depth from flat and conical bottom geometry",
          "[geometry][parity-b153][hole-straight-cutting]") {
  const bool conical = GENERATE(false, true);
  auto p = history::native_base();
  dimension(p, "hole.diameter", 2);
  dimension(p, "hole.depth", 3);
  REQUIRE(p.set_parameter_value(ParameterId("angle"), checked(Quantity::angle_deg(90))));
  REQUIRE(p.add_hole_feature(fixture::hole(
      "hole", ParameterId("hole.diameter"), HoleBlindExtent{ParameterId("hole.depth")},
      conical ? HoleTip{HoleConicalTip{ParameterId("angle")}} : HoleTip{HoleFlatTip{}})));
  const auto cache = evaluate(p);
  const double removed = std::numbers::pi * (3 + (conical ? 1. / 3 : 0));
  CHECK(volume(cache) == Catch::Approx(1600 - removed).margin(1e-6));
  CHECK_FALSE(material_at(cache, {0, 0, 6}));
  CHECK(material_at(cache, {0, 0, 3.9}));
  CHECK(material_at(cache, {.75, 0, 4.5}));
  CHECK(material_at(cache, {0, 0, 4.5}) == !conical);
}

TEST_CASE("Datum-supported Hole placements cut independently and publish atomically",
          "[geometry][parity-b153][hole-straight-cutting]") {
  const bool invalid_second = GENERATE(false, true);
  auto p = history::native_base();
  const auto original = p;
  auto cache = evaluate(p);
  const auto support = checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("datum.xy")));
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Two holes", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("left"), Point2{-3, 0}},
       {HolePlacementId("right"), Point2{invalid_second ? 100. : 3., 0}}},
      ParameterId("radius"), HoleThroughAllExtent{}, HoleDirection::SupportNormal))));
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, cache);
  if (invalid_second) {
    REQUIRE_FALSE(result);
    REQUIRE(cache.validate_complete_geometry_result(original));
    CHECK(volume(cache) == Catch::Approx(1600));
    CHECK(cache.find_feature_shape(FeatureId("hole")) == nullptr);
  } else {
    REQUIRE(result);
    REQUIRE(cache.validate_complete_geometry_result(p));
    CHECK(volume(cache) == Catch::Approx(1600 - 2 * std::numbers::pi * .25 * 8).margin(1e-6));
    CHECK_FALSE(material_at(cache, {-3, 0, 4}));
    CHECK_FALSE(material_at(cache, {3, 0, 4}));
    CHECK(material_at(cache, {0, 0, 4}));
  }
}

TEST_CASE("Invalid Hole tools preserve the previously certified Body result",
          "[geometry][parity-b153][hole-straight-cutting]") {
  const int scenario = GENERATE(0, 1, 2, 3, 4);
  CAPTURE(scenario);
  auto p = history::native_base();
  dimension(p, "hole.diameter", scenario == 3 ? 100 : 2);
  dimension(p, "hole.depth", scenario == 2 ? 7 : scenario == 4 ? 3 : 9);
  REQUIRE(p.set_parameter_value(ParameterId("angle"), checked(Quantity::angle_deg(90))));
  const auto original = p;
  auto cache = evaluate(p);
  const auto top = fixture::hole().support();
  const auto datum = checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("datum.xy")));
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Invalid hole", HoleKind::Plain, BodyId("body.base"),
      scenario >= 3 ? datum : top,
      {{HolePlacementId("center"), Point2{scenario == 4 ? 9.5 : 0, 0}}},
      ParameterId("hole.diameter"),
      scenario == 1 || scenario == 2 || scenario == 4
          ? HoleExtent{HoleBlindExtent{ParameterId("hole.depth")}}
          : HoleExtent{HoleThroughAllExtent{}},
      scenario == 0 || scenario >= 3 ? HoleDirection::SupportNormal
                                     : HoleDirection::OppositeSupportNormal,
      scenario == 2 ? HoleTip{HoleConicalTip{ParameterId("angle")}} : HoleTip{HoleFlatTip{}}))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(volume(cache) == Catch::Approx(1600));
  CHECK(cache.find_body_result(BodyId("body.base"))->source_feature_id == FeatureId("base"));
}

TEST_CASE("Projected Hole centers follow their Sketch frame and reject noncoplanar supports",
          "[geometry][parity-b153][hole-straight-cutting]") {
  const bool coplanar = GENERATE(false, true);
  auto p = history::native_base();
  REQUIRE(p.add_construction_point(checked(
      ConstructionPoint::create_explicit(ConstructionPointId("anchor"), "Anchor", {2, -3, 0}))));
  auto sketch = checked(Sketch::create(SketchId("centers"), "Centers", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_reference(checked(ProjectedSketchPoint::create_from_construction_point(
      SketchEntityId("projected"), ConstructionPointId("anchor")))));
  REQUIRE(p.add_sketch(std::move(sketch)));
  const auto original = p;
  auto cache = evaluate(p);
  const auto support =
      coplanar ? checked(PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                            DatumPlaneId("datum.xy")))
               : fixture::hole().support();
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Projected Hole", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("stable"),
        HoleSketchPointReference{SketchId("centers"), SketchEntityId("projected")}}},
      ParameterId("radius"), HoleThroughAllExtent{}, HoleDirection::SupportNormal))));
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, cache);
  if (!coplanar) {
    REQUIRE_FALSE(result);
    CHECK(result.error().message().find("must lie on its support plane") != std::string::npos);
    REQUIRE(cache.validate_complete_geometry_result(original));
    return;
  }
  REQUIRE(result);
  CHECK_FALSE(material_at(cache, {2, -3, 4}));
  REQUIRE(p.update_construction_point(checked(
      ConstructionPoint::create_explicit(ConstructionPointId("anchor"), "Anchor", {4, 1, 0}))));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  CHECK(material_at(cache, {2, -3, 4}));
  CHECK_FALSE(material_at(cache, {4, 1, 4}));
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * 8).margin(1e-6));
}

TEST_CASE("Construction-plane Hole changes only its explicit target Body",
          "[geometry][parity-b153][hole-straight-cutting]") {
  auto p = history::native_base();
  REQUIRE(p.add_feature(
      checked(checked(Feature::create_additive_extrude(FeatureId("second"), "Second",
                                                       SketchId("profile"), ParameterId("depth")))
                  .with_body_result_context(history::new_body(p, "second")))));
  REQUIRE(p.add_construction_plane(checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("support"), "Support", {0, 0, 8}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  const auto support = checked(PlaneReference::create_construction_plane(
      PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("support")));
  REQUIRE(p.add_hole_feature(checked(
      HoleFeature::create(FeatureId("hole"), "Hole", HoleKind::Plain, BodyId("body.base"), support,
                          {{HolePlacementId("center"), Point2{}}}, ParameterId("radius")))));
  const auto cache = evaluate(p);
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * 8).margin(1e-6));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.second")))
            .volume_mm3 == Catch::Approx(1600));
  CHECK(cache.find_body_result(BodyId("body.second"))->source_feature_id == FeatureId("second"));
  REQUIRE(cache.validate_complete_geometry_result(p));
}

TEST_CASE("Hole extents use actual material and preserve their limit Body",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  const int mode = GENERATE(0, 1, 2);
  CAPTURE(mode);
  auto p = history::native_base();
  dimension(p, "limit.depth", 4);
  REQUIRE(p.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit", SketchId("profile"),
                                               ParameterId("limit.depth")))
          .with_body_result_context(history::new_body(p, "limit")))));
  const auto face = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("limit"), SemanticFace::Top))));
  HoleExtent extent = HoleToNextExtent{};
  if (mode == 1)
    extent = HoleToFaceExtent{face};
  if (mode == 2)
    extent = HoleUpToBodyExtent{BodyId("body.limit")};
  REQUIRE(p.add_hole_feature(fixture::hole("hole", ParameterId("radius"), extent)));
  const auto cache = evaluate(p);
  const double depth = mode == 0 ? 8 : 4;
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * depth).margin(1e-6));
  CHECK_FALSE(material_at(cache, {0, 0, 6}));
  CHECK(material_at(cache, {0, 0, 2}) == (mode != 0));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.limit")))
            .volume_mm3 == Catch::Approx(800));
  CHECK(cache.find_body_result(BodyId("body.limit"))->source_feature_id == FeatureId("limit"));
  const auto reopened =
      checked(deserialize_part_document_from_json(checked(serialize_part_document_to_json(p))));
  CHECK(volume(evaluate(reopened)) == Catch::Approx(volume(cache)).margin(1e-6));
}

TEST_CASE("Symmetric Hole uses total depth about its support without becoming ThroughAll",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  auto p = history::native_base();
  dimension(p, "hole.total", 4);
  const auto support = construction_support(p, {0, 0, 4});
  REQUIRE(p.add_hole_feature(checked(
      HoleFeature::create(FeatureId("hole"), "Symmetric", HoleKind::Plain, BodyId("body.base"),
                          support, {{HolePlacementId("center"), Point2{}}}, ParameterId("radius"),
                          HoleSymmetricExtent{ParameterId("hole.total")}))));
  auto cache = evaluate(p);
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi).margin(1e-6));
  CHECK(material_at(cache, {0, 0, 1.9}));
  CHECK_FALSE(material_at(cache, {0, 0, 3}));
  CHECK_FALSE(material_at(cache, {0, 0, 5}));
  CHECK(material_at(cache, {0, 0, 6.1}));
  REQUIRE(p.set_parameter_value(ParameterId("hole.total"), checked(Quantity::length_mm(6))));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * 1.5).margin(1e-6));
}

TEST_CASE("Blind depth remains measured from an offset support rather than the material entry",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  auto p = history::native_base();
  dimension(p, "hole.depth", 5);
  const auto support = construction_support(p, {0, 0, 10});
  REQUIRE(p.add_hole_feature(checked(
      HoleFeature::create(FeatureId("hole"), "Offset blind", HoleKind::Plain, BodyId("body.base"),
                          support, {{HolePlacementId("center"), Point2{}}}, ParameterId("radius"),
                          HoleBlindExtent{ParameterId("hole.depth")}))));
  const auto cache = evaluate(p);
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * 3).margin(1e-6));
  CHECK_FALSE(material_at(cache, {0, 0, 6}));
  CHECK(material_at(cache, {0, 0, 4.9}));
}

TEST_CASE("ToNext follows a curved cavity exit and leaves its opposite wall intact",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  auto p = history::native_base();
  dimension(p, "large.diameter", 2);
  dimension(p, "small.diameter", .5);
  REQUIRE(p.add_hole_feature(fixture::hole("bore", ParameterId("large.diameter"))));
  const auto before = evaluate(p);
  CHECK(material_at(before, {-.98, .2, 4}));
  const auto spans = checked(ClosedProfileAdapter{}.axis_material_spans(
      *before.find_body_shape(BodyId("body.base")), {-20, 0, 4}, {1, 0, 0}));
  REQUIRE(spans.size() == 2);
  CHECK(spans[0].minimum == Catch::Approx(10));
  CHECK(spans[0].maximum == Catch::Approx(19));
  CHECK(spans[1].minimum == Catch::Approx(21));
  CHECK(spans[1].maximum == Catch::Approx(30));
  const auto support = construction_support(p, {-10, 0, 4}, {0, -1, 0}, {0, 0, 1}, {-1, 0, 0});
  REQUIRE(p.add_hole_feature(
      checked(HoleFeature::create(FeatureId("cross"), "Cross", HoleKind::Plain, BodyId("body.base"),
                                  support, {{HolePlacementId("center"), Point2{}}},
                                  ParameterId("small.diameter"), HoleToNextExtent{}))));
  const auto cache = evaluate(p);
  CHECK_FALSE(material_at(cache, {-.98, .2, 4}));
  CHECK_FALSE(material_at(cache, {-5, 0, 4}));
  CHECK(material_at(cache, {5, 0, 4}));
  CHECK(volume(cache) < volume(before));
}

TEST_CASE("UpToBody clips to a curved limit without modifying that Body",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  auto p = history::native_base();
  dimension(p, "limit.diameter", 4);
  dimension(p, "small.diameter", .5);
  auto sketch =
      checked(Sketch::create(SketchId("round.limit"), "Round limit", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("limit.circle"), ParameterId("limit.diameter")))));
  REQUIRE(p.add_sketch(std::move(sketch)));
  REQUIRE(p.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit", SketchId("round.limit"),
                                               ParameterId("depth")))
          .with_body_result_context(history::new_body(p, "limit")))));
  const auto support = construction_support(p, {-10, 0, 4}, {0, -1, 0}, {0, 0, 1}, {-1, 0, 0});
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Limited", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("center"), Point2{}}}, ParameterId("small.diameter"),
      HoleUpToBodyExtent{BodyId("body.limit")}))));
  const auto cache = evaluate(p);
  CHECK_FALSE(material_at(cache, {-1.99, .2, 4}));
  CHECK_FALSE(material_at(cache, {-5, 0, 4}));
  CHECK(material_at(cache, {0, 0, 4}));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.limit")))
            .volume_mm3 == Catch::Approx(std::numbers::pi * 4 * 8).margin(1e-6));
  CHECK(cache.find_body_result(BodyId("body.limit"))->source_feature_id == FeatureId("limit"));
}

TEST_CASE("ToNext rejects a tool that wraps around a narrow cavity into later material",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  auto p = history::native_base();
  dimension(p, "small.diameter", .5);
  dimension(p, "large.diameter", 2);
  REQUIRE(p.add_hole_feature(fixture::hole("bore", ParameterId("small.diameter"))));
  const auto support = construction_support(p, {-10, 0, 4}, {0, -1, 0}, {0, 0, 1}, {-1, 0, 0});
  const auto original = p;
  auto cache = evaluate(p);
  REQUIRE(p.add_hole_feature(
      checked(HoleFeature::create(FeatureId("cross"), "Cross", HoleKind::Plain, BodyId("body.base"),
                                  support, {{HolePlacementId("center"), Point2{}}},
                                  ParameterId("large.diameter"), HoleToNextExtent{}))));
  const auto failed = GeometryRecomputeExecutor{}.execute_document(p, cache);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().message().find("wraps around") != std::string::npos);
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(material_at(cache, {-5, 0, 4}));
  CHECK(material_at(cache, {5, 0, 4}));
}

TEST_CASE("Hole face limits reject a trimmed-away opening and a nonnormal face atomically",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  const bool trimmed = GENERATE(false, true);
  auto p = history::native_base();
  dimension(p, "limit.depth", 4);
  dimension(p, "large.diameter", 2);
  REQUIRE(p.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit", SketchId("profile"),
                                               ParameterId("limit.depth")))
          .with_body_result_context(history::new_body(p, "limit")))));
  if (trimmed) {
    const auto datum = checked(PlaneReference::create_datum_plane(
        PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("datum.xy")));
    REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
        FeatureId("limit.opening"), "Opening", HoleKind::Plain, BodyId("body.limit"), datum,
        {{HolePlacementId("center"), Point2{}}}, ParameterId("large.diameter"),
        HoleThroughAllExtent{}, HoleDirection::SupportNormal))));
  }
  const auto original = p;
  auto cache = evaluate(p);
  const auto limit = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("limit"),
                                            trimmed ? SemanticFace::Top : SemanticFace::Right))));
  REQUIRE(
      p.add_hole_feature(fixture::hole("hole", ParameterId("radius"), HoleToFaceExtent{limit})));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(volume(cache) == Catch::Approx(1600));
}

TEST_CASE("UpToBody rejects a boundary that the tool can bypass without publishing a cut",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  auto p = history::native_base();
  dimension(p, "limit.diameter", .25);
  dimension(p, "limit.depth", 4);
  auto sketch = checked(Sketch::create(SketchId("round.limit"), "Limit", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("limit.circle"), ParameterId("limit.diameter")))));
  REQUIRE(p.add_sketch(std::move(sketch)));
  REQUIRE(p.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit", SketchId("round.limit"),
                                               ParameterId("limit.depth")))
          .with_body_result_context(history::new_body(p, "limit")))));
  const auto original = p;
  auto cache = evaluate(p);
  REQUIRE(p.add_hole_feature(
      fixture::hole("hole", ParameterId("radius"), HoleUpToBodyExtent{BodyId("body.limit")})));
  const auto failed = GeometryRecomputeExecutor{}.execute_document(p, cache);
  REQUIRE_FALSE(failed);
  CHECK(failed.error().message().find("wraps around") != std::string::npos);
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(volume(cache) == Catch::Approx(1600));
}

TEST_CASE("Material axis intervals exclude tangencies and reject nonfinite inputs",
          "[geometry][parity-b153][hole-bounded-cutting]") {
  const auto p = history::native_base();
  const auto cache = evaluate(p);
  const auto& body = *cache.find_body_shape(BodyId("body.base"));
  const ClosedProfileAdapter adapter;
  CHECK(checked(adapter.axis_material_spans(body, {10, 0, -2}, {0, 0, 1})).empty());
  CHECK(checked(adapter.axis_material_spans(body, {11, 0, -2}, {0, 0, 1})).empty());
  const auto forward = checked(adapter.axis_material_spans(body, {0, 0, 10}, {0, 0, -1}));
  REQUIRE(forward.size() == 1);
  CHECK(forward.front().minimum == Catch::Approx(2));
  CHECK(forward.front().maximum == Catch::Approx(10));
  CHECK_FALSE(adapter.axis_material_spans(body, {}, {}));
  CHECK_FALSE(adapter.axis_material_spans(body, {std::numeric_limits<double>::quiet_NaN(), 0, 0},
                                          {0, 0, 1}));
}
