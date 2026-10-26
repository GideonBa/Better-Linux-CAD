#include "../support/loft_section_refinement_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/geometry/loft_section_compatibility.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <limits>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
using namespace blcad::test::loft_refinement;
namespace {
void on_shape(const GeometryShape& shape, Point3 point) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  // Query faces, not the solid volume: a merely interior point must not count as boundary evidence.
  BRep_Builder builder;
  TopoDS_Compound faces;
  builder.MakeCompound(faces);
  for (TopExp_Explorer face(*native, TopAbs_FACE); face.More(); face.Next())
    builder.Add(faces, face.Current());
  BRepExtrema_DistShapeShape distance(
      BRepBuilderAPI_MakeVertex(gp_Pnt(point.x, point.y, point.z)).Vertex(), faces);
  REQUIRE(distance.IsDone());
  CHECK(distance.Value() < 2e-6);
}
std::vector<ClosedProfileCurveSegment> polygon(const std::vector<Point3>& points) {
  std::vector<ClosedProfileCurveSegment> result;
  for (std::size_t i = 0; i < points.size(); ++i)
    result.push_back(
        {ClosedProfileCurveKind::Line, points[i], {}, points[(i + 1) % points.size()]});
  return result;
}
} // namespace

TEST_CASE("Automatic Loft refines eight nine and six spline edges without changing source intent",
          "[geometry][loft-section-compatibility]") {
  const auto source = sections();
  const auto refined = checked(make_loft_sections_compatible(FeatureId("loft.test"), source));
  REQUIRE(refined.adjusted);
  for (std::size_t i = 0; i < 3; ++i) {
    REQUIRE(refined.sections[i].size() == 16);
    REQUIRE(refined.source_vertices[i].size() == source[i].size());
    for (std::size_t j = 0; j < source[i].size(); ++j)
      CHECK(distance(refined.sections[i][refined.source_vertices[i][j]].start, source[i][j].start) <
            1e-10);
  }
  auto part = inputs();
  add_loft(part);
  const auto before = checked(serialize_part_document_to_json(part));
  auto shapes = checked(ShapeCache::create(ShapeCacheId("test.refinement")));
  const GeometryRecomputeExecutor executor;
  REQUIRE(executor.execute_document(part, shapes));
  const auto* shape = shapes.find_body_shape(BodyId("body.loft"));
  REQUIRE(shape);
  REQUIRE(BRepCheck_Analyzer(*OcctShapeView::get(*shape)).IsValid());
  const auto summary = RectangleExtrusionAdapter{}.summarize(*shape);
  CHECK(summary.solid_count == 1);
  CHECK(summary.volume_mm3 > 0);
  // Independent Bernstein samples from the complete original two-curve outline.
  for (std::size_t station = 0; station < 3; ++station)
    for (bool upper : {false, true})
      for (int i = 0; i <= 16; ++i)
        on_shape(*shape, model(station, at(poles(station, upper), i / 16.0)));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  auto reopened = checked(deserialize_part_document_from_json(before));
  auto other = checked(ShapeCache::create(ShapeCacheId("test.reopened")));
  REQUIRE(executor.execute_document(reopened, other));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*other.find_body_shape(BodyId("body.loft")))
            .volume_mm3 == Catch::Approx(summary.volume_mm3).epsilon(1e-9));
  CHECK(checked(serialize_part_document_to_json(reopened)) == before);
}

TEST_CASE("Equal Loft edge counts retain existing correspondence exactly",
          "[geometry][loft-section-compatibility]") {
  const auto s = sections();
  const std::vector<std::vector<ClosedProfileCurveSegment>> same{s[0], s[0], s[0]};
  const auto result = checked(make_loft_sections_compatible(FeatureId("same"), same));
  CHECK_FALSE(result.adjusted);
  for (std::size_t i = 0; i < same.size(); ++i)
    for (std::size_t j = 0; j < same[i].size(); ++j) {
      CHECK(result.source_vertices[i][j] == j);
      const auto& a = same[i][j];
      const auto& b = result.sections[i][j];
      CHECK(distance(a.start, b.start) == 0);
      CHECK(distance(a.end, b.end) == 0);
      CHECK(distance(a.mid, b.mid) == 0);
      CHECK(distance(a.control2, b.control2) == 0);
      CHECK(a.weights == b.weights);
      CHECK(a.kind == b.kind);
    }
}

