#include "../support/thread_feature_fixture.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/thread_adapter.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "imported_body_test_fixture.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <numbers>
#include <sstream>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::thread_fixture;
using f::checked;
namespace {
ShapeCache evaluate(const PartDocument& p, StepImportTopologyContext* context = nullptr) {
  auto result = checked(ShapeCache::create(ShapeCacheId("thread.combined")));
  (void)checked(GeometryRecomputeExecutor{context}.execute_document(p, result));
  REQUIRE(result.validate_complete_geometry_result(p));
  return result;
}
double volume(const GeometryShape& shape) {
  GProp_GProps properties;
  BRepGProp::VolumeProperties(*OcctShapeView::get(shape), properties, 1e-9);
  return properties.Mass();
}
std::string brep(const GeometryShape& shape) {
  std::ostringstream text;
  BRepTools::Write(*OcctShapeView::get(shape), text);
  return text.str();
}
double removed(bool internal, double pitch, double length = 4) {
  return 4 * std::numbers::pi * std::tan(55 * std::numbers::pi / 360) * length / pitch *
         (internal ? (5. / 2 - 1. / 3) : (4. / 2 + 1. / 3));
}
std::array<double, 6> bounds(const GeometryShape& shape) {
  Bnd_Box box;
  BRepBndLib::AddOptimal(*OcctShapeView::get(shape), box);
  std::array<double, 6> result;
  box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
  return result;
}
void dimensions(PartDocument& p, const std::string& id, double value) {
  REQUIRE(p.set_parameter_value(ParameterId(id), checked(Quantity::length_mm(value))));
}
} // namespace

TEST_CASE("Thread physical failures and workload limits preserve complete cached Bodies",
          "[geometry][parity-b155][thread-combined][thread-boundaries]") {
  const auto original = f::pair_part();
  for (int mode = 0; mode < 7; ++mode) {
    CAPTURE(mode);
    auto cache = evaluate(original);
    auto candidate = original;
    const auto base = brep(*cache.find_body_shape(BodyId("body.base")));
    const auto shaft = brep(*cache.find_body_shape(BodyId("body.shaft")));
    if (mode == 0) {
      dimensions(candidate, "thread.pitch", .01);
      REQUIRE(candidate.add_thread_feature(f::external(f::definition(
          f::custom(), false, ThreadHandedness::Right, ThreadRepresentation::Modeled))));
    } else if (mode == 1) {
      dimensions(candidate, "thread.start", 7);
      REQUIRE(candidate.add_thread_feature(f::external(f::definition(f::custom(), true))));
    } else if (mode == 2) {
      auto profile = f::custom();
      profile.major_diameter = ParameterId("width");
      REQUIRE(candidate.add_thread_feature(f::external(f::definition(profile))));
    } else {
      auto prototype = f::internal();
      auto placements = prototype.placements();
      HoleManufacturing seat{};
      auto definition = prototype.thread();
      if (mode == 3)
        seat = HoleCounterbore{ParameterId("thread.major"), ParameterId("thread.start")};
      if (mode == 4)
        placements.front().source = Point2{5.5, 0};
      if (mode >= 5) {
        placements.clear();
        for (int i = 0; i < (mode == 5 ? 17 : 13); ++i)
          placements.push_back({HolePlacementId("p" + std::to_string(i)),
                                Point2{double(i % 4) * 12 - 18, double(i / 4) * 12 - 18}});
        definition = f::definition(f::custom(), false, ThreadHandedness::Right,
                                   ThreadRepresentation::Modeled);
        if (mode == 6) {
          dimensions(candidate, "width", 80);
          dimensions(candidate, "height", 80);
          dimensions(candidate, "depth", 30);
          dimensions(candidate, "drill.depth", 26);
          dimensions(candidate, "thread.length", 20);
          dimensions(candidate, "thread.pitch", 2);
        }
      }
      REQUIRE(candidate.add_hole_feature(checked(HoleFeature::create(
          prototype.id(), prototype.name(), prototype.kind(), prototype.target_body(),
          prototype.support(), placements, prototype.diameter(), prototype.extent(),
          prototype.direction(), prototype.tip(), seat, definition))));
    }
    const auto result = GeometryRecomputeExecutor{}.execute_document(candidate, cache);
    REQUIRE_FALSE(result);
    INFO(result.error().message());
    if (mode == 5)
      CHECK(result.error().message().find("16 placements") != std::string::npos);
    if (mode == 6)
      CHECK(result.error().message().find("128 aggregate") != std::string::npos);
    CHECK(cache.validate_complete_geometry_result(original));
    CHECK(brep(*cache.find_body_shape(BodyId("body.base"))) == base);
    CHECK(brep(*cache.find_body_shape(BodyId("body.shaft"))) == shaft);
  }
}

