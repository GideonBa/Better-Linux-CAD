#include "../support/hole_feature_fixture.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "imported_body_test_fixture.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pln.hxx>

#include <catch2/generators/catch_generators.hpp>
#include <numbers>

using namespace imported_body_test;

TEST_CASE("Editable imported base keeps ordered Solid and Surface outputs in normal feature caches",
          "[geometry][imported-body-recompute]") {
  Fixture fixture;
  const auto* unit = GENERATE("MM", "INCH");
  fixture.write(
      compound(
          {BRepPrimAPI_MakeBox(20, 20, 10).Shape(),
           BRepPrimAPI_MakeBox(gp_Pnt(40, 0, 0), 2, 3, 4).Shape(),
           BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(80, 0, 0), gp_Dir(0, 0, 1)), 0, 7, 0, 9).Face()}),
      unit);
  const auto source_bytes = fixture.source_bytes();
  const auto document = fixture.editable(true);
  const auto base = document.imported_body_features().front();
  const auto json = checked(serialize_part_document_to_json(document));
  auto shapes = cache();
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(document, shapes));
  CHECK(shapes.feature_shape_count() == 0);
  StepImportTopologyContext context(fixture.root);
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_document(document, shapes))
            .executed_feature_count == 1);
  CHECK(shapes.reference_result() == nullptr);
  CHECK(shapes.feature_shape_count() == 1);
  CHECK(shapes.body_shape_count() == 3);
  CHECK_FALSE(shapes.final_shape());
  const auto* evidence = checked(shapes.validated_imported_body_result(document, base));
  REQUIRE(evidence->imported.bodies.size() == 3);
  double solids = 0;
  int surfaces = 0;
  for (std::size_t index = 0; index < base.definition().outputs().size(); ++index) {
    const auto& output = base.definition().outputs()[index];
    CHECK(evidence->imported.bodies[index].output == output);
    const auto* result = shapes.find_body_result(output.body);
    REQUIRE(result);
    CHECK(result->source_feature_id == base.id());
    REQUIRE(shapes.find_imported_body_shape(base.id(), output.body));
    const auto summary = RectangleExtrusionAdapter{}.summarize(result->shape);
    if (output.kind == BodyKind::Solid) {
      CHECK(summary.solid_count == 1);
      solids += summary.volume_mm3;
    } else {
      CHECK(summary.solid_count == 0);
      ++surfaces;
    }
  }
  CHECK(solids == Approx(4024));
  CHECK(surfaces == 1);
  auto reopened = checked(deserialize_part_document_from_json(json));
  CHECK(checked(serialize_part_document_to_json(reopened)) == json);
  auto reopened_shapes = cache();
  StepImportTopologyContext reopened_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&reopened_context}.execute_document(reopened, reopened_shapes));
  CHECK(checked(reopened_shapes.validated_imported_body_result(
                    reopened, reopened.imported_body_features().front()))
            ->imported.definition.outputs() == base.definition().outputs());
  CHECK(fixture.source_bytes() == source_bytes);
}

