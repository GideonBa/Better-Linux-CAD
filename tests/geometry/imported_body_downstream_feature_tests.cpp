#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "imported_body_test_fixture.hpp"

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <utility>

using namespace imported_body_test;

namespace {
PartDocument command_import(const Fixture& fixture, bool from_reference) {
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("supplier"), "Supplier", "supplier.step",
      checked(step_import_sha256(fixture.source_bytes())),
      from_reference ? StepImportMode::Reference : StepImportMode::EditableBody));
  const auto imported = checked(StepImportReader{}.read(source, fixture.root));
  REQUIRE(imported.products.size() == 1);
  const auto catalog = checked(
      StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), imported, 0));
  if (!from_reference)
    return checked(StepEditableCommands{}.create_definition(
        source, catalog, DocumentId("editable.direct"), "Direct editable", FeatureId("import.base"),
        fixture.root));
  const auto reference = checked(StepReferenceCommands{}.create_definition(
      source, catalog, DocumentId("reference.original"), "Original Reference", fixture.root));
  const auto original = checked(serialize_part_document_to_json(reference));
  const auto converted = checked(StepReferenceCommands{}.convert(
      reference, DocumentId("editable.converted"), "Converted editable", FeatureId("import.base"),
      fixture.root));
  CHECK(converted.part.id() != reference.id());
  CHECK_FALSE(converted.part.is_reference_part());
  CHECK(reference.is_reference_part());
  CHECK(reference.imported_body_features().empty());
  CHECK(converted.part.imported_body_features().front().definition().outputs().front().body !=
        reference.reference_part_definition()->outputs().front().body);
  CHECK(checked(serialize_part_document_to_json(reference)) == original);
  return converted.part;
}

