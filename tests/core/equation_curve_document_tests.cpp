#include "../support/engineering_resource_fixture.hpp"
#include "../support/equation_curve_fixture.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/sketch_modify.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"
#include "blcad/core/sketch_transform_pattern.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <utility>

using namespace blcad;
namespace f = blcad::test::equation_fixture;
namespace {
std::string text(const PartDocument& part) {
  return f::checked(serialize_part_document_to_json(part));
}
bool contains(const std::vector<std::string>& nodes, const char* id) {
  return std::find(nodes.begin(), nodes.end(), id) != nodes.end();
}
} // namespace
TEST_CASE("Equation definitions join existing dependency graphs and reject invalid domain writes "
          "atomically",
          "[core][parity-b160][equation-document]") {
  auto part = f::part();
  const auto before = text(part);
  const auto edges = part.dependency_graph().dependencies();
  const auto dirty = part.invalidation_state().nodes_with_status(InvalidationStatus::Dirty);
  CHECK_FALSE(part.set_parameter_value(ParameterId("count"), Quantity::count(1).value()));
  CHECK(text(part) == before);
  CHECK(part.dependency_graph().dependencies() == edges);
  CHECK(part.invalidation_state().nodes_with_status(InvalidationStatus::Dirty) == dirty);
  const auto changed = part.set_parameter_value(ParameterId("count"), Quantity::count(5).value());
  REQUIRE(changed);
  CHECK(contains(changed.value(), "planar"));
  CHECK(contains(changed.value(), "spatial"));
  auto sketch = *part.find_sketch(SketchId("planar"));
  REQUIRE(sketch.replace_equation_curve(f::planar("@{radius}/1mm")));
  REQUIRE(part.update_sketch(sketch));
  const auto count = part.set_parameter_value(ParameterId("count"), Quantity::count(4).value());
  REQUIRE(count);
  CHECK_FALSE(contains(count.value(), "planar"));
  CHECK(contains(count.value(), "spatial"));
  const auto radius =
      part.set_parameter_value(ParameterId("radius"), Quantity::length_mm(6).value());
  REQUIRE(radius);
  CHECK(contains(radius.value(), "planar"));
  CHECK(contains(radius.value(), "spatial"));
  REQUIRE(part.add_expression_parameter(ParameterId("upper"), "Upper", ParameterType::Count,
                                        "Count + 1"));
  sketch = *part.find_sketch(SketchId("planar"));
  REQUIRE(sketch.replace_equation_curve(f::planar("@{upper}")));
  REQUIRE(part.update_sketch(sketch));
  const auto formula_before = text(part);
  CHECK_FALSE(part.set_parameter_formula(ParameterId("upper"), "1"));
  CHECK(text(part) == formula_before);
  CHECK_FALSE(part.set_parameter_formula(ParameterId("upper"), "Upper + 1"));
  CHECK(text(part) == formula_before);
}

TEST_CASE("Equation owner scopes and shared entity IDs fail before publication",
          "[core][parity-b160][equation-document]") {
  auto part = f::part();
  const auto before = text(part);
  auto sketch = *part.find_sketch(SketchId("planar"));
  CHECK_FALSE(sketch.add_entity(f::spatial()));
  CHECK_FALSE(
      sketch.add_entity(LineSegment::create(SketchEntityId("equation"), {0, 0}, {1, 0}).value()));
  CHECK_FALSE(sketch.add_entity(
      SplineSegment::create_cubic_bezier(SketchEntityId("equation"), {0, 0}, {1, 0}, {2, 1}, {3, 1})
          .value()));
  auto missing =
      EquationCurve::create(SketchEntityId("missing"), EquationCurveMode::Parametric2D,
                            {"u", CurveUnit::Scalar}, "0", "1", {"@{missing}*u", "0mm", ""})
          .value();
  REQUIRE(sketch.add_entity(missing));
  CHECK_FALSE(part.update_sketch(sketch));
  CHECK(text(part) == before);
  auto space = *part.find_sketch_3d(Sketch3DId("spatial"));
  CHECK_FALSE(space.add_equation_curve(f::planar()));
  REQUIRE(space.add_guide_curve(SketchGuideCurve3D::create(SketchEntityId("guide"),
                                                           SketchEntityId("equation"),
                                                           SketchGuideCurve3DRole::SweepPath)
                                    .value()));
  CHECK_FALSE(space.remove_equation_curve(SketchEntityId("equation")));
  REQUIRE(space.remove_guide_curve(SketchEntityId("guide")));
  REQUIRE(space.remove_equation_curve(SketchEntityId("equation")));
  CHECK(space.entity_count() == 0);
}

