#include "blcad/core/interactive_sketcher_acceptance.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

using namespace blcad;

namespace {
SketchCoordinate3D explicit_coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "coordinate");
  REQUIRE(quantity);
  auto coordinate = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(coordinate);
  return coordinate.value();
}
SketchCoordinate3D parameter_coordinate(const char* id) {
  auto coordinate = SketchCoordinate3D::create_parameter(ParameterId(id));
  REQUIRE(coordinate);
  return coordinate.value();
}
SketchPoint3D point(const char* id, double x, double y, double z) {
  auto value = SketchPoint3D::create(SketchEntityId(id), explicit_coordinate(x),
                                     explicit_coordinate(y), explicit_coordinate(z));
  REQUIRE(value);
  return value.value();
}
SketchPoint3D parameter_point(const char* id) {
  auto value = SketchPoint3D::create(SketchEntityId(id), parameter_coordinate("path.x"),
                                     explicit_coordinate(-2.0), parameter_coordinate("path.z"));
  REQUIRE(value);
  return value.value();
}
Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}
Parameter angle_parameter(const char* id, double value) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}
Sketch3D populated_sketch() {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial path");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point("point.a", 0.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point("point.b", 10.0, 0.0, 5.0)));
  REQUIRE(sketch.value().add_point(point("point.c", 10.0, 8.0, 12.0)));
  auto line = SketchLine3D::create(SketchEntityId("line.ab"), SketchEntityId("point.a"),
                                   SketchEntityId("point.b"));
  REQUIRE(line);
  REQUIRE(sketch.value().add_line(line.value()));
  auto polyline = SketchPolyline3D::create(
      SketchEntityId("polyline.path"),
      {SketchEntityId("point.a"), SketchEntityId("point.b"), SketchEntityId("point.c")});
  REQUIRE(polyline);
  REQUIRE(sketch.value().add_polyline(polyline.value()));
  return sketch.value();
}
} // namespace

TEST_CASE("Block 75 freezes explicit and parameter-driven model-space coordinates", "[core][sketch-3d]") {
  auto explicit_value = explicit_coordinate(-12.5);
  CHECK(explicit_value.source() == SketchCoordinate3DSource::Explicit);
  REQUIRE(explicit_value.explicit_coordinate().has_value());
  CHECK(explicit_value.explicit_coordinate()->millimeters() == -12.5);
  CHECK_FALSE(explicit_value.parameter().has_value());
  auto driven = parameter_coordinate("path.x");
  CHECK(driven.source() == SketchCoordinate3DSource::Parameter);
  REQUIRE(driven.parameter().has_value());
  CHECK(*driven.parameter() == ParameterId("path.x"));
  CHECK_FALSE(driven.explicit_coordinate().has_value());
  auto wrong_unit = Quantity::angle_deg(5.0, "coordinate");
  REQUIRE(wrong_unit);
  CHECK(SketchCoordinate3D::create_explicit(wrong_unit.value()).has_error());
  CHECK(SketchCoordinate3D::create_parameter(ParameterId()).has_error());
}

TEST_CASE("Block 75 owns points lines and ordered polyline vertices in one entity scope", "[core][sketch-3d]") {
  auto sketch = populated_sketch();
  CHECK(sketch.entity_count() == 5U);
  REQUIRE(sketch.find_polyline(SketchEntityId("polyline.path")) != nullptr);
  const auto& vertices = sketch.find_polyline(SketchEntityId("polyline.path"))->ordered_vertices();
  REQUIRE(vertices.size() == 3U);
  CHECK(vertices[0] == SketchEntityId("point.a"));
  CHECK(vertices[1] == SketchEntityId("point.b"));
  CHECK(vertices[2] == SketchEntityId("point.c"));
  CHECK(sketch.add_point(point("line.ab", 1.0, 2.0, 3.0)).has_error());
  auto missing = SketchLine3D::create(SketchEntityId("line.missing"), SketchEntityId("point.a"),
                                      SketchEntityId("point.missing"));
  REQUIRE(missing);
  CHECK(sketch.add_line(missing.value()).has_error());
  CHECK(sketch.remove_point(SketchEntityId("point.a")).has_error());
  REQUIRE(sketch.remove_line(SketchEntityId("line.ab")));
  REQUIRE(sketch.remove_polyline(SketchEntityId("polyline.path")));
  REQUIRE(sketch.remove_point(SketchEntityId("point.a")));
  CHECK(sketch.entity_count() == 2U);
}

