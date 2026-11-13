#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace {
std::vector<gp_Pnt> witnesses(const GeometryShape& shape) {
  const auto* native = OcctShapeView::get(shape);
  REQUIRE(native);
  std::vector<gp_Pnt> points;
  for (TopExp_Explorer it(*native, TopAbs_EDGE); it.More(); it.Next()) {
    BRepAdaptor_Curve curve(TopoDS::Edge(it.Current()));
    for (double t : {0., .125, .5, .875, 1.})
      points.push_back(curve.Value(std::lerp(curve.FirstParameter(), curve.LastParameter(), t)));
  }
  return points;
}
double length(const GeometryShape& shape) {
  GProp_GProps properties;
  BRepGProp::LinearProperties(*OcctShapeView::get(shape), properties);
  return properties.Mass();
}
} // namespace
TEST_CASE(
    "Spatial schema preserves actual historical arcs fit control splines and helix interpretation",
    "[geometry][parity-b163][spatial-legacy-geometry]") {
  auto doc = f::part();
  auto s = f::sketch("spatial", true);
  REQUIRE(doc.add_sketch_3d(s));
  const auto before = f::checked(Sketch3DGeometryAdapter{}.convert(doc, s.id()));
  REQUIRE(s.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("radius"), SpatialConstraintKind::Radius,
                                {f::curve("helix")}, f::length(4)))));
  REQUIRE(doc.update_sketch_3d(s));
  const auto after = f::checked(Sketch3DGeometryAdapter{}.convert(doc, s.id()));
  REQUIRE(before.products().size() == after.products().size());
  for (const auto& product : before.products()) {
    INFO(product.entity_id.value());
    const auto* current = after.find_product(product.entity_id);
    REQUIRE(current);
    CHECK(product.kind == current->kind);
    CHECK(product.edge_count == current->edge_count);
    CHECK(product.vertex_count == current->vertex_count);
    const auto a = witnesses(product.shape), b = witnesses(current->shape);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
      CHECK(a[i].Distance(b[i]) < 1e-12);
  }
  const auto* helix = after.find_product(SketchEntityId("helix"));
  REQUIRE(helix);
  const double expected = 3 * std::hypot(2 * std::numbers::pi * 4., 2.);
  CHECK(length(helix->shape) == Catch::Approx(expected).margin(1e-4));
  const auto points = witnesses(helix->shape);
  REQUIRE(points.size() == 5);
  CHECK(points.front().Z() == Catch::Approx(0).margin(1e-10));
  CHECK(points.back().Z() == Catch::Approx(6).margin(1e-7));
}
TEST_CASE("Spatial explicit radius pitch bindings execute the same native curve as equal legacy "
          "parameters",
          "[geometry][parity-b163][spatial-scalar-geometry]") {
  auto doc = f::part();
  auto s = f::sketch();
  REQUIRE(doc.add_sketch_3d(s));
  const auto before = f::checked(Sketch3DGeometryAdapter{}.convert(doc, s.id()));
  const auto h = *s.find_helix(SketchEntityId("helix"));
  REQUIRE(s.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
      h.id(), h.axis(),
      f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(4)))),
      f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(2)))),
      h.turns_parameter(), h.handedness()))));
  REQUIRE(doc.update_sketch_3d(s));
  const auto after = f::checked(Sketch3DGeometryAdapter{}.convert(doc, s.id()));
  const auto& a = before.find_product(h.id())->shape;
  const auto& b = after.find_product(h.id())->shape;
  const auto pa = witnesses(a), pb = witnesses(b);
  REQUIRE(pa.size() == pb.size());
  for (std::size_t i = 0; i < pa.size(); ++i)
    CHECK(pa[i].Distance(pb[i]) < 1e-12);
  CHECK(length(a) == Catch::Approx(length(b)).margin(1e-12));
}
