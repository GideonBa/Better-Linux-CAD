#include "../support/hole_feature_fixture.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>
#include <gp_Cylinder.hxx>

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

namespace {
GeometryAffineTransform pose_body(PartDocument& p, int mode) {
  if (mode == 0) {
    REQUIRE(p.add_body_transform(checked(BodyTransform::create_translate(
        BodyTransformId("pose"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
        std::nullopt, {12, -7, 5}, false, false))));
    return GeometryAffineTransform::translation({12, -7, 5});
  }
  if (mode == 1) {
    REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
        BodyTransformId("pose"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
        std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0})), 90, false,
        false))));
    return GeometryAffineTransform::rotation({}, {1, 0, 0}, 90);
  }
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_uniform_scale(
      BodyTransformId("pose"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, 2, {}, false, false))));
  return GeometryAffineTransform::uniform_scale({}, 2);
}
} // namespace

TEST_CASE("Semantic Hole frames follow every rectangular face through Body placement and scale",
          "[geometry][parity-b153][hole-current-support]") {
  const auto face = GENERATE(SemanticFace::Top, SemanticFace::Bottom, SemanticFace::Left,
                             SemanticFace::Right, SemanticFace::Front, SemanticFace::Back);
  const int mode = GENERATE(0, 1, 2);
  const bool offset = GENERATE(false, true);
  auto p = history::native_base();
  const auto reference = checked(SemanticFaceReference::create(FeatureId("base"), face));
  const auto raw = checked(WorkplaneResolver{}.resolve_generated_face(p, reference));
  const auto pose = pose_body(p, mode);
  const auto support = checked(
      PlaneReference::create_semantic_face(PartFeatureInputRole::HoleSupportPlane, reference));
  const Point2 local = offset ? Point2{1, 1} : Point2{};
  REQUIRE(p.add_hole_feature(checked(
      HoleFeature::create(FeatureId("hole"), "Posed Hole", HoleKind::Plain, BodyId("body.base"),
                          support, {{HolePlacementId("center"), local}}, ParameterId("radius")))));
  const auto cache = evaluate(p);
  const double scale = mode == 2 ? 2. : 1.;
  const double depth = scale * (face == SemanticFace::Top || face == SemanticFace::Bottom   ? 8.
                                : face == SemanticFace::Left || face == SemanticFace::Right ? 20.
                                                                                            : 10.);
  CHECK(volume(cache) ==
        Catch::Approx(1600 * scale * scale * scale - std::numbers::pi * .25 * depth).margin(1e-6));
  const auto center = pose.transform_point(raw.origin);
  const auto x = normalized(pose.transform_vector(raw.x_axis));
  const auto y = normalized(pose.transform_vector(raw.y_axis));
  const auto normal = normalized(pose.transform_vector(raw.normal));
  const Point3 probe{center.x + x.x * local.x + y.x * local.y - normal.x * depth / 2,
                     center.y + x.y * local.x + y.y * local.y - normal.y * depth / 2,
                     center.z + x.z * local.x + y.z * local.y - normal.z * depth / 2};
  CHECK_FALSE(material_at(cache, probe));
  CHECK(material_at(cache, {probe.x + x.x, probe.y + x.y, probe.z + x.z}));
  REQUIRE(cache.validate_complete_geometry_result(p));
}

TEST_CASE("Repeated semantic-face Holes retain their frame on the current trimmed face",
          "[geometry][parity-b153][hole-current-support]") {
  auto p = history::native_base();
  const auto support = fixture::hole().support();
  for (const auto& [id, x] : {std::pair{"left", -3.}, {"right", 3.}})
    REQUIRE(p.add_hole_feature(checked(
        HoleFeature::create(FeatureId(id), id, HoleKind::Plain, BodyId("body.base"), support,
                            {{HolePlacementId("center"), Point2{x, 0}}}, ParameterId("radius")))));
  const auto cache = evaluate(p);
  CHECK(volume(cache) == Catch::Approx(1600 - 2 * std::numbers::pi * .25 * 8).margin(1e-6));
  CHECK_FALSE(material_at(cache, {-3, 0, 4}));
  CHECK_FALSE(material_at(cache, {3, 0, 4}));
  const auto choices =
      checked(ResultFaceSupportGeometry::selectable_faces(p, cache, BodyId("body.base")));
  for (const auto& choice : choices)
    for (const auto& origin : choice.reference.origins())
      CHECK_FALSE(origin.role.starts_with("hole-support/"));
}