TEST_CASE("Block 75 connects coordinate parameters to 3D sketch invalidation and removal", "[core][sketch-3d]") {
  auto document = PartDocument::create(DocumentId("part.sketch3d"), "3D sketch");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("path.x", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("path.z", 5.0)));
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial path");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(parameter_point("point.driven")));
  REQUIRE(document.value().add_sketch_3d(sketch.value()));
  CHECK(document.value().sketch_3d_count() == 1U);
  CHECK(document.value().find_sketch_3d(Sketch3DId("sketch3d.path")) != nullptr);
  CHECK(document.value().dependency_graph().has_dependency("path.x", "sketch3d.path"));
  CHECK(document.value().dependency_graph().has_dependency("path.z", "sketch3d.path"));
  auto serialized = serialize_part_document_to_json(document.value());
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().find_sketch_3d(Sketch3DId("sketch3d.path")) != nullptr);
  document.value().mark_all_clean();
  auto changed = Quantity::length_mm(14.0, "path.x");
  REQUIRE(changed);
  auto affected = document.value().set_parameter_value(ParameterId("path.x"), changed.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "sketch3d.path") != affected.value().end());
  auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(std::any_of(plan.value().steps().begin(), plan.value().steps().end(),
                    [](const RecomputeStep& step) { return step.node_id == "sketch3d.path"; }));
  REQUIRE(document.value().remove_sketch_3d(Sketch3DId("sketch3d.path")));
  CHECK(document.value().sketch_3d_count() == 0U);
  CHECK_FALSE(document.value().dependency_graph().has_node("sketch3d.path"));
}

TEST_CASE("Block 75 validates document and entity identity transactionally", "[core][sketch-3d]") {
  auto document = PartDocument::create(DocumentId("part.sketch3d-validation"), "Validation");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(angle_parameter("wrong.angle", 15.0)));
  auto wrong_unit = Sketch3D::create(Sketch3DId("sketch3d.wrong"), "Wrong unit");
  REQUIRE(wrong_unit);
  auto driven = SketchPoint3D::create(SketchEntityId("point.driven"), parameter_coordinate("wrong.angle"),
                                      explicit_coordinate(0.0), explicit_coordinate(0.0));
  REQUIRE(driven);
  REQUIRE(wrong_unit.value().add_point(driven.value()));
  CHECK(document.value().add_sketch_3d(wrong_unit.value()).has_error());
  CHECK(document.value().sketch_3d_count() == 0U);
  CHECK_FALSE(document.value().dependency_graph().has_node("sketch3d.wrong"));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto planar = Sketch::create(SketchId("shared.sketch"), "Planar", DatumPlaneId("datum.xy"));
  REQUIRE(planar);
  REQUIRE(document.value().add_sketch(planar.value()));
  auto spatial = Sketch3D::create(Sketch3DId("shared.sketch"), "Spatial");
  REQUIRE(spatial);
  CHECK(document.value().add_sketch_3d(spatial.value()).has_error());
  CHECK(document.value().remove_sketch_3d(Sketch3DId("missing")).has_error());
  CHECK(SketchLine3D::create(SketchEntityId("line.zero"), SketchEntityId("point.a"), SketchEntityId("point.a")).has_error());
  CHECK(SketchPolyline3D::create(SketchEntityId("polyline.short"), {SketchEntityId("point.a")}).has_error());
}

