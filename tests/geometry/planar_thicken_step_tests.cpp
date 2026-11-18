#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/surface_offset_adapter.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepTools.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <sstream>
#include <utility>
#include <vector>
using namespace blcad;
using namespace blcad::geometry;
namespace pf = blcad::test::spatial_fixture;
namespace {
std::string planar_source_bytes(const GeometryShape& shape) {
  std::ostringstream bytes;
  BRepTools::Write(*OcctShapeView::get(shape), bytes);
  return bytes.str();
}
} // namespace
TEST_CASE("Planar Thicken preserves circular trim through strict STEP in every side direction",
          "[geometry][surface-offset-geometry][surface-offset-planar-step][parity-b188]") {
  auto p = pf::part();
  REQUIRE(p.add_parameter(pf::checked(Parameter::create_length(
      ParameterId("hole.diameter"), "Hole diameter", pf::checked(Quantity::length_mm(.2))))));
  auto sketch =
      pf::checked(Sketch::create(SketchId("bore.sketch"), "Bore", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      pf::checked(CircleProfile::create(ProfileId("bore"), ParameterId("hole.diameter")))));
  REQUIRE(p.add_sketch(sketch));
  auto cut = pf::checked(Feature::create_subtractive_extrude(FeatureId("bore"), "Bore", sketch.id(),
                                                             FeatureId("base.feature")));
  REQUIRE(p.add_feature(
      pf::checked(cut.with_body_result_context(pf::checked(FeatureBodyResultContext::create(
          FeatureBodyOperationMode::Cut, BodyId("base.body"), std::nullopt))))));
  auto cache = pf::checked(ShapeCache::create(ShapeCacheId("planar.step")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, cache));
  for (const bool underside : {false, true}) {
    auto reference = pf::checked(SurfaceReference::create_faces(
        BodyId("base.body"),
        {pf::checked(FaceReference::create_result(
            PartFeatureInputRole::SurfaceSourceFace,
            pf::checked(ResultFaceReference::create(
                FeatureId("bore"), BodyId("base.body"),
                {{FeatureId("base.feature"),
                  underside ? "extrude/lower-cap" : "extrude/upper-cap"}}))))}));
    const auto source = pf::checked(
        SurfaceSelectionGeometry::resolve(p, cache, reference, FeatureId("select"), 1e-6));
    const auto source_bytes = planar_source_bytes(source);
    const auto offset = pf::checked(SurfaceOffsetAdapter{}.construct(
        FeatureId("offset"), source, SurfaceFeatureKind::OffsetSurface, 1,
        SurfaceOffsetSide::AlongNormal, 1e-6, SurfaceOffsetApproximation::ExactAnalytic));
    const auto offset_bytes = planar_source_bytes(offset.shape);
    const double station = underside ? -1 : 6, direction = underside ? -1 : 1;
    for (const auto side : {SurfaceOffsetSide::AlongNormal, SurfaceOffsetSide::OppositeNormal,
                            SurfaceOffsetSide::Symmetric}) {
      CAPTURE(underside, side);
      const auto result = pf::checked(SurfaceOffsetAdapter{}.construct(
          FeatureId("thick"), offset.shape, SurfaceFeatureKind::Thicken, .5, side, 1e-6,
          SurfaceOffsetApproximation::ExactAnalytic));
      const auto physical = pf::checked(inspect_shape_physical_properties(result.shape));
      REQUIRE(physical.volume);
      CHECK(physical.solid_count == 1);
      CHECK(physical.volume->volume_mm3 ==
            Catch::Approx((48 - std::numbers::pi * .01) * .5).margin(1e-7));
      CHECK(physical.volume->centroid_mm.x == Catch::Approx(0).margin(1e-8));
      CHECK(physical.volume->centroid_mm.y == Catch::Approx(0).margin(1e-8));
      const double center =
          station +
          (side == SurfaceOffsetSide::Symmetric
               ? 0
               : (side == SurfaceOffsetSide::AlongNormal ? direction * .25 : -direction * .25));
      CHECK(physical.volume->centroid_mm.z == Catch::Approx(center).margin(1e-8));
      CHECK(result.verification.criterion == SurfaceDistanceCriterion::WallThickness);
      CHECK(result.verification.minimum_mm == Catch::Approx(.5).margin(1e-7));
      CHECK(result.verification.maximum_mm == Catch::Approx(.5).margin(1e-7));
      CHECK(result.verification.maximum_error_mm <= result.verification.permitted_error_mm);
      CHECK(result.verification.stations >= 300);
      CHECK(BRepAlgoAPI_Check(*OcctShapeView::get(result.shape), false, true).IsValid());
      int cylinders = 0;
      for (TopExp_Explorer face(*OcctShapeView::get(result.shape), TopAbs_FACE); face.More();
           face.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(face.Current()));
        if (surface.GetType() == GeomAbs_Cylinder) {
          ++cylinders;
          CHECK(surface.Cylinder().Radius() == Catch::Approx(.1).margin(1e-9));
        }
      }
      CHECK(cylinders >= 1);
      imported_body_test::Fixture files;
      const auto path = files.root / "planar-thick.step";
      REQUIRE(StepExporter{}.write_step(result.shape, path.string()));
      const auto step = pf::checked(StepImportSource::create(
          StepImportSourceId("roundtrip"), "Roundtrip", path.filename().string(),
          pf::checked(step_import_sha256(imported_body_test::bytes(path))),
          StepImportMode::EditableBody));
      const auto read = pf::checked(StepImportReader{}.read(step, files.root));
      REQUIRE(read.body_count() == 1);
      for (const auto& product : read.products)
        for (const auto& body : product.bodies) {
          const auto reimported = pf::checked(inspect_shape_physical_properties(body.shape));
          REQUIRE(reimported.volume);
          CHECK(reimported.volume->volume_mm3 ==
                Catch::Approx(physical.volume->volume_mm3).margin(1e-7));
          CHECK(reimported.volume->centroid_mm.z == Catch::Approx(center).margin(1e-8));
        }
      CHECK(planar_source_bytes(source) == source_bytes);
      CHECK(planar_source_bytes(offset.shape) == offset_bytes);
    }
  }
}

