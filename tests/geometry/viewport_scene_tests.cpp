#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

std::vector<SketchEntityId> add_polygon(Sketch& sketch, std::string_view id_prefix,
                                        const std::vector<Point2>& vertices) {
  std::vector<SketchEntityId> ids;
  ids.reserve(vertices.size());
  for (std::size_t index = 0U; index < vertices.size(); ++index) {
    const std::string id = std::string(id_prefix) + "." + std::to_string(index);
    auto line = LineSegment::create(SketchEntityId(id), vertices[index],
                                    vertices[(index + 1U) % vertices.size()]);
    REQUIRE(line);
    REQUIRE(sketch.add_entity(line.value()));
    ids.emplace_back(id);
  }
  return ids;
}

double polygon_area(const std::vector<Point2>& vertices) {
  double area_twice = 0.0;
  for (std::size_t index = 0U; index < vertices.size(); ++index) {
    const Point2& current = vertices[index];
    const Point2& next = vertices[(index + 1U) % vertices.size()];
    area_twice += current.x * next.y - next.x * current.y;
  }
  return std::abs(area_twice) * 0.5;
}

std::size_t count_subshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind) {
  std::size_t count = 0U;
  for (TopExp_Explorer explorer(shape, kind); explorer.More(); explorer.Next())
    ++count;
  return count;
}

double surface_area(const TopoDS_Shape& shape) {
  GProp_GProps properties;
  BRepGProp::SurfaceProperties(shape, properties);
  return properties.Mass();
}

} // namespace

TEST_CASE("Viewport presents a line rectangle around a circle as two selectable regions",
          "[geometry][viewport-scene][nested-profile]") {
  const std::array<Point2, 4U> counterclockwise{
      {{-20.0, -10.0}, {20.0, -10.0}, {20.0, 10.0}, {-20.0, 10.0}}};
  const std::array<Point2, 4U> clockwise{
      {{-20.0, -10.0}, {-20.0, 10.0}, {20.0, 10.0}, {20.0, -10.0}}};
  const std::array<const char*, 4U> line_ids{{"outer.a", "outer.b", "outer.c", "outer.d"}};

  for (const bool clockwise_outer : {false, true}) {
    CAPTURE(clockwise_outer);
    auto document = PartDocument::create(DocumentId("part.viewport_nested"), "ViewportNested");
    REQUIRE(document);
    REQUIRE(document.value().add_parameter(
        make_length_parameter("part.hole_diameter", "hole_diameter", 6.0)));
    REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));

    auto sketch = Sketch::create(SketchId("sketch.nested"), "Nested", DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    const auto& corners = clockwise_outer ? clockwise : counterclockwise;
    for (std::size_t index = 0U; index < corners.size(); ++index) {
      add_line(sketch.value(), line_ids[index], corners[index],
               corners[(index + 1U) % corners.size()]);
    }
    auto outer = ClosedProfile::create(ProfileId("profile.outer"),
                                       {SketchEntityId(line_ids[0]), SketchEntityId(line_ids[1]),
                                        SketchEntityId(line_ids[2]), SketchEntityId(line_ids[3])});
    REQUIRE(outer);
    REQUIRE(sketch.value().add_profile(outer.value()));
    auto circle = CircleProfile::create(ProfileId("profile.circle"),
                                        ParameterId("part.hole_diameter"), Point2{});
    REQUIRE(circle);
    REQUIRE(sketch.value().add_profile(circle.value()));
    REQUIRE(document.value().add_sketch(sketch.value()));

    auto cache = ShapeCache::create(ShapeCacheId("cache.viewport_nested"));
    REQUIRE(cache);
    auto scene = ViewportSceneBuilder{}.build_part(document.value(), cache.value());
    REQUIRE(scene);

    const auto find_profile = [&scene](std::string_view semantic_id) {
      return std::find_if(
          scene.value().begin(), scene.value().end(),
          [semantic_id](const ViewportSceneItem& item) { return item.semantic_id == semantic_id; });
    };
    const auto outer_item = find_profile("sketch/sketch.nested/profile/profile.outer");
    const auto circle_item = find_profile("sketch/sketch.nested/profile/profile.circle");
    REQUIRE(outer_item != scene.value().end());
    REQUIRE(circle_item != scene.value().end());
    CHECK(outer_item->kind == ViewportSceneKind::SketchEntity);
    CHECK(circle_item->kind == ViewportSceneKind::SketchEntity);

    const TopoDS_Shape* outer_shape = OcctShapeView::get(outer_item->shape);
    const TopoDS_Shape* circle_shape = OcctShapeView::get(circle_item->shape);
    REQUIRE(outer_shape != nullptr);
    REQUIRE(circle_shape != nullptr);
    REQUIRE_FALSE(outer_shape->IsNull());
    REQUIRE_FALSE(circle_shape->IsNull());
    CHECK(outer_shape->ShapeType() == TopAbs_FACE);
    CHECK(circle_shape->ShapeType() == TopAbs_FACE);

    // The rectangle item is one face bounded by its outer wire and exactly one
    // circular inner wire. The circle remains its own filled, selectable disk.
    CHECK(count_subshapes(*outer_shape, TopAbs_WIRE) == 2U);
    CHECK(count_subshapes(*outer_shape, TopAbs_EDGE) == 5U);
    CHECK(count_subshapes(*circle_shape, TopAbs_WIRE) == 1U);
    CHECK(count_subshapes(*circle_shape, TopAbs_EDGE) == 1U);

    constexpr double rectangle_area = 40.0 * 20.0;
    constexpr double circle_area = std::numbers::pi * 3.0 * 3.0;
    CHECK(surface_area(*outer_shape) == Approx(rectangle_area - circle_area).margin(1.0e-7));
    CHECK(surface_area(*circle_shape) == Approx(circle_area).margin(1.0e-7));
  }
}

