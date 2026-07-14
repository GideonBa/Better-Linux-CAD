#include "blcad/gui/gui_part_foundation_workbench.hpp"
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
} // namespace

TEST_CASE("Block 100 GUI creates a deterministic foundational Part and survives persistence",
          "[gui][parameters][gui][part-foundation][gui][extrude-revolve]") {
  GuiDocumentSession session;
  GuiPartFoundationWorkbench workbench;
  GuiSketchWorkbench sketches;
  REQUIRE(session.create_part(DocumentId("part.tutorial"), "Tutorial"));
  REQUIRE(workbench.create_parameter(session, length("width", 40.0)));
  REQUIRE(workbench.create_parameter(session, length("height", 25.0)));
  REQUIRE(workbench.create_parameter(session, length("depth", 12.0)));
  REQUIRE(workbench.create_expression_parameter(session, ParameterId("double_depth"),
                                                 "Double depth", ParameterType::Length,
                                                 "depth * 2"));
  REQUIRE(sketches.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                       ParameterId("width"),
                                                       ParameterId("height")).value()));
  REQUIRE(sketches.create_sketch(session, std::move(sketch)));
  REQUIRE(workbench.create_body(session, Body::create(BodyId("body.main"), "Main",
                                                       BodyKind::Solid).value()));
  REQUIRE(workbench.activate_body(session, BodyId("body.main")));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"),
      ExtrudeFeatureIntent::create(ExtrudeExtentIntent::distance(ParameterId("depth")).value()).value())
                     .value()
                     .with_body_result_context(new_body("body.main")).value();
  const auto preview = workbench.preview_extrude(session, feature);
  REQUIRE(preview);
  CHECK(preview.value().semantic_summary.find("distance") != std::string::npos);
  CHECK(session.part_document()->feature_count() == 0);
  REQUIRE(workbench.apply_extrude(session, feature));
  REQUIRE(session.recompute());
  auto body = workbench.inspect_body(session, BodyId("body.main"));
  REQUIRE(body);
  CHECK(body.value().has_cached_shape);
  CHECK(workbench.active_body()->value() == "body.main");

  const auto path = std::filesystem::temp_directory_path() / "blcad-block100-tutorial.blcad";
  REQUIRE(session.save_as(path));
  GuiDocumentSession restored;
  REQUIRE(restored.open_file(path));
  REQUIRE(restored.recompute());
  CHECK(restored.part_document()->find_feature(FeatureId("feature.base")) != nullptr);
  CHECK(restored.part_document()->find_parameter(ParameterId("double_depth"))->value().millimeters() == 24.0);
  std::filesystem::remove(path);
}

TEST_CASE("Block 100 hole workflow and unsupported suppression fail or apply atomically",
          "[gui][part-foundation][gui][extrude-revolve]") {
  GuiDocumentSession session;
  GuiPartFoundationWorkbench workbench;
  GuiSketchWorkbench sketches;
  REQUIRE(session.create_part(DocumentId("part.holes"), "Holes"));
  for (auto parameter : {length("radius", 20), count("count", 6), length("diameter", 4),
                         length("depth", 10)})
    REQUIRE(workbench.create_parameter(session, std::move(parameter)));
  REQUIRE(sketches.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(sketches.create_sketch(session, Sketch::create(SketchId("sketch.holes"), "Holes",
                                                          DatumPlaneId("datum.xy")).value()));
  REQUIRE(workbench.create_body(session, Body::create(BodyId("body.main"), "Main", BodyKind::Solid).value()));
  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.holes"), "Holes",
                                                  SketchId("sketch.holes"), FeatureId("feature.base"),
                                                  SubtractiveExtrudeDepth::ThroughAll).value();
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                   BodyId("body.main"), std::nullopt).value();
  cut = cut.with_body_result_context(context).value();
  auto result = workbench.apply_hole_workflow(
      session, SketchId("sketch.holes"),
      CircularHolePattern::create(ProfileId("pattern.holes"), ParameterId("radius"),
                                  ParameterId("count"), ParameterId("diameter")).value(), cut);
  REQUIRE(result.has_error());
  CHECK(session.part_document()->find_sketch(SketchId("sketch.holes"))->circular_hole_patterns().empty());
  CHECK(workbench.suppress_feature(session, FeatureId("feature.any")).has_error());
}

TEST_CASE("Block 100 prompts state semantic input capabilities", "[gui][part-foundation]") {
  CHECK(GuiPartFoundationWorkbench::prompt_for(GuiPartFoundationCommand::Revolve).required_capability ==
        "profile_and_axis");
  CHECK(GuiPartFoundationWorkbench::prompt_for(GuiPartFoundationCommand::Parameter).text.find("mm") !=
        std::string::npos);
}