TEST_CASE("Equation formulas survive topology materialization and ordinary sketch modifications",
          "[core][parity-b160][equation-document]") {
  const auto part = f::part();
  const auto source = *part.find_sketch(SketchId("planar"));
  const auto topology = SketchTopology::migrate_legacy(source).value();
  const auto* equation = topology.find_entity("entity/equation");
  REQUIRE(equation);
  CHECK(equation->points().empty());
  const auto line_end = topology.find_entity("entity/line")->points()[1];
  auto points = topology.points();
  for (auto& p : points)
    if (p.id() == line_end)
      p = p.with_position({5, 2}).value();
  const auto moved =
      SketchTopology::create(source.id(), points, topology.entities(), topology.dependencies())
          .value();
  const auto materialized = SketchTopologyLegacyMaterializer{}.materialize(source, moved);
  REQUIRE(materialized);
  CHECK(materialized.value().equation_curves() == source.equation_curves());
  CHECK(materialized.value().find_line_segment(SketchEntityId("line"))->end() == Point2{5, 2});
  CHECK(SketchTopology::migrate_legacy(materialized.value()).value() == moved);
  auto entities = topology.entities();
  for (auto& e : entities)
    if (e.id() == "entity/equation")
      e = SketchTopologyEntity::create(e.id(), SketchTopologyEntityKind::Line,
                                       topology.find_entity("entity/line")->points())
              .value();
  const auto sampled =
      SketchTopology::create(source.id(), topology.points(), entities, topology.dependencies())
          .value();
  CHECK_FALSE(SketchTopologyLegacyMaterializer{}.materialize(source, sampled));
  const auto split = SketchModifyService::split(source, SketchEntityId("line"), {1, .5});
  REQUIRE(split);
  CHECK(split.value().sketch.equation_curves() == source.equation_curves());
  const auto translated =
      SketchTransformPatternService::move(source, {SketchEntityId("line")}, {2, 3});
  REQUIRE(translated);
  CHECK(translated.value().sketch.equation_curves() == source.equation_curves());
  CHECK_FALSE(SketchTransformPatternService::move(source, {SketchEntityId("equation")}, {2, 3}));
  auto role = source;
  REQUIRE(role.set_construction({SketchEntityId("equation")}, {}, true));
  CHECK(role.find_equation_curve(SketchEntityId("equation"))->construction());
}

TEST_CASE("Part V8 roundtrips exact planar and spatial intent without authored samples",
          "[core][parity-b160][equation-json]") {
  auto part = f::part();
  const auto encoded = text(part);
  const auto root = nlohmann::json::parse(encoded);
  CHECK(root.at("version") == 8);
  CHECK(root.at("history_version") == 1);
  CHECK(root.at("feature_order").empty());
  const auto restored = deserialize_part_document_from_json(encoded);
  REQUIRE(restored);
  CHECK(text(restored.value()) == encoded);
  CHECK(restored.value().find_sketch(SketchId("planar"))->equation_curves() ==
        part.find_sketch(SketchId("planar"))->equation_curves());
  CHECK(restored.value().find_sketch_3d(Sketch3DId("spatial"))->equation_curves() ==
        part.find_sketch_3d(Sketch3DId("spatial"))->equation_curves());
  CHECK(part.find_sketch(SketchId("planar"))->points().empty());
  CHECK(part.find_sketch_3d(Sketch3DId("spatial"))->points().empty());
  auto sketch = *part.find_sketch(SketchId("planar"));
  REQUIRE(sketch.remove_equation_curve(SketchEntityId("equation")));
  REQUIRE(part.update_sketch(sketch));
  auto space = *part.find_sketch_3d(Sketch3DId("spatial"));
  REQUIRE(space.remove_equation_curve(SketchEntityId("equation")));
  REQUIRE(part.update_sketch_3d(space));
  const auto legacy = nlohmann::json::parse(text(part));
  CHECK(legacy.at("version") == 1);
  CHECK_FALSE(legacy.at("sketches")[0].contains("equation_curves"));
  CHECK_FALSE(legacy.at("sketches_3d")[0].contains("equation_curves"));
  const auto again = deserialize_part_document_from_json(legacy.dump());
  REQUIRE(again);
  CHECK(text(again.value()) == text(part));
}