TEST_CASE("Viewport presents arbitrary nested line loops as distinct selectable regions",
          "[geometry][viewport-scene][nested-profile][polygon-hole]") {
  const std::vector<Point2> outer_counterclockwise{{-16.0, -12.0}, {16.0, -12.0}, {16.0, 12.0},
                                                   {8.0, 12.0},    {8.0, 5.0},    {4.0, 5.0},
                                                   {4.0, 12.0},    {-16.0, 12.0}};
  const std::vector<Point2> middle_counterclockwise{
      {-10.0, -4.0}, {0.0, -6.0}, {3.0, 0.0}, {0.0, 7.0}, {-10.0, 5.0}};
  const std::vector<Point2> inner_counterclockwise{{-5.0, -1.0}, {-1.0, 0.0}, {-3.0, 4.0}};

  for (const bool reverse_outer : {false, true}) {
    for (const bool reverse_middle : {false, true}) {
      for (const bool reverse_inner : {false, true}) {
        CAPTURE(reverse_outer, reverse_middle, reverse_inner);
        auto outer = outer_counterclockwise;
        auto middle = middle_counterclockwise;
        auto inner = inner_counterclockwise;
        if (reverse_outer)
          std::reverse(outer.begin(), outer.end());
        if (reverse_middle)
          std::reverse(middle.begin(), middle.end());
        if (reverse_inner)
          std::reverse(inner.begin(), inner.end());

        auto document = PartDocument::create(DocumentId("part.viewport_polygon_nested"),
                                             "ViewportPolygonNested");
        REQUIRE(document);
        REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
        auto sketch = Sketch::create(SketchId("sketch.polygon_nested"), "PolygonNested",
                                     DatumPlaneId("datum.xy"));
        REQUIRE(sketch);

        const auto outer_ids = add_polygon(sketch.value(), "outer", outer);
        const auto middle_ids = add_polygon(sketch.value(), "middle", middle);
        const auto inner_ids = add_polygon(sketch.value(), "inner", inner);
        auto outer_profile = ClosedProfile::create(ProfileId("profile.outer"), outer_ids);
        auto middle_profile = ClosedProfile::create(ProfileId("profile.middle"), middle_ids);
        auto inner_profile = ClosedProfile::create(ProfileId("profile.inner"), inner_ids);
        REQUIRE(outer_profile);
        REQUIRE(middle_profile);
        REQUIRE(inner_profile);
        REQUIRE(sketch.value().add_profile(outer_profile.value()));
        REQUIRE(sketch.value().add_profile(middle_profile.value()));
        REQUIRE(sketch.value().add_profile(inner_profile.value()));
        REQUIRE(document.value().add_sketch(sketch.value()));

        auto cache = ShapeCache::create(ShapeCacheId("cache.viewport_polygon_nested"));
        REQUIRE(cache);
        auto scene = ViewportSceneBuilder{}.build_part(document.value(), cache.value());
        REQUIRE(scene);

        const auto profile_shape = [&scene](std::string_view profile) -> const TopoDS_Shape* {
          const std::string semantic_id =
              "sketch/sketch.polygon_nested/profile/" + std::string(profile);
          const auto item = std::find_if(scene.value().begin(), scene.value().end(),
                                         [&semantic_id](const ViewportSceneItem& candidate) {
                                           return candidate.semantic_id == semantic_id;
                                         });
          return item == scene.value().end() ? nullptr : OcctShapeView::get(item->shape);
        };
        const TopoDS_Shape* outer_shape = profile_shape("profile.outer");
        const TopoDS_Shape* middle_shape = profile_shape("profile.middle");
        const TopoDS_Shape* inner_shape = profile_shape("profile.inner");
        REQUIRE(outer_shape != nullptr);
        REQUIRE(middle_shape != nullptr);
        REQUIRE(inner_shape != nullptr);

        // Only the immediate child contour is a hole. This makes each planar
        // band independently selectable even for three levels of nesting.
        CHECK(count_subshapes(*outer_shape, TopAbs_WIRE) == 2U);
        CHECK(count_subshapes(*outer_shape, TopAbs_EDGE) == outer.size() + middle.size());
        CHECK(count_subshapes(*middle_shape, TopAbs_WIRE) == 2U);
        CHECK(count_subshapes(*middle_shape, TopAbs_EDGE) == middle.size() + inner.size());
        CHECK(count_subshapes(*inner_shape, TopAbs_WIRE) == 1U);
        CHECK(count_subshapes(*inner_shape, TopAbs_EDGE) == inner.size());

        CHECK(surface_area(*outer_shape) ==
              Approx(polygon_area(outer) - polygon_area(middle)).margin(1.0e-7));
        CHECK(surface_area(*middle_shape) ==
              Approx(polygon_area(middle) - polygon_area(inner)).margin(1.0e-7));
        CHECK(surface_area(*inner_shape) == Approx(polygon_area(inner)).margin(1.0e-7));
      }
    }
  }
}
