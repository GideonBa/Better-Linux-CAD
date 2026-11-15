#include "blcad/geometry/surface_boundary_query.hpp"
#include "blcad/geometry/sweep_adapter.hpp"
#include "derived_curve_internal.hpp"
#include "native_parameter_breaks.hpp"
#include <Geom_BSplineCurve.hxx>
#include <Geom_OffsetCurve.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::geometry;
namespace {
template <class T> T checked(Result<T> value) {
  if (!value)
    FAIL(value.error().message());
  return std::move(value.value());
}
GeometryShape narrow(bool bump) {
  std::vector<double> unique = {0};
  for (int i = 0; i <= 8; ++i)
    unique.push_back(.501 + i * .00005);
  unique.push_back(1);
  TColStd_Array1OfReal knots(1, int(unique.size()));
  TColStd_Array1OfInteger multiplicities(1, int(unique.size()));
  std::vector<double> expanded;
  for (int i = 0; i < int(unique.size()); ++i) {
    knots(i + 1) = unique[i];
    const int count = i == 0 || i == int(unique.size()) - 1 ? 4 : 1;
    multiplicities(i + 1) = count;
    for (int j = 0; j < count; ++j)
      expanded.push_back(unique[i]);
  }
  const int count = int(expanded.size()) - 4;
  TColgp_Array1OfPnt poles(1, count);
  for (int i = 0; i < count; ++i)
    poles(i + 1) = gp_Pnt(100 * (expanded[i + 1] + expanded[i + 2] + expanded[i + 3]) / 3,
                          bump && i == 7 ? .03 : 0, 0);
  Handle(Geom_Curve) curve = new Geom_BSplineCurve(poles, knots, multiplicities, 3, false);
  curve = new Geom_OffsetCurve(curve, 1e-6, gp_Dir(0, 0, 1));
  const auto a = curve->Value(0), b = curve->Value(1);
  const auto reference = std::make_shared<DerivedCurve>(curve, false);
  return checked(SweepAdapter{}.path_shape(PathCurveId(bump ? "bump" : "flat"),
                                           {{SweepPathSegmentKind::DerivedNative,
                                             {a.X(), a.Y(), a.Z()},
                                             {},
                                             {b.X(), b.Y(), b.Z()},
                                             reference}},
                                           false));
}
} // namespace
TEST_CASE("Native gap refinement visits narrow spline cells below offset and trim wrappers",
          "[geometry][surface-boundary-query][surface-gap-knots][parity-b172]") {
  const auto result = checked(SurfaceBoundaryQuery::measure_gap(narrow(false), narrow(true), 1e-4));
  CHECK(result.minimum_mm < 1e-7);
  CHECK(result.maximum_mm > .019);
  CHECK(result.maximum_mm < .021);
  CHECK_FALSE(result.within_tolerance);
}
TEST_CASE("Native periodic knots repeat across shifted trims without treating mm as parameters",
          "[geometry][surface-gap-knots][parity-b172]") {
  std::vector<double> values;
  blcad::geometry::detail::append_native_break(values, .05, .9, 1.1, true, 1);
  REQUIRE(values.size() == 1);
  CHECK(std::abs(values.front() - 1.05) < 1e-14);
  values.clear();
  blcad::geometry::detail::append_native_break(values, .05, .9, 1.1, false, 0);
  CHECK(values.empty());
}
