#include "../support/part_history_fixture.hpp"
#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "imported_body_test_fixture.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
void parameter(PartDocument& p, const char* id, double v) {
  f::accept(p.add_parameter(f::checked(
      Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(v))))));
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("direct.cache")));
}
void execute(const PartDocument& p, ShapeCache& c, StepImportTopologyContext* imports = nullptr) {
  auto result = GeometryRecomputeExecutor(imports).execute_document(p, c);
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
}
double volume(const ShapeCache& c, BodyId body = BodyId("body.base")) {
  const auto* shape = c.find_body_shape(body);
  REQUIRE(shape);
  GProp_GProps p;
  BRepGProp::VolumeProperties(*OcctShapeView::get(*shape), p);
  return p.Mass();
}
FaceReference face(SemanticFace side) {
  return f::checked(FaceReference::create_planar(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(SemanticFaceReference::create(FeatureId("base"), side))));
}
void add(PartDocument& p, FaceReference r, DirectEditOperation operation,
         DirectEditFaceCapability capability = DirectEditFaceCapability::Planar,
         BodyId body = BodyId("body.base"), FeatureId predecessor = FeatureId("base"),
         DirectEditFrame frame = {}) {
  f::accept(p.add_direct_edit_feature(
      f::checked(DirectEditFeature::create(FeatureId("direct"), "Direct Edit", body, predecessor,
                                           {{r, capability}}, operation, frame))));
}
FaceReference imported_face(const PartDocument& p, ImportedGeometryClass type,
                            std::optional<double> z = std::nullopt) {
  const auto& catalog = p.imported_topology_catalogs().front();
  for (const auto& entry : catalog.entries()) {
    if (entry.descriptor.kind != ImportedTopologyKind::Face || entry.descriptor.geometry != type)
      continue;
    if (z && std::abs(entry.descriptor.centroid.z - *z) > 1e-5)
      continue;
    return f::checked(FaceReference::create_imported(
        PartFeatureInputRole::SurfaceSourceFace,
        f::checked(ImportedTopologyReference::create(catalog.id(), entry.id,
                                                     ImportedTopologyCapability::Face))));
  }
  FAIL("Expected imported face is missing");
  throw std::runtime_error("Expected imported face is missing");
}
} // namespace

TEST_CASE("Direct Edit moves selected planar support and preserves unrelated material",
          "[geometry][direct-edit][parity-b186][D01]") {
  auto p = f::native_base();
  parameter(p, "dx", 1);
  parameter(p, "dy", 1);
  parameter(p, "dz", 3);
  auto c = cache();
  execute(p, c);
  const double before = volume(c);
  const auto base = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  const auto immutable = f::checked(serialize_part_document_to_json(p));
  add(p, face(SemanticFace::Top),
      DirectFaceMove{ParameterId("dx"), ParameterId("dy"), ParameterId("dz"), {0, 0, 1}});
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(before + 20 * 10 * 3).margin(1e-5));
  CHECK(BRepCheck_Analyzer(base, true).IsValid());
  GProp_GProps original;
  BRepGProp::VolumeProperties(base, original);
  CHECK(original.Mass() == Catch::Approx(before).margin(1e-7));
  const auto* result = OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  REQUIRE(result);
  bool top = false;
  for (TopExp_Explorer it(*result, TopAbs_FACE); it.More(); it.Next()) {
    BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
    REQUIRE(surface.GetType() == GeomAbs_Plane);
    if (std::abs(surface.Plane().Axis().Direction().Z()) > .99 &&
        std::abs(surface.Plane().Location().Z() - 11) < 1e-6)
      top = true;
  }
  CHECK(top);
  // The old producer identity remains in lineage, while the direct edit owns
  // an explicit selectable continuation for downstream support consumers.
  auto reference = f::checked(ResultFaceReference::create(FeatureId("direct"), BodyId("body.base"),
                                                          {{FeatureId("direct"), "face/0"}}));
  REQUIRE(ResultFaceSupportGeometry::resolve_face(p, c, reference));
  auto reopened = f::checked(
      deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
  auto fresh = cache();
  execute(reopened, fresh);
  CHECK(volume(fresh) == Catch::Approx(volume(c)).margin(1e-7));
  PartFeatureHistoryIntent history;
  for (const auto& entry : f::checked(p.executable_history()))
    history.order.push_back(entry.node_id);
  auto suppressed = history;
  suppressed.suppressed.push_back("direct");
  REQUIRE(p.apply_history_intent(suppressed));
  execute(p, fresh);
  CHECK(volume(fresh) == Catch::Approx(before).margin(1e-7));
  REQUIRE(p.apply_history_intent(history));
  REQUIRE(p.set_parameter_value(ParameterId("dz"), f::checked(Quantity::length_mm(30))));
  REQUIRE(p.update_direct_edit_feature(f::checked(DirectEditFeature::create(
      FeatureId("direct"), "Direct Edit", BodyId("body.base"), FeatureId("base"),
      {{face(SemanticFace::Top), DirectEditFaceCapability::Planar}},
      DirectFaceMove{ParameterId("dx"), ParameterId("dy"), ParameterId("dz"), {0, 0, -1}}))));
  const auto certified = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(certified));
  auto primary = f::checked(deserialize_part_document_from_json(immutable));
  auto untouched = cache();
  execute(primary, untouched);
  CHECK(volume(untouched) == Catch::Approx(before).margin(1e-7));
}