void rectangle(PartDocument& document, const std::string& id, Point2 center, double width,
               double height) {
  REQUIRE(document.add_parameter(length(id + ".width", width)));
  REQUIRE(document.add_parameter(length(id + ".height", height)));
  auto sketch = checked(Sketch::create(SketchId("sketch." + id), id, DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(RectangleProfile::create(ProfileId("profile." + id), ParameterId(id + ".width"),
                                       ParameterId(id + ".height"), center))));
  REQUIRE(document.add_sketch(sketch));
}
void add_boss(PartDocument& document, const BodyId& body) {
  rectangle(document, "boss", {10, 10}, 4, 4);
  REQUIRE(document.add_parameter(length("boss.depth", 14)));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("boss"), "Boss", SketchId("sketch.boss"), ParameterId("boss.depth")));
  feature = checked(feature.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, body, std::nullopt))));
  REQUIRE(document.add_feature(feature));
}
void add_cut(PartDocument& document, const BodyId& body, FeatureId target = FeatureId("boss")) {
  REQUIRE(document.add_parameter(length("cut.width", 4)));
  REQUIRE(document.add_parameter(length("cut.height", 4)));
  auto sketch = checked(Sketch::create(SketchId("sketch.cut"), "Cut", DatumPlaneId("datum.xy")));
  const std::array<Point2, 4> corners{{{13, 13}, {17, 13}, {17, 17}, {13, 17}}};
  std::vector<SketchEntityId> lines;
  for (std::size_t index = 0; index < corners.size(); ++index) {
    const SketchEntityId id("cut.edge." + std::to_string(index));
    REQUIRE(sketch.add_entity(
        checked(LineSegment::create(id, corners[index], corners[(index + 1) % corners.size()]))));
    lines.push_back(id);
  }
  // The existing SubtractiveExtrude contract consumes a ClosedProfile rather
  // than a RectangleProfile. Real dimensions keep the Cut's parameter edges.
  REQUIRE(sketch.add_dimension(checked(SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("cut.width.dimension"),
      checked(SketchReferenceTarget::create_line_segment_start(lines[0])),
      checked(SketchReferenceTarget::create_line_segment_end(lines[0])),
      ParameterId("cut.width")))));
  REQUIRE(sketch.add_dimension(checked(SketchDrivingDimension::create_vertical_distance(
      SketchDimensionId("cut.height.dimension"),
      checked(SketchReferenceTarget::create_line_segment_start(lines[1])),
      checked(SketchReferenceTarget::create_line_segment_end(lines[1])),
      ParameterId("cut.height")))));
  REQUIRE(sketch.add_dimension(checked(SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("cut.width.opposite.dimension"),
      checked(SketchReferenceTarget::create_line_segment_start(lines[2])),
      checked(SketchReferenceTarget::create_line_segment_end(lines[2])),
      ParameterId("cut.width")))));
  REQUIRE(sketch.add_dimension(checked(SketchDrivingDimension::create_vertical_distance(
      SketchDimensionId("cut.height.opposite.dimension"),
      checked(SketchReferenceTarget::create_line_segment_start(lines[3])),
      checked(SketchReferenceTarget::create_line_segment_end(lines[3])),
      ParameterId("cut.height")))));
  REQUIRE(sketch.add_profile(checked(ClosedProfile::create(ProfileId("profile.cut"), lines))));
  REQUIRE(document.add_sketch(sketch));
  auto feature = checked(Feature::create_subtractive_extrude(
      FeatureId("cut"), "Rectangular pocket", SketchId("sketch.cut"), std::move(target)));
  feature = checked(feature.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, body, std::nullopt))));
  REQUIRE(document.add_feature(feature));
}
void add_hole(PartDocument& document, const BodyId& body, FeatureId target = FeatureId("cut")) {
  REQUIRE(document.add_parameter(length("hole.diameter", 2)));
  auto sketch = checked(Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(
      CircleProfile::create(ProfileId("profile.hole"), ParameterId("hole.diameter"), {5, 5}))));
  REQUIRE(document.add_sketch(sketch));
  auto feature = checked(Feature::create_subtractive_extrude(FeatureId("hole"), "Hole", sketch.id(),
                                                             std::move(target)));
  feature = checked(feature.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, body, std::nullopt))));
  REQUIRE(document.add_feature(feature));
}
void add_imported_fillet(PartDocument& document, const BodyId& body) {
  const auto& catalog = document.imported_topology_catalogs().front();
  const auto edge =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
        const auto& d = entry.descriptor;
        return d.kind == ImportedTopologyKind::Edge && d.geometry == ImportedGeometryClass::Line &&
               std::abs(d.centroid.x) < 1e-8 && std::abs(d.centroid.y) < 1e-8 &&
               std::abs(d.centroid.z - 5) < 1e-8;
      });
  REQUIRE(edge != catalog.entries().end());
  const auto reference = checked(EdgeReference::create_imported(
      PartFeatureInputRole::FilletEdge,
      checked(ImportedTopologyReference::create(catalog.id(), edge->id,
                                                ImportedTopologyCapability::Edge))));
  REQUIRE(document.add_parameter(length("fillet.radius", 0.5)));
  REQUIRE(document.add_fillet_feature(
      checked(FilletFeature::create(FeatureId("fillet"), "Imported edge fillet", body, {reference},
                                    ParameterId("fillet.radius")))));
}
double expected(double boss_depth = 14, double hole_diameter = 2, double radius = 0.5) {
  return 4000 + 16 * (boss_depth - 10) - 160 -
         std::numbers::pi * hole_diameter * hole_diameter * 10 / 4 -
         10 * radius * radius * (1 - std::numbers::pi / 4);
}
void reexport(const PartDocument& document, const ShapeCache& shapes, const Fixture& fixture,
              double expected_volume) {
  StepImportTopologyContext context(fixture.root);
  const auto path = fixture.root / "modeled.step";
  REQUIRE(StepExporter{&context}.write_document_step(document, shapes, path.string()));
  auto source = checked(StepImportSource::create(
      StepImportSourceId("reexport"), "Modeled result", "modeled.step",
      checked(step_import_sha256(bytes(path))), StepImportMode::EditableBody));
  const auto imported = checked(StepImportReader{}.read(source, fixture.root));
  const auto preview = checked(StepImportReader{}.preview_shapes(imported));
  REQUIRE(preview.size() == 1);
  CHECK(volume(preview.front()) == Approx(expected_volume).margin(1e-5));
  const auto* result = shapes.find_body_shape(document.bodies().front().id());
  REQUIRE(result);
  const auto actual_bounds = checked(BodyTransformAdapter{}.bounds(*result));
  const auto readback_bounds = checked(BodyTransformAdapter{}.bounds(preview.front()));
  for (const auto& [actual, readback] : std::array<std::pair<double, double>, 6>{
           {{actual_bounds.minimum.x, readback_bounds.minimum.x},
            {actual_bounds.minimum.y, readback_bounds.minimum.y},
            {actual_bounds.minimum.z, readback_bounds.minimum.z},
            {actual_bounds.maximum.x, readback_bounds.maximum.x},
            {actual_bounds.maximum.y, readback_bounds.maximum.y},
            {actual_bounds.maximum.z, readback_bounds.maximum.z}}})
    CHECK(actual == Approx(readback).margin(1e-6));
}
} // namespace

