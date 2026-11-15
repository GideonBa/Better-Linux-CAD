#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/surface_boundary_query.hpp"
#include "blcad/geometry/surface_offset_adapter.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "blcad/geometry/surface_trim_adapter.hpp"
#include "imported_body_test_fixture.hpp"
#include "semantic_edge_resolution.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::curved_surface;
namespace {
ShapeCache cache() {
  return blcad::test::surface_offset::cache();
}
void execute(PartDocument& p, ShapeCache& c) {
  const auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
double area(const GeometryShape& shape) {
  GProp_GProps properties;
  BRepGProp::SurfaceProperties(*OcctShapeView::get(shape), properties);
  return properties.Mass();
}
std::string brep(const GeometryShape& shape) {
  std::ostringstream text;
  BRepTools::Write(*OcctShapeView::get(shape), text);
  return text.str();
}
} // namespace
TEST_CASE("G04 explicit curved Trim partitions a multi-face quilt through its native seam",
          "[geometry][curved-surface][curved-trim][parity-b172]") {
  const double opening = 2 * std::acos(.8), total = 10 * std::numbers::pi * 20;
  for (const auto side : {SurfaceTrimSide::KeepInside, SurfaceTrimSide::RemoveInside}) {
    auto p = f::part();
    f::trim(p, "trim", side);
    auto c = cache();
    execute(p, c);
    const auto* shape = c.find_body_shape(BodyId("trim.body"));
    REQUIRE(shape);
    const double expected =
        side == SurfaceTrimSide::KeepInside ? 10 * opening * 4 : total - 10 * opening * 4;
    CHECK(area(*shape) == Catch::Approx(expected).margin(1e-6));
    CHECK(RectangleExtrusionAdapter{}.summarize(*shape).solid_count == 0);
    const auto thick = f::checked(SurfaceOffsetAdapter{}.construct(
        FeatureId("signed"), *shape, SurfaceFeatureKind::Thicken, 1, SurfaceOffsetSide::AlongNormal,
        1e-6, SurfaceOffsetApproximation::ExactAnalytic));
    CHECK(blcad::test::integrated_solid_volume(thick.shape).volume_mm3 ==
          Catch::Approx(expected * 1.05).margin(1e-5));
    const double middle = blcad::test::distance_to_faces(*shape, {10, 0, 10});
    if (side == SurfaceTrimSide::KeepInside)
      CHECK(middle < 1e-6);
    else
      CHECK(middle > 1.9);
    const auto original = brep(*shape), bytes = blcad::test::surface_offset::saved(p);
    auto reopened = f::checked(deserialize_part_document_from_json(bytes));
    auto fresh = cache();
    execute(reopened, fresh);
    CHECK(area(*fresh.find_body_shape(BodyId("trim.body"))) ==
          Catch::Approx(expected).margin(1e-6));
    REQUIRE(
        p.set_parameter_value(ParameterId("window.height"), f::checked(Quantity::length_mm(6))));
    execute(p, c);
    CHECK(area(*c.find_body_shape(BodyId("trim.body"))) ==
          Catch::Approx(side == SurfaceTrimSide::KeepInside ? 10 * opening * 6
                                                            : total - 10 * opening * 6)
              .margin(1e-6));
    CHECK(!original.empty());
  }
  auto invalid = f::part();
  f::trim(invalid, "backwards", SurfaceTrimSide::KeepInside, f::quadrant(), {0, -1, 0});
  auto c = cache();
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(invalid, c));
  CHECK(c.body_shape_count() == 0);
}
TEST_CASE("G04 natural cylindrical boundary extension measures actual arc and axial distance",
          "[geometry][curved-surface][curved-extend][parity-b172]") {
  auto p = f::part();
  f::arc_path(p, "top.arc", 0, std::numbers::pi / 2, "source.height");
  f::extend(p, "extended", f::quadrant({0}), "top.arc");
  auto c = cache();
  execute(p, c);
  const auto* shape = c.find_body_shape(BodyId("extended.body"));
  REQUIRE(shape);
  CHECK(area(*shape) == Catch::Approx(10 * std::numbers::pi / 2 * 23).margin(1e-6));
  CHECK(blcad::test::distance_to_faces(*shape, {std::sqrt(50.), std::sqrt(50.), 23}) < 1e-6);
  const auto target = f::checked(
      SurfaceSelectionGeometry::resolve(p, c, f::quadrant({0}), FeatureId("frame"), 1e-6));
  const auto segments =
      f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(p, PathCurveId("top.arc"), c));
  const auto frame =
      f::checked(SurfaceTrimAdapter{}.extension_frame(FeatureId("frame"), target, segments, 1e-6));
  CHECK(frame.direction.z == Catch::Approx(1));
  CHECK(frame.origin.z == Catch::Approx(20));
  CHECK(frame.origin.x == Catch::Approx(std::sqrt(50.)));
  CHECK(frame.origin.y == Catch::Approx(std::sqrt(50.)));
  const auto source_before = brep(target);
  for (const auto mode : {SurfaceExtendMode::Natural, SurfaceExtendMode::Tangent}) {
    const std::vector<SweepPathSegment> boundary = {
        {SweepPathSegmentKind::Line, {0, 10, 0}, {}, {0, 10, 20}}};
    const auto extended = f::checked(SurfaceTrimAdapter{}.extend(
        FeatureId("angular"), target, boundary, 3, mode, 1e-6,
        mode == SurfaceExtendMode::Tangent ? std::optional(1e-4) : std::nullopt, "generator"));
    CHECK(area(extended) == Catch::Approx(100 * std::numbers::pi + 60).margin(1e-6));
    const Point3 end = mode == SurfaceExtendMode::Natural
                           ? Point3{10 * std::cos(std::numbers::pi / 2 + .3),
                                    10 * std::sin(std::numbers::pi / 2 + .3), 10}
                           : Point3{-3, 10, 10};
    CHECK(blcad::test::distance_to_faces(extended, end) < 1e-6);
    CHECK(brep(target) == source_before);
  }
}