TEST_CASE("Pinned Thread revisions combine modeled geometry mass history reopening and actual STEP",
          "[geometry][parity-b155][thread-combined][thread-step]") {
  auto p = f::pair_part();
  const auto definition =
      f::definition(f::selected(), true, ThreadHandedness::Left, ThreadRepresentation::Modeled);
  REQUIRE(p.add_hole_feature(f::internal(definition)));
  REQUIRE(p.add_thread_feature(f::external(definition)));
  blcad::test::engineering_fixture::annotate(p);
  auto cache = evaluate(p);
  REQUIRE(p.adopt_engineering_resource_revision(f::selected().table, f::table("r2", 2.2)));
  const auto updated = checked(
      GeometryRecomputeExecutor{}.execute_plan(p,
                                               checked(RecomputePlan::from_graph_and_changed_nodes(
                                                   p.dependency_graph(), {"external", "internal"})),
                                               cache));
  CHECK(updated.executed_feature_count > 0);
  const double internal = 2560 - 96 * std::numbers::pi - removed(true, 2.2);
  const double external = 200 * std::numbers::pi - removed(false, 2.2);
  CHECK(volume(*cache.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(internal).margin(2e-4));
  CHECK(volume(*cache.find_body_shape(BodyId("body.shaft"))) ==
        Catch::Approx(external).margin(2e-4));
  const auto physical = checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(physical.data().complete_mass());
  CHECK(physical.data().complete_mass()->mass_kg ==
        Catch::Approx((internal + external) * 4e-6).margin(2e-9));
  const auto saved = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(saved));
  const auto fresh = evaluate(reopened);
  CHECK(volume(*fresh.find_body_shape(BodyId("body.shaft"))) ==
        Catch::Approx(external).margin(2e-4));
  const auto rows = checked(p.executable_history());
  std::vector<std::string> order;
  for (const auto& row : rows)
    order.push_back(row.node_id);
  auto suppressed = p;
  REQUIRE(suppressed.apply_history_intent({order, {"external"}}));
  auto plain = evaluate(suppressed);
  CHECK(volume(*plain.find_body_shape(BodyId("body.shaft"))) ==
        Catch::Approx(200 * std::numbers::pi).margin(1e-7));
  const auto cutoff = std::find(order.begin(), order.end(), "external") - order.begin();
  auto prefix = checked(ShapeCache::create(ShapeCacheId("thread.prefix")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, cutoff, prefix));
  CHECK_FALSE(prefix.validate_complete_geometry_result(p));
  CHECK_FALSE(prefix.find_feature_shape(FeatureId("external")));
  imported_body_test::Fixture output;
  REQUIRE(StepExporter{}.write_part_step(p, cache, output.path()));
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("roundtrip"), "Roundtrip", "supplier.step",
      checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
  const auto imported = checked(StepImportReader{}.read(source, output.root));
  REQUIRE(imported.body_count() == 2);
  std::vector<double> volumes;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies)
      volumes.push_back(body.volume_mm3);
  std::sort(volumes.begin(), volumes.end());
  CHECK(volumes[0] == Catch::Approx(external).margin(2e-4));
  CHECK(volumes[1] == Catch::Approx(internal).margin(2e-4));
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies) {
      const bool inside = body.volume_mm3 > 1000;
      for (double z : {.35, 1.05, 2.7})
        for (double theta : {.23, 2.39, 5.31})
          for (double r : {4.25, 4.75}) {
            const double phase = std::remainder(z + 2.2 * theta / (2 * std::numbers::pi), 2.2);
            const double width = (inside ? 5 - r : r - 4) * std::tan(55 * std::numbers::pi / 360);
            if (std::abs(std::abs(phase) - width) < .025)
              continue;
            const bool cut = std::abs(phase) < width;
            const gp_Pnt point(inside ? r * std::cos(theta) : 30 + r * std::cos(theta),
                               (inside ? -1 : 1) * r * std::sin(theta), inside ? 7 - z : 1 + z);
            CHECK(
                BRepClass3d_SolidClassifier(*OcctShapeView::get(body.shape), point, 1e-7).State() ==
                (cut ? TopAbs_OUT : TopAbs_IN));
          }
    }
  CHECK(output.source_bytes().find("Synthetic table system") == std::string::npos);
  CHECK(checked(serialize_part_document_to_json(p)) == saved);
}