TEST_CASE("Editable STEP base supports additive subtractive hole and imported-edge fillet history",
          "[geometry][imported-body-downstream-feature][integration][step-import-mvp]") {
  const bool from_reference = GENERATE(false, true);
  INFO("Public command entry: " << (from_reference ? "Reference conversion" : "direct Editable"));
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  const auto original = fixture.source_bytes();
  auto document = command_import(fixture, from_reference);
  REQUIRE(document.imported_body_features().size() == 1);
  CHECK(document.step_import_sources().front().mode() == StepImportMode::EditableBody);
  const auto base_id = document.imported_body_features().front().id();
  const auto body = document.imported_body_features().front().definition().outputs().front().body;
  add_boss(document, body);
  add_cut(document, body);
  add_hole(document, body);
  add_imported_fillet(document, body);
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  const auto full = checked(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  CHECK(full.executed_feature_count == 5);
  CHECK(shapes.feature_shape_count() == 5);
  CHECK(shapes.body_shape_count() == 1);
  CHECK(shapes.find_body_result(body)->source_feature_id == FeatureId("fillet"));
  CHECK(shapes.final_feature_id() == FeatureId("fillet"));
  CHECK(body_volume(shapes, body) == Approx(expected()).margin(1e-5));
  const auto base_shape = topology(*shapes.find_feature_shape(base_id));
  const auto per_body_base = topology(*shapes.find_imported_body_shape(base_id, body));
  CHECK(volume(*shapes.find_imported_body_shape(base_id, body)) == Approx(4000));
  document.mark_all_clean();
  REQUIRE(
      document.set_parameter_value(ParameterId("boss.depth"), checked(Quantity::length_mm(16))));
  auto plan = checked(document.create_recompute_plan());
  CHECK_FALSE(plan.contains(base_id.value()));
  CHECK(plan.contains("boss"));
  CHECK(plan.contains("cut"));
  CHECK(plan.contains("hole"));
  CHECK(plan.contains("fillet"));
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_plan(document, plan, shapes))
            .executed_feature_count == 4);
  CHECK(topology(*shapes.find_feature_shape(base_id)) == base_shape);
  CHECK(topology(*shapes.find_imported_body_shape(base_id, body)) == per_body_base);
  CHECK(body_volume(shapes, body) == Approx(expected(16)).margin(1e-5));
  document.mark_all_clean();
  REQUIRE(
      document.set_parameter_value(ParameterId("hole.diameter"), checked(Quantity::length_mm(4))));
  plan = checked(document.create_recompute_plan());
  CHECK_FALSE(plan.contains(base_id.value()));
  CHECK_FALSE(plan.contains("boss"));
  CHECK_FALSE(plan.contains("cut"));
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_plan(document, plan, shapes))
            .executed_feature_count == 2);
  CHECK(body_volume(shapes, body) == Approx(expected(16, 4)).margin(1e-5));
  auto full_shapes = cache();
  StepImportTopologyContext full_context(fixture.root);
  CHECK(checked(GeometryRecomputeExecutor{&full_context}.execute_document(document, full_shapes))
            .executed_feature_count == 5);
  CHECK(body_volume(full_shapes, body) == Approx(body_volume(shapes, body)).margin(1e-6));
  CHECK(full_shapes.find_body_result(body)->source_feature_id == FeatureId("fillet"));
  CHECK(full_shapes.final_feature_id() == shapes.final_feature_id());
  CHECK(full_shapes.feature_shape_count() == shapes.feature_shape_count());
  const auto json = checked(serialize_part_document_to_json(document));
  auto reopened = checked(deserialize_part_document_from_json(json));
  auto fresh_shapes = cache();
  StepImportTopologyContext reopened_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&reopened_context}.execute_document(reopened, fresh_shapes));
  CHECK(body_volume(fresh_shapes, body) == Approx(body_volume(shapes, body)).margin(1e-6));
  CHECK(fresh_shapes.find_body_result(body)->source_feature_id == FeatureId("fillet"));
  CHECK(fresh_shapes.final_feature_id() == FeatureId("fillet"));
  CHECK(volume(*fresh_shapes.find_imported_body_shape(base_id, body)) == Approx(4000));
  CHECK(checked(serialize_part_document_to_json(reopened)) == json);
  reexport(reopened, fresh_shapes, fixture, expected(16, 4));
  CHECK(fixture.source_bytes() == original);
}