TEST_CASE("Captured result-face Hole supports follow later Body placement",
          "[geometry][parity-b153][hole-current-support]") {
  auto p = history::native_base();
  const auto original = evaluate(p);
  const auto targets =
      checked(ResultFaceSupportGeometry::selectable_faces(p, original, BodyId("body.base")));
  const auto top = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    return std::any_of(target.reference.origins().begin(), target.reference.origins().end(),
                       [](const auto& origin) { return origin.role == "extrude/upper-cap"; });
  });
  REQUIRE(top != targets.end());
  REQUIRE(p.add_result_face_workplane(
      checked(ResultFaceWorkplane::create(DatumPlaneId("captured"), "Captured", top->reference))));
  const auto pose = pose_body(p, 1);
  const auto support = checked(PlaneReference::create_datum_plane(
      PartFeatureInputRole::HoleSupportPlane, DatumPlaneId("captured")));
  const auto added = p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Captured Hole", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("center"), Point2{}}}, ParameterId("radius"))));
  INFO((added ? "ok" : added.error().message()));
  REQUIRE(added);
  const auto cache = evaluate(p);
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * 8).margin(1e-6));
  CHECK_FALSE(material_at(cache, pose.transform_point({0, 0, 4})));
  const auto reopened =
      checked(deserialize_part_document_from_json(checked(serialize_part_document_to_json(p))));
  CHECK(volume(evaluate(reopened)) == Catch::Approx(volume(cache)).margin(1e-6));
}

TEST_CASE("ToFace retains an oblique planar bottom and its complete elliptical section",
          "[geometry][parity-b153][hole-current-support]") {
  const bool valid = GENERATE(false, true);
  auto p = history::native_base();
  dimension(p, "limit.depth", 4);
  REQUIRE(p.add_feature(checked(
      checked(Feature::create_additive_extrude(FeatureId("limit"), "Limit", SketchId("profile"),
                                               ParameterId("limit.depth")))
          .with_body_result_context(history::new_body(p, "limit")))));
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("limit.pose"), BodyId("body.limit"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0})), 30, false,
      false))));
  const auto face = checked(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      checked(SemanticFaceReference::create(FeatureId("limit"), SemanticFace::Top))));
  const auto original = p;
  auto cache = evaluate(p);
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Oblique Hole", HoleKind::Plain, BodyId("body.base"),
      fixture::hole().support(), {{HolePlacementId("center"), Point2{0, valid ? 0. : 2.3}}},
      ParameterId("radius"), HoleToFaceExtent{face}))));
  const auto evaluated = GeometryRecomputeExecutor{}.execute_document(p, cache);
  if (!valid) {
    REQUIRE_FALSE(evaluated);
    CHECK(evaluated.error().message().find("selected trimmed limit face") != std::string::npos);
    REQUIRE(cache.validate_complete_geometry_result(original));
    CHECK(volume(cache) == Catch::Approx(1600));
    return;
  }
  REQUIRE(evaluated);
  const double depth = 8 - 4 / std::cos(std::numbers::pi / 6);
  CHECK(volume(cache) == Catch::Approx(1600 - std::numbers::pi * .25 * depth).margin(1e-6));
  CHECK(material_at(cache, {0, .4, 4.75}));
  CHECK_FALSE(material_at(cache, {0, -.4, 4.5}));
  CHECK_FALSE(material_at(cache, {0, 0, 6}));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("body.limit")))
            .volume_mm3 == Catch::Approx(800));
}