TEST_CASE("Block 120 resolves orthogonal locks and typed distance-angle input", "[core][sketch-3d][gui][sketch-3d-edit]") {
  auto locked = Sketch3DInteractionService::resolve_point({1.0, 2.0, 3.0}, {8.0, 9.0, 10.0}, Sketch3DLock::AxisX);
  REQUIRE(locked);
  CHECK(locked.value().x == Catch::Approx(8.0));
  CHECK(locked.value().y == Catch::Approx(2.0));
  CHECK(locked.value().z == Catch::Approx(3.0));
  Sketch3DTypedInput typed;
  typed.distance = 10.0;
  typed.azimuth_radians = 0.0;
  typed.elevation_radians = 3.14159265358979323846 / 6.0;
  auto directed = Sketch3DInteractionService::resolve_point({0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, Sketch3DLock::Free, typed);
  REQUIRE(directed);
  CHECK(directed.value().x == Catch::Approx(8.6602540378));
  CHECK(directed.value().y == Catch::Approx(0.0));
  CHECK(directed.value().z == Catch::Approx(5.0));
}

TEST_CASE("Block 120 creates line points and guide role atomically", "[core][sketch-3d][integration][sketch-3d-direct-manipulation]") {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.interactive"), "Interactive");
  REQUIRE(sketch);
  auto result = Sketch3DInteractionService::add_line(sketch.value(), SketchEntityId("line.path"),
      SketchEntityId("point.start"), {0.0, 0.0, 0.0}, SketchEntityId("point.end"), {10.0, 5.0, 2.0},
      SketchGuideCurve3DRole::SweepPath);
  REQUIRE(result);
  CHECK(result.value().created_entities.size() == 4U);
  REQUIRE(result.value().sketch.find_line(SketchEntityId("line.path")) != nullptr);
  const auto* guide = result.value().sketch.find_guide_curve(SketchEntityId("line.path.guide"));
  REQUIRE(guide != nullptr);
  CHECK(guide->role() == SketchGuideCurve3DRole::SweepPath);
}

TEST_CASE("Block 120 moves a shared point handle while preserving curve identity", "[core][sketch-3d][integration][sketch-3d-direct-manipulation]") {
  auto sketch = populated_sketch();
  auto moved = Sketch3DInteractionService::move_point(sketch, SketchEntityId("point.b"), {20.0, 4.0, -3.0});
  REQUIRE(moved);
  const auto* point_b = moved.value().sketch.find_point(SketchEntityId("point.b"));
  REQUIRE(point_b != nullptr);
  REQUIRE(point_b->x().explicit_coordinate().has_value());
  CHECK(point_b->x().explicit_coordinate()->millimeters() == Catch::Approx(20.0));
  REQUIRE(moved.value().sketch.find_line(SketchEntityId("line.ab")) != nullptr);
  REQUIRE(moved.value().sketch.find_polyline(SketchEntityId("polyline.path")) != nullptr);
  CHECK(sketch.find_point(SketchEntityId("point.b"))->x().explicit_coordinate()->millimeters() == Catch::Approx(10.0));
}

TEST_CASE("Block 120 projects a planar Sketch point with explicit source intent", "[core][sketch-3d][gui][sketch-3d-edit]") {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.project"), "Projection");
  REQUIRE(sketch);
  auto target = SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.planar"));
  REQUIRE(target);
  auto projected = Sketch3DInteractionService::project_planar_point(sketch.value(), SketchEntityId("point.projected"),
      {3.0, 4.0, 5.0}, SketchId("sketch.planar"), target.value());
  REQUIRE(projected);
  REQUIRE(projected.value().projected_point.has_value());
  CHECK(projected.value().projected_point->source_sketch == SketchId("sketch.planar"));
  CHECK(projected.value().projected_point->local_point == SketchEntityId("point.projected"));
  REQUIRE(projected.value().sketch.find_point(SketchEntityId("point.projected")) != nullptr);
}

TEST_CASE("Block 121 coverage manifest contains every planned Sketch capability",
          "[integration][interactive-sketcher][integration][sketch-gui-headless]") {
  constexpr auto coverage = InteractiveSketcherAcceptance::coverage();
  REQUIRE(coverage.size() == 15U);
  for (std::size_t i = 0; i < coverage.size(); ++i) {
    INFO("capability: " << coverage[i].capability);
    CHECK_FALSE(coverage[i].capability.empty());
    CHECK_FALSE(coverage[i].contract.empty());
    CHECK_FALSE(coverage[i].focused_tag.empty());
    CHECK(InteractiveSketcherAcceptance::covers_all_evidence(coverage[i].evidence));
    for (std::size_t j = i + 1U; j < coverage.size(); ++j)
      CHECK(coverage[i].capability != coverage[j].capability);
  }
}

TEST_CASE("Block 121 acceptance evidence freezes all required interaction invariants",
          "[integration][interactive-sketcher]") {
  constexpr unsigned expected =
      static_cast<unsigned>(SketchAcceptanceEvidence::MouseScriptEquivalence) |
      static_cast<unsigned>(SketchAcceptanceEvidence::PersistenceRecompute) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ExactUndoRedo) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ConflictAtomicity) |
      static_cast<unsigned>(SketchAcceptanceEvidence::ReferenceRepair) |
      static_cast<unsigned>(SketchAcceptanceEvidence::KeyboardApplyCancel) |
      static_cast<unsigned>(SketchAcceptanceEvidence::HighDpiMapping) |
      static_cast<unsigned>(SketchAcceptanceEvidence::NoStalePreview);
  CHECK(InteractiveSketcherAcceptance::all_evidence == expected);
}

TEST_CASE("Block 121 performance budgets are deterministic and bounded",
          "[performance][sketch-interaction]") {
  constexpr auto budgets = InteractiveSketcherAcceptance::performance_budgets();
  REQUIRE(budgets.size() == 4U);
  CHECK(budgets[0].operation == std::string_view("hover-hit-test"));
  CHECK(budgets[1].operation == std::string_view("solver-backed-drag-frame"));
  CHECK(budgets[2].operation == std::string_view("planar-solve"));
  CHECK(budgets[3].operation == std::string_view("region-recognition"));
  for (const auto& budget : budgets) {
    CHECK(budget.representative_entity_count > 0U);
    CHECK(budget.budget_milliseconds > 0.0);
    CHECK(budget.budget_milliseconds <= 50.0);
  }
}