TEST_CASE("Incremental imported base rejects changed missing and accepted stale sources atomically",
          "[geometry][imported-body-recompute]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  auto shapes = cache();
  StepImportTopologyContext initial(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&initial}.execute_document(document, shapes));
  document.mark_all_clean();
  const auto empty = checked(document.create_recompute_plan());
  REQUIRE(empty.empty());
  const auto baseline = topology(*shapes.find_feature_shape(base.id()));
  const auto body = base.definition().outputs().front().body;
  const auto body_topology = topology(*shapes.find_body_shape(body));
  const auto original = fixture.source_bytes();
  const auto original_json = checked(serialize_part_document_to_json(document));
  SECTION("no explicit root context") {
    CHECK_FALSE(GeometryRecomputeExecutor{}.execute_plan(document, empty, shapes));
  }
  SECTION("changed unaccepted bytes") {
    fixture.write_bytes(original + "\n/* unaccepted */\n");
    StepImportTopologyContext current(fixture.root);
    CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_plan(document, empty, shapes));
  }
  SECTION("missing source") {
    REQUIRE(std::filesystem::remove(fixture.path()));
    StepImportTopologyContext current(fixture.root);
    CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_plan(document, empty, shapes));
  }
  SECTION("accepted source requires its dependency plan") {
    fixture.write(BRepPrimAPI_MakeBox(20, 20, 12).Shape());
    accept_current(document, fixture);
    const auto candidate_json = checked(serialize_part_document_to_json(document));
    StepImportTopologyContext current(fixture.root);
    CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_plan(document, empty, shapes));
    CHECK(checked(serialize_part_document_to_json(document)) == candidate_json);
    CHECK(topology(*shapes.find_feature_shape(base.id())) == baseline);
    auto plan = checked(document.create_recompute_plan());
    REQUIRE(plan.contains(base.id().value()));
    auto updated =
        checked(GeometryRecomputeExecutor{&current}.execute_plan(document, plan, shapes));
    CHECK(updated.executed_feature_count == 1);
    CHECK(body_volume(shapes, body) == Approx(4800));
    CHECK(shapes.find_body_result(body)->source_feature_id == base.id());
    CHECK(checked(shapes.validated_imported_body_result(document, base))->imported.source ==
          document.step_import_sources().front());
    return;
  }
  CHECK(topology(*shapes.find_feature_shape(base.id())) == baseline);
  CHECK(topology(*shapes.find_body_shape(body)) == body_topology);
  CHECK(body_volume(shapes, body) == Approx(4000));
  CHECK(shapes.feature_shape_count() == 1);
  CHECK(shapes.body_shape_count() == 1);
  CHECK(checked(serialize_part_document_to_json(document)) == original_json);
}

TEST_CASE("Lost and Ambiguous editable base recovery preserves prior cache and model intent",
          "[geometry][imported-body-recompute]") {
  Fixture fixture;
  const auto box = BRepPrimAPI_MakeBox(20, 20, 10).Shape();
  fixture.write(box);
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto& output = base.definition().outputs().front();
  auto shapes = cache();
  StepImportTopologyContext initial(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&initial}.execute_document(document, shapes));
  document.mark_all_clean();
  const auto before = topology(*shapes.find_body_shape(output.body));
  auto status = ImportedTopologyRecoveryStatus::Lost;
  SECTION("removed base") {
    fixture.write(BRepPrimAPI_MakeBox(gp_Pnt(100, 200, 300), 2, 3, 4).Shape());
  }
  SECTION("indistinguishable alternative bodies") {
    fixture.write(compound({box, BRepBuilderAPI_Copy(box).Shape()}));
    status = ImportedTopologyRecoveryStatus::Ambiguous;
  }
  accept_current(document, fixture);
  const auto candidate_json = checked(serialize_part_document_to_json(document));
  StepImportTopologyContext current(fixture.root);
  const auto* report = checked(current.recover(document, base.definition().catalog()));
  REQUIRE(report->find(output.imported_body));
  CHECK(report->find(output.imported_body)->status == status);
  CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_document(document, shapes));
  CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_plan(
      document, checked(document.create_recompute_plan()), shapes));
  CHECK(topology(*shapes.find_body_shape(output.body)) == before);
  CHECK(body_volume(shapes, output.body) == Approx(4000));
  CHECK(checked(serialize_part_document_to_json(document)) == candidate_json);
}

TEST_CASE("Imported base evidence rejects another owner and clears with feature/cache removal",
          "[geometry][imported-body-recompute]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  auto other = fixture.editable(false, "other.part");
  CHECK_FALSE(shapes.validated_imported_body_result(other, other.imported_body_features().front()));
  REQUIRE(shapes.remove_feature_shape(base.id()));
  CHECK_FALSE(shapes.find_imported_body_result(base.id()));
  CHECK_FALSE(shapes.find_imported_body_shape(base.id(), base.definition().outputs().front().body));
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  shapes.clear();
  CHECK_FALSE(shapes.find_imported_body_result(base.id()));
  CHECK(shapes.feature_shape_count() == 0);
  CHECK(shapes.body_shape_count() == 0);
}