TEST_CASE("Trimmed and split semantic Hole supports reject atomically",
          "[geometry][parity-b153][hole-current-support]") {
  const bool split = GENERATE(false, true);
  auto p = history::native_base();
  dimension(p, "opening.diameter", split ? 12 : 2);
  const auto construction = construction_support(p);
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("opening"), "Opening", HoleKind::Plain, BodyId("body.base"), construction,
      {{HolePlacementId("center"), Point2{}}}, ParameterId("opening.diameter")))));
  const auto original = p;
  auto cache = evaluate(p);
  REQUIRE(p.add_hole_feature(fixture::hole()));
  const auto evaluated = GeometryRecomputeExecutor{}.execute_document(p, cache);
  REQUIRE_FALSE(evaluated);
  CHECK(evaluated.error().message().find(split ? "multiple descendants" : "Hole opening") !=
        std::string::npos);
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(cache.find_feature_shape(FeatureId("hole")) == nullptr);
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

TEST_CASE("Hole placement roots remain distinct through reorder dimensions and a downstream pose",
          "[geometry][parity-b153][hole-result-reference]") {
  auto p = history::native_base();
  dimension(p, "diameter", 2);
  dimension(p, "blind", 3);
  const auto original =
      fixture::hole("hole", ParameterId("diameter"), HoleBlindExtent{ParameterId("blind")});
  const auto make = [&](bool reverse) {
    std::vector<HolePlacement> placements{{HolePlacementId("a:/one"), Point2{-3, 0}},
                                          {HolePlacementId("a:/two"), Point2{3, 0}}};
    if (reverse)
      std::reverse(placements.begin(), placements.end());
    return checked(HoleFeature::create(original.id(), original.name(), original.kind(),
                                       original.target_body(), original.support(),
                                       std::move(placements), original.diameter(),
                                       original.extent(), original.direction(), original.tip()));
  };
  REQUIRE(p.add_hole_feature(make(false)));
  auto c = evaluate(p);
  const auto targets = checked(
      FinishingSelectionBuilder::build(p, c, BodyId("body.base"), PartFeatureInputRole::DraftFace));
  std::vector<ResultFaceReference> walls;
  for (const auto& target : targets) {
    const auto& input = std::get<FaceReference>(target.reference);
    const auto* ref = std::get_if<ResultFaceReference>(&input.source());
    if (ref && ref->origins().front().role.ends_with("/wall"))
      walls.push_back(*ref);
  }
  REQUIRE(walls.size() == 2);
  CHECK(walls[0].identity() != walls[1].identity());
  const auto planes =
      checked(ResultFaceSupportGeometry::selectable_faces(p, c, BodyId("body.base")));
  CHECK(std::count_if(planes.begin(), planes.end(), [](const auto& target) {
          return target.reference.origins().front().role.ends_with("/bottom");
        }) == 2);
  REQUIRE(p.update_hole_feature(make(true)));
  REQUIRE(p.set_parameter_value(ParameterId("diameter"), checked(Quantity::length_mm(3))));
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("pose"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0})), 90, false,
      false))));
  c = evaluate(p);
  for (const auto& ref : walls) {
    const auto face = checked(ResultFaceSupportGeometry::resolve_face(p, c, ref));
    BRepAdaptor_Surface surface(TopoDS::Face(*OcctShapeView::get(face)));
    REQUIRE(surface.GetType() == GeomAbs_Cylinder);
    CHECK(surface.Cylinder().Radius() == Catch::Approx(1.5));
    CHECK(std::abs(surface.Cylinder().Axis().Direction().Y()) == Catch::Approx(1));
  }
  CHECK(volume(c) == Catch::Approx(1600 - 2 * std::numbers::pi * 2.25 * 3).margin(1e-6));
  const auto encoded = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(encoded));
  auto reopened_cache = evaluate(reopened);
  for (const auto& ref : walls)
    REQUIRE(ResultFaceSupportGeometry::resolve_face(reopened, reopened_cache, ref));
  REQUIRE(p.apply_history_intent({{"base", "hole", "pose"}, {"hole"}}));
  c = evaluate(p);
  CHECK_FALSE(ResultFaceSupportGeometry::resolve_face(p, c, walls.front()));
}

