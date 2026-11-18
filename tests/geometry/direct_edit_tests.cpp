#include "../support/part_history_fixture.hpp"
#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "imported_body_test_fixture.hpp"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <numbers>
#include <sstream>

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
  const bool reflected = GENERATE(false, true);
  imported_body_test::Fixture fixture;
  auto source =
      BRepAlgoAPI_Cut(
          BRepPrimAPI_MakeBox(20, 30, 10).Shape(),
          BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10, 15, -1), gp_Dir(0, 0, 1)), 2, 12).Shape())
          .Shape();
  if (reflected) {
    gp_Trsf reflection;
    reflection.SetMirror(gp_Ax2(gp_Pnt(), gp_Dir(1, 0, 0)));
    source = BRepBuilderAPI_Transform(source, reflection, true).Shape();
  }
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

TEST_CASE("Direct Edit after an existing imported blend preserves its exact unselected carrier",
          "[geometry][direct-edit][direct-edit-existing-blend][parity-b186][D01]") {
  const auto bored =
      BRepAlgoAPI_Cut(
          BRepPrimAPI_MakeBox(20, 30, 10).Shape(),
          BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10, 15, -1), gp_Dir(0, 0, 1)), 2, 12).Shape())
          .Shape();
  TopoDS_Edge corner;
  for (TopExp_Explorer it(bored, TopAbs_EDGE); it.More(); it.Next()) {
    const auto edge = TopoDS::Edge(it.Current());
    BRepAdaptor_Curve curve(edge);
    if (curve.GetType() != GeomAbs_Line)
      continue;
    const auto at = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
    if (std::abs(at.X()) < 1e-8 && std::abs(at.Y()) < 1e-8 && std::abs(at.Z() - 5) < 1e-8) {
      corner = edge;
      break;
    }
  }
  REQUIRE_FALSE(corner.IsNull());
  BRepFilletAPI_MakeFillet round(bored);
  round.Add(1, corner);
  round.Build();
  REQUIRE(round.IsDone());
  REQUIRE(BRepCheck_Analyzer(round.Shape(), true).IsValid());
  const auto blend_loss = 10 * (1 - std::numbers::pi / 4);
  imported_body_test::Fixture fixture;
  fixture.write(round.Shape());
  const auto immutable_bytes = fixture.source_bytes();
  for (const bool heal : {false, true}) {
    auto p = fixture.editable();
    parameter(p, "bore.diameter", 6);
    StepImportTopologyContext context(fixture.root);
    auto c = cache();
    execute(p, c, &context);
    const auto base = *OcctShapeView::get(
        *c.find_imported_body_shape(FeatureId("import.base"), BodyId("body.1")));
    std::ostringstream before;
    BRepTools::Write(base, before);
    const auto source = p.step_import_sources();
    const auto catalog = p.imported_topology_catalogs();
    std::optional<FaceReference> bore;
    const auto& authority = p.imported_topology_catalogs().front();
    for (const auto& entry : authority.entries())
      if (entry.descriptor.kind == ImportedTopologyKind::Face &&
          entry.descriptor.geometry == ImportedGeometryClass::Cylinder &&
          std::abs(entry.descriptor.radius_mm - 2) < 1e-7)
        bore = f::checked(FaceReference::create_imported(
            PartFeatureInputRole::SurfaceSourceFace,
            f::checked(ImportedTopologyReference::create(authority.id(), entry.id,
                                                         ImportedTopologyCapability::Face))));
    REQUIRE(bore);
    add(p, *bore,
        heal ? DirectEditOperation(DirectFaceDeleteHeal{})
             : DirectEditOperation(
                   DirectFaceSize{ParameterId("bore.diameter"), DirectEditSizeMode::Diameter}),
        DirectEditFaceCapability::Cylindrical, BodyId("body.1"), FeatureId("import.base"));
    execute(p, c, &context);
    CHECK(volume(c, BodyId("body.1")) ==
          Catch::Approx(6000 - blend_loss - (heal ? 0 : 90 * std::numbers::pi)).margin(3e-4));
    const auto* result = OcctShapeView::get(*c.find_body_shape(BodyId("body.1")));
    REQUIRE(BRepCheck_Analyzer(*result, true).IsValid());
    std::size_t unchanged_blends = 0;
    for (TopExp_Explorer it(*result, TopAbs_FACE); it.More(); it.Next()) {
      BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
      if (surface.GetType() != GeomAbs_Cylinder || std::abs(surface.Cylinder().Radius() - 1) > 1e-7)
        continue;
      CHECK(surface.Cylinder().Axis().Location().X() == Catch::Approx(1).margin(1e-7));
      CHECK(surface.Cylinder().Axis().Location().Y() == Catch::Approx(1).margin(1e-7));
      CHECK(std::abs(surface.Cylinder().Axis().Direction().Z()) == Catch::Approx(1).margin(1e-8));
      GProp_GProps area;
      BRepGProp::SurfaceProperties(it.Current(), area);
      CHECK(area.Mass() == Catch::Approx(5 * std::numbers::pi).margin(1e-5));
      ++unchanged_blends;
    }
    CHECK(unchanged_blends == 1);
    std::ostringstream after;
    BRepTools::Write(base, after);
    CHECK(after.str() == before.str());
    CHECK(fixture.source_bytes() == immutable_bytes);
    CHECK(p.step_import_sources() == source);
    CHECK(p.imported_topology_catalogs() == catalog);
    auto reopened = f::checked(
        deserialize_part_document_from_json(f::checked(serialize_part_document_to_json(p))));
    auto fresh = cache();
    execute(reopened, fresh, &context);
    CHECK(volume(fresh, BodyId("body.1")) ==
          Catch::Approx(volume(c, BodyId("body.1"))).margin(1e-6));
  }
}

