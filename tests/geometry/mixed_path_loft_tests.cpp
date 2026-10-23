#include "../support/mixed_path_loft_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <catch2/catch_approx.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace fixture = blcad::test::mixed_loft;
using fixture::checked;
namespace {
void recompute(const PartDocument& part, ShapeCache& shapes) {
  auto result = GeometryRecomputeExecutor{}.execute_document(part, shapes);
  if (!result)
    FAIL(result.error().message());
  REQUIRE(result);
}
ShapeCache cache() {
  return checked(ShapeCache::create(ShapeCacheId("mixed.cache")));
}
const GeometryShape& body(const ShapeCache& shapes) {
  const auto* result = shapes.find_body_shape(BodyId("mixed.body"));
  REQUIRE(result);
  return *result;
}
TopoDS_Compound boundary(const GeometryShape& shape) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  CHECK(BRepCheck_Analyzer(*native, true).IsValid());
  BRep_Builder builder;
  TopoDS_Compound faces;
  builder.MakeCompound(faces);
  for (TopExp_Explorer face(*native, TopAbs_FACE); face.More(); face.Next())
    builder.Add(faces, face.Current());
  return faces;
}
void on_boundary(const TopoDS_Compound& faces, Point3 expected) {
  INFO("Expected boundary point " << expected.x << "," << expected.y << "," << expected.z);
  const auto vertex =
      BRepBuilderAPI_MakeVertex(gp_Pnt(expected.x, expected.y, expected.z)).Vertex();
  BRepExtrema_DistShapeShape distance(vertex, faces);
  REQUIRE(distance.IsDone());
  CHECK(distance.Value() < 2e-5);
}
void authored_sections(const PartDocument& part, const GeometryShape& shape, std::size_t count) {
  const auto faces = boundary(shape);
  for (std::size_t i = 0; i < count; ++i) {
    CAPTURE(i);
    auto expected = fixture::station(count, i);
    if (i % 2 == 0) {
      const auto* width =
          part.find_parameter(ParameterId("section." + std::to_string(i) + ".width"));
      const auto original = expected.outline[1].x * 2;
      expected = fixture::station(count, i, width->value().millimeters() / original);
    }
    for (std::size_t j = 0; j < 4; ++j) {
      const auto a = expected.outline[j], b = expected.outline[(j + 1) % 4];
      on_boundary(faces, fixture::model_point(expected, a));
      on_boundary(faces, fixture::model_point(expected, {(a.x + b.x) / 2, (a.y + b.y) / 2}));
    }
  }
}
} // namespace

TEST_CASE("Mixed line spline arc lofts interpolate three and five distinct oriented sections",
          "[geometry][guided-loft][mixed-path-loft]") {
  for (std::size_t count : {3U, 5U}) {
    CAPTURE(count);
    auto part = fixture::inputs(count);
    fixture::add_loft(part, count);
    auto shapes = cache();
    const GeometryRecomputeExecutor executor;
    const auto segments =
        checked(executor.resolve_path_segments(part, PathCurveId("mixed.path"), shapes));
    REQUIRE(segments.size() == 34);
    CHECK(segments.front().kind == SweepPathSegmentKind::Line);
    CHECK(segments.back().kind == SweepPathSegmentKind::CircularArc);
    CHECK(segments.back().start == Point3{10, 0, 60});
    CHECK(segments.back().end == Point3{40, 0, 90});
    recompute(part, shapes);
    const auto summary = RectangleExtrusionAdapter{}.summarize(body(shapes));
    CHECK(summary.solid_count == 1);
    CHECK(summary.volume_mm3 > 100);
    authored_sections(part, body(shapes), count);
    // An intermediate control section is on the analytic arc, not its two chords.
    if (count == 5) {
      const auto a = fixture::station(count, 3), b = fixture::station(count, 4);
      const auto target = fixture::path_point(.875);
      const auto center = fixture::interpolate(a.center, b.center, .5);
      const auto faces = boundary(body(shapes));
      for (std::size_t j = 0; j < 4; ++j) {
        auto p = fixture::interpolate(fixture::model_point(a, a.outline[j]),
                                      fixture::model_point(b, b.outline[j]), .5);
        p = {p.x + target.x - center.x, p.y + target.y - center.y, p.z + target.z - center.z};
        on_boundary(faces, p);
      }
    }
    const auto saved = checked(serialize_part_document_to_json(part));
    const auto restored = checked(deserialize_part_document_from_json(saved));
    CHECK(checked(serialize_part_document_to_json(restored)) == saved);
    auto reopened = cache();
    recompute(restored, reopened);
    CHECK(RectangleExtrusionAdapter{}.summarize(body(reopened)).volume_mm3 ==
          Catch::Approx(summary.volume_mm3).epsilon(1e-8));
  }
}