TEST_CASE("Automatic Loft preserves exact circular and rational section boundaries",
          "[geometry][loft-section-compatibility]") {
  const bool rational = GENERATE(false, true);
  std::vector<std::vector<ClosedProfileCurveSegment>> original;
  const std::array counts{4, 6, 8};
  for (std::size_t section = 0; section < 3; ++section) {
    original.emplace_back();
    for (int i = 0; i < counts[section]; ++i) {
      const double a = 2 * std::numbers::pi * i / counts[section],
                   b = 2 * std::numbers::pi * (i + 1) / counts[section];
      const auto point = [&](double angle) {
        return Point3{5 * std::cos(angle), 5 * std::sin(angle), 10 * double(section)};
      };
      if (rational) {
        auto spline = checked(SplineSegment::create_elliptical_arc(SketchEntityId("arc"), {0, 0},
                                                                   {5, 0}, 5, a, b - a));
        const auto p = [&](Point2 v) { return Point3{v.x, v.y, 10 * double(section)}; };
        original.back().push_back({ClosedProfileCurveKind::CubicBezierSpline, p(spline.start()),
                                   p(spline.control1()), p(spline.end()), p(spline.control2()),
                                   spline.weights()});
      } else
        original.back().push_back(
            {ClosedProfileCurveKind::CircularArc, point(a), point((a + b) / 2), point(b)});
    }
  }
  const auto compatible = checked(make_loft_sections_compatible(FeatureId("circles"), original));
  REQUIRE(compatible.adjusted);
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < original[i].size(); ++j)
      CHECK(distance(original[i][j].start,
                     compatible.sections[i][compatible.source_vertices[i][j]].start) < 1e-9);
  const auto shape =
      checked(LoftAdapter{}.loft_closed_sections(FeatureId("circles"), original, true));
  for (std::size_t i = 0; i < 3; ++i)
    for (int j = 0; j < 24; ++j) {
      const double a = 2 * std::numbers::pi * j / 24;
      on_shape(shape, {5 * std::cos(a), 5 * std::sin(a), 10 * double(i)});
    }
}

TEST_CASE("Unequal open Loft sections retain center and endpoint guide bindings",
          "[geometry][loft-section-compatibility]") {
  std::vector<std::vector<SweepPathSegment>> original;
  for (int n = 1; n <= 3; ++n) {
    original.emplace_back();
    for (int i = 0; i < n; ++i)
      original.back().push_back({SweepPathSegmentKind::Line,
                                 {-5 + 10. * i / n, 0, 10. * (n - 1)},
                                 {},
                                 {-5 + 10. * (i + 1) / n, 0, 10. * (n - 1)}});
  }
  const auto compatible = checked(make_loft_sections_compatible(FeatureId("open"), original));
  for (std::size_t i = 0; i < 3; ++i) {
    CHECK(compatible.sections[i].size() == 4);
    CHECK(compatible.source_vertices[i].back() == 4);
  }
  const std::vector<SweepPathSegment> path{{SweepPathSegmentKind::Line, {0, 0, 0}, {}, {0, 0, 20}}};
  const std::vector<std::vector<SweepPathSegment>> guides{
      {{SweepPathSegmentKind::Line, {-5, 0, 0}, {}, {-5, 0, 20}}},
      {{SweepPathSegmentKind::Line, {5, 0, 0}, {}, {5, 0, 20}}}};
  const auto shape =
      checked(LoftAdapter{}.loft_open_sections(FeatureId("open"), original, &path, guides));
  CHECK(RectangleExtrusionAdapter{}.summarize(shape).solid_count == 0);
  on_shape(shape, {-5, 0, 5});
  on_shape(shape, {5, 0, 15});
  auto missed = guides;
  missed[1][0].start.x += .1;
  CHECK_FALSE(LoftAdapter{}.loft_open_sections(FeatureId("open.invalid"), original, &path, missed));
}