TEST_CASE("Removed imported base cannot survive full or empty-plan recompute in a previous cache",
          "[geometry][imported-body-recompute]") {
  const bool incremental = GENERATE(false, true);
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  REQUIRE(document.remove_imported_body_feature(base.id()));
  REQUIRE(document.find_body(body));
  document.mark_all_clean();
  const auto plan = checked(document.create_recompute_plan());
  REQUIRE(plan.empty());
  const auto intent = checked(serialize_part_document_to_json(document));
  const auto result = incremental
                          ? GeometryRecomputeExecutor{&context}.execute_plan(document, plan, shapes)
                          : GeometryRecomputeExecutor{&context}.execute_document(document, shapes);
  REQUIRE(result);
  CHECK(result.value().executed_feature_count == 0);
  CHECK_FALSE(shapes.find_imported_body_result(base.id()));
  CHECK_FALSE(shapes.find_feature_shape(base.id()));
  CHECK_FALSE(shapes.find_body_shape(body));
  CHECK_FALSE(shapes.final_shape());
  CHECK(shapes.feature_shape_count() == 0);
  CHECK(shapes.body_shape_count() == 0);
  const auto inspection = checked(BodyResultInspector{}.inspect(document, shapes));
  REQUIRE(inspection.bodies.size() == 1);
  CHECK_FALSE(inspection.bodies.front().shape_summary);
  CHECK_FALSE(BodyResultInspector{}.resolve_compatible_final_shape(document, shapes));
  CHECK(checked(serialize_part_document_to_json(document)) == intent);
}

TEST_CASE("Deleted imported modifiers fail closed until an explicit clean cache rebuild",
          "[geometry][imported-body-recompute]") {
  const bool incremental = GENERATE(false, true);
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  for (const auto& [id, value] :
       std::vector<std::pair<std::string, double>>{{"width", 4}, {"height", 4}, {"depth", 14}})
    REQUIRE(document.add_parameter(length(id, value)));
  auto sketch = checked(Sketch::create(SketchId("boss.sketch"), "Boss", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("boss.profile"), ParameterId("width"), ParameterId("height"), {10, 10}))));
  REQUIRE(document.add_sketch(sketch));
  auto boss = checked(Feature::create_additive_extrude(FeatureId("boss"), "Boss", sketch.id(),
                                                       ParameterId("depth")));
  boss = checked(boss.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, body, std::nullopt))));
  REQUIRE(document.add_feature(boss));
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto previous = topology(*shapes.find_body_shape(body));
  CHECK(body_volume(shapes, body) == Approx(4064));
  REQUIRE(document.remove_feature(boss.id()));
  REQUIRE(document.remove_imported_body_feature(base.id()));
  document.mark_all_clean();
  const auto plan = checked(document.create_recompute_plan());
  REQUIRE(plan.empty());
  const auto intent = checked(serialize_part_document_to_json(document));
  const auto result = incremental
                          ? GeometryRecomputeExecutor{&context}.execute_plan(document, plan, shapes)
                          : GeometryRecomputeExecutor{&context}.execute_document(document, shapes);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("deleted modifier") != std::string::npos);
  CHECK(topology(*shapes.find_body_shape(body)) == previous);
  CHECK(body_volume(shapes, body) == Approx(4064));
  CHECK(shapes.feature_shape_count() == 2);
  CHECK(shapes.final_feature_id() == boss.id());
  CHECK(checked(serialize_part_document_to_json(document)) == intent);
  auto replacement = checked(Feature::create_additive_extrude(
      FeatureId("replacement"), "Replacement", sketch.id(), ParameterId("depth")));
  replacement = checked(replacement.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, body))));
  REQUIRE(document.add_feature(replacement));
  CHECK_FALSE(GeometryRecomputeExecutor{&context}.execute_additive_extrude(
      document, replacement.id(), shapes));
  CHECK(topology(*shapes.find_body_shape(body)) == previous);
  // An explicitly new cache never republishes the removed authority.
  auto clean = cache();
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, clean));
  CHECK_FALSE(clean.find_imported_body_result(base.id()));
  CHECK(clean.find_body_result(body)->source_feature_id == replacement.id());
  CHECK(checked(serialize_part_document_to_json(document)) != intent);
}