TEST_CASE("Thread starts seats conical drill clearance and full shaft length remain distinct",
          "[geometry][parity-b155][thread-combined]") {
  for (const auto hand : {ThreadHandedness::Right, ThreadHandedness::Left}) {
    CAPTURE(hand == ThreadHandedness::Right);
    auto p = f::pair_part();
    REQUIRE(p.add_parameter(checked(Parameter::create_angle(
        ParameterId("drill.angle"), "Drill angle", checked(Quantity::angle_deg(150))))));
    const auto hole = f::internal(
        f::definition(f::custom(), true, ThreadHandedness::Right, ThreadRepresentation::Modeled));
    REQUIRE(p.add_hole_feature(checked(HoleFeature::create(
        hole.id(), hole.name(), hole.kind(), hole.target_body(), hole.support(), hole.placements(),
        hole.diameter(), hole.extent(), hole.direction(),
        HoleConicalTip{ParameterId("drill.angle")},
        HoleCounterbore{ParameterId("thread.major"), ParameterId("thread.start")},
        hole.thread()))));
    const auto full = checked(ThreadDefinition::create(f::custom(), ParameterId("depth"), {}, hand,
                                                       ThreadRepresentation::Modeled));
    REQUIRE(p.add_thread_feature(f::external(full)));
    const auto cache = evaluate(p);
    const double tip = 4 / std::tan(75 * std::numbers::pi / 180);
    const double expected =
        2560 - std::numbers::pi * (16 * 6 + 16 * tip / 3 + 9) - removed(true, 1.7);
    const auto& base = *cache.find_body_shape(BodyId("body.base"));
    CHECK(volume(base) == Catch::Approx(expected).margin(2e-4));
    CHECK(volume(*cache.find_body_shape(BodyId("body.shaft"))) ==
          Catch::Approx(200 * std::numbers::pi - removed(false, 1.7, 8)).margin(3e-4));
    CHECK(
        BRepClass3d_SolidClassifier(*OcctShapeView::get(base), gp_Pnt(4.5, 0, 2.5), 1e-7).State() ==
        TopAbs_IN);
    CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(base), gp_Pnt(0, 0, 1.5), 1e-7).State() ==
          TopAbs_OUT);
    CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(base), gp_Pnt(3, 0, 1.5), 1e-7).State() ==
          TopAbs_IN);
  }
}