TEST_CASE("Modeled import failures preserve intent cache and previously exported output",
          "[geometry][imported-body-downstream-feature][integration][step-import-mvp]") {
  enum class Failure { Changed, Missing, Lost, Ambiguous };
  const auto failure =
      GENERATE(Failure::Changed, Failure::Missing, Failure::Lost, Failure::Ambiguous);
  Fixture fixture;
  const auto box = BRepPrimAPI_MakeBox(20, 20, 10).Shape();
  fixture.write(box);
  auto document = command_import(fixture, false);
  const auto base = document.imported_body_features().front();
  const auto output = base.definition().outputs().front();
  add_boss(document, output.body);
  add_cut(document, output.body);
  add_hole(document, output.body);
  add_imported_fillet(document, output.body);
  auto shapes = cache();
  StepImportTopologyContext initial(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&initial}.execute_document(document, shapes));
  reexport(document, shapes, fixture, expected());
  document.mark_all_clean();
  const auto plan = checked(document.create_recompute_plan());
  REQUIRE(plan.empty());
  const auto before = checked(serialize_part_document_to_json(document));
  const auto* body_before = topology(*shapes.find_body_shape(output.body));
  const auto* base_before = topology(*shapes.find_feature_shape(base.id()));
  const auto* final_before = topology(*shapes.final_shape());
  const auto exported = bytes(fixture.root / "modeled.step");
  if (failure == Failure::Missing)
    REQUIRE(std::filesystem::remove(fixture.path()));
  else if (failure == Failure::Changed)
    fixture.write_bytes(fixture.source_bytes() + "\n/* unaccepted supplier revision */\n");
  else if (failure == Failure::Lost)
    fixture.write(BRepPrimAPI_MakeBox(20, 20, 14).Shape());
  else
    fixture.write(compound({box, BRepBuilderAPI_Copy(box).Shape()}));
  const auto supplier_after_external_edit =
      failure == Failure::Missing ? std::string{} : fixture.source_bytes();
  StepImportTopologyContext current(fixture.root);
  CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_document(document, shapes));
  CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_plan(document, plan, shapes));
  CHECK_FALSE(StepExporter{&current}.write_document_step(document, shapes,
                                                         (fixture.root / "modeled.step").string()));
  CHECK(bytes(fixture.root / "modeled.step") == exported);

  const auto refreshed = StepEditableCommands{}.refresh(document, {}, fixture.root);
  if (failure == Failure::Changed) {
    // Explicit refresh may accept metadata-only changes, but returning a
    // disposable candidate must not publish it into the input Part or cache.
    REQUIRE(refreshed);
    CHECK(refreshed.value().part.imported_body_features() == document.imported_body_features());
    CHECK(refreshed.value().part.imported_topology_catalogs() ==
          document.imported_topology_catalogs());
  } else {
    CHECK_FALSE(refreshed);
  }
  if (failure == Failure::Lost || failure == Failure::Ambiguous) {
    auto accepted = document;
    accept_current(accepted, fixture);
    const auto accepted_json = checked(serialize_part_document_to_json(accepted));
    StepImportTopologyContext accepted_context(fixture.root);
    const auto* recovery = checked(accepted_context.recover(accepted, base.definition().catalog()));
    const auto* body = recovery->find(output.imported_body);
    REQUIRE(body);
    if (failure == Failure::Ambiguous) {
      CHECK(body->status == ImportedTopologyRecoveryStatus::Ambiguous);
    } else {
      CHECK(body->status == ImportedTopologyRecoveryStatus::Resolved);
      const auto& reference = std::get<ImportedTopologyReference>(
          accepted.fillet_features().front().edges().front().source());
      REQUIRE(recovery->find(reference.entry()));
      CHECK(recovery->find(reference.entry())->status == ImportedTopologyRecoveryStatus::Lost);
    }
    CHECK_FALSE(GeometryRecomputeExecutor{&accepted_context}.execute_document(accepted, shapes));
    CHECK_FALSE(GeometryRecomputeExecutor{&accepted_context}.execute_plan(
        accepted, checked(accepted.create_recompute_plan()), shapes));
    CHECK_FALSE(StepExporter{&accepted_context}.write_document_step(
        accepted, shapes, (fixture.root / "modeled.step").string()));
    CHECK(checked(serialize_part_document_to_json(accepted)) == accepted_json);
  }
  CHECK(checked(serialize_part_document_to_json(document)) == before);
  CHECK(topology(*shapes.find_body_shape(output.body)) == body_before);
  CHECK(topology(*shapes.find_feature_shape(base.id())) == base_before);
  CHECK(topology(*shapes.final_shape()) == final_before);
  CHECK(body_volume(shapes, output.body) == Approx(expected()).margin(1e-5));
  CHECK(shapes.feature_shape_count() == 5);
  CHECK(shapes.body_shape_count() == 1);
  CHECK(shapes.find_body_result(output.body)->source_feature_id == FeatureId("fillet"));
  CHECK(shapes.final_feature_id() == FeatureId("fillet"));
  CHECK(bytes(fixture.root / "modeled.step") == exported);
  if (failure == Failure::Missing)
    CHECK_FALSE(std::filesystem::exists(fixture.path()));
  else
    CHECK(fixture.source_bytes() == supplier_after_external_edit);
}