TEST_CASE("Blind Hole bottom supports a dependent Sketch and additive feature with editable depth",
          "[geometry][parity-b153][hole-result-reference]") {
  auto p = history::native_base();
  dimension(p, "diameter", 4);
  dimension(p, "blind", 3);
  dimension(p, "post.diameter", 1);
  dimension(p, "post.height", 1);
  REQUIRE(p.add_hole_feature(
      fixture::hole("hole", ParameterId("diameter"), HoleBlindExtent{ParameterId("blind")})));
  auto c = evaluate(p);
  const auto faces =
      checked(ResultFaceSupportGeometry::selectable_faces(p, c, BodyId("body.base")));
  const auto bottom = std::find_if(faces.begin(), faces.end(), [](const auto& target) {
    return target.reference.origins().front().role.ends_with("/bottom");
  });
  REQUIRE(bottom != faces.end());
  REQUIRE(p.add_result_face_workplane(checked(
      ResultFaceWorkplane::create(DatumPlaneId("bottom"), "Hole bottom", bottom->reference))));
  auto sketch = checked(Sketch::create(SketchId("post"), "Post", DatumPlaneId("bottom")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("post"), ParameterId("post.diameter")))));
  REQUIRE(p.add_sketch(sketch));
  auto post = checked(Feature::create_additive_extrude(
      FeatureId("post.feature"), "Post", SketchId("post"), ParameterId("post.height")));
  REQUIRE(
      p.add_feature(checked(post.with_body_result_context(checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::Join, BodyId("body.base"), std::nullopt))))));
  for (double depth : {3., 4.}) {
    REQUIRE(p.set_parameter_value(ParameterId("blind"), checked(Quantity::length_mm(depth))));
    c = evaluate(p);
    CHECK(volume(c) ==
          Catch::Approx(1600 - std::numbers::pi * 4 * depth + std::numbers::pi * .25).margin(1e-6));
    CHECK(material_at(c, {0, 0, 8 - depth + .5}));
    CHECK_FALSE(material_at(c, {1, 0, 8 - depth + .5}));
    const auto encoded = checked(serialize_part_document_to_json(p));
    CHECK(volume(evaluate(checked(deserialize_part_document_from_json(encoded)))) ==
          Catch::Approx(volume(c)).margin(1e-6));
  }
  auto prefix = checked(ShapeCache::create(ShapeCacheId("prefix")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, 1, prefix));
  CHECK_FALSE(ResultFaceSupportGeometry::resolve_face(p, prefix, bottom->reference));
}

TEST_CASE("A generated bore wall drives a downstream Draft and survives portable intent",
          "[geometry][parity-b153][hole-result-reference]") {
  auto p = history::native_base();
  dimension(p, "diameter", 4);
  REQUIRE(p.add_hole_feature(fixture::hole("hole", ParameterId("diameter"))));
  auto c = evaluate(p);
  const auto targets = checked(
      FinishingSelectionBuilder::build(p, c, BodyId("body.base"), PartFeatureInputRole::DraftFace));
  const auto wall = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    const auto* ref =
        std::get_if<ResultFaceReference>(&std::get<FaceReference>(target.reference).source());
    return ref && ref->origins().front().role.ends_with("/wall");
  });
  REQUIRE(wall != targets.end());
  REQUIRE(p.add_draft_feature(checked(DraftFeature::create(
      FeatureId("draft"), "Draft bore", BodyId("body.base"),
      {std::get<FaceReference>(wall->reference)},
      history::axis(PartFeatureInputRole::DraftPullDirection),
      history::plane(PartFeatureInputRole::DraftNeutralPlane), ParameterId("angle")))));
  c = evaluate(p);
  const double slope = std::tan(2. * std::numbers::pi / 180.);
  const double r = 2 - 8 * slope;
  CHECK(volume(c) ==
        Catch::Approx(1600 - std::numbers::pi * 8 / 3 * (4 + 2 * r + r * r)).margin(1e-5));
  const auto encoded = checked(serialize_part_document_to_json(p));
  CHECK(volume(evaluate(checked(deserialize_part_document_from_json(encoded)))) ==
        Catch::Approx(volume(c)).margin(1e-6));
  const auto& reference =
      std::get<ResultFaceReference>(std::get<FaceReference>(wall->reference).source());
  const auto face = checked(ResultFaceSupportGeometry::resolve_face(p, c, reference));
  CHECK(BRepAdaptor_Surface(TopoDS::Face(*OcctShapeView::get(face))).GetType() == GeomAbs_Cone);
}