TEST_CASE(
    "Cosmetic Threads remain derived through Body motion tiny pitch and posed Project instances",
    "[geometry][parity-b155][thread-combined][thread-display]") {
  auto p = f::pair_part();
  dimensions(p, "thread.pitch", 1e-4);
  REQUIRE(p.add_hole_feature(f::internal()));
  REQUIRE(p.add_thread_feature(f::external()));
  auto cache = evaluate(p);
  const auto original =
      checked(ThreadAdapter::cosmetic_presentations(*cache.find_body_shape(BodyId("body.shaft"))));
  REQUIRE(original.size() == 1);
  const auto before = bounds(original.front().shape);
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("move"), BodyId("body.shaft"), BodyTransformCoordinateSpace::World,
      std::nullopt, {2, 3, 4}, false, false))));
  cache = evaluate(p);
  const auto moved =
      checked(ThreadAdapter::cosmetic_presentations(*cache.find_body_shape(BodyId("body.shaft"))));
  REQUIRE(moved.size() == 1);
  const auto after = bounds(moved.front().shape);
  for (int i = 0; i < 6; ++i)
    CHECK(after[i] - before[i] == Catch::Approx(std::array{2., 3., 4.}[i % 3]).margin(1e-5));
  auto assembly = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(assembly.add_member_part(p.id()));
  REQUIRE(assembly.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("first"), "First", p.id()))));
  const auto second =
      checked(ComponentInstance::create(ComponentInstanceId("second"), "Second", p.id()));
  REQUIRE(
      assembly.add_component_instance(checked(second.with_transform({{100, 0, 0}, {0, 0, 0}}))));
  auto project = checked(Project::create(DocumentId("thread.project"), "Threads", assembly));
  REQUIRE(project.add_part_document(p));
  const auto saved = checked(serialize_project_to_json(project));
  project = checked(deserialize_project_from_json(saved));
  const auto verify = [&](const auto& scene) {
    std::vector<std::string> ids;
    std::vector<double> x;
    for (const auto& item : scene)
      if (item.kind == ViewportSceneKind::ThreadAnnotation) {
        ids.push_back(item.semantic_id);
        x.push_back(bounds(item.shape)[0]);
        CHECK(volume(item.shape) == 0);
      }
    REQUIRE(ids.size() == 4);
    std::sort(ids.begin(), ids.end());
    CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
    std::sort(x.begin(), x.end());
    CHECK(x[2] - x[0] == Catch::Approx(100).margin(1e-5));
    CHECK(x[3] - x[1] == Catch::Approx(100).margin(1e-5));
  };
  verify(checked(ViewportSceneBuilder{}.build_project(project)));
  verify(checked(ViewportSceneBuilder{}.build_project(
      project, [&](DocumentId) -> const ShapeCache* { return &cache; })));
  CHECK(checked(serialize_project_to_json(project)) == saved);
}