TEST_CASE("Direct imported target accepts a hole and rectangular cut without an authored extrude",
          "[geometry][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  SECTION("hole") {
    add_hole(document, body, base.id());
  }
  SECTION("rectangular subtractive feature") {
    add_cut(document, body, base.id());
  }
  const auto last_id = document.features().front().id();
  const double wanted = last_id == FeatureId("hole") ? 4000 - 10 * std::numbers::pi : 3840;
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_document(document, shapes))
            .executed_feature_count == 2);
  CHECK(body_volume(shapes, body) == Approx(wanted));
  CHECK(shapes.find_body_result(body)->source_feature_id == last_id);
  document.mark_all_clean();
  const auto parameter =
      last_id == FeatureId("hole") ? ParameterId("hole.diameter") : ParameterId("cut.width");
  REQUIRE(document.mark_parameter_changed(parameter));
  const auto plan = checked(document.create_recompute_plan());
  CHECK_FALSE(plan.contains(base.id().value()));
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_plan(document, plan, shapes))
            .executed_feature_count == 1);
  CHECK(body_volume(shapes, body) == Approx(wanted));
  reexport(document, shapes, fixture, wanted);
}

TEST_CASE("Multi-body imported feature selects one base Body for transforms and incremental edits",
          "[geometry][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(compound({BRepPrimAPI_MakeBox(20, 20, 10).Shape(),
                          BRepPrimAPI_MakeBox(gp_Pnt(40, 0, 0), 2, 3, 4).Shape()}));
  auto document = fixture.editable(true);
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto base = document.imported_body_features().front();
  const auto large =
      std::find_if(base.definition().outputs().begin(), base.definition().outputs().end(),
                   [&](const auto& output) { return body_volume(shapes, output.body) > 100; });
  REQUIRE(large != base.definition().outputs().end());
  const auto small =
      std::find_if(base.definition().outputs().begin(), base.definition().outputs().end(),
                   [&](const auto& output) { return output.body != large->body; });
  REQUIRE(small != base.definition().outputs().end());
  add_boss(document, large->body);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto small_shape = topology(*shapes.find_body_shape(small->body));
  CHECK(body_volume(shapes, large->body) == Approx(4064));
  CHECK(body_volume(shapes, small->body) == Approx(24));
  document.mark_all_clean();
  REQUIRE(
      document.set_parameter_value(ParameterId("boss.depth"), checked(Quantity::length_mm(16))));
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_plan(
                    document, checked(document.create_recompute_plan()), shapes))
            .executed_feature_count == 1);
  CHECK(body_volume(shapes, large->body) == Approx(4096));
  CHECK(topology(*shapes.find_body_shape(small->body)) == small_shape);
  REQUIRE(document.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("move.small"), small->body, BodyTransformCoordinateSpace::World, std::nullopt,
      {0, 5, 0}, false, false))));
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  CHECK(body_volume(shapes, large->body) == Approx(4096));
  CHECK(body_volume(shapes, small->body) == Approx(24));
  const auto bounds = checked(BodyTransformAdapter{}.bounds(*shapes.find_body_shape(small->body)));
  CHECK(bounds.minimum.x == Approx(40));
  CHECK(bounds.minimum.y == Approx(5));
  CHECK(shapes.find_body_result(small->body)->source_feature_id == FeatureId("move.small"));
}

