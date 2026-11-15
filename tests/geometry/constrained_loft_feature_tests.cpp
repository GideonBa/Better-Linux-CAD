#include "../../src/geometry/surface_join_verification.hpp"
#include "../support/constrained_loft_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <TopExp_Explorer.hxx>
#include <catch2/catch_approx.hpp>
#include <nlohmann/json.hpp>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::constrained_loft;
namespace lf = blcad::test::loft_refinement;
TEST_CASE("Verified constrained Loft assembles unequal rolled Airfoil sections and two rails",
          "[geometry][constrained-loft-feature][loft-constraints][parity-b169]") {
  for (const bool solid : {false, true}) {
    auto part = f::airfoil(solid);
    auto cache = f::cache();
    const auto saved = lf::checked(serialize_part_document_to_json(part));
    const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
    INFO((result ? std::string("ok") : result.error().message()));
    REQUIRE(result);
    const auto* shape = cache.find_body_shape(BodyId("main"));
    REQUIRE(shape);
    const auto* native = OcctShapeView::get(*shape);
    REQUIRE(native);
    REQUIRE(BRepCheck_Analyzer(*native, true).IsValid());
    std::size_t faces = 0;
    for (TopExp_Explorer e(*native, TopAbs_FACE); e.More(); e.Next())
      ++faces;
    CHECK(faces == (solid ? 4 : 2));
    for (std::size_t station = 0; station < 3; ++station)
      for (const bool upper : {false, true})
        for (int i = 0; i <= 16; ++i) {
          const auto p = lf::model(station, lf::at(lf::poles(station, upper), i / 16.0));
          CHECK(blcad::test::distance_to_faces(*shape, p) <= 1e-5);
        }
    auto reopened = lf::checked(deserialize_part_document_from_json(saved));
    auto fresh = f::cache();
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(reopened, fresh));
    CHECK(lf::checked(serialize_part_document_to_json(reopened)) == saved);
    if (solid) {
      CHECK(f::volume(cache) > 1000);
      CHECK(f::volume(fresh) == Catch::Approx(f::volume(cache)).margin(1e-6));
    }
  }
}
TEST_CASE("Impossible constrained Loft edits preserve exact published geometry and can be repaired",
          "[geometry][constrained-loft-feature][loft-constraints][parity-b169]") {
  auto part = f::airfoil();
  auto cache = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto volume = f::volume(cache);
  const auto before = lf::checked(serialize_part_document_to_json(part));
  const auto original = *part.find_loft_feature(FeatureId("main"));
  for (int mutation = 0; mutation < 4; ++mutation) {
    auto intent = *original.constraints();
    if (mutation == 0)
      intent.patch_boundaries[0].condition.reverse_normal = false;
    if (mutation == 1)
      intent.patch_boundaries[0].condition.tangent_scale = 2;
    if (mutation == 2)
      intent.guides[0].anchors[1].boundary_fraction = .1;
    if (mutation == 3)
      intent.guides[0].anchors[1].guide_fraction = .2;
    REQUIRE(part.update_loft_feature(lf::checked(original.with_constraints(intent))));
    const auto failed = GeometryRecomputeExecutor{}.execute_document(part, cache);
    REQUIRE_FALSE(failed);
    CHECK(f::volume(cache) == volume);
    REQUIRE(part.update_loft_feature(original));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    CHECK(f::volume(cache) == Catch::Approx(volume).margin(1e-6));
    CHECK(lf::checked(serialize_part_document_to_json(part)) == before);
  }
}