TEST_CASE("Reused imported base identity cannot retain a previous output Body cache",
          "[geometry][imported-body-recompute]") {
  const bool incremental = GENERATE(false, true);
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto original = document.imported_body_features().front();
  const auto output = original.definition().outputs().front();
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto before = topology(*shapes.find_body_shape(output.body));
  REQUIRE(document.remove_imported_body_feature(original.id()));
  const BodyId replacement_body("replacement.body");
  REQUIRE(document.add_body(checked(Body::create(replacement_body, "Replacement", output.kind))));
  auto definition = checked(ImportedPartDefinition::create(
      original.definition().source(), original.definition().catalog(),
      {{output.imported_body, replacement_body, output.kind}}));
  REQUIRE(document.add_imported_body_feature(
      checked(ImportedBodyFeature::create(original.id(), "Rebound base", definition))));
  document.mark_all_clean();
  const auto plan = checked(document.create_recompute_plan());
  REQUIRE(plan.empty());
  const auto intent = checked(serialize_part_document_to_json(document));
  const auto result = incremental
                          ? GeometryRecomputeExecutor{&context}.execute_plan(document, plan, shapes)
                          : GeometryRecomputeExecutor{&context}.execute_document(document, shapes);
  REQUIRE_FALSE(result);
  CHECK(topology(*shapes.find_body_shape(output.body)) == before);
  CHECK_FALSE(shapes.find_body_shape(replacement_body));
  CHECK(shapes.feature_shape_count() == 1);
  CHECK(shapes.body_shape_count() == 1);
  CHECK(shapes.final_feature_id() == original.id());
  CHECK(body_volume(shapes, output.body) == Approx(4000));
  CHECK(checked(serialize_part_document_to_json(document)) == intent);
  auto clean = cache();
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, clean));
  CHECK_FALSE(clean.find_body_shape(output.body));
  CHECK(body_volume(clean, replacement_body) == Approx(4000));
}