TEST_CASE("G04 read-only imported boundary frame uses the current verified Body placement",
          "[geometry][curved-surface][curved-frame-import][parity-b172]") {
  imported_body_test::Fixture files;
  files.write(BRepPrimAPI_MakeCylinder(10, 20).Shape());
  auto p = files.editable();
  const BodyId owner("body.1");
  REQUIRE(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("move"), owner, BodyTransformCoordinateSpace::World, std::nullopt, {2, 3, 4},
      false, false))));
  const auto& catalog = p.imported_topology_catalogs().front();
  const auto face =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
        return entry.descriptor.kind == ImportedTopologyKind::Face &&
               entry.descriptor.geometry == ImportedGeometryClass::Cylinder;
      });
  const auto rim =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
        return entry.descriptor.kind == ImportedTopologyKind::Edge &&
               entry.descriptor.geometry == ImportedGeometryClass::Circle &&
               std::abs(entry.descriptor.centroid.z - 20) < 1e-6;
      });
  REQUIRE(face != catalog.entries().end());
  REQUIRE(rim != catalog.entries().end());
  const auto edge = f::checked(EdgeReference::create_imported(
      PartFeatureInputRole::PathSegment,
      f::checked(ImportedTopologyReference::create(catalog.id(), rim->id,
                                                   ImportedTopologyCapability::Edge))));
  const auto segment = f::checked(PathSegmentReference::create_feature_edge(edge, owner));
  REQUIRE(p.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("original.rim"), "Original rim", {segment}, PathClosure::Closed,
                        PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
  const auto source = f::checked(SurfaceReference::create_faces(
      owner, {f::checked(FaceReference::create_imported(
                 PartFeatureInputRole::SurfaceSourceFace,
                 f::checked(ImportedTopologyReference::create(
                     catalog.id(), face->id, ImportedTopologyCapability::Face))))}));
  auto c = cache();
  StepImportTopologyContext imports(files.root);
  REQUIRE(GeometryRecomputeExecutor{&imports}.execute_document(p, c));
  const auto old_boundary =
      f::checked(BoundaryCurveReference::create_path_curve(PathCurveId("original.rim")));
  CHECK_FALSE(
      GeometryRecomputeExecutor{&imports}.surface_extension_frame(p, source, old_boundary, c));
  // Imported Path bindings deliberately refer to the immutable base. A boundary
  // on the moved Body uses its actual producer's complete current result witness.
  TopTools_IndexedMapOfShape edges;
  TopExp::MapShapes(*OcctShapeView::get(*c.find_body_shape(owner)), TopAbs_EDGE, edges);
  std::vector<TopoDS_Edge> matches;
  for (int i = 1; i <= edges.Extent(); ++i) {
    BRepAdaptor_Curve curve(TopoDS::Edge(edges(i)));
    if (curve.GetType() == GeomAbs_Circle && std::abs(curve.Circle().Location().Z() - 24) < 1e-6)
      matches.push_back(TopoDS::Edge(edges(i)));
  }
  REQUIRE(matches.size() == 1);
  const auto witness = f::checked(
      make_result_edge_reference(c.find_body_result(owner)->source_feature_id, matches.front()));
  const auto current_edge =
      f::checked(EdgeReference::create_result(PartFeatureInputRole::PathSegment, witness));
  const auto current_segment = f::checked(PathSegmentReference::create_feature_edge(current_edge));
  REQUIRE(p.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("rim"), "Current rim", {current_segment}, PathClosure::Closed,
                        PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
  REQUIRE(GeometryRecomputeExecutor{&imports}.execute_document(p, c));
  const auto before = brep(*c.find_body_shape(owner)),
             bytes = blcad::test::surface_offset::saved(p);
  const auto boundary = f::checked(BoundaryCurveReference::create_path_curve(PathCurveId("rim")));
  const auto frame =
      GeometryRecomputeExecutor{&imports}.surface_extension_frame(p, source, boundary, c);
  INFO((frame ? "ok" : frame.error().message()));
  REQUIRE(frame);
  CHECK(frame.value().origin.z == Catch::Approx(24));
  CHECK(std::hypot(frame.value().origin.x - 2, frame.value().origin.y - 3) == Catch::Approx(10));
  CHECK(frame.value().direction.z == Catch::Approx(1));
  CHECK(brep(*c.find_body_shape(owner)) == before);
  CHECK(blcad::test::surface_offset::saved(p) == bytes);
}

TEST_CASE("G04 disconnected trim regions and partial or overlapping extensions reject explicitly",
          "[geometry][curved-surface][curved-rejection][parity-b172]") {
  auto p = f::part();
  REQUIRE(p.update_construction_plane(f::checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("window.plane"), "Window plane",
                                         {-10, -20, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
  f::trim(p, "ambiguous", SurfaceTrimSide::KeepInside, f::quadrant({0, 1, 2, 3}));
  auto c = cache();
  const auto rejected = GeometryRecomputeExecutor{}.execute_document(p, c);
  REQUIRE_FALSE(rejected);
  CHECK(c.body_shape_count() == 0);
  auto source = f::part();
  f::arc_path(source, "partial", 0, std::numbers::pi / 4, "source.height");
  execute(source, c);
  const auto target = f::checked(
      SurfaceSelectionGeometry::resolve(source, c, f::quadrant({0}), FeatureId("frame"), 1e-6));
  const auto before = brep(target);
  const auto partial = f::checked(
      GeometryRecomputeExecutor{}.resolve_path_segments(source, PathCurveId("partial"), c));
  const auto incomplete =
      SurfaceTrimAdapter{}.extend(FeatureId("partial"), target, partial, 3,
                                  SurfaceExtendMode::Natural, 1e-6, std::nullopt, "partial");
  REQUIRE_FALSE(incomplete);
  CHECK(incomplete.error().message().find("boundary") != std::string::npos);
  const std::vector<SweepPathSegment> generator = {
      {SweepPathSegmentKind::Line, {0, 10, 0}, {}, {0, 10, 20}}};
  const auto overlap =
      SurfaceTrimAdapter{}.extend(FeatureId("overlap"), target, generator, 60,
                                  SurfaceExtendMode::Natural, 1e-6, std::nullopt, "generator");
  REQUIRE_FALSE(overlap);
  CHECK(overlap.error().message().find("overlap") != std::string::npos);
  CHECK(brep(target) == before);
}