TEST_CASE(
    "Constrained Loft retains logical supports through reversed sections and a middle-section edit",
    "[geometry][constrained-loft-feature][constrained-loft-edits][parity-b169]") {
  auto part = f::airfoil();
  auto cache = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto original = f::volume(cache);
  const auto feature = *part.find_loft_feature(FeatureId("main"));
  std::vector<ProfileSectionReference> reversed;
  for (const auto& section : feature.sections())
    reversed.push_back(lf::checked(ProfileSectionReference::create_closed_region(
        std::get<ProfileRegionReference>(section.source()), section.alignment_reference(), true,
        section.rotation_offset())));
  auto flipped = lf::checked(LoftFeature::create_loft(
      feature.id(), feature.name(), reversed, feature.body_result_context(), feature.path_curve(),
      feature.guide_curves(), feature.continuity()));
  REQUIRE(part.update_loft_feature(lf::checked(flipped.with_constraints(feature.constraints()))));
  const auto reversed_result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((reversed_result ? std::string("ok") : reversed_result.error().message()));
  REQUIRE(reversed_result);
  CHECK(f::volume(cache) == Catch::Approx(original).margin(1e-6));
  REQUIRE(part.update_loft_feature(feature));
  const auto* old = part.find_sketch(SketchId("section.1"));
  REQUIRE(old);
  auto sketch = lf::checked(Sketch::create(old->id(), old->name(), old->workplane()));
  auto poles = lf::poles(1, true);
  poles[1].y += .7;
  const auto count = lf::counts[1][0];
  for (const auto& spline : old->spline_segments()) {
    const auto name = spline.id().value();
    if (!name.starts_with("upper.")) {
      REQUIRE(sketch.add_entity(spline));
      continue;
    }
    const auto i = std::stoul(name.substr(6));
    const double a = double(i) / count, b = double(i + 1) / count, d = (b - a) / 3;
    const auto first = lf::at(poles, a), last = lf::at(poles, b), da = lf::derivative(poles, a),
               db = lf::derivative(poles, b);
    REQUIRE(sketch.add_entity(lf::checked(SplineSegment::create_cubic_bezier(
        spline.id(), first, {first.x + d * da.x, first.y + d * da.y},
        {last.x - d * db.x, last.y - d * db.y}, last))));
  }
  REQUIRE(sketch.add_profile(*old->find_arc_closed_profile(ProfileId("outline"))));
  REQUIRE(part.update_sketch(sketch));
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result ? std::string("ok") : result.error().message()));
  REQUIRE(result);
  CHECK(f::volume(cache) > original + 1e-4);
  for (int i = 0; i <= 16; ++i) {
    const auto p = lf::model(1, lf::at(poles, i / 16.0));
    CHECK(blcad::test::distance_to_faces(*cache.find_body_shape(BodyId("main")), p) <= 1e-5);
  }
  const auto bytes = lf::checked(serialize_part_document_to_json(part));
  auto restored = lf::checked(deserialize_part_document_from_json(bytes));
  auto fresh = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored, fresh));
  CHECK(f::volume(fresh) == Catch::Approx(f::volume(cache)).margin(1e-6));
  CHECK(lf::checked(serialize_part_document_to_json(restored)) == bytes);
}

TEST_CASE(
    "A nonanchor spatial guide edit keeps G2 germs and updates true-length binding explicitly",
    "[geometry][constrained-loft-feature][constrained-loft-edits][parity-b169]") {
  auto part = f::airfoil();
  auto cache = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto original = f::volume(cache);
  auto spatial = *part.find_sketch_3d(Sketch3DId("main.left.sketch3d"));
  for (const auto& [id, delta] : {std::pair{"p3", .5}, std::pair{"p4", -.125}}) {
    const auto* point = spatial.find_point(SketchEntityId(id));
    REQUIRE(point);
    REQUIRE(spatial.replace_point(lf::checked(
        SketchPoint3D::create(point->id(),
                              blcad::test::spatial_fixture::coordinate(
                                  point->x().explicit_coordinate()->millimeters() + delta),
                              point->y(), point->z()))));
  }
  REQUIRE(part.update_sketch_3d(spatial));
  TColgp_Array1OfPnt poles(1, 9);
  for (int i = 0; i < 9; ++i) {
    const auto* p = spatial.find_point(SketchEntityId("p" + std::to_string(i)));
    REQUIRE(p);
    poles(i + 1) = gp_Pnt(p->x().explicit_coordinate()->millimeters(),
                          p->y().explicit_coordinate()->millimeters(),
                          p->z().explicit_coordinate()->millimeters());
  }
  TColStd_Array1OfReal knots(1, 7);
  TColStd_Array1OfInteger mult(1, 7);
  for (int i = 1; i <= 7; ++i) {
    knots(i) = i - 1;
    mult(i) = i == 1 || i == 7 ? 4 : 1;
  }
  const Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(poles, knots, mult, 3);
  GeomAdaptor_Curve adaptor(curve);
  const auto feature = *part.find_loft_feature(FeatureId("main"));
  auto intent = *feature.constraints();
  intent.guides[0].anchors[1].guide_fraction = GCPnts_AbscissaPoint::Length(adaptor, 0, 3, 1e-11) /
                                               GCPnts_AbscissaPoint::Length(adaptor, 0, 6, 1e-11);
  REQUIRE(part.update_loft_feature(lf::checked(feature.with_constraints(intent))));
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result ? std::string("ok") : result.error().message()));
  REQUIRE(result);
  CHECK(std::abs(f::volume(cache) - original) > 1e-4);
  for (int i = 1; i < 12; ++i) {
    const auto p = curve->Value(i / 2.0);
    CHECK(blcad::test::distance_to_faces(*cache.find_body_shape(BodyId("main")),
                                         {p.X(), p.Y(), p.Z()}) <= 1e-5);
  }
  const auto bytes = lf::checked(serialize_part_document_to_json(part));
  auto restored = lf::checked(deserialize_part_document_from_json(bytes));
  auto fresh = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored, fresh));
  CHECK(f::volume(fresh) == Catch::Approx(f::volume(cache)).margin(1e-6));
  CHECK(lf::checked(serialize_part_document_to_json(restored)) == bytes);
}

