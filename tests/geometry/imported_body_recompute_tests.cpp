#include "blcad/geometry/body_result_inspector.hpp"
#include "imported_body_test_fixture.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pln.hxx>

#include <catch2/generators/catch_generators.hpp>

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