TEST_CASE("Consumed and split bore walls reject downstream references without publishing a partial "
          "result",
          "[geometry][parity-b153][hole-result-reference]") {
  const bool split = GENERATE(false, true);
  auto p = history::native_base();
  dimension(p, "diameter", 2);
  dimension(p, "second.diameter", 4);
  REQUIRE(p.add_hole_feature(fixture::hole("hole", ParameterId("diameter"))));
  auto c = evaluate(p);
  const auto reference = checked(ResultFaceReference::create(
      FeatureId("hole"), BodyId("body.base"), {{FeatureId("hole"), "hole/6:center/wall"}}));
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, reference));
  const auto support =
      split ? checked(PlaneReference::create_semantic_face(
                  PartFeatureInputRole::HoleSupportPlane,
                  checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Left))))
            : construction_support(p);
  REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
      FeatureId("second"), "Second", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("center"), Point2{}}}, ParameterId("second.diameter")))));
  c = evaluate(p);
  const auto unavailable = ResultFaceSupportGeometry::resolve_face(p, c, reference);
  REQUIRE_FALSE(unavailable);
  CHECK(unavailable.error().message().find(split ? "multiple descendants" : "deleted") !=
        std::string::npos);
  const auto prior = p;
  const auto prior_shape = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  const double prior_volume = volume(c);
  REQUIRE(p.add_draft_feature(checked(DraftFeature::create(
      FeatureId("draft"), "Draft lost bore", BodyId("body.base"),
      {checked(FaceReference::create_result(PartFeatureInputRole::DraftFace, reference))},
      history::axis(PartFeatureInputRole::DraftPullDirection),
      history::plane(PartFeatureInputRole::DraftNeutralPlane), ParameterId("angle")))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  REQUIRE(c.validate_complete_geometry_result(prior));
  CHECK(volume(c) == prior_volume);
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(prior_shape));
}

