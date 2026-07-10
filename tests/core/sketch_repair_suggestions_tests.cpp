#include "blcad/core/sketch_repair_suggestions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace blcad;

namespace {

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

SketchReferenceTarget line_target(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

SketchReferenceTarget line_start(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment_start(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

SketchReferenceTarget line_end(const char* id) {
  auto target = SketchReferenceTarget::create_line_segment_end(SketchEntityId(id));
  REQUIRE(target);
  return target.value();
}

bool has_action(const SketchRepairSuggestionReport& report, SketchRepairSuggestionAction action) {
  return std::any_of(
      report.suggestions().begin(), report.suggestions().end(),
      [action](const SketchRepairSuggestion& suggestion) { return suggestion.action() == action; });
}

const SketchRepairSuggestion* find_action(const SketchRepairSuggestionReport& report,
                                          SketchRepairSuggestionAction action) {
  const auto found = std::find_if(
      report.suggestions().begin(), report.suggestions().end(),
      [action](const SketchRepairSuggestion& suggestion) { return suggestion.action() == action; });
  return found == report.suggestions().end() ? nullptr : &*found;
}

} // namespace

TEST_CASE("Sketch repair suggestions propose fixed constraints for unconstrained endpoints",
          "[core][sketch-repair]") {
  auto sketch =
      Sketch::create(SketchId("sketch.repair.free_line"), "FreeLine", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.free", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch.value());
  const SketchRepairSuggester suggester;
  const auto suggestions = suggester.suggest(diagnostic_report);

  CHECK(suggestions.suggestion_count() == 2U);
  CHECK(has_action(suggestions, SketchRepairSuggestionAction::AddFixedEndpointConstraint));
  REQUIRE(suggestions.suggestions().size() == 2U);
  CHECK(suggestions.suggestions()[0].originating_diagnostic_kind() ==
        SketchDiagnosticKind::UnconstrainedLineEndpoint);
  CHECK(suggestions.suggestions()[0].target() == "line.free:start");
  CHECK(suggestions.suggestions()[1].target() == "line.free:end");
}

TEST_CASE("Sketch repair suggestions propose removing orientation conflicts",
          "[core][sketch-repair]") {
  auto sketch =
      Sketch::create(SketchId("sketch.repair.conflict"), "Conflict", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.locked", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  auto horizontal = SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("constraint.horizontal"), line_target("line.locked"));
  auto vertical = SketchGeometricConstraint::create_vertical(
      SketchConstraintId("constraint.vertical"), line_target("line.locked"));
  REQUIRE(horizontal);
  REQUIRE(vertical);
  REQUIRE(sketch.value().add_constraint(horizontal.value()));
  REQUIRE(sketch.value().add_constraint(vertical.value()));

  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch.value());
  const SketchRepairSuggester suggester;
  const auto suggestions = suggester.suggest(diagnostic_report);

  const auto* suggestion = find_action(
      suggestions, SketchRepairSuggestionAction::RemoveConflictingOrientationConstraint);
  REQUIRE(suggestion != nullptr);
  CHECK(suggestion->target() == "line.locked");
  CHECK(suggestion->originating_diagnostic_kind() ==
        SketchDiagnosticKind::ConflictingHorizontalVerticalConstraints);
}

TEST_CASE("Sketch repair suggestions propose removing duplicate fixed and dimension records",
          "[core][sketch-repair]") {
  auto sketch =
      Sketch::create(SketchId("sketch.repair.duplicates"), "Duplicates", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.a", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  auto fixed_a = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.fixed.a"),
                                                         line_start("line.a"));
  auto fixed_b = SketchGeometricConstraint::create_fixed(SketchConstraintId("constraint.fixed.b"),
                                                         line_start("line.a"));
  REQUIRE(fixed_a);
  REQUIRE(fixed_b);
  REQUIRE(sketch.value().add_constraint(fixed_a.value()));
  REQUIRE(sketch.value().add_constraint(fixed_b.value()));

  auto dimension_a = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.a"), line_start("line.a"), line_end("line.a"),
      ParameterId("part.width"));
  auto dimension_b = SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("dim.b"), line_start("line.a"), line_end("line.a"),
      ParameterId("part.width.duplicate"));
  REQUIRE(dimension_a);
  REQUIRE(dimension_b);
  REQUIRE(sketch.value().add_dimension(dimension_a.value()));
  REQUIRE(sketch.value().add_dimension(dimension_b.value()));

  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch.value());
  const SketchRepairSuggester suggester;
  const auto suggestions = suggester.suggest(diagnostic_report);

  CHECK(has_action(suggestions,
                   SketchRepairSuggestionAction::RemoveDuplicateFixedEndpointConstraint));
  CHECK(has_action(suggestions, SketchRepairSuggestionAction::RemoveDuplicateDrivingDimension));
}

TEST_CASE("Sketch repair suggestions propose adding dimensions to undimensioned profile sketches",
          "[core][sketch-repair]") {
  auto sketch =
      Sketch::create(SketchId("sketch.repair.profile"), "Profile", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch.value());
  const SketchRepairSuggester suggester;
  const auto suggestions = suggester.suggest(diagnostic_report);

  const auto* suggestion =
      find_action(suggestions, SketchRepairSuggestionAction::AddDrivingDimension);
  REQUIRE(suggestion != nullptr);
  CHECK(suggestion->target() == "sketch.repair.profile");
  CHECK(suggestion->originating_diagnostic_kind() ==
        SketchDiagnosticKind::ProfileWithoutDrivingDimensions);
}

TEST_CASE("Sketch repair suggestions serialize to debug JSON outside model intent",
          "[core][sketch-repair][json]") {
  auto sketch = Sketch::create(SketchId("sketch.repair.json"), "Json", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.free", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  const SketchConstraintDiagnostics diagnostics;
  const auto diagnostic_report = diagnostics.analyze(sketch.value());
  const SketchRepairSuggester suggester;
  const auto suggestions = suggester.suggest(diagnostic_report);
  auto serialized = serialize_sketch_repair_suggestion_report_to_json(suggestions);
  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.sketch_repair_suggestions.debug") != std::string::npos);
  CHECK(serialized.value().find("add_fixed_endpoint_constraint") != std::string::npos);
  CHECK(serialized.value().find("mutates_model") != std::string::npos);
  CHECK(serialized.value().find("unconstrained_line_endpoint") != std::string::npos);
}
