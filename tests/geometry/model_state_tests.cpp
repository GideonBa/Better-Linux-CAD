#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/assembly_exchange_graph.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace r = blcad::test::engineering_fixture;
namespace {
ModelState state(const char* id, double width) {
  return {ModelStateId(id),
          id,
          {{ParameterId("width"), f::checked(Quantity::length_mm(width))}},
          {},
          {},
          {},
          {}};
}
PartDocument part() {
  auto p = f::native_base();
  r::annotate(p);
  f::accept(p.replace_model_states({state("large", 40), state("same", 40)}));
  return p;
}
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("state.cache")));
}
double volume(const GeometryShape& shape) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(*OcctShapeView::get(shape), p);
  return p.Mass();
}
void execute(const PartDocument& p, ShapeCache& c) {
  auto result = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
}
Project project() {
  auto p = part();
  auto assembly = f::checked(AssemblyDocument::create(DocumentId("root"), "Variants"));
  f::accept(assembly.add_member_part(p.id()));
  for (const auto& [id, model_state, x] :
       {std::tuple{"small", "primary", 0.}, {"large", "large", 50.}}) {
    auto component = f::checked(ComponentInstance::create(
        ComponentInstanceId(id), id, p.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Grounded, {{x, 0, 0}, {}},
        ModelStateId(model_state)));
    f::accept(assembly.add_component_instance(component));
  }
  auto project = f::checked(Project::create(DocumentId("variants"), "Variants", assembly));
  f::accept(project.add_part_document(p));
  return project;
}
double step_volume(const std::filesystem::path& path) {
  STEPControl_Reader reader;
  REQUIRE(reader.ReadFile(path.c_str()) == IFSelect_RetDone);
  REQUIRE(reader.TransferRoots() > 0);
  GProp_GProps p;
  BRepGProp::VolumeProperties(reader.OneShape(), p);
  return p.Mass();
}
} // namespace

TEST_CASE("Effective states have independent complete freshness even for equal evaluated values",
          "[geometry][model-state][parity-b190][V01]") {
  auto primary = part();
  const auto authored = f::checked(serialize_part_document_to_json(primary));
  auto large = f::checked(primary.evaluate_model_state(ModelStateId("large")));
  auto same = f::checked(primary.evaluate_model_state(ModelStateId("same")));
  auto c = cache();
  execute(large, c);
  REQUIRE(c.validate_complete_geometry_result(large));
  CHECK_FALSE(c.validate_complete_geometry_result(same));
  CHECK_FALSE(c.validate_complete_geometry_result(primary));
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) == Catch::Approx(3200));
  for (int i = 0; i < 3; ++i) {
    execute(primary, c);
    CHECK(volume(*c.find_body_shape(BodyId("body.base"))) == Catch::Approx(1600));
    execute(large, c);
    CHECK(volume(*c.find_body_shape(BodyId("body.base"))) == Catch::Approx(3200));
  }
  auto fresh = cache();
  execute(large, fresh);
  CHECK(volume(*fresh.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(volume(*c.find_body_shape(BodyId("body.base")))));
  auto physical = f::checked(BodyResultInspector{}.inspect_physical_properties(large));
  REQUIRE(physical.validate_current(large));
  CHECK_FALSE(physical.validate_current(same));
  CHECK_FALSE(physical.validate_current(primary));
  CHECK(f::checked(serialize_part_document_to_json(primary)) == authored);
}

TEST_CASE("One shared Part produces independent state shapes mass scene and STEP definitions",
          "[geometry][model-state][parity-b190][V01][V02]") {
  auto p = project();
  const auto authored = f::checked(serialize_project_to_json(p));
  auto scene = f::checked(ViewportSceneBuilder{}.build_project(p));
  REQUIRE(scene.size() == 2);
  double total = 0;
  for (const auto& item : scene)
    total += volume(item.shape);
  CHECK(total == Catch::Approx(4800));
  auto mass = f::checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(mass.data().occurrences.size() == 2);
  CHECK(mass.data().computed_part_count == 2);
  REQUIRE(mass.data().occurrences[0].bodies.size() == 1);
  REQUIRE(mass.data().occurrences[1].bodies.size() == 1);
  CHECK(mass.data().volume_mm3 == Catch::Approx(4800));
  REQUIRE(mass.validate_current(p));
  auto graph = f::checked(AssemblyExchangeGraph::build(p));
  REQUIRE(graph.part_product_definition_count() == 2);
  CHECK(graph.part_product_definitions()[0].identity.selected_model_state !=
        graph.part_product_definitions()[1].identity.selected_model_state);
  const auto folder = std::filesystem::temp_directory_path() / "blcad-model-state-export";
  std::filesystem::create_directories(folder);
  const auto flat = folder / "flat.step", structured = folder / "structured.step";
  REQUIRE(AssemblyStepExporter{}.write_step(p, flat.string()));
  REQUIRE(AssemblyStructuredStepExporter{}.write_step(p, structured.string()));
  CHECK(step_volume(flat) == Catch::Approx(4800).margin(1e-4));
  CHECK(step_volume(structured) == Catch::Approx(4800).margin(1e-4));
  auto reopened = f::checked(deserialize_project_from_json(authored));
  CHECK(f::checked(BodyResultInspector{}.inspect_physical_properties(reopened)).data().volume_mm3 ==
        Catch::Approx(4800));
  CHECK(f::checked(serialize_project_to_json(p)) == authored);
  REQUIRE(p.assembly().set_component_model_state(ComponentInstanceId("large"),
                                                 primary_model_state_id()));
  CHECK_FALSE(mass.validate_current(p));
  CHECK(f::checked(BodyResultInspector{}.inspect_physical_properties(p)).data().volume_mm3 ==
        Catch::Approx(3200));
  std::filesystem::remove_all(folder);
}

TEST_CASE("Suppressed creator states have no geometry and do not contaminate Primary",
          "[geometry][model-state][parity-b190][V01]") {
  auto primary = part();
  ModelState empty{ModelStateId("empty"), "Empty", {}, {{"base", true}}, {}, {}, {}};
  auto definitions = primary.model_states();
  definitions.push_back(empty);
  REQUIRE(primary.replace_model_states(definitions));
  auto effective = f::checked(primary.evaluate_model_state(empty.id));
  auto c = cache();
  execute(effective, c);
  CHECK(c.body_shape_count() == 0);
  CHECK_FALSE(c.has_final_shape());
  execute(primary, c);
  REQUIRE(c.find_body_shape(BodyId("body.base")));
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) == Catch::Approx(1600));
}