TEST_CASE(
    "Mixed path Loft recomputes a middle profile and retains cached geometry on invalid edits",
    "[geometry][guided-loft][mixed-path-loft]") {
  auto part = fixture::inputs();
  fixture::add_loft(part);
  auto shapes = cache();
  const GeometryRecomputeExecutor executor;
  recompute(part, shapes);
  const auto before_volume = RectangleExtrusionAdapter{}.summarize(body(shapes)).volume_mm3;
  REQUIRE(
      part.set_parameter_value(ParameterId("section.2.width"), checked(Quantity::length_mm(4.5))));
  recompute(part, shapes);
  const auto volume = RectangleExtrusionAdapter{}.summarize(body(shapes)).volume_mm3;
  CHECK(volume > before_volume + 5);
  authored_sections(part, body(shapes), 5);
  const GeometryShape stable = body(shapes);
  const auto native = *OcctShapeView::get(stable);
  SECTION("misbound section") {
    auto plane = fixture::station(5, 2);
    plane.center.x += .25;
    REQUIRE(part.update_construction_plane(checked(
        ConstructionPlane::create_explicit(ConstructionPlaneId("section.plane.2"), "Misbound",
                                           plane.center, plane.x, plane.y, plane.normal))));
  }
  SECTION("disconnected path after upstream Sketch edit") {
    auto trajectory = checked(Sketch::create(SketchId("trajectory"), "Broken trajectory",
                                             DatumPlaneId("trajectory.plane")));
    REQUIRE(trajectory.add_entity(
        checked(LineSegment::create(SketchEntityId("lead"), {0, 0}, {0, 29}))));
    const auto* source = part.find_sketch(SketchId("trajectory"));
    REQUIRE(trajectory.add_entity(source->spline_segments().front()));
    REQUIRE(trajectory.add_entity(source->arc_segments().front()));
    REQUIRE(part.update_sketch(std::move(trajectory)));
  }
  SECTION("unsupported G2") {
    const auto original = part.loft_features().front();
    REQUIRE(part.update_loft_feature(checked(LoftFeature::create_loft(
        original.id(), original.name(), original.sections(), original.body_result_context(),
        original.path_curve(), {}, LoftContinuity::G2))));
  }
  CHECK_FALSE(executor.execute_document(part, shapes));
  CHECK(OcctShapeView::get(body(shapes))->IsSame(native));
  CHECK(RectangleExtrusionAdapter{}.summarize(body(shapes)).volume_mm3 ==
        Catch::Approx(volume).epsilon(1e-12));
}

TEST_CASE(
    "Mixed line spline arc trajectory supports a five-section LoftSurface and a constant Sweep",
    "[geometry][guided-loft][mixed-path-loft]") {
  auto part = fixture::inputs();
  auto shapes = cache();
  SECTION("surface") {
    fixture::add_loft(part, 5, true, LoftFeatureKind::LoftSurface);
    recompute(part, shapes);
    CHECK(RectangleExtrusionAdapter{}.summarize(body(shapes)).solid_count == 0);
    authored_sections(part, body(shapes), 5);
  }
  SECTION("sweep") {
    REQUIRE(
        part.add_body(checked(Body::create(BodyId("mixed.body"), "Mixed sweep", BodyKind::Solid))));
    const auto context = checked(FeatureBodyResultContext::create(
        FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("mixed.body")));
    const auto profile =
        checked(SweepProfileReference::create_closed_region(checked(ProfileRegionReference::create(
            SketchId("section.0"), ProfileId("profile.0"), PartFeatureInputRole::SweepProfile))));
    REQUIRE(part.add_sweep_feature(checked(SweepFeature::create_sweep(
        FeatureId("mixed.sweep"), "Mixed sweep", profile, PathCurveId("mixed.path"), context))));
    recompute(part, shapes);
    const auto summary = RectangleExtrusionAdapter{}.summarize(body(shapes));
    CHECK(summary.solid_count == 1);
    CHECK(summary.volume_mm3 == Catch::Approx(12 * fixture::path_length()).epsilon(.03));
    (void)boundary(body(shapes));
  }
}

