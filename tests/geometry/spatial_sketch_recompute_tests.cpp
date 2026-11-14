#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/spatial_constraint_geometry.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("spatial.recompute")));
}
std::string text(const PartDocument& part) {
  return f::checked(serialize_part_document_to_json(part));
}
double volume(const ShapeCache& cache) {
  const auto* shape = cache.find_body_shape(BodyId("driven.body"));
  REQUIRE(shape);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
} // namespace
TEST_CASE(
    "Spatial dimensions drive actual native path Sweep and scene without rewriting source seeds",
    "[geometry][parity-b165][spatial-derived-recompute]") {
  auto part = sf::straight_sweep();
  auto result = cache();
  {
    const auto execution = GeometryRecomputeExecutor{}.execute_document(part, result);
    INFO((execution ? "Executed"
                    : execution.error().object_id() + ": " + execution.error().message()));
    REQUIRE(execution);
  }
  CHECK(volume(result) == Catch::Approx(.8 * .8 * 10).margin(1e-6));
  REQUIRE(
      part.set_parameter_value(ParameterId("spatial.length"), f::checked(Quantity::length_mm(15))));
  const auto before = text(part);
  {
    const auto execution = GeometryRecomputeExecutor{}.execute_document(part, result);
    INFO((execution ? "Executed"
                    : execution.error().object_id() + ": " + execution.error().message()));
    REQUIRE(execution);
  }
  CHECK(text(part) == before);
  CHECK(part.find_sketch_3d(Sketch3DId("driven"))
            ->find_point(SketchEntityId("b"))
            ->z()
            .explicit_coordinate()
            ->millimeters() == 10);
  CHECK(volume(result) == Catch::Approx(.8 * .8 * 15).margin(1e-6));
  auto fresh = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, fresh));
  CHECK(volume(fresh) == Catch::Approx(volume(result)).margin(1e-9));
  const auto path = f::checked(
      GeometryRecomputeExecutor{}.resolve_path_segments(part, PathCurveId("driven.path"), result));
  REQUIRE(path.size() == 1);
  CHECK(path[0].end.z == Catch::Approx(15).margin(1e-7));
  const auto scene = f::checked(ViewportSceneBuilder{}.build_part(part, result));
  bool found = false;
  for (const auto& item : scene)
    if (item.semantic_id == "sketch3d/driven/entity/line") {
      const auto* shape = OcctShapeView::get(item.shape);
      REQUIRE(shape);
      TopExp_Explorer edge(*shape, TopAbs_EDGE);
      REQUIRE(edge.More());
      BRepAdaptor_Curve curve(TopoDS::Edge(edge.Current()));
      CHECK(curve.Value(curve.LastParameter()).Z() == Catch::Approx(15).margin(1e-7));
      found = true;
    }
  REQUIRE(found);
  CHECK(text(part) == before);
}

TEST_CASE(
    "Solved mixed-path connectivity is checked atomically instead of comparing authored seeds",
    "[geometry][parity-b165][spatial-derived-path]") {
  auto part = sf::straight_sweep();
  REQUIRE(
      part.set_parameter_value(ParameterId("spatial.length"), f::checked(Quantity::length_mm(15))));
  auto extension = f::checked(Sketch3D::create(Sketch3DId("extension"), "Extension"));
  REQUIRE(extension.add_point(f::point("start", {0, 0, 15})));
  REQUIRE(extension.add_point(f::point("end", {0, 0, 20})));
  REQUIRE(extension.add_line(f::checked(SketchLine3D::create(
      SketchEntityId("line"), SketchEntityId("start"), SketchEntityId("end")))));
  REQUIRE(part.add_sketch_3d(extension));
  REQUIRE(part.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("mixed"), "Mixed",
      {f::checked(PathSegmentReference::create_sketch_3d(
           Sketch3DId("driven"), SketchEntityId("line"), Sketch3DPathCurveKind::Line)),
       f::checked(PathSegmentReference::create_sketch_3d(extension.id(), SketchEntityId("line"),
                                                         Sketch3DPathCurveKind::Line))},
      PathClosure::Open, PathOrientationRule::MinimumTwist))));
  auto result = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, result));
  const auto old = result;
  const auto segments = f::checked(
      GeometryRecomputeExecutor{}.resolve_path_segments(part, PathCurveId("mixed"), result));
  REQUIRE(segments.size() == 2);
  CHECK(distance(segments[0].end, segments[1].start) < 1e-7);
  REQUIRE(extension.replace_point(f::point("start", {0, 0, 16})));
  REQUIRE(part.update_sketch_3d(extension));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, result));
  CHECK(volume(result) == Catch::Approx(volume(old)).margin(1e-12));
  REQUIRE(result.find_spatial_sketch_solution(Sketch3DId("driven")));
  CHECK(result.find_spatial_sketch_solution(Sketch3DId("driven"))
            ->find_point(SketchEntityId("b"))
            ->z()
            .explicit_coordinate()
            ->millimeters() == Catch::Approx(15).margin(1e-7));
}
TEST_CASE(
    "Spatial dependent owners resolve current constrained external curves and exact cache scope",
    "[geometry][parity-b165][spatial-derived-sources]") {
  auto part = sf::straight_sweep();
  REQUIRE(
      part.set_parameter_value(ParameterId("spatial.length"), f::checked(Quantity::length_mm(15))));
  auto owner = f::checked(Sketch3D::create(Sketch3DId("dependent"), "Dependent"));
  REQUIRE(owner.add_point(f::point("p", {2, 1, 5})));
  const auto curve = f::checked(
      SpatialConstraintTarget::external_curve(f::checked(PathSegmentReference::create_sketch_3d(
          Sketch3DId("driven"), SketchEntityId("line"), Sketch3DPathCurveKind::Line))));
  REQUIRE(owner.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("on"), SpatialConstraintKind::PointOnCurve, {f::target("p"), curve}))));
  REQUIRE(owner.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("fixed"), SpatialConstraintKind::Fixed,
                                {f::target("p")}, {}, Point3{0, 0, 12}))));
  REQUIRE(part.add_sketch_3d(owner));
  auto result = cache();
  {
    const auto execution = GeometryRecomputeExecutor{}.execute_document(part, result);
    INFO((execution ? "Executed"
                    : execution.error().object_id() + ": " + execution.error().message()));
    REQUIRE(execution);
  }
  const auto before = text(part);
  auto resolved = resolve_spatial_sketch_geometry(part, owner.id(), result);
  INFO((resolved ? "Resolved" : resolved.error().message()));
  REQUIRE(resolved);
  CHECK(
      resolved.value().find_point(SketchEntityId("p"))->z().explicit_coordinate()->millimeters() ==
      Catch::Approx(12).margin(1e-7));
  CHECK_FALSE(resolve_spatial_sketch_geometry(part, owner.id(), result, nullptr,
                                              SpatialHistoryPrefixScope{0}));
  auto prefix = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(part, 0, prefix));
  CHECK_FALSE(resolve_spatial_sketch_geometry(part, owner.id(), prefix));
  REQUIRE(resolve_spatial_sketch_geometry(part, owner.id(), prefix, nullptr,
                                          SpatialHistoryPrefixScope{0}));
  CHECK(text(part) == before);
  REQUIRE(
      part.set_parameter_value(ParameterId("spatial.length"), f::checked(Quantity::length_mm(16))));
  CHECK_FALSE(resolve_spatial_sketch_geometry(part, owner.id(), result));
}