TEST_CASE("Imported cylinder modeled Threads and conical cosmetic Threads retain immutable bases",
          "[geometry][parity-b155][thread-combined][thread-import]") {
  for (bool taper : {false, true})
    for (auto hand : {ThreadHandedness::Right, ThreadHandedness::Left}) {
      CAPTURE(taper, hand == ThreadHandedness::Right);
      imported_body_test::Fixture input;
      const double slope = std::tan(-2 * std::numbers::pi / 180);
      input.write(taper ? BRepPrimAPI_MakeCone(5, 5 + 8 * slope, 8).Shape()
                        : BRepPrimAPI_MakeCylinder(5, 8).Shape());
      const auto supplier = input.source_bytes();
      auto p = input.editable();
      const auto seed = f::part();
      for (const auto& parameter : seed.parameters())
        REQUIRE(p.add_parameter(parameter));
      StepImportTopologyContext context(input.root);
      auto cache = evaluate(p, &context);
      const auto body = p.imported_body_features().front().definition().outputs().front().body;
      const auto original =
          brep(checked(cache.validated_imported_body_result(p, p.imported_body_features().front()))
                   ->imported.bodies.front()
                   .shape);
      double axis_z = 0;
      for (TopExp_Explorer it(*OcctShapeView::get(*cache.find_body_shape(body)), TopAbs_FACE);
           it.More(); it.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
        if (surface.GetType() == (taper ? GeomAbs_Cone : GeomAbs_Cylinder))
          axis_z = taper ? surface.Cone().Axis().Direction().Z()
                         : surface.Cylinder().Axis().Direction().Z();
      }
      REQUIRE(std::abs(axis_z) == Catch::Approx(1));
      const auto& catalog = p.imported_topology_catalogs().front();
      const ImportedTopologyEntry* wall = nullptr;
      for (const auto& entry : catalog.entries())
        if (entry.descriptor.kind == ImportedTopologyKind::Face &&
            entry.descriptor.geometry ==
                (taper ? ImportedGeometryClass::Opaque : ImportedGeometryClass::Cylinder))
          wall = &entry;
      REQUIRE(wall);
      auto support = checked(FaceReference::create_imported(
          PartFeatureInputRole::ThreadSupportFace,
          checked(ImportedTopologyReference::create(catalog.id(), wall->id,
                                                    ImportedTopologyCapability::Face))));
      const auto before_query = checked(serialize_part_document_to_json(p));
      const auto before_shape = brep(*cache.find_body_shape(body));
      for (bool reverse : {false, true}) {
        const bool positive_axis = (axis_z > 0) != reverse;
        const auto inspected = checked(
            ThreadAdapter::inspect_support(p, body, support, cache,
                                           positive_axis ? ThreadDirection::AlongSupportAxis
                                                         : ThreadDirection::OppositeSupportAxis,
                                           &context));
        CHECK(inspected.origin.x == Catch::Approx(0).margin(1e-6));
        CHECK(inspected.origin.y == Catch::Approx(0).margin(1e-6));
        CHECK(inspected.origin.z == Catch::Approx(reverse ? 8 : 0).margin(1e-6));
        CHECK(inspected.axis.z == Catch::Approx(reverse ? -1 : 1).margin(1e-7));
        CHECK(inspected.radius_mm ==
              Catch::Approx(reverse && taper ? 5 + 8 * slope : 5).margin(1e-6));
        CHECK(inspected.axial_length_mm == Catch::Approx(8).margin(1e-6));
        CHECK(inspected.taper_angle_deg ==
              Catch::Approx(taper ? (reverse ? 2 : -2) : 0).margin(1e-7));
      }
      CHECK(checked(serialize_part_document_to_json(p)) == before_query);
      CHECK(brep(*cache.find_body_shape(body)) == before_shape);
      CHECK(input.source_bytes() == supplier);
      const auto definition =
          f::definition(f::custom(taper), false, hand,
                        taper ? ThreadRepresentation::Cosmetic : ThreadRepresentation::Modeled);
      auto feature = checked(ThreadFeature::create(
          FeatureId("thread"), "Thread", body, support, definition,
          axis_z > 0 ? ThreadDirection::AlongSupportAxis : ThreadDirection::OppositeSupportAxis));
      REQUIRE(p.add_thread_feature(feature));
      cache = evaluate(p, &context);
      const double r1 = 5 + 8 * slope;
      const double expected = taper ? std::numbers::pi * 8 * (25 + 5 * r1 + r1 * r1) / 3
                                    : 200 * std::numbers::pi - removed(false, 1.7);
      CHECK(volume(*cache.find_body_shape(body)) == Catch::Approx(expected).margin(2e-4));
      if (taper) {
        REQUIRE(
            checked(ThreadAdapter::cosmetic_presentations(*cache.find_body_shape(body))).size() ==
            1);
        auto unsupported = p;
        REQUIRE(unsupported.update_thread_feature(checked(ThreadFeature::create(
            feature.id(), feature.name(), body, support,
            f::definition(f::custom(true), false, hand, ThreadRepresentation::Modeled),
            feature.direction()))));
        auto untouched = cache;
        const auto rejected =
            GeometryRecomputeExecutor{&context}.execute_document(unsupported, untouched);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().message().find("modeled tapered") != std::string::npos);
        CHECK(untouched.validate_complete_geometry_result(p));
      }
      imported_body_test::Fixture output;
      REQUIRE(StepExporter{&context}.write_part_step(p, cache, output.path()));
      const auto source = checked(StepImportSource::create(
          StepImportSourceId("roundtrip"), "Roundtrip", "supplier.step",
          checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
      const auto imported = checked(StepImportReader{}.read(source, output.root));
      REQUIRE(imported.body_count() == 1);
      CHECK(imported.products.front().bodies.front().volume_mm3 ==
            Catch::Approx(expected).margin(2e-4));
      CHECK(
          brep(checked(cache.validated_imported_body_result(p, p.imported_body_features().front()))
                   ->imported.bodies.front()
                   .shape) == original);
      CHECK(input.source_bytes() == supplier);
    }
}

TEST_CASE("Thread support inspection rejects changed supplier bytes on an already resolved context",
          "[geometry][parity-b157][thread-support-query][thread-import]") {
  imported_body_test::Fixture input;
  input.write(BRepPrimAPI_MakeCylinder(5, 8).Shape());
  const auto supplier = input.source_bytes();
  const auto part = input.editable();
  StepImportTopologyContext context(input.root);
  const auto cache = evaluate(part, &context);
  const auto body = part.bodies().front().id();
  const auto& catalog = part.imported_topology_catalogs().front();
  const ImportedTopologyEntry* wall = nullptr;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        entry.descriptor.geometry == ImportedGeometryClass::Cylinder)
      wall = &entry;
  REQUIRE(wall);
  const auto support = checked(FaceReference::create_imported(
      PartFeatureInputRole::ThreadSupportFace,
      checked(ImportedTopologyReference::create(catalog.id(), wall->id,
                                                ImportedTopologyCapability::Face))));
  REQUIRE(ThreadAdapter::inspect_support(part, body, support, cache,
                                         ThreadDirection::AlongSupportAxis, &context));
  const auto before = checked(serialize_part_document_to_json(part));
  const auto shape = brep(*cache.find_body_shape(body));
  input.write_bytes(supplier + "\n");
  CHECK_FALSE(ThreadAdapter::inspect_support(part, body, support, cache,
                                             ThreadDirection::AlongSupportAxis, &context));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(brep(*cache.find_body_shape(body)) == shape);
  input.write_bytes(supplier);
  StepImportTopologyContext restored(input.root);
  CHECK(ThreadAdapter::inspect_support(part, body, support, cache,
                                       ThreadDirection::AlongSupportAxis, &restored));
}

