#include "blcad/core/sketch_diagnostics.hpp"

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

bool has_kind(const SketchDiagnosticReport& report, SketchDiagnosticKind kind) {
  return std::any_of(
      report.diagnostics().begin(), report.diagnostics().end(),
      [kind](const SketchConstraintDiagnostic& diagnostic) { return diagnostic.kind() == kind; });
}

bool has_target(const SketchDiagnosticReport& report, std::string target) {
  return std::any_of(report.diagnostics().begin(), report.diagnostics().end(),
                     [&target](const SketchConstraintDiagnostic& diagnostic) {
                       return diagnostic.target() == target;
                     });
}

} // namespace

TEST_CASE("Sketch diagnostics reports simple under-constrained line endpoints",
          "[core][sketch-diagnostics]") {
  auto sketch = Sketch::create(SketchId("sketch.diagnostics.free_line"), "FreeLine",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.free", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  const SketchConstraintDiagnostics diagnostics;
  const auto report = diagnostics.analyze(sketch.value());

  CHECK(report.error_count() == 0U);
  CHECK(report.warning_count() == 2U);
  CHECK(has_kind(report, SketchDiagnosticKind::UnconstrainedLineEndpoint));
  CHECK(has_target(report, "line.free:start"));
  CHECK(has_target(report, "line.free:end"));
}

TEST_CASE("Sketch diagnostics reports free spline control points and undimensioned profiles",
          "[core][sketch-diagnostics]") {
  auto sketch =
      Sketch::create(SketchId("sketch.diagnostics.spline"), "Spline", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto spline =
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.a"), Point2{0.0, 0.0},
                                         Point2{2.0, 4.0}, Point2{8.0, 4.0}, Point2{10.0, 0.0});
  REQUIRE(spline);
  REQUIRE(sketch.value().add_entity(spline.value()));
  auto rectangle = RectangleProfile::create(ProfileId("profile.spline"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));

  const SketchConstraintDiagnostics diagnostics;
  const auto report = diagnostics.analyze(sketch.value());

  CHECK(has_kind(report, SketchDiagnosticKind::FreeSplineControlPoint));
  CHECK(has_kind(report, SketchDiagnosticKind::ProfileWithoutDrivingDimensions));
  CHECK(has_target(report, "spline.a:control1"));
  CHECK(has_target(report, "spline.a:control2"));
}

TEST_CASE("Sketch diagnostics reports conflicting horizontal and vertical constraints",
          "[core][sketch-diagnostics]") {
  auto sketch =
      Sketch::create(SketchId("sketch.diagnostics.conflict"), "Conflict", DatumPlaneId("datum.xy"));
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
  const auto report = diagnostics.analyze(sketch.value());

  CHECK(report.has_errors());
  CHECK(has_kind(report, SketchDiagnosticKind::ConflictingHorizontalVerticalConstraints));
}

TEST_CASE("Sketch diagnostics reports duplicate fixed endpoints and duplicate dimensions",
          "[core][sketch-diagnostics]") {
  auto sketch = Sketch::create(SketchId("sketch.diagnostics.duplicates"), "Duplicates",
                               DatumPlaneId("datum.xy"));
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
  const auto report = diagnostics.analyze(sketch.value());

  CHECK(has_kind(report, SketchDiagnosticKind::DuplicateFixedEndpoint));
  CHECK(has_kind(report, SketchDiagnosticKind::DuplicateDrivingDimensionTarget));
}

TEST_CASE("Sketch diagnostics serialize to debug JSON outside model intent",
          "[core][sketch-diagnostics][json]") {
  auto sketch =
      Sketch::create(SketchId("sketch.diagnostics.json"), "Json", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "line.free", Point2{0.0, 0.0}, Point2{10.0, 0.0});

  const SketchConstraintDiagnostics diagnostics;
  const auto report = diagnostics.analyze(sketch.value());
  auto serialized = serialize_sketch_diagnostic_report_to_json(report);
  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.sketch_diagnostics.debug") != std::string::npos);
  CHECK(serialized.value().find("unconstrained_line_endpoint") != std::string::npos);
  CHECK(serialized.value().find("sketch.diagnostics.json") != std::string::npos);
}