TEST_CASE(
    "Failed imported downstream edits preserve the entire previous cache and immutable source",
    "[geometry][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  add_imported_fillet(document, body);
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto original = fixture.source_bytes();
  const auto old_body = topology(*shapes.find_body_shape(body));
  const auto old_base = topology(*shapes.find_feature_shape(base.id()));
  const double old_volume = body_volume(shapes, body);
  document.mark_all_clean();
  REQUIRE(
      document.set_parameter_value(ParameterId("fillet.radius"), checked(Quantity::length_mm(50))));
  const auto intent = checked(serialize_part_document_to_json(document));
  CHECK_FALSE(GeometryRecomputeExecutor{&context}.execute_plan(
      document, checked(document.create_recompute_plan()), shapes));
  CHECK_FALSE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  CHECK(topology(*shapes.find_body_shape(body)) == old_body);
  CHECK(topology(*shapes.find_feature_shape(base.id())) == old_base);
  CHECK(body_volume(shapes, body) == Approx(old_volume));
  CHECK(shapes.find_body_result(body)->source_feature_id == FeatureId("fillet"));
  CHECK(shapes.feature_shape_count() == 2);
  CHECK(shapes.body_shape_count() == 1);
  CHECK(checked(serialize_part_document_to_json(document)) == intent);
  CHECK(fixture.source_bytes() == original);
}