TEST_CASE("Imported Thread inspection includes the current terminal Body placement",
          "[geometry][parity-b157][thread-support-query][thread-import]") {
  imported_body_test::Fixture input;
  input.write(BRepPrimAPI_MakeCylinder(5, 8).Shape());
  auto part = input.editable();
  const auto body = part.bodies().front().id();
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("move"), body, BodyTransformCoordinateSpace::World, {}, {12, -5, 10}, false,
      false))));
  StepImportTopologyContext context(input.root);
  const auto cache = evaluate(part, &context);
  const auto& catalog = part.imported_topology_catalogs().front();
  const ImportedTopologyEntry* wall = nullptr;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        entry.descriptor.geometry == ImportedGeometryClass::Cylinder)
      wall = &entry;
  REQUIRE(wall);
  const auto reference = checked(FaceReference::create_imported(
      PartFeatureInputRole::ThreadSupportFace,
      checked(ImportedTopologyReference::create(catalog.id(), wall->id,
                                                ImportedTopologyCapability::Face))));
  const auto before = checked(serialize_part_document_to_json(part));
  for (auto direction : {ThreadDirection::AlongSupportAxis, ThreadDirection::OppositeSupportAxis}) {
    const auto inspected =
        checked(ThreadAdapter::inspect_support(part, body, reference, cache, direction, &context));
    CHECK(inspected.origin.x == Catch::Approx(12).margin(1e-6));
    CHECK(inspected.origin.y == Catch::Approx(-5).margin(1e-6));
    CHECK(std::abs(inspected.axis.z) == Catch::Approx(1).margin(1e-7));
    CHECK(inspected.origin.z == Catch::Approx(inspected.axis.z > 0 ? 10 : 18).margin(1e-6));
    CHECK(inspected.radius_mm == Catch::Approx(5).margin(1e-6));
    CHECK(inspected.axial_length_mm == Catch::Approx(8).margin(1e-6));
  }
  CHECK(checked(serialize_part_document_to_json(part)) == before);
}