TEST_CASE(
    "Imported Hole support keeps catalog identity portable history and actual source freshness",
    "[geometry][parity-b152][hole-combined-acceptance][hole-import-combined]") {
  const bool active = GENERATE(false, true);
  const bool posed = GENERATE(false, true);
  const bool bounded = GENERATE(false, true);
  const bool repeated = GENERATE(false, true);
  namespace holes = blcad::test::hole_fixture;
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 10, 8).Shape());
  const auto supplier = fixture.source_bytes();
  auto part = fixture.editable();
  const auto& catalog = part.imported_topology_catalogs().front();
  const ImportedTopologyEntry* top = nullptr;
  for (const auto& entry : catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        entry.descriptor.geometry == ImportedGeometryClass::Plane &&
        (!top || entry.descriptor.centroid.z > top->descriptor.centroid.z))
      top = &entry;
  REQUIRE(top);
  CHECK(top->descriptor.centroid.z == Approx(8));
  const auto plane = checked(
      ImportedTopologyReference::create(catalog.id(), top->id, ImportedTopologyCapability::Plane));
  const auto support =
      checked(PlaneReference::create_imported(PartFeatureInputRole::HoleSupportPlane, plane));
  const auto body = part.imported_body_features().front().definition().outputs().front().body;
  HoleExtent extent = HoleThroughAllExtent{};
  if (bounded) {
    const auto bottom = std::min_element(
        catalog.entries().begin(), catalog.entries().end(), [](const auto& a, const auto& b) {
          const auto score = [](const auto& entry) {
            return entry.descriptor.kind == ImportedTopologyKind::Face &&
                           entry.descriptor.geometry == ImportedGeometryClass::Plane
                       ? entry.descriptor.centroid.z
                       : 1e100;
          };
          return score(a) < score(b);
        });
    REQUIRE(bottom != catalog.entries().end());
    const auto limit = checked(ImportedTopologyReference::create(catalog.id(), bottom->id,
                                                                 ImportedTopologyCapability::Face));
    extent = HoleToFaceExtent{
        checked(FaceReference::create_imported(PartFeatureInputRole::HoleLimitFace, limit))};
  }
  if (posed)
    REQUIRE(part.add_body_transform(checked(BodyTransform::create_rotate(
        BodyTransformId("pose"), body, BodyTransformCoordinateSpace::World, std::nullopt,
        checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0})), 90, false, false))));
  REQUIRE(part.add_engineering_resource(holes::table()));
  REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
      FeatureId("hole"), "Imported Hole", HoleKind::Clearance, body, support,
      {{HolePlacementId("center"), Point2{-5, 5}}}, holes::standard(), extent))));
  if (repeated)
    REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
        FeatureId("hole.second"), "Second Hole", HoleKind::Clearance, body, support,
        {{HolePlacementId("center"), Point2{-5, 15}}}, holes::standard(), extent))));
  std::vector<std::string> order{"import.base"};
  if (posed)
    order.push_back("pose");
  order.push_back("hole");
  if (repeated)
    order.push_back("hole.second");
  std::vector<std::string> suppressed;
  if (!active) {
    suppressed.push_back("hole");
    if (repeated)
      suppressed.push_back("hole.second");
  }
  REQUIRE(part.apply_history_intent({order, suppressed}));
  const auto serialized = checked(serialize_part_document_to_json(part));
  auto reopened = checked(deserialize_part_document_from_json(serialized));
  const auto& restored =
      std::get<ImportedTopologyReference>(reopened.hole_features().front().support().source());
  CHECK(restored == plane);
  CHECK(checked(serialize_part_document_to_json(reopened)) == serialized);
  StepImportTopologyContext context(fixture.root);
  auto shapes = cache();
  const auto recomputed = GeometryRecomputeExecutor{&context}.execute_document(reopened, shapes);
  INFO(active);
  INFO(posed);
  INFO((recomputed ? "ok" : recomputed.error().message()));
  REQUIRE(recomputed);
  REQUIRE(shapes.validate_complete_geometry_result(reopened));
  const double expected_volume =
      1600 - (active ? (repeated ? 2 : 1) * std::numbers::pi * 3.3 * 3.3 * 8 : 0);
  CHECK(body_volume(shapes, body) == Approx(expected_volume).margin(1e-6));
  const auto wall = checked(ResultFaceReference::create(
      FeatureId("hole"), body, {{FeatureId("hole"), "hole/6:center/wall"}}));
  if (active) {
    REQUIRE(ResultFaceSupportGeometry::resolve_face(reopened, shapes, wall, &context));
    CHECK_FALSE(ResultFaceSupportGeometry::resolve_face(reopened, shapes, wall));
  }
  const auto original = topology(*shapes.find_body_shape(body));
  fixture.write_bytes(supplier + "\n");
  StepImportTopologyContext changed(fixture.root);
  if (active)
    CHECK_FALSE(ResultFaceSupportGeometry::resolve_face(reopened, shapes, wall, &changed));
  CHECK_FALSE(GeometryRecomputeExecutor{&changed}.execute_document(reopened, shapes));
  CHECK(topology(*shapes.find_body_shape(body)) == original);
  CHECK(checked(serialize_part_document_to_json(reopened)) == serialized);
  fixture.write_bytes(supplier);
  StepImportTopologyContext recovered(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&recovered}.execute_document(reopened, shapes));
  REQUIRE(shapes.validate_complete_geometry_result(reopened));
  CHECK(body_volume(shapes, body) == Approx(expected_volume).margin(1e-6));
  CHECK(fixture.source_bytes() == supplier);
  const auto& base = part.imported_body_features().front();
  const auto* source = part.find_step_import_source(base.definition().source());
  REQUIRE(source);
  auto reference_source = checked(
      StepImportSource::create(source->id(), source->name(), source->project_relative_asset_path(),
                               source->sha256_digest(), StepImportMode::Reference));
  auto reference = checked(
      PartDocument::create_reference(DocumentId("readonly"), "Reference", reference_source,
                                     part.imported_topology_catalogs().front(), base.definition()));
  const auto reference_json = checked(serialize_part_document_to_json(reference));
  CHECK_FALSE(reference.add_hole_feature(part.hole_features().front()));
  CHECK(checked(serialize_part_document_to_json(reference)) == reference_json);
}