TEST_CASE("Imported edge selection follows explicit translated and rotated Body authority",
          "[geometry][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  REQUIRE(document.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("move"), body, BodyTransformCoordinateSpace::World, std::nullopt, {30, -5, 4},
      false, false))));
  REQUIRE(document.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("turn"), body, BodyTransformCoordinateSpace::World, std::nullopt,
      checked(BodyTransformRotationAxis::create_explicit({}, {0, 0, 1})), 90, false, false))));
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto choices = checked(FinishingSelectionBuilder::build(
      document, shapes, body, PartFeatureInputRole::FilletEdge, &context));
  const auto imported_count = std::count_if(choices.begin(), choices.end(), [](const auto& target) {
    const auto* edge = std::get_if<EdgeReference>(&target.reference);
    return edge && std::holds_alternative<ImportedTopologyReference>(edge->source());
  });
  CHECK(imported_count == 12);
  const auto moved_corner = std::find_if(choices.begin(), choices.end(), [](const auto& target) {
    return std::abs(target.anchor.x - 5) < 1e-6 && std::abs(target.anchor.y - 30) < 1e-6 &&
           std::abs(target.anchor.z - 9) < 1e-6;
  });
  REQUIRE(moved_corner != choices.end());
  add_imported_fillet(document, body);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const double wanted = 4000 - 10 * 0.25 * (1 - std::numbers::pi / 4);
  CHECK(body_volume(shapes, body) == Approx(wanted).margin(1e-6));
  auto bounds = checked(BodyTransformAdapter{}.bounds(*shapes.find_body_shape(body)));
  CHECK(bounds.minimum.x == Approx(-15));
  CHECK(bounds.maximum.x == Approx(5));
  CHECK(bounds.minimum.y == Approx(30));
  CHECK(bounds.maximum.y == Approx(50));
  CHECK(bounds.minimum.z == Approx(4));
  CHECK(bounds.maximum.z == Approx(14));
  document.mark_all_clean();
  REQUIRE(document.mark_parameter_changed(ParameterId("fillet.radius")));
  CHECK(checked(GeometryRecomputeExecutor{&context}.execute_plan(
                    document, checked(document.create_recompute_plan()), shapes))
            .executed_feature_count == 1);
  CHECK(body_volume(shapes, body) == Approx(wanted).margin(1e-6));
  const auto before = topology(*shapes.find_body_shape(body));
  fixture.write(BRepPrimAPI_MakeBox(gp_Pnt(100, 200, 300), 2, 3, 4).Shape());
  accept_current(document, fixture);
  StepImportTopologyContext lost(fixture.root);
  const auto intent = checked(serialize_part_document_to_json(document));
  CHECK_FALSE(GeometryRecomputeExecutor{&lost}.execute_document(document, shapes));
  CHECK(topology(*shapes.find_body_shape(body)) == before);
  CHECK(body_volume(shapes, body) == Approx(wanted).margin(1e-6));
  CHECK(checked(serialize_part_document_to_json(document)) == intent);
}