TEST_CASE("Direct Edit rejects native self-interference even when topology reports one valid solid",
          "[geometry][direct-edit][direct-edit-native-si][parity-b186][D01]") {
  auto box = BRepPrimAPI_MakeBox(gp_Pnt(-10, -10, 0), 20, 20, 8).Shape();
  const auto left_tool =
      BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(-2, 0, -1), gp_Dir(0, 0, 1)), 1, 10).Shape();
  const auto right_tool =
      BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(2, 0, -1), gp_Dir(0, 0, 1)), 1, 10).Shape();
  const auto source_shape =
      BRepAlgoAPI_Cut(BRepAlgoAPI_Cut(box, left_tool).Shape(), right_tool).Shape();
  imported_body_test::Fixture fixture;
  fixture.write(source_shape);
  const auto source_bytes = fixture.source_bytes();
  auto original = fixture.editable();
  parameter(original, "bore.diameter", 4);
  StepImportTopologyContext context(fixture.root);
  auto certified_cache = cache();
  execute(original, certified_cache, &context);
  const auto* base_shape = certified_cache.find_body_shape(BodyId("body.1"));
  REQUIRE(base_shape);
  const auto certified = *OcctShapeView::get(*base_shape);
  CHECK(volume(certified_cache, BodyId("body.1")) ==
        Catch::Approx(3200 - 16 * std::numbers::pi).margin(1e-5));
  std::ostringstream base_bytes;
  BRepTools::Write(certified, base_bytes);
  const auto before = f::checked(serialize_part_document_to_json(original));
  std::optional<FaceReference> selected;
  const auto& catalog = original.imported_topology_catalogs().front();
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        entry.descriptor.geometry == ImportedGeometryClass::Cylinder &&
        entry.descriptor.centroid.x < 0 && std::abs(entry.descriptor.radius_mm - 1) < 1e-7)
      selected = f::checked(FaceReference::create_imported(
          PartFeatureInputRole::SurfaceSourceFace,
          f::checked(ImportedTopologyReference::create(catalog.id(), entry.id,
                                                       ImportedTopologyCapability::Face))));
  REQUIRE(selected);
  // Reproduce the exact native failure on a private copy of the imported Solid.
  // This establishes why a topological validity flag alone is insufficient.
  BRepBuilderAPI_Copy native_copy(certified, true, false);
  auto input = native_copy.Shape();
  TopoDS_Face native_face;
  for (TopExp_Explorer it(input, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    const BRepAdaptor_Surface support(face);
    if (support.GetType() == GeomAbs_Cylinder && support.Cylinder().Location().X() < 0)
      native_face = face;
  }
  REQUIRE_FALSE(native_face.IsNull());
  const auto cylinder = BRepAdaptor_Surface(native_face).Cylinder();
  const bool radial_outward =
      (native_face.Orientation() == TopAbs_REVERSED) != cylinder.Position().Direct();
  BRepOffset_MakeOffset native;
  native.Initialize(input, 0, 1e-6, BRepOffset_Skin, true, false, GeomAbs_Intersection);
  native.SetOffsetOnFace(native_face, radial_outward ? 4 : -4);
  native.MakeOffsetShape();
  REQUIRE(native.IsDone());
  REQUIRE_FALSE(native.Shape().IsNull());
  TopTools_IndexedMapOfShape solids;
  TopExp::MapShapes(native.Shape(), TopAbs_SOLID, solids);
  REQUIRE(solids.Extent() == 1);
  REQUIRE(BRepCheck_Analyzer(native.Shape(), true).IsValid());
  GProp_GProps invalid_properties;
  BRepGProp::VolumeProperties(native.Shape(), invalid_properties);
  CHECK(invalid_properties.Mass() > 0);
  CHECK_FALSE(BRepAlgoAPI_Check(native.Shape(), false, true).IsValid());
  auto candidate = original;
  REQUIRE(candidate.set_parameter_value(ParameterId("bore.diameter"),
                                        f::checked(Quantity::length_mm(10))));
  add(candidate, *selected,
      DirectFaceSize{ParameterId("bore.diameter"), DirectEditSizeMode::Diameter},
      DirectEditFaceCapability::Cylindrical, BodyId("body.1"), FeatureId("import.base"));
  auto candidate_cache = certified_cache;
  const auto rejected =
      GeometryRecomputeExecutor{&context}.execute_document(candidate, candidate_cache);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().message().find("self-interfering") != std::string::npos);
  CHECK(OcctShapeView::get(*candidate_cache.find_body_shape(BodyId("body.1")))->IsSame(certified));
  CHECK(candidate_cache.find_feature_shape(FeatureId("direct")) == nullptr);
  CHECK(f::checked(serialize_part_document_to_json(original)) == before);
  CHECK(fixture.source_bytes() == source_bytes);
  std::ostringstream retained;
  BRepTools::Write(*OcctShapeView::get(*certified_cache.find_body_shape(BodyId("body.1"))),
                   retained);
  CHECK(retained.str() == base_bytes.str());
  // Radius2 remains within the explicit capability, keeps the second bore and
  // supplies an independent analytic volume rather than relaxing this boundary.
  add(original, *selected,
      DirectFaceSize{ParameterId("bore.diameter"), DirectEditSizeMode::Diameter},
      DirectEditFaceCapability::Cylindrical, BodyId("body.1"), FeatureId("import.base"));
  execute(original, certified_cache, &context);
  CHECK(volume(certified_cache, BodyId("body.1")) ==
        Catch::Approx(3200 - 40 * std::numbers::pi).margin(1e-5));
  const auto* resized = OcctShapeView::get(*certified_cache.find_body_shape(BodyId("body.1")));
  REQUIRE(resized);
  CHECK(BRepAlgoAPI_Check(*resized, false, true).IsValid());
  std::size_t preserved = 0;
  for (TopExp_Explorer it(*resized, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    const BRepAdaptor_Surface support(face);
    if (support.GetType() == GeomAbs_Cylinder &&
        std::abs(support.Cylinder().Location().X() - 2) < 1e-7) {
      ++preserved;
      CHECK(support.Cylinder().Radius() == Catch::Approx(1).margin(1e-7));
      GProp_GProps area;
      BRepGProp::SurfaceProperties(face, area);
      CHECK(area.Mass() == Catch::Approx(16 * std::numbers::pi).margin(1e-5));
    }
  }
  CHECK(preserved == 1);
  CHECK(fixture.source_bytes() == source_bytes);
}