TEST_CASE("Explicit modeled switches return both Thread consumers to identical cosmetic intent",
          "[geometry][parity-b155][thread-combined]") {
  auto p = f::pair_part();
  const auto cosmetic = f::definition(f::custom(), true);
  const auto modeled =
      f::definition(f::custom(), true, ThreadHandedness::Right, ThreadRepresentation::Modeled);
  REQUIRE(p.add_hole_feature(f::internal(cosmetic)));
  REQUIRE(p.add_thread_feature(f::external(cosmetic)));
  const auto original = checked(serialize_part_document_to_json(p));
  auto cache = evaluate(p);
  const auto base = brep(*cache.find_body_shape(BodyId("body.base")));
  const auto shaft = brep(*cache.find_body_shape(BodyId("body.shaft")));
  REQUIRE(p.update_hole_feature(f::internal(modeled)));
  REQUIRE(p.update_thread_feature(f::external(modeled)));
  cache = evaluate(p);
  CHECK(volume(*cache.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(2560 - 96 * std::numbers::pi - removed(true, 1.7)).margin(2e-4));
  CHECK(checked(ThreadAdapter::cosmetic_presentations(*cache.find_body_shape(BodyId("body.shaft"))))
            .empty());
  REQUIRE(p.update_hole_feature(f::internal(cosmetic)));
  REQUIRE(p.update_thread_feature(f::external(cosmetic)));
  cache = evaluate(p);
  CHECK(checked(serialize_part_document_to_json(p)) == original);
  CHECK(brep(*cache.find_body_shape(BodyId("body.base"))) == base);
  CHECK(brep(*cache.find_body_shape(BodyId("body.shaft"))) == shaft);
  CHECK(checked(ThreadAdapter::cosmetic_presentations(*cache.find_body_shape(BodyId("body.shaft"))))
            .size() == 1);
}

TEST_CASE("A rotated shaft retains the authored thread axis seam pitch and handedness",
          "[geometry][parity-b155][thread-combined]") {
  auto p = f::pair_part();
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("rotate"), BodyId("body.shaft"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({0, 0, 0}, {0, 1, 0})), 90,
      false, false))));
  REQUIRE(p.add_thread_feature(f::external(
      f::definition(f::custom(), true, ThreadHandedness::Right, ThreadRepresentation::Modeled))));
  const auto cache = evaluate(p);
  const auto& shape = *cache.find_body_shape(BodyId("body.shaft"));
  CHECK(volume(shape) == Catch::Approx(200 * std::numbers::pi - removed(false, 1.7)).margin(2e-4));
  for (double z : {.35, 1.05, 2.7})
    for (double theta : {.23, 2.39, 5.31})
      for (double r : {4.25, 4.75}) {
        const double phase = std::remainder(z - 1.7 * theta / (2 * std::numbers::pi), 1.7);
        const double width = (r - 4) * std::tan(55 * std::numbers::pi / 360);
        if (std::abs(std::abs(phase) - width) < .025)
          continue;
        const gp_Pnt point(1 + z, r * std::sin(theta), -30 - r * std::cos(theta));
        CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(shape), point, 1e-7).State() ==
              (std::abs(phase) < width ? TopAbs_OUT : TopAbs_IN));
      }
}
