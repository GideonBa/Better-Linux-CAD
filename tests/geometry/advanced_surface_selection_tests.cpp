#include "../support/curved_surface_fixture.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::curved_surface;
TEST_CASE("Advanced Surface selectors expose typed owned faces and exact native boundary paths",
          "[geometry][advanced-surface-selection][parity-b173]") {
  auto p = f::combination();
  auto cache = blcad::test::surface_offset::cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  const BodyId owner("quilt.body");
  for (const auto role :
       {PartFeatureInputRole::SurfaceSourceFace, PartFeatureInputRole::LoftBoundarySupport}) {
    const auto targets = f::checked(FinishingSelectionBuilder::build(p, cache, owner, role));
    REQUIRE(targets.size() == 3);
    std::set<std::string> identities;
    for (const auto& target : targets) {
      const auto& face = std::get<FaceReference>(target.reference);
      CHECK(face.role() == role);
      CHECK(identities.insert(target.semantic_id).second);
      CHECK(face.source_kind() == PartFeatureInputSourceKind::ResultFace);
      if (role == PartFeatureInputRole::SurfaceSourceFace) {
        const auto selected = f::checked(SurfaceReference::create_faces(owner, {face}));
        CHECK(SurfaceSelectionGeometry::resolve(p, cache, selected, FeatureId("selection"), 1e-6));
      }
    }
  }
  const auto edges = f::checked(
      FinishingSelectionBuilder::build(p, cache, owner, PartFeatureInputRole::PathSegment));
  REQUIRE(!edges.empty());
  bool curved = false;
  for (std::size_t i = 0; i < edges.size(); ++i) {
    const auto& edge = std::get<EdgeReference>(edges[i].reference);
    CHECK(edge.role() == PartFeatureInputRole::PathSegment);
    const auto id = PathCurveId("selected." + std::to_string(i));
    REQUIRE(p.add_path_curve(f::checked(PathCurve::create(
        id, "Selected", {f::checked(PathSegmentReference::create_feature_edge(edge))},
        PathClosure::Open, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
        f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
  }
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  for (std::size_t i = 0; i < edges.size(); ++i) {
    INFO("Selected native edge " << i);
    const auto id = PathCurveId("selected." + std::to_string(i));
    const auto segments =
        f::checked(GeometryRecomputeExecutor{}.resolve_path_segments(p, id, cache));
    REQUIRE(segments.size() == 1);
    curved |= segments.front().kind == SweepPathSegmentKind::DerivedNative;
  }
  CHECK(curved);
  CHECK_FALSE(FinishingSelectionBuilder::build(p, cache, owner, PartFeatureInputRole::FilletEdge));
  REQUIRE(p.set_body_visibility(owner, BodyVisibility::Hidden));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  CHECK_FALSE(
      FinishingSelectionBuilder::build(p, cache, owner, PartFeatureInputRole::SurfaceSourceFace));
  CHECK(FinishingSelectionBuilder::build(p, cache, owner, PartFeatureInputRole::SurfaceSourceFace,
                                         nullptr, {},
                                         FinishingSelectionVisibility::IncludeHiddenInputs));
}
TEST_CASE("Viewport native paths on placed imported Bodies bind their current result witness",
          "[geometry][advanced-surface-selection][parity-b173]") {
  imported_body_test::Fixture files;
  files.write(BRepPrimAPI_MakeCylinder(10, 20).Shape());
  auto p = files.editable();
  const BodyId owner("body.1");
  REQUIRE(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("move"), owner, BodyTransformCoordinateSpace::World, {}, {2, 3, 4}, false,
      false))));
  auto cache = blcad::test::surface_offset::cache();
  StepImportTopologyContext imports(files.root);
  REQUIRE(GeometryRecomputeExecutor{&imports}.execute_document(p, cache));
  const auto targets = f::checked(FinishingSelectionBuilder::build(
      p, cache, owner, PartFeatureInputRole::PathSegment, &imports));
  bool found = false;
  for (const auto& target : targets) {
    const auto& edge = std::get<EdgeReference>(target.reference);
    CHECK(edge.source_kind() == PartFeatureInputSourceKind::ResultEdge);
    BRepAdaptor_Curve curve(TopoDS::Edge(*OcctShapeView::get(target.shape)));
    if (curve.GetType() != GeomAbs_Circle || std::abs(curve.Circle().Location().Z() - 24) > 1e-6)
      continue;
    found = true;
    REQUIRE(p.add_path_curve(f::checked(
        PathCurve::create(PathCurveId("placed.rim"), "Placed rim",
                          {f::checked(PathSegmentReference::create_feature_edge(edge))},
                          PathClosure::Closed, PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                          f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
    REQUIRE(GeometryRecomputeExecutor{&imports}.execute_document(p, cache));
    const auto resolved = f::checked(GeometryRecomputeExecutor{&imports}.resolve_path_segments(
        p, PathCurveId("placed.rim"), cache));
    REQUIRE(resolved.size() == 1);
    CHECK(resolved.front().start.z == Catch::Approx(24));
  }
  CHECK(found);
}