TEST_CASE(
    "Equation persistence rejects malformed records unsupported envelopes and unresolved scope",
    "[core][parity-b160][equation-json]") {
  const auto root = nlohmann::json::parse(text(f::part()));
  for (int mutation = 0; mutation < 15; ++mutation) {
    CAPTURE(mutation);
    auto value = root;
    auto& record = value["sketches"][0]["equation_curves"][0];
    switch (mutation) {
    case 0:
      value["version"] = 7;
      break;
    case 1:
      record["samples"] = nlohmann::json::array();
      break;
    case 2:
      record["policy"] = "unsafe_eval";
      break;
    case 3:
      record["domain"]["unit"] = "count";
      break;
    case 4:
      record["domain"]["lower"] = "u";
      break;
    case 5:
      record["coordinates"]["z"] = "0mm";
      break;
    case 6:
      record["coordinates"]["x"] = "system(1)";
      break;
    case 7:
      record["coordinates"]["x"] = "@{missing}";
      break;
    case 8:
      record["coordinates"]["x"] = "sin(u)";
      break;
    case 9:
      record["tolerance_mm"] = true;
      break;
    case 10:
      record["construction"] = "false";
      break;
    case 11:
      record["mode"] = "parametric_3d";
      record["coordinates"]["z"] = "0mm";
      break;
    case 12:
      value["sketches"][0]["equation_curves"].push_back(record);
      break;
    case 13:
      record["domain"]["upper"] = "1";
      break;
    case 14:
      value["version"] = 8.5;
      break;
    }
    CHECK_FALSE(deserialize_part_document_from_json(value.dump()));
  }
  auto empty = root;
  empty["sketches"][0]["equation_curves"] = nlohmann::json::array();
  empty["sketches_3d"][0]["equation_curves"] = nlohmann::json::array();
  CHECK_FALSE(deserialize_part_document_from_json(empty.dump()));
}

TEST_CASE("Equation V8 retains Pattern Hole history and Project resource ownership",
          "[core][parity-b160][equation-json]") {
  auto member = blcad::test::hole_pattern_fixture::part();
  REQUIRE(member.add_linear_pattern_feature(blcad::test::hole_pattern_fixture::linear()));
  blcad::test::engineering_fixture::annotate(member);
  auto sketch =
      Sketch::create(SketchId("equation.extra"), "Equation", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_entity(
      EquationCurve::create(SketchEntityId("equation"), EquationCurveMode::Parametric2D,
                            {"u", CurveUnit::Scalar}, "0", "1", {"@{width}*u", "0mm", ""})
          .value()));
  REQUIRE(member.add_sketch(sketch));
  const auto serialized = text(member);
  CHECK(nlohmann::json::parse(serialized).at("version") == 8);
  const auto restored = deserialize_part_document_from_json(serialized);
  REQUIRE(restored);
  CHECK(text(restored.value()) == serialized);
  auto assembly = AssemblyDocument::create(DocumentId("assembly"), "Assembly").value();
  auto project = Project::create(DocumentId("project"), "Equations", assembly).value();
  REQUIRE(project.add_part_document(member));
  const auto json = serialize_project_to_json(project);
  REQUIRE(json);
  auto root = nlohmann::json::parse(json.value());
  CHECK(root.at("version") == 2);
  CHECK(root.at("parts")[0].at("version") == 8);
  CHECK(root.at("parts")[0].contains("engineering_resource_refs"));
  CHECK_FALSE(root.at("parts")[0].contains("engineering_resources"));
  const auto reopened = deserialize_project_from_json(json.value());
  REQUIRE(reopened);
  CHECK(serialize_project_to_json(reopened.value()).value() == json.value());
}
