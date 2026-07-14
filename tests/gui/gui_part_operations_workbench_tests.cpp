#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_part_operations_workbench.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace blcad;
using namespace blcad::gui;

namespace {
Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
Parameter count(const char* id, double value) {
  return Parameter::create_count(ParameterId(id), id, Quantity::count(value, id).value()).value();
}
FeatureBodyResultContext new_body(const char* id) {
  return FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                          BodyId(id)).value();
}

void seed_base(GuiDocumentSession& session) {
  GuiPartFoundationWorkbench foundation;
  GuiSketchWorkbench sketches;
  REQUIRE(session.create_part(DocumentId("part.operations"), "Operations"));
  REQUIRE(foundation.create_parameter(session, length("width", 20)));
  REQUIRE(foundation.create_parameter(session, length("height", 12)));
  REQUIRE(foundation.create_parameter(session, length("depth", 8)));
  REQUIRE(foundation.create_parameter(session, length("spacing", 35)));
  REQUIRE(foundation.create_parameter(session, count("instances", 2)));
  REQUIRE(sketches.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(sketches.create_datum_axis(session, DatumAxis::create_explicit(
      DatumAxisId("axis.x"), "X", {}, {1, 0, 0}).value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
      ParameterId("width"), ParameterId("height")).value()));
  REQUIRE(sketches.create_sketch(session, std::move(sketch)));
  for (const char* id : {"body.main", "body.pattern", "body.boolean"})
    REQUIRE(foundation.create_body(session, Body::create(BodyId(id), id, BodyKind::Solid).value()));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
      SketchId("sketch.base"), ParameterId("depth")).value()
      .with_body_result_context(new_body("body.main")).value();
  REQUIRE(foundation.apply_extrude(session, std::move(feature)));
}
} // namespace

TEST_CASE("Block 101 GUI patterns Bodies composes transforms and exports fresh STEP",
          "[gui][part-pattern][gui][body-operation]") {
  GuiDocumentSession session;
  seed_base(session);
  GuiPartOperationsWorkbench operations;

  auto direction = AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis,
                                                     DatumAxisId("axis.x")).value();
  auto pattern = LinearPatternFeature::create(
      FeatureId("pattern.linear"), "Linear", {PatternSourceReference::body(BodyId("body.main")).value()},
      direction, ParameterId("instances"), LinearPatternExtentMode::Spacing,
      ParameterId("spacing"), PatternDirectionSign::Positive, new_body("body.pattern")).value();
  auto preview = operations.preview_linear_pattern(session, pattern);
  REQUIRE(preview);
  CHECK(session.part_document()->linear_pattern_features().empty());
  REQUIRE(operations.apply_linear_pattern(session, pattern));

  auto boolean = BodyBooleanFeature::create(FeatureId("boolean.add"), BodyBooleanOperation::Add,
      BodyId("body.main"), {BodyId("body.pattern")}, BodyBooleanResultMode::NewBody,
      BodyId("body.boolean"), true).value();
  REQUIRE(operations.apply_body_boolean(session, boolean));
  auto transform = BodyTransform::create_translate(BodyTransformId("move.boolean"),
      BodyId("body.boolean"), BodyTransformCoordinateSpace::World, std::nullopt, {0, 25, 0},
      false, false).value();
  REQUIRE(operations.apply_body_transform(session, transform));
  REQUIRE(session.recompute());
  REQUIRE(session.part_shape_cache()->find_body_shape(BodyId("body.boolean")) != nullptr);

  const auto step = std::filesystem::temp_directory_path() / "blcad-block101.step";
  const auto exported = operations.export_visible_bodies_step(session, step);
  REQUIRE(exported);
  CHECK(exported.value() > 0);
  CHECK(std::filesystem::exists(step));
  std::filesystem::remove(step);

  const auto file = std::filesystem::temp_directory_path() / "blcad-block101.blcad";
  REQUIRE(session.save_as(file));
  GuiDocumentSession restored;
  REQUIRE(restored.open_file(file));
  REQUIRE(restored.recompute());
  CHECK(restored.part_document()->linear_pattern_features().size() == 1);
  CHECK(restored.part_document()->body_boolean_features().size() == 1);
  CHECK(restored.part_document()->body_transforms().size() == 1);
  std::filesystem::remove(file);
}

TEST_CASE("Block 101 finishing preview rejects lost topology without mutation",
          "[gui][part-finishing]") {
  GuiDocumentSession session;
  seed_base(session);
  GuiPartOperationsWorkbench operations;
  auto missing = SemanticEdgeReference::create(FeatureId("feature.missing"), SemanticEdge::TopFront);
  REQUIRE(missing);
  auto edge = EdgeReference::create_linear(PartFeatureInputRole::FilletEdge, missing.value());
  REQUIRE(edge);
  auto fillet = FilletFeature::create(FeatureId("fillet.invalid"), "Invalid", BodyId("body.main"),
                                      std::vector<EdgeReference>{edge.value()}, ParameterId("depth"));
  REQUIRE(fillet);
  const auto revision_before = session.presentation_revision();
  const auto cache_count_before = session.part_shape_cache()
                                      ? session.part_shape_cache()->body_shape_count() : 0U;
  const auto result = operations.preview_fillet(session, fillet.value());
  CHECK(result.has_error());
  CHECK(session.part_document()->fillet_features().empty());
  CHECK(session.presentation_revision() == revision_before);
  REQUIRE(session.part_shape_cache() != nullptr);
  CHECK(session.part_shape_cache()->body_shape_count() == cache_count_before);
}

TEST_CASE("Block 101 prompts preserve ordered and capability-specific selection",
          "[gui][part-pattern][gui][part-finishing][gui][body-operation]") {
  CHECK(GuiPartOperationsWorkbench::prompt_for(GuiPartOperationCommand::LinearPattern)
            .required_capability == "ordered_sources_and_axis");
  CHECK(GuiPartOperationsWorkbench::prompt_for(GuiPartOperationCommand::Shell).text.find("ordered") !=
        std::string::npos);
  CHECK(GuiPartOperationsWorkbench::prompt_for(GuiPartOperationCommand::BodyBoolean)
            .required_kind == GuiSelectionKind::Body);
}