TEST_CASE("Unequal spline Loft guides keep the same leading and trailing anchors",
          "[geometry][loft-section-compatibility]") {
  const auto original = sections();
  auto straight = original;
  for (std::size_t i = 0; i < 3; ++i) {
    // Same physical outline, retaining each station's unequal authored partition.
    auto own = original[i];
    for (auto& s : own) {
      const double angle = i == 1 ? .1 : i == 2 ? -.08 : 0;
      const auto unmap = [&](Point3 p) {
        return Point3{(p.x - 3 * double(i)) / std::cos(angle) / (1 - .1 * i),
                      (p.y - 2 * double(i)) / (1 - .15 * i), 20 * double(i)};
      };
      s.start = unmap(s.start);
      s.mid = unmap(s.mid);
      s.control2 = unmap(s.control2);
      s.end = unmap(s.end);
    }
    straight[i] = std::move(own);
  }
  const std::vector<std::vector<SweepPathSegment>> guides{
      {{SweepPathSegmentKind::Line, {-20, 0, 0}, {}, {-20, 0, 40}}},
      {{SweepPathSegmentKind::Line, {30, 0, 0}, {}, {30, 0, 40}}}};
  const auto shape = checked(
      LoftAdapter{}.loft_closed_sections(FeatureId("guided"), straight, true, nullptr, guides));
  CHECK(RectangleExtrusionAdapter{}.summarize(shape).solid_count == 1);
  on_shape(shape, {-20, 0, 10});
  on_shape(shape, {30, 0, 30});
  auto invalid = guides;
  invalid[1][0].end.x += .5;
  CHECK_FALSE(LoftAdapter{}.loft_closed_sections(FeatureId("guided.bad"), straight, true, nullptr,
                                                 invalid));
}

TEST_CASE("Automatic Loft does not let added vertices redefine authored path centers",
          "[geometry][loft-section-compatibility]") {
  std::vector<std::vector<ClosedProfileCurveSegment>> original;
  for (int i = 0; i < 3; ++i) {
    std::vector<Point3> vertices{
        {-5, -5, 10. * i}, {5, -5, 10. * i}, {5, 5, 10. * i}, {-5, 5, 10. * i}};
    if (i >= 1)
      vertices.insert(vertices.begin() + 1, {0, -5, 10. * i});
    if (i == 2)
      vertices.insert(vertices.begin() + 3, {5, 0, 10. * i});
    Point3 center{};
    for (auto p : vertices) {
      center.x += p.x / vertices.size();
      center.y += p.y / vertices.size();
    }
    for (auto& p : vertices) {
      p.x -= center.x;
      p.y -= center.y;
    }
    original.push_back(polygon(vertices));
  }
  const std::vector<SweepPathSegment> path{{SweepPathSegmentKind::Line, {0, 0, 0}, {}, {0, 0, 20}}};
  const auto shape =
      checked(LoftAdapter{}.loft_closed_sections(FeatureId("center"), original, true, &path));
  on_shape(shape, {-5, -4.5, 5});
  on_shape(shape, {-5 - 5. / 12, -5 + 11. / 12, 15});
}

TEST_CASE("Automatic Loft rejects malformed sections and bounded refinement overflow",
          "[geometry][loft-section-compatibility]") {
  auto original = sections();
  SECTION("disconnected") {
    original[1][1].start.x += 1;
  }
  SECTION("non-finite") {
    original[1][1].start.x = std::numeric_limits<double>::quiet_NaN();
  }
  SECTION("invalid weight") {
    original[1][1].weights[0] = 0;
  }
  SECTION("oversized") {
    original[1].resize(4097, original[1][0]);
  }
  SECTION("derived budget") {
    original.clear();
    for (std::size_t n : {2053U, 2063U}) {
      original.emplace_back();
      for (std::size_t i = 0; i < n; ++i) {
        const auto p = piece(0, true, i, n);
        original.back().push_back({ClosedProfileCurveKind::CubicBezierSpline, model(0, p[0]),
                                   model(0, p[1]), model(0, p[3]), model(0, p[2])});
      }
      auto p = piece(0, false, 0, 1);
      std::reverse(p.begin(), p.end());
      original.back().push_back({ClosedProfileCurveKind::CubicBezierSpline, model(0, p[0]),
                                 model(0, p[1]), model(0, p[3]), model(0, p[2])});
    }
    const auto result = make_loft_sections_compatible(FeatureId("budget"), original);
    REQUIRE_FALSE(result);
    CHECK(result.error().message().find("budget") != std::string::npos);
  }
  CHECK_FALSE(make_loft_sections_compatible(FeatureId("bad"), original));
  CHECK_FALSE(LoftAdapter{}.loft_closed_sections(FeatureId("bad"), original, true));
}