TEST_CASE("Actual imported planar face supports a persistent Sketch and additive feature",
          "[geometry][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  auto shapes = cache();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  const auto faces =
      checked(ResultFaceSupportGeometry::selectable_faces(document, shapes, body, &context));
  REQUIRE(faces.size() == 6);
  for (const auto& face : faces) {
    CHECK(face.reference.producer() == base.id());
    REQUIRE(face.reference.origins().size() == 1);
    CHECK(face.reference.origins().front().feature == base.id());
    CHECK(parse_imported_face_origin_role(face.reference.origins().front().role));
  }
  const auto top = std::find_if(faces.begin(), faces.end(), [](const auto& face) {
    return std::abs(face.anchor.z - 10) < 1e-8;
  });
  REQUIRE(top != faces.end());
  auto plane = checked(
      ResultFaceWorkplane::create(DatumPlaneId("imported.top"), "Imported top", top->reference));
  const auto frame = checked(ResultFaceSupportGeometry::resolve(document, plane, shapes, &context));
  REQUIRE(document.add_result_face_workplane(plane));
  REQUIRE(document.add_parameter(length("cap.width", 2)));
  REQUIRE(document.add_parameter(length("cap.height", 2)));
  REQUIRE(document.add_parameter(length("cap.depth", 3)));
  const Vector3 delta{top->anchor.x - frame.origin.x, top->anchor.y - frame.origin.y,
                      top->anchor.z - frame.origin.z};
  const Point2 center{dot(delta, frame.x_axis), dot(delta, frame.y_axis)};
  auto sketch = checked(Sketch::create(SketchId("cap.sketch"), "Face sketch", plane.id()));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("cap.profile"), ParameterId("cap.width"), ParameterId("cap.height"), center))));
  REQUIRE(document.add_sketch(sketch));
  auto feature = checked(Feature::create_additive_extrude(FeatureId("cap"), "Face boss",
                                                          sketch.id(), ParameterId("cap.depth")));
  feature = checked(feature.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, body, std::nullopt))));
  REQUIRE(document.add_feature(feature));
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, shapes));
  CHECK(body_volume(shapes, body) == Approx(4012));
  CHECK(shapes.find_body_result(body)->source_feature_id == FeatureId("cap"));
  const auto json = checked(serialize_part_document_to_json(document));
  auto reopened = checked(deserialize_part_document_from_json(json));
  auto reopened_shapes = cache();
  StepImportTopologyContext reopened_context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&reopened_context}.execute_document(reopened, reopened_shapes));
  CHECK(body_volume(reopened_shapes, body) == Approx(4012));
  CHECK(checked(serialize_part_document_to_json(reopened)) == json);
}

TEST_CASE("Direct downstream entrypoints verify imported inputs even with a fresh unused context",
          "[geometry][imported-body-downstream-feature]") {
  Fixture fixture;
  fixture.write(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
  auto document = fixture.editable();
  const auto base = document.imported_body_features().front();
  const auto body = base.definition().outputs().front().body;
  add_boss(document, body);
  add_hole(document, body, FeatureId("boss"));
  auto shapes = cache();
  StepImportTopologyContext initial(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&initial}.execute_document(document, shapes));
  const auto before = topology(*shapes.find_body_shape(body));
  const auto base_before = topology(*shapes.find_feature_shape(base.id()));
  const auto old_volume = body_volume(shapes, body);
  const auto original = fixture.source_bytes();
  SECTION("fresh context sees changed bytes") {
    fixture.write_bytes(original + "\n/* changed */\n");
  }
  SECTION("fresh context sees missing source") {
    REQUIRE(std::filesystem::remove(fixture.path()));
  }
  SECTION("accepted new digest cannot use a stale computed base") {
    fixture.write(BRepPrimAPI_MakeBox(20, 20, 12).Shape());
    accept_current(document, fixture);
  }
  const auto intent = checked(serialize_part_document_to_json(document));
  StepImportTopologyContext current(fixture.root);
  CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_additive_extrude(
      document, FeatureId("boss"), shapes));
  CHECK_FALSE(GeometryRecomputeExecutor{&current}.execute_subtractive_extrude(
      document, FeatureId("hole"), shapes));
  CHECK(topology(*shapes.find_body_shape(body)) == before);
  CHECK(topology(*shapes.find_feature_shape(base.id())) == base_before);
  CHECK(body_volume(shapes, body) == Approx(old_volume));
  CHECK(checked(serialize_part_document_to_json(document)) == intent);
}