TEST_CASE("Mixed path controls open Loft sections and closed Loft guide stations",
          "[geometry][guided-loft][mixed-path-loft]") {
  auto shapes = cache();
  const auto path = checked(GeometryRecomputeExecutor{}.resolve_path_segments(
      fixture::inputs(), PathCurveId("mixed.path"), shapes));
  SECTION("open sections bound to the center path") {
    std::vector<std::vector<SweepPathSegment>> sections;
    for (std::size_t i = 0; i < 5; ++i) {
      const auto frame = fixture::station(5, i);
      sections.push_back({{SweepPathSegmentKind::Line,
                           fixture::model_point(frame, {-2, 0}),
                           {},
                           fixture::model_point(frame, {2, 0})}});
    }
    const auto shape =
        checked(LoftAdapter{}.loft_open_sections(FeatureId("mixed.open"), sections, &path));
    CHECK(RectangleExtrusionAdapter{}.summarize(shape).solid_count == 0);
    const auto faces = boundary(shape);
    for (std::size_t i = 0; i < 5; ++i) {
      on_boundary(faces, sections[i].front().start);
      on_boundary(faces, sections[i].front().end);
    }
    on_boundary(faces, fixture::path_point(.875));
  }
  SECTION("mixed path bound as a guide to the first boundary point") {
    std::vector<std::vector<ClosedProfileCurveSegment>> sections;
    for (std::size_t i = 0; i < 5; ++i) {
      auto frame = fixture::station(5, i);
      // Same seam at the path station; distinct outlines on all five rotated planes.
      const auto anchor = frame.outline.front();
      std::vector<ClosedProfileCurveSegment> edges;
      for (std::size_t j = 0; j < 4; ++j) {
        const auto a = frame.outline[j], b = frame.outline[(j + 1) % 4];
        edges.push_back({ClosedProfileCurveKind::Line,
                         fixture::model_point(frame, {a.x - anchor.x, a.y - anchor.y}),
                         {},
                         fixture::model_point(frame, {b.x - anchor.x, b.y - anchor.y})});
      }
      sections.push_back(std::move(edges));
    }
    const auto shape = checked(LoftAdapter{}.loft_closed_sections(FeatureId("mixed.guide"),
                                                                  sections, true, nullptr, {path}));
    CHECK(RectangleExtrusionAdapter{}.summarize(shape).solid_count == 1);
    const auto faces = boundary(shape);
    for (const auto& section : sections)
      for (const auto& edge : section)
        on_boundary(faces, edge.start);
    on_boundary(faces, fixture::path_point(.875));
  }
  SECTION("invalid circular control geometry returns a diagnostic") {
    const std::vector<SweepPathSegment> invalid{
        {SweepPathSegmentKind::CircularArc, {0, 0, 0}, {0, 0, 45}, {0, 0, 90}}};
    const std::vector<std::vector<SweepPathSegment>> sections{
        {{SweepPathSegmentKind::Line, {-2, 0, 0}, {}, {2, 0, 0}}},
        {{SweepPathSegmentKind::Line, {-2, 0, 45}, {}, {2, 0, 45}}},
        {{SweepPathSegmentKind::Line, {-2, 0, 90}, {}, {2, 0, 90}}}};
    const auto result =
        LoftAdapter{}.loft_open_sections(FeatureId("mixed.invalid"), sections, &invalid);
    REQUIRE_FALSE(result);
    CHECK(result.error().message().find("invalid circular arc") != std::string::npos);
  }
}