namespace {
void annulus_length(PartDocument& p, const char* id, double value) {
  (void)pf::checked(p.add_parameter(pf::checked(
      Parameter::create_length(ParameterId(id), id, pf::checked(Quantity::length_mm(value))))));
}
PartDocument annulus_document(int placement) {
  auto p = pf::part();
  annulus_length(p, "annulus.outer", 10);
  annulus_length(p, "annulus.inner", 9.4);
  annulus_length(p, "annulus.thickness", .1);
  annulus_length(p, "annulus.tolerance", 1e-6);
  auto outer = pf::checked(
      Sketch::create(SketchId("base.sketch"), "Annular stock", DatumPlaneId("datum.xy")));
  (void)pf::checked(outer.add_profile(
      pf::checked(CircleProfile::create(ProfileId("base.profile"), ParameterId("annulus.outer")))));
  (void)pf::checked(p.update_sketch(outer));
  auto inner = pf::checked(
      Sketch::create(SketchId("annulus.bore.sketch"), "Annular bore", DatumPlaneId("datum.xy")));
  (void)pf::checked(inner.add_profile(
      pf::checked(CircleProfile::create(ProfileId("annulus.bore"), ParameterId("annulus.inner")))));
  (void)pf::checked(p.add_sketch(inner));
  auto bore = pf::checked(Feature::create_subtractive_extrude(
      FeatureId("annulus.bore"), "Annular bore", inner.id(), FeatureId("base.feature")));
  bore = pf::checked(bore.with_body_result_context(pf::checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("base.body"), {}))));
  (void)pf::checked(p.add_feature(bore));
  BodyId owner("base.body");
  if (placement == 2) {
    owner = BodyId("annulus.reflected");
    (void)pf::checked(
        p.add_body(pf::checked(Body::create(owner, "Reflected annulus", BodyKind::Solid))));
    (void)pf::checked(p.add_mirror_feature(pf::checked(MirrorFeature::create(
        FeatureId("annulus.reflect"), "Reflected annulus",
        {pf::checked(MirrorSourceReference::body(BodyId("base.body")))},
        pf::checked(PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                                       DatumPlaneId("datum.xy"))),
        pf::checked(
            FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {}, owner))))));
  }
  if (placement != 0)
    (void)pf::checked(p.add_body_transform(pf::checked(BodyTransform::create_rotate(
        BodyTransformId("annulus.tilt"), owner, BodyTransformCoordinateSpace::World, {},
        pf::checked(BodyTransformRotationAxis::create_explicit({}, {0, 1, 0})), 30, false,
        false))));
  return p;
}
void point_matches(Point3 actual, Point3 expected) {
  CHECK(actual.x == Catch::Approx(expected.x).epsilon(0).margin(1e-7));
  CHECK(actual.y == Catch::Approx(expected.y).epsilon(0).margin(1e-7));
  CHECK(actual.z == Catch::Approx(expected.z).epsilon(0).margin(1e-7));
}
void annulus_inertia(const std::array<double, 9>& actual, Vector3 normal) {
  constexpr double height = .1, sum_radii_squared = 25 + 4.7 * 4.7;
  constexpr double mass = std::numbers::pi * (25 - 4.7 * 4.7) * height;
  constexpr double transverse = mass * (3 * sum_radii_squared + height * height) / 12;
  constexpr double axial = mass * sum_radii_squared / 2;
  const std::array<double, 3> axis{normal.x, normal.y, normal.z};
  // I = I_transverse*identity + (I_axial-I_transverse)*n*n^T.
  // This independent expression rotates the analytic annular tensor and also
  // remains valid after an orientation-reversing reflection.
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j) {
      CAPTURE(i, j);
      const double expected = (i == j ? transverse : 0) + (axial - transverse) * axis[i] * axis[j];
      CHECK(actual[3 * i + j] == Catch::Approx(expected).epsilon(1e-8).margin(1e-7));
    }
}
Point3 skin_center(Point3 source, Vector3 normal, double distance) {
  return {source.x + normal.x * distance, source.y + normal.y * distance,
          source.z + normal.z * distance};
}
std::string annulus_segment(const std::string& value) {
  return std::to_string(value.size()) + ":" + value + "/";
}
} // namespace
TEST_CASE("Planar annular Thicken retains placed oriented normals and authored "
          "skin lineage",
          "[geometry][surface-offset-planar-step][surface-offset-geometry]["
          "parity-b188]") {
  // The R5/R4.7 annulus contains no interior point in the transient 5x5
  // Surface direction grid. Exact planar construction must not depend on it.
  constexpr double area = std::numbers::pi * (25 - 4.7 * 4.7);
  for (int placement : {0, 1, 2}) {
    CAPTURE(placement);
    const auto primary = annulus_document(placement);
    const BodyId owner(placement == 2 ? "annulus.reflected" : "base.body");
    auto baseline_cache = pf::checked(ShapeCache::create(ShapeCacheId("annulus.source")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(primary, baseline_cache));
    const auto primary_intent = pf::checked(serialize_part_document_to_json(primary));
    const auto original_body = planar_source_bytes(*baseline_cache.find_body_shape(owner));
    // Independent 30-degree Y rotation and XY reflection of the original
    // upper cap at z5, including reflection of its outward +Z direction.
    const double sign = placement == 2 ? -1 : 1;
    const Vector3 normal =
        placement == 0 ? Vector3{0, 0, 1} : Vector3{sign * .5, 0, sign * std::sqrt(3.) / 2};
    const Point3 center{5 * normal.x, 0, 5 * normal.z};
    const auto faces =
        pf::checked(ResultFaceSupportGeometry::selectable_faces(primary, baseline_cache, owner));
    const auto cap = std::find_if(faces.begin(), faces.end(), [&](const auto& candidate) {
      const auto a = candidate.anchor;
      return std::hypot(a.x - center.x, a.y - center.y, a.z - center.z) < 1e-7;
    });
    REQUIRE(cap != faces.end());
    const auto reference = pf::checked(SurfaceReference::create_faces(
        owner, {pf::checked(FaceReference::create_result(PartFeatureInputRole::SurfaceSourceFace,
                                                         cap->reference))}));
    const auto source = pf::checked(SurfaceSelectionGeometry::resolve(
        primary, baseline_cache, reference, FeatureId("annulus.select"), 1e-6));
    const auto original_source = planar_source_bytes(source);
    for (const auto side : {SurfaceOffsetSide::AlongNormal, SurfaceOffsetSide::OppositeNormal,
                            SurfaceOffsetSide::Symmetric}) {
      CAPTURE(side);
      auto candidate = primary;
      const BodyId output("annulus.thick.body");
      REQUIRE(candidate.add_body(pf::checked(Body::create(output, "Thin ring", BodyKind::Solid))));
      REQUIRE(candidate.add_surface_feature(pf::checked(SurfaceOffsetFeature::create_thicken(
          FeatureId("annulus.thick"), "Thin annular Thicken", reference,
          {ParameterId("annulus.thickness"),
           ParameterId("annulus.tolerance"),
           side,
           SurfaceOffsetApproximation::ExactAnalytic,
           {}},
          pf::checked(
              FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {}, output))))));
      auto cache = pf::checked(ShapeCache::create(ShapeCacheId("annulus.thick")));
      const auto recomputed = GeometryRecomputeExecutor{}.execute_document(candidate, cache);
      INFO((recomputed ? "ok" : recomputed.error().message()));
      REQUIRE(recomputed);
      const auto* shape = cache.find_body_shape(output);
      REQUIRE(shape);
      const auto physical = pf::checked(inspect_shape_physical_properties(*shape));
      REQUIRE(physical.volume);
      CHECK(physical.solid_count == 1);
      CHECK(physical.volume->volume_mm3 == Catch::Approx(area * .1).margin(1e-7));
      const double first = side == SurfaceOffsetSide::Symmetric ? -.05 : 0;
      const double last = side == SurfaceOffsetSide::Symmetric        ? .05
                          : side == SurfaceOffsetSide::OppositeNormal ? -.1
                                                                      : .1;
      const auto expected_center = skin_center(center, normal, (first + last) / 2);
      point_matches(physical.volume->centroid_mm, expected_center);
      annulus_inertia(physical.volume->central_inertia_mm5, normal);
      const auto report = SurfaceOffsetAdapter::verification(*shape);
      REQUIRE(report);
      CHECK(report->minimum_mm == Catch::Approx(.1).margin(1e-7));
      CHECK(report->maximum_mm == Catch::Approx(.1).margin(1e-7));
      CHECK(report->maximum_error_mm <= report->permitted_error_mm);
      CHECK(BRepAlgoAPI_Check(*OcctShapeView::get(*shape), false, true).IsValid());
      std::vector<double> radii;
      for (TopExp_Explorer face(*OcctShapeView::get(*shape), TopAbs_FACE); face.More();
           face.Next()) {
        BRepAdaptor_Surface native(TopoDS::Face(face.Current()));
        if (native.GetType() == GeomAbs_Cylinder)
          radii.push_back(native.Cylinder().Radius());
      }
      REQUIRE(radii.size() >= 2);
      CHECK(*std::min_element(radii.begin(), radii.end()) == Catch::Approx(4.7).margin(1e-9));
      CHECK(*std::max_element(radii.begin(), radii.end()) == Catch::Approx(5).margin(1e-9));
      const auto skins =
          pf::checked(ResultFaceSupportGeometry::selectable_faces(candidate, cache, output));
      REQUIRE(skins.size() == 2);
      for (const auto& [role, position] :
           {std::pair{side == SurfaceOffsetSide::Symmetric ? "negative" : "source", first},
            std::pair{last < 0 ? "negative" : "positive", last}}) {
        std::vector<ResultFaceOrigin> expected_origins;
        for (const auto& origin : cap->reference.origins())
          expected_origins.push_back(
              {FeatureId("annulus.thick"), "surface/skin/" + std::string(role) + "/" +
                                               annulus_segment(origin.feature.value()) +
                                               annulus_segment(origin.role)});
        std::sort(
            expected_origins.begin(), expected_origins.end(), [](const auto& a, const auto& b) {
              return std::pair{a.feature.value(), a.role} < std::pair{b.feature.value(), b.role};
            });
        const auto actual = std::find_if(skins.begin(), skins.end(), [&](const auto& skin) {
          return skin.reference.origins() == expected_origins;
        });
        REQUIRE(actual != skins.end());
        point_matches(actual->anchor, skin_center(center, normal, position));
        REQUIRE(ResultFaceSupportGeometry::resolve_face(candidate, cache, actual->reference));
      }
      imported_body_test::Fixture files;
      const auto path = files.root / "annular-thick.step";
      REQUIRE(StepExporter{}.write_step(*shape, path.string()));
      const auto step = pf::checked(StepImportSource::create(
          StepImportSourceId("annulus.exchange"), "Annular exchange", path.filename().string(),
          pf::checked(step_import_sha256(imported_body_test::bytes(path))),
          StepImportMode::EditableBody));
      const auto read = pf::checked(StepImportReader{}.read(step, files.root));
      REQUIRE(read.body_count() == 1);
      for (const auto& product : read.products)
        for (const auto& body : product.bodies) {
          const auto imported = pf::checked(inspect_shape_physical_properties(body.shape));
          REQUIRE(imported.volume);
          CHECK(imported.volume->volume_mm3 == Catch::Approx(area * .1).margin(1e-7));
          point_matches(imported.volume->centroid_mm, expected_center);
          annulus_inertia(imported.volume->central_inertia_mm5, normal);
        }
      CHECK(planar_source_bytes(source) == original_source);
      CHECK(planar_source_bytes(*baseline_cache.find_body_shape(owner)) == original_body);
      CHECK(pf::checked(serialize_part_document_to_json(primary)) == primary_intent);
    }
  }
}