TEST_CASE("A single open G2 LoftSurface patch joins finite planar supports",
          "[geometry][constrained-loft-feature][constrained-loft-open][parity-b169]") {
  namespace sf = blcad::test::spatial_fixture;
  auto part = sf::part();
  const double front = -part.find_parameter(ParameterId("height"))->value().millimeters() / 2;
  (void)lf::checked(part.add_parameter(lf::checked(Parameter::create_length(
      ParameterId("tolerance"), "Tolerance", lf::checked(Quantity::length_mm(1e-5))))));
  (void)lf::checked(part.add_construction_plane(lf::checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("tip.plane"), "Tip", {0, 0, 45}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
  auto tip = lf::checked(Sketch::create(SketchId("tip.sketch"), "Tip", DatumPlaneId("tip.plane")));
  REQUIRE(tip.add_profile(lf::checked(RectangleProfile::create(
      ProfileId("tip.profile"), ParameterId("width"), ParameterId("height")))));
  (void)lf::checked(part.add_sketch(tip));
  (void)lf::checked(
      part.add_body(lf::checked(Body::create(BodyId("tip.body"), "Tip", BodyKind::Solid))));
  auto extrude = lf::checked(Feature::create_additive_extrude(FeatureId("tip.feature"), "Tip",
                                                              tip.id(), ParameterId("depth")));
  (void)lf::checked(
      part.add_feature(lf::checked(extrude.with_body_result_context(f::context("tip.body")))));
  std::vector<ProfileSectionReference> sections;
  for (int i = 0; i < 3; ++i) {
    const std::string id = "section.path." + std::to_string(i);
    f::add_rail(part, id, {-2, front + (i == 1 ? 1 : 0), 5.0 + 20 * i}, {},
                {2, front + (i == 1 ? 1 : 0), 5.0 + 20 * i}, true);
    sections.push_back(lf::checked(ProfileSectionReference::create_open_path(PathCurveId(id))));
  }
  LoftConstraintIntent intent{ParameterId("tolerance"), 1e-5, 1e-5, {}, {}, {}};
  for (const bool right : {false, true}) {
    const std::string id = right ? "right" : "left";
    const double x = right ? 2 : -2;
    f::add_rail(part, id, {x, front, 5}, {x, front + 1, 25}, {x, front, 45});
    intent.guides.push_back({PathCurveId(id),
                             false,
                             {{sections[0].identity(), {}, right ? 1.0 : 0.0, 0},
                              {sections[1].identity(), {}, right ? 1.0 : 0.0, .5},
                              {sections[2].identity(), {}, right ? 1.0 : 0.0, 1}}});
  }
  for (const bool first : {true, false}) {
    const auto support = lf::checked(FaceReference::create_planar(
        PartFeatureInputRole::LoftBoundarySupport,
        lf::checked(SemanticFaceReference::create(FeatureId(first ? "base.feature" : "tip.feature"),
                                                  SemanticFace::Back))));
    (first ? intent.start : intent.end) =
        LoftBoundaryCondition{(first ? sections.front() : sections.back()).identity(),
                              support,
                              {},
                              LoftBoundaryContinuity::G2,
                              false,
                              1};
  }
  (void)lf::checked(
      part.add_body(lf::checked(Body::create(BodyId("open.body"), "Open", BodyKind::Surface))));
  const auto feature = lf::checked(LoftFeature::create_loft_surface(
      FeatureId("open.loft"), "Open", sections, f::context("open.body"), {},
      {PathCurveId("left"), PathCurveId("right")}));
  (void)lf::checked(part.add_loft_feature(lf::checked(feature.with_constraints(intent))));
  auto cache = f::cache();
  const auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result ? std::string("ok") : result.error().message()));
  REQUIRE(result);
  const auto* shape = cache.find_body_shape(BodyId("open.body"));
  REQUIRE(shape);
  CHECK(RectangleExtrusionAdapter{}.summarize(*shape).solid_count == 0);
  const auto* native = OcctShapeView::get(*shape);
  REQUIRE(native);
  CHECK(native->ShapeType() == TopAbs_FACE);
  for (const double x : {-2.0, -1.0, 0.0, 1.0, 2.0})
    for (int i = 0; i < 3; ++i)
      CHECK(blcad::test::distance_to_faces(*shape, {x, front + (i == 1 ? 1 : 0), 5.0 + 20 * i}) <=
            1e-5);
  const auto bytes = lf::checked(serialize_part_document_to_json(part));
  auto restored = lf::checked(deserialize_part_document_from_json(bytes));
  auto fresh = f::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored, fresh));
  CHECK(lf::checked(serialize_part_document_to_json(restored)) == bytes);
}