TEST_CASE("Direct Edit rotates a planar face around its explicit in-plane frame",
          "[geometry][direct-edit][parity-b186][D01]") {
  auto p = f::native_base();
  add(p, face(SemanticFace::Top), DirectFaceRotate{{1, 0, 0}, ParameterId("angle")},
      DirectEditFaceCapability::Planar, BodyId("body.base"), FeatureId("base"),
      {{0, 0, 8}, {1, 0, 0}, {0, 1, 0}});
  auto c = cache();
  execute(p, c);
  bool tilted = false;
  const double angle = 2 * std::numbers::pi / 180;
  for (TopExp_Explorer it(*OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))),
                          TopAbs_FACE);
       it.More(); it.Next()) {
    BRepAdaptor_Surface s(TopoDS::Face(it.Current()));
    if (s.GetType() != GeomAbs_Plane)
      continue;
    const auto n = s.Plane().Axis().Direction();
    if (std::abs(std::abs(n.Z()) - std::cos(angle)) < 1e-7 && std::abs(n.Y()) > .01) {
      CHECK(n.Y() / n.Z() == Catch::Approx(-std::tan(angle)).margin(1e-7));
      tilted = true;
    }
  }
  CHECK(tilted);
}

TEST_CASE("Direct Edit resizes and heals imported bores without modifying source bytes",
          "[geometry][direct-edit][parity-b186][D01]") {
  imported_body_test::Fixture fixture;
  const auto source =
      BRepAlgoAPI_Cut(
          BRepPrimAPI_MakeBox(20, 30, 10).Shape(),
          BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10, 15, -1), gp_Dir(0, 0, 1)), 2, 12).Shape())
          .Shape();
  fixture.write(source);
  const auto bytes = fixture.source_bytes();
  for (const bool heal : {false, true}) {
    auto p = fixture.editable();
    parameter(p, "size", 6);
    const auto base = f::checked(serialize_part_document_to_json(p));
    add(p, imported_face(p, ImportedGeometryClass::Cylinder),
        heal ? DirectEditOperation(DirectFaceDeleteHeal{})
             : DirectEditOperation(
                   DirectFaceSize{ParameterId("size"), DirectEditSizeMode::Diameter}),
        DirectEditFaceCapability::Cylindrical, BodyId("body.1"), FeatureId("import.base"));
    StepImportTopologyContext context(fixture.root);
    auto c = cache();
    execute(p, c, &context);
    CHECK(volume(c, BodyId("body.1")) ==
          Catch::Approx(6000 - (heal ? 0 : 90 * std::numbers::pi)).margin(2e-4));
    CHECK(fixture.source_bytes() == bytes);
    auto reopened = f::checked(
        deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
    auto fresh = cache();
    execute(reopened, fresh, &context);
    CHECK(volume(fresh, BodyId("body.1")) ==
          Catch::Approx(volume(c, BodyId("body.1"))).margin(1e-6));
    REQUIRE(StepExporter{}.write_step(*c.find_body_shape(BodyId("body.1")),
                                      (fixture.root / "result.step").string()));
    auto independent = f::checked(deserialize_part_document_from_json(base));
    auto original = cache();
    execute(independent, original, &context);
    CHECK(volume(original, BodyId("body.1")) ==
          Catch::Approx(6000 - 40 * std::numbers::pi).margin(2e-4));
  }
}

TEST_CASE("Direct Edit rejects incomplete healing and preserves its certified cache",
          "[geometry][direct-edit][parity-b186][D01]") {
  auto p = f::native_base();
  auto c = cache();
  execute(p, c);
  const auto certified = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  add(p, face(SemanticFace::Top), DirectFaceDeleteHeal{});
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(certified));
}