TEST_CASE("A captured Hole bottom limits another Body and owns its new planar termination",
          "[geometry][parity-b153][hole-result-reference]") {
  auto p = history::native_base();
  auto second = checked(Feature::create_additive_extrude(
      FeatureId("second"), "Second", SketchId("profile"), ParameterId("depth")));
  REQUIRE(p.add_feature(checked(second.with_body_result_context(history::new_body(p, "second")))));
  dimension(p, "diameter", 4);
  dimension(p, "blind", 3);
  REQUIRE(p.add_hole_feature(
      fixture::hole("hole", ParameterId("diameter"), HoleBlindExtent{ParameterId("blind")})));
  auto c = evaluate(p);
  const auto bottom = checked(ResultFaceReference::create(
      FeatureId("hole"), BodyId("body.base"), {{FeatureId("hole"), "hole/6:center/bottom"}}));
  const auto limit =
      checked(FaceReference::create_result(PartFeatureInputRole::HoleLimitFace, bottom));
  REQUIRE(p.add_hole_feature(checked(
      HoleFeature::create(FeatureId("limited"), "Limited", HoleKind::Plain, BodyId("body.second"),
                          construction_support(p), {{HolePlacementId("center"), Point2{}}},
                          ParameterId("radius"), HoleToFaceExtent{limit}))));
  c = evaluate(p);
  CHECK(volume(c) == Catch::Approx(1600 - std::numbers::pi * 4 * 3).margin(1e-6));
  CHECK(
      RectangleExtrusionAdapter{}.summarize(*c.find_body_shape(BodyId("body.second"))).volume_mm3 ==
      Catch::Approx(1600 - std::numbers::pi * .25 * 3).margin(1e-6));
  const auto end =
      checked(ResultFaceReference::create(FeatureId("limited"), BodyId("body.second"),
                                          {{FeatureId("limited"), "hole/6:center/bottom"}}));
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, end));
  const auto faces =
      checked(ResultFaceSupportGeometry::selectable_faces(p, c, BodyId("body.second")));
  CHECK(std::any_of(faces.begin(), faces.end(),
                    [&](const auto& face) { return face.reference == end; }));
  const auto encoded = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(encoded));
  auto restored = evaluate(reopened);
  REQUIRE(ResultFaceSupportGeometry::resolve_face(reopened, restored, end));
}

TEST_CASE(
    "Hole authoring support queries share posed execution frames and require exact cache scope",
    "[geometry][parity-b157][hole-support-query]") {
  const auto face = GENERATE(SemanticFace::Top, SemanticFace::Bottom, SemanticFace::Left,
                             SemanticFace::Right, SemanticFace::Front, SemanticFace::Back);
  const int mode = GENERATE(0, 1, 2);
  auto part = history::native_base();
  const auto reference = checked(SemanticFaceReference::create(FeatureId("base"), face));
  const auto raw = checked(WorkplaneResolver{}.resolve_generated_face(part, reference));
  const auto pose = pose_body(part, mode);
  const auto support = checked(
      PlaneReference::create_semantic_face(PartFeatureInputRole::HoleSupportPlane, reference));
  auto current = evaluate(part);
  const GeometryRecomputeExecutor executor;
  const auto inspected =
      checked(executor.resolve_hole_support(part, support, current, FeatureId("")));
  const auto expected_origin = pose.transform_point(raw.origin);
  CHECK(distance(inspected.origin, expected_origin) < 1e-7);
  CHECK(dot(inspected.normal, normalized(pose.transform_vector(raw.normal))) == Catch::Approx(1));
  CHECK(dot(inspected.x_axis, normalized(pose.transform_vector(raw.x_axis))) == Catch::Approx(1));
  REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
      FeatureId("query.hole"), "Query Hole", HoleKind::Plain, BodyId("body.base"), support,
      {{HolePlacementId("p"), Point2{}}}, ParameterId("radius")))));
  CHECK_FALSE(executor.resolve_hole_support(part, support, current, FeatureId("query.hole")));
  const auto history_order = checked(part.executable_history());
  const auto hole = std::find_if(history_order.begin(), history_order.end(),
                                 [](const auto& item) { return item.node_id == "query.hole"; });
  REQUIRE(hole != history_order.end());
  const auto prefix = std::size_t(hole - history_order.begin());
  auto inputs = checked(ShapeCache::create(ShapeCacheId("query.inputs")));
  REQUIRE(executor.execute_history_prefix(part, prefix, inputs));
  CHECK_FALSE(executor.resolve_hole_support(part, support, inputs, FeatureId("query.hole")));
  const auto scoped = checked(
      executor.resolve_hole_support(part, support, inputs, FeatureId("query.hole"), prefix));
  CHECK(distance(scoped.origin, expected_origin) < 1e-7);
  CHECK_FALSE(
      executor.resolve_hole_support(part, support, inputs, FeatureId("query.hole"), prefix + 1));
  REQUIRE(part.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(9))));
  CHECK_FALSE(
      executor.resolve_hole_support(part, support, inputs, FeatureId("query.hole"), prefix));
}
