#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "imported_body_test_fixture.hpp"
#include "semantic_edge_resolution.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <algorithm>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace {
void import_base(PartDocument& part, const PartDocument& source) {
  for (const auto& value : source.step_import_sources())
    (void)f::checked(part.add_step_import_source(value));
  for (const auto& value : source.imported_topology_catalogs())
    (void)f::checked(part.add_imported_topology_catalog(value));
  for (const auto& value : source.bodies())
    (void)f::checked(part.add_body(value));
  for (const auto& value : source.imported_body_features())
    (void)f::checked(part.add_imported_body_feature(value));
}
bool material_at(const GeometryShape& shape, Point3 p) {
  BRepClass3d_SolidClassifier classifier(*OcctShapeView::get(shape), gp_Pnt(p.x, p.y, p.z), 1e-7);
  REQUIRE(classifier.State() != TopAbs_ON);
  return classifier.State() == TopAbs_IN;
}
void circle_section(PartDocument& part, Point3 origin, Vector3 x, Vector3 y, Vector3 normal) {
  (void)f::checked(part.add_construction_plane(f::checked(ConstructionPlane::create_explicit(
      ConstructionPlaneId("section.plane"), "Normal section", origin, x, y, normal))));
  auto sketch = f::checked(
      Sketch::create(SketchId("section"), "Hollow profile", DatumPlaneId("section.plane")));
  for (auto [id, value] : {std::pair{"outer", 1.6}, {"inner", .8}}) {
    (void)f::checked(part.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
    (void)f::checked(
        sketch.add_profile(f::checked(CircleProfile::create(ProfileId(id), ParameterId(id)))));
  }
  (void)f::checked(part.add_sketch(sketch));
  (void)f::checked(
      part.add_body(f::checked(Body::create(BodyId("pipe"), "Pipe", BodyKind::Solid))));
  (void)f::checked(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
      FeatureId("pipe.feature"), "Mixed hollow pipe",
      f::checked(SweepProfileReference::create_closed_region(
          f::checked(ProfileRegionReference::create(sketch.id(), ProfileId("outer"),
                                                    PartFeatureInputRole::SweepProfile)),
          SweepRegionPolicy::MaterialRegion)),
      PathCurveId("route"),
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                  BodyId("pipe")))))));
}
} // namespace
TEST_CASE("G01 reversed generated rational spatial and imported sources "
          "preserve one physical void",
          "[geometry][parity-b167][semantic-sweep-acceptance]") {
  imported_body_test::Fixture supplier;
  supplier.write(BRepPrimAPI_MakeBox(gp_Pnt(25, 0, 25), 5, 3, 4).Shape(), "INCH");
  const auto imported = supplier.editable();
  for (bool backwards : {false, true}) {
    auto part = f::part();
    auto base =
        f::checked(Sketch::create(SketchId("base.sketch"), "Base", DatumPlaneId("datum.xy")));
    REQUIRE(base.add_profile(f::checked(RectangleProfile::create(
        ProfileId("base.profile"), ParameterId("width"), ParameterId("height"), {4, 3}))));
    REQUIRE(part.update_sketch(base));
    import_base(part, imported);
    std::array<Point3, ResultEdgeReference::sample_count> samples;
    for (std::size_t i = 0; i < samples.size(); ++i)
      samples[i] = {0, 0, 5 * double(i) / double(samples.size() - 1)};
    const auto producer = f::checked(EdgeReference::create_result(
        PartFeatureInputRole::PathSegment,
        f::checked(ResultEdgeReference::create(FeatureId("base.feature"), 0, 5, samples))));
    std::vector<PathSegmentReference> route{
        f::checked(PathSegmentReference::create_feature_edge(producer, {}, backwards))};
    REQUIRE(part.add_construction_plane(f::checked(ConstructionPlane::create_explicit(
        ConstructionPlaneId("bend.plane"), "Bend", {0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}))));
    auto bend =
        f::checked(Sketch::create(SketchId("bend"), "Exact bend", DatumPlaneId("bend.plane")));
    const double c = std::sqrt(.5), w = (1 + 2 * c) / 3, h = 40 * c / (1 + 2 * c);
    REQUIRE(bend.add_entity(f::checked(SplineSegment::create_cubic_bezier(
        SketchEntityId("arc"), {0, 5}, {0, 5 + h}, {20 - h, 25}, {20, 25}, {1, w, w, 1}))));
    REQUIRE(part.add_sketch(bend));
    route.push_back(f::checked(PathSegmentReference::create_planar(
        bend.id(), SketchEntityId("arc"), PlanarPathCurveKind::Spline, backwards)));
    auto tail = f::checked(Sketch3D::create(Sketch3DId("tail"), "Nonuniform tail"));
    for (auto [id, x] : {std::pair{"a", 20.}, {"b", 21.}, {"c", 24.}, {"d", 25.}})
      REQUIRE(tail.add_point(f::point(id, {x, 0, 25})));
    REQUIRE(tail.add_spline(f::checked(SketchSpline3D::create(
        SketchEntityId("spline"), SketchSpline3DRepresentation::ControlPoints,
        {f::local("a"), f::local("b"), f::local("c"), f::local("d")}, 3,
        SketchSpline3DContinuity::Curvature))));
    REQUIRE(part.add_sketch_3d(tail));
    route.push_back(f::checked(PathSegmentReference::create_sketch_3d(
        tail.id(), SketchEntityId("spline"), Sketch3DPathCurveKind::Spline, backwards)));
    const auto& catalog = part.imported_topology_catalogs().front();
    const auto entry =
        std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& e) {
          return e.descriptor.kind == ImportedTopologyKind::Edge &&
                 std::abs(e.descriptor.measure - 5) < 1e-6 &&
                 distance(e.descriptor.centroid, {27.5, 0, 25}) < 1e-6;
        });
    REQUIRE(entry != catalog.entries().end());
    const auto reference = f::checked(ImportedTopologyReference::create(
        catalog.id(), entry->id, ImportedTopologyCapability::Edge));
    const auto edge =
        f::checked(EdgeReference::create_imported(PartFeatureInputRole::PathSegment, reference));
    route.push_back(f::checked(PathSegmentReference::create_feature_edge(
        edge, part.imported_body_features().front().definition().outputs().front().body,
        backwards)));
    if (backwards)
      std::reverse(route.begin(), route.end());
    REQUIRE(part.add_path_curve(f::checked(
        PathCurve::create(PathCurveId("route"), "Native mixed route", route, PathClosure::Open,
                          PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                          f::checked(PathExecutionPolicy::curve_preserving(1e-5))))));
    if (backwards)
      circle_section(part, {30, 0, 25}, {0, 1, 0}, {0, 0, -1}, {-1, 0, 0});
    else
      circle_section(part, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1});
    auto cache = f::checked(ShapeCache::create(ShapeCacheId("mixed.acceptance")));
    StepImportTopologyContext context(supplier.root);
    GeometryRecomputeExecutor executor(&context);
    const auto run = executor.execute_document(part, cache);
    INFO((run ? "Computed" : run.error().message()));
    REQUIRE(run);
    const auto path = f::checked(executor.resolve_path_segments(part, PathCurveId("route"), cache));
    const double length = 15 + 10 * std::numbers::pi;
    CHECK(f::checked(SweepAdapter{}.measure_path_length(PathCurveId("route"), path)) ==
          Catch::Approx(length).margin(1e-7));
    const auto* shape = cache.find_body_shape(BodyId("pipe"));
    REQUIRE(shape);
    const double volume = .48 * std::numbers::pi * length;
    CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 ==
          Catch::Approx(volume).margin(2e-5));
    const double angle = std::numbers::pi / 4;
    const std::array<Point3, 4> centers{{{0, 0, 2.5},
                                         {20 * (1 - std::cos(angle)), 0, 5 + 20 * std::sin(angle)},
                                         {22.5, 0, 25},
                                         {27.5, 0, 25}}};
    for (const auto center : centers) {
      CHECK_FALSE(material_at(*shape, center));
      CHECK(material_at(*shape, {center.x, center.y + .6, center.z}));
      CHECK(blcad::test::distance_to_faces(*shape, {center.x, center.y + .4, center.z}) < 2e-5);
      CHECK(blcad::test::distance_to_faces(*shape, {center.x, center.y + .8, center.z}) < 2e-5);
    }
    const auto bytes = f::checked(serialize_part_document_to_json(part));
    auto restored = f::checked(deserialize_part_document_from_json(bytes));
    auto fresh = f::checked(ShapeCache::create(ShapeCacheId("mixed.fresh")));
    StepImportTopologyContext reopened(supplier.root);
    REQUIRE(GeometryRecomputeExecutor{&reopened}.execute_document(restored, fresh));
    CHECK(bool(f::checked(serialize_part_document_to_json(restored)) == bytes));
    REQUIRE(fresh.find_body_shape(BodyId("pipe")));
    CHECK(
        RectangleExtrusionAdapter{}.summarize(*fresh.find_body_shape(BodyId("pipe"))).volume_mm3 ==
        Catch::Approx(volume).margin(2e-5));
  }
}
TEST_CASE("G01 placed imported circular result drives a hollow Sweep and "
          "physical STEP roundtrip",
          "[geometry][parity-b167][semantic-sweep-acceptance]") {
  imported_body_test::Fixture fixture;
  fixture.write(BRepPrimAPI_MakeCylinder(20, 5).Shape());
  auto part = fixture.editable();
  const auto imported_body =
      part.imported_body_features().front().definition().outputs().front().body;
  REQUIRE(part.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("move"), imported_body, BodyTransformCoordinateSpace::World, {}, {30, -5, 4},
      false, false))));
  REQUIRE(part.add_body_transform(f::checked(BodyTransform::create_rotate(
      BodyTransformId("turn"), imported_body, BodyTransformCoordinateSpace::World, {},
      f::checked(BodyTransformRotationAxis::create_explicit({}, {0, 0, 1})), 90, false, false))));
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("placed.path")));
  StepImportTopologyContext context(fixture.root);
  GeometryRecomputeExecutor executor(&context);
  REQUIRE(executor.execute_document(part, cache));
  const auto* posed = cache.find_feature_shape(FeatureId("turn"));
  REQUIRE(posed);
  std::optional<ResultEdgeReference> witness;
  TopTools_IndexedMapOfShape edges;
  TopExp::MapShapes(*OcctShapeView::get(*posed), TopAbs_EDGE, edges);
  for (int index = 1; index <= edges.Extent(); ++index) {
    const auto native = TopoDS::Edge(edges(index));
    BRepAdaptor_Curve curve(native);
    if (curve.GetType() == GeomAbs_Circle && std::abs(curve.Circle().Location().Z() - 4) < 1e-6) {
      REQUIRE_FALSE(witness);
      witness = f::checked(make_result_edge_reference(FeatureId("turn"), native));
    }
  }
  REQUIRE(witness);
  const auto edge =
      f::checked(EdgeReference::create_result(PartFeatureInputRole::PathSegment, *witness));
  REQUIRE(part.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("route"), "Placed circular rim",
                        {f::checked(PathSegmentReference::create_feature_edge(edge))},
                        PathClosure::Closed, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-5))))));
  REQUIRE(executor.execute_document(part, cache));
  const auto path = f::checked(executor.resolve_path_segments(part, PathCurveId("route"), cache));
  CHECK(f::checked(SweepAdapter{}.measure_path_length(PathCurveId("route"), path)) ==
        Catch::Approx(40 * std::numbers::pi).margin(1e-7));
  const auto start = path.front().start;
  CHECK(std::hypot(start.x - 5, start.y - 30) == Catch::Approx(20).margin(1e-7));
  CHECK(start.z == Catch::Approx(4).margin(1e-7));
  const auto radial = normalized(Vector3{start.x - 5, start.y - 30, 0});
  circle_section(part, start, radial, {0, 0, 1}, {radial.y, -radial.x, 0});
  REQUIRE(part.set_body_visibility(imported_body, BodyVisibility::Hidden));
  REQUIRE(executor.execute_document(part, cache));
  const auto* pipe = cache.find_body_shape(BodyId("pipe"));
  REQUIRE(pipe);
  const double expected = 19.2 * std::numbers::pi * std::numbers::pi;
  CHECK(RectangleExtrusionAdapter{}.summarize(*pipe).volume_mm3 ==
        Catch::Approx(expected).margin(2e-5));
  CHECK_FALSE(material_at(*pipe, {25, 30, 4}));
  CHECK(material_at(*pipe, {25.6, 30, 4}));
  CHECK(blcad::test::distance_to_faces(*pipe, {25.4, 30, 4}) < 2e-5);
  const auto bytes = f::checked(serialize_part_document_to_json(part));
  auto reopened = f::checked(deserialize_part_document_from_json(bytes));
  auto fresh = f::checked(ShapeCache::create(ShapeCacheId("placed.fresh")));
  StepImportTopologyContext restored(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&restored}.execute_document(reopened, fresh));
  CHECK(f::checked(serialize_part_document_to_json(reopened)) == bytes);
  REQUIRE(fresh.find_body_shape(BodyId("pipe")));
  CHECK(RectangleExtrusionAdapter{}.summarize(*fresh.find_body_shape(BodyId("pipe"))).volume_mm3 ==
        Catch::Approx(expected).margin(2e-5));
  const auto output = fixture.root / "pipe-export.step";
  const auto exported =
      f::checked(StepExporter{&context}.write_part_step(part, cache, output.string()));
  CHECK(exported.exported_body_count == 1);
  const auto source = f::checked(
      StepImportSource::create(StepImportSourceId("roundtrip"), "Roundtrip", "pipe-export.step",
                               f::checked(step_import_sha256(imported_body_test::bytes(output))),
                               StepImportMode::Reference));
  const auto imported = f::checked(StepImportReader{}.read(source, fixture.root));
  const auto shapes = f::checked(StepImportReader{}.preview_shapes(imported));
  REQUIRE(shapes.size() == 1);
  CHECK(RectangleExtrusionAdapter{}.summarize(shapes.front()).volume_mm3 ==
        Catch::Approx(expected).margin(2e-5));
  CHECK_FALSE(material_at(shapes.front(), {25, 30, 4}));
  CHECK(material_at(shapes.front(), {25.6, 30, 4}));
}
TEST_CASE("G01 identical native copies reject an ambiguous result witness "
          "without first-match recovery",
          "[geometry][parity-b167][semantic-sweep-acceptance]") {
  const auto part = f::part();
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("ambiguous.path")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto* shape = cache.find_feature_shape(FeatureId("base.feature"));
  REQUIRE(shape);
  const auto* native = OcctShapeView::get(*shape);
  REQUIRE(native);
  TopExp_Explorer edges(*native, TopAbs_EDGE);
  REQUIRE(edges.More());
  const auto witness = f::checked(
      make_result_edge_reference(FeatureId("base.feature"), TopoDS::Edge(edges.Current())));
  const auto reference =
      f::checked(EdgeReference::create_result(PartFeatureInputRole::PathSegment, witness));
  const auto duplicate =
      imported_body_test::compound({BRepBuilderAPI_Copy(*native, true, false).Shape(),
                                    BRepBuilderAPI_Copy(*native, true, false).Shape()});
  const auto result =
      resolve_semantic_edge(part, cache, duplicate, reference, FeatureId("path.proof"), "path");
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("ambiguous") != std::string::npos);
  CHECK(cache.validate_complete_geometry_result(part));
}

TEST_CASE("Periodic STEP conversion rejects a void shell outside the regular torus material",
          "[geometry][parity-b167][semantic-sweep-acceptance]") {
  imported_body_test::Fixture fixture;
  const auto outer = BRepPrimAPI_MakeTorus(20, .8).Shape();
  const auto outside = BRepPrimAPI_MakeTorus(25, .4).Shape();
  TopExp_Explorer outer_shell(outer, TopAbs_SHELL), void_shell(outside, TopAbs_SHELL);
  REQUIRE(outer_shell.More());
  REQUIRE(void_shell.More());
  TopoDS_Solid invalid;
  BRep_Builder builder;
  builder.MakeSolid(invalid);
  builder.Add(invalid, outer_shell.Current());
  builder.Add(invalid, void_shell.Current().Reversed());
  fixture.write(invalid);
  const auto source = f::checked(StepImportSource::create(
      StepImportSourceId("invalid.torus"), "Invalid torus void", "supplier.step",
      f::checked(step_import_sha256(fixture.source_bytes())), StepImportMode::Reference));
  CHECK_FALSE(StepImportReader{}.read(source, fixture.root));
}