TEST_CASE("Spatial source geometry stays before its first consumer when a Join covers its support",
          "[geometry][parity-b165][spatial-derived-stage]") {
  auto part = f::part();
  REQUIRE(part.add_parameter(f::checked(
      Parameter::create_length(ParameterId("side"), "Side", f::checked(Quantity::length_mm(.8))))));
  REQUIRE(part.add_derived_workplane(f::checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("contact.plane"), "Contact",
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))))));
  auto profile = f::checked(
      Sketch::create(SketchId("contact.profile"), "Contact", DatumPlaneId("contact.plane")));
  REQUIRE(profile.add_profile(f::checked(RectangleProfile::create(
      ProfileId("square"), ParameterId("side"), ParameterId("side"), Point2{1, 1}))));
  REQUIRE(part.add_sketch(profile));
  auto owner = f::checked(Sketch3D::create(Sketch3DId("contact"), "Contact path"));
  REQUIRE(owner.add_point(f::point("start", {1, 1, 5})));
  REQUIRE(owner.add_point(f::point("end", {1, 1, 10})));
  REQUIRE(owner.add_line(f::checked(SketchLine3D::create(
      SketchEntityId("line"), SketchEntityId("start"), SketchEntityId("end")))));
  REQUIRE(owner.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("on.face"), SpatialConstraintKind::PointOnFace,
                                {f::target("start"), f::face()}))));
  REQUIRE(owner.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("fixed"), SpatialConstraintKind::Fixed,
                                {f::target("end")}, {}, Point3{1, 1, 10}))));
  REQUIRE(part.add_sketch_3d(owner));
  REQUIRE(part.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("contact.path"), "Contact",
                        {f::checked(PathSegmentReference::create_sketch_3d(
                            owner.id(), SketchEntityId("line"), Sketch3DPathCurveKind::Line))},
                        PathClosure::Open, PathOrientationRule::MinimumTwist))));
  REQUIRE(part.add_sweep_feature(f::checked(SweepFeature::create_sweep(
      FeatureId("contact.join"), "Join",
      f::checked(
          SweepProfileReference::create_closed_region(f::checked(ProfileRegionReference::create(
              profile.id(), ProfileId("square"), PartFeatureInputRole::SweepProfile)))),
      PathCurveId("contact.path"),
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::Join,
                                                  BodyId("base.body"), {}))))));
  const auto before = text(part);
  auto result = cache();
  const auto executed = GeometryRecomputeExecutor{}.execute_document(part, result);
  INFO((executed ? "Executed" : executed.error().object_id() + ": " + executed.error().message()));
  REQUIRE(executed);
  const auto* body = result.find_body_shape(BodyId("base.body"));
  REQUIRE(body);
  CHECK(RectangleExtrusionAdapter{}.summarize(*body).volume_mm3 ==
        Catch::Approx(240 + .8 * .8 * 5).margin(1e-6));
  const auto* solved = result.find_spatial_sketch_solution(owner.id());
  REQUIRE(solved);
  CHECK(solved->find_point(SketchEntityId("start"))->z().explicit_coordinate()->millimeters() == 5);
  // The pillar covers the original support's interior. Re-solving against the final Body would
  // introduce feedback; the scene consumes the same pre-Join numeric product as the feature.
  CHECK_FALSE(SpatialConstraintGeometry{}
                  .queries(part, result)
                  .project_face(std::get<FaceReference>(f::face().source()), {1, 1, 5}));
  REQUIRE(ViewportSceneBuilder{}.build_part(part, result));
  CHECK(text(part) == before);
}