TEST_CASE("Semantic Hole export and history preserve an explicit second Body and hidden mass",
          "[geometry][parity-b153][hole-combined-acceptance]") {
  namespace holes = blcad::test::hole_fixture;
  namespace history = blcad::test::history_fixture;
  auto part = history::native_base();
  auto second = checked(Feature::create_additive_extrude(
      FeatureId("second"), "Second", SketchId("profile"), ParameterId("depth")));
  REQUIRE(part.add_feature(
      checked(second.with_body_result_context(history::new_body(part, "second")))));
  REQUIRE(part.add_engineering_resource(holes::table()));
  blcad::test::engineering_fixture::annotate(part);
  REQUIRE(part.set_body_material(
      BodyId("body.second"), blcad::test::engineering_fixture::key("synthetic.material", "r2")));
  REQUIRE(part.add_hole_feature(holes::hole("hole", holes::standard())));
  REQUIRE(part.apply_history_intent({{"base", "second", "hole"}, {}}));
  auto shapes = cache();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, shapes));
  const auto summary = [&](const BodyId& id) {
    return RectangleExtrusionAdapter{}.summarize(*shapes.find_body_shape(id)).volume_mm3;
  };
  const double cut = 1600 - std::numbers::pi * 3.3 * 3.3 * 8;
  CHECK(summary(BodyId("body.base")) == Approx(cut));
  CHECK(summary(BodyId("body.second")) == Approx(1600));
  REQUIRE(part.set_body_visibility(BodyId("body.second"), BodyVisibility::Hidden));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, shapes));
  const auto physical = checked(BodyResultInspector{}.inspect_physical_properties(part));
  CHECK(physical.data().volume_mm3 == Approx(cut + 1600));
  CHECK(physical.data().complete_mass()->mass_kg == Approx(cut * 4e-6 + 1600 * 6e-6));
  Fixture output;
  const auto exported = checked(StepExporter{}.write_part_step(part, shapes, output.path()));
  CHECK(exported.exported_body_count == 1);
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("export"), "Export", "supplier.step",
      checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
  const auto imported = checked(StepImportReader{}.read(source, output.root));
  CHECK(imported.body_count() == 1);
  CHECK(imported.products.front().bodies.front().volume_mm3 == Approx(cut).margin(1e-6));
  const auto before = checked(serialize_part_document_to_json(part));
  CHECK_FALSE(part.apply_history_intent({{"hole", "base", "second"}, {}}));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  REQUIRE(part.apply_history_intent({{"base", "second", "hole"}, {"hole"}}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, shapes));
  CHECK(summary(BodyId("body.base")) == Approx(1600));
  CHECK(summary(BodyId("body.second")) == Approx(1600));
  REQUIRE(part.apply_history_intent({{"base", "second", "hole"}, {}}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, shapes));
  CHECK(summary(BodyId("body.base")) == Approx(cut));
  CHECK(summary(BodyId("body.second")) == Approx(1600));
}
