#include "../support/adaptive_project_fixture.hpp"
#include "../support/assembly_hole_fixture.hpp"
#include "../support/engineering_resource_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/assembly_exchange_graph.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/assembly_hole_coordinator.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <numbers>

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

TEST_CASE("Stateful Assembly definitions compound visible Solid and Surface Bodies while mass "
          "retains hidden sources",
          "[geometry][assembly-multi-body][model-state][parity-b198][X02]") {
  auto p = f::native_base();
  r::annotate(p);
  f::rectangle(p, "second.profile");
  auto second = f::checked(Feature::create_additive_extrude(
      FeatureId("second"), "Second solid", SketchId("second.profile"), ParameterId("depth")));
  f::accept(p.add_feature(f::checked(second.with_body_result_context(f::new_body(p, "second")))));
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("second.place"), BodyId("body.second"), BodyTransformCoordinateSpace::World,
      std::nullopt, {40, 0, 0}, false, false))));
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("skin.distance"), "Skin distance", f::checked(Quantity::length_mm(1))))));
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("skin.tolerance"), "Skin tolerance", f::checked(Quantity::length_mm(1e-6))))));
  auto source = f::checked(FaceReference::create_planar(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  f::accept(p.add_surface_feature(f::checked(SurfaceOffsetFeature::create_offset(
      FeatureId("skin.offset"), "Visible surface from hidden solid",
      f::checked(SurfaceReference::create_faces(BodyId("body.base"), {source})),
      {ParameterId("skin.distance"), ParameterId("skin.tolerance"), SurfaceOffsetSide::AlongNormal,
       SurfaceOffsetApproximation::ExactAnalytic, std::nullopt},
      f::new_body(p, "skin", BodyKind::Surface)))));
  f::accept(p.set_body_visibility(BodyId("body.base"), BodyVisibility::Hidden));
  auto wide = state("wide", 40);
  wide.features = {{"skin.offset", true}};
  wide.materials = {{std::nullopt, r::key("synthetic.material", "r2")}};
  f::accept(p.replace_model_states({wide}));
  auto root = f::checked(AssemblyDocument::create(DocumentId("multi.root"), "Multiple Bodies"));
  f::accept(root.add_member_part(p.id()));
  f::accept(root.add_component_instance(f::checked(ComponentInstance::create(
      ComponentInstanceId("primary"), "Primary", p.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Grounded))));
  f::accept(root.add_component_instance(f::checked(ComponentInstance::create(
      ComponentInstanceId("wide"), "Wide", p.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Grounded,
      {{100, 20, 5}, {0, 0, 90}}, wide.id))));
  auto project = f::checked(Project::create(DocumentId("multi.project"), "Multiple Bodies", root));
  f::accept(project.add_part_document(p));
  const auto original = f::checked(serialize_project_to_json(project));
  const auto posed = f::checked(AssemblyStepExporter{}.build_posed_shape(project));
  const auto solid_summary = [](const TopoDS_Shape& shape) {
    std::pair<std::size_t, double> result{};
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
      ++result.first;
      GProp_GProps properties;
      BRepGProp::VolumeProperties(it.Current(), properties);
      result.second += properties.Mass();
    }
    return result;
  };
  const auto free_faces = [](const TopoDS_Shape& shape) {
    std::size_t count{};
    for (TopExp_Explorer it(shape, TopAbs_FACE, TopAbs_SOLID); it.More(); it.Next())
      ++count;
    return count;
  };
  const auto physical_shape = solid_summary(*OcctShapeView::get(posed));
  CHECK(physical_shape.first == 2);
  CHECK(physical_shape.second == Catch::Approx(4800).margin(1e-6));
  CHECK(free_faces(*OcctShapeView::get(posed)) == 1);
  CHECK(blcad::test::classify_solid_point(posed, {40, 0, 4}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(posed, {100, 60, 9}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(posed, {0, 0, 4}) ==
        blcad::test::SolidPointLocation::Outside);
  CHECK(blcad::test::classify_solid_point(posed, {100, 20, 9}) ==
        blcad::test::SolidPointLocation::Outside);
  const auto mass = f::checked(BodyResultInspector{}.inspect_physical_properties(project));
  REQUIRE(mass.data().complete_mass());
  CHECK(mass.data().volume_mm3 == Catch::Approx(9600).margin(1e-6));
  CHECK(mass.data().complete_mass()->mass_kg == Catch::Approx(.0512).margin(1e-10));
  CHECK(mass.data().computed_part_count == 2);
  auto scene = f::checked(ViewportSceneBuilder{}.build_project(project));
  REQUIRE(scene.size() == 2);
  double displayed{};
  for (const auto& item : scene)
    displayed += solid_summary(*OcctShapeView::get(item.shape)).second;
  CHECK(displayed == Catch::Approx(4800).margin(1e-6));
  struct Directory {
    std::filesystem::path root;
    Directory() {
      char pattern[] = "/tmp/blcad-assembly-multibody-XXXXXX";
      auto* path = ::mkdtemp(pattern);
      REQUIRE(path);
      root = path;
    }
    ~Directory() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } directory;
  REQUIRE(AssemblyStepExporter{}.write_step(project, (directory.root / "flat.step").string()));
  REQUIRE(AssemblyStructuredStepExporter{}.write_step(
      project, (directory.root / "structured.step").string()));
  for (const auto* name : {"flat.step", "structured.step"}) {
    STEPControl_Reader reader;
    REQUIRE(reader.ReadFile((directory.root / name).c_str()) == IFSelect_RetDone);
    REQUIRE(reader.TransferRoots() > 0);
    const auto shape = reader.OneShape();
    const auto actual = solid_summary(shape);
    CHECK(actual.first == 2);
    CHECK(actual.second == Catch::Approx(4800).margin(1e-5));
    CHECK(free_faces(shape) == 1);
  }
  CHECK(f::checked(serialize_project_to_json(project)) == original);
}

TEST_CASE("Display-only hiding cannot invalidate directed Project physical evaluation",
          "[geometry][model-state][project-physical-visibility][parity-b198]") {
  const int family = GENERATE(0, 1);
  const bool bodies_hidden = GENERATE(false, true);
  INFO("family=" << family << "; bodies hidden=" << bodies_hidden);
  auto p =
      family == 0
          ? f::checked(AdaptiveProjectEvaluator{}.bind(blcad::test::adaptive_project(),
                                                       blcad::test::adaptive_plane_reference()))
                .project
          : f::checked(AssemblyHoleCoordinator{}.bind(blcad::test::assembly_hole::project(),
                                                      blcad::test::assembly_hole::feature()))
                .project;
  if (family == 0) {
    for (auto& part : p.part_documents())
      r::annotate(part);
    f::accept(p.synchronize_engineering_resources());
  }
  const double expected = family == 0 ? 3200
                                      : 400 - 2 * std::numbers::pi * 1.2 * 1.2 * 2 + 1600 -
                                            2 * std::numbers::pi * .8 * .8 * 4 + 72;
  if (bodies_hidden) {
    for (auto& part : p.part_documents())
      for (const auto& body : part.bodies())
        f::accept(part.set_body_visibility(body.id(), BodyVisibility::Hidden));
  } else {
    AssemblyDesignView view{AssemblyDesignViewId("hidden"), "All hidden", {}, {}};
    const auto leaves = f::checked(
        AssemblyLeafOccurrenceResolver{}.resolve(p, AssemblyLeafParticipation::PhysicalActive));
    for (const auto& leaf : leaves)
      view.components.push_back(
          {{leaf.subassembly_path, leaf.component_instance, leaf.referenced_part_document}, false});
    f::accept(p.replace_assembly_design_views({view}));
    p = f::checked(p.evaluate_assembly_design_view(view.id));
  }
  const auto original = f::checked(serialize_project_to_json(p));
  auto normalized = evaluate_project_geometry(p);
  INFO((normalized ? std::string{} : normalized.error().message()));
  REQUIRE(normalized);
  auto adaptive = AdaptiveProjectEvaluator{}.evaluate(p);
  INFO((adaptive ? std::string{} : adaptive.error().message()));
  REQUIRE(adaptive);
  const auto physical = f::checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(physical.data().complete_mass());
  CHECK(physical.data().volume_mm3 == Catch::Approx(expected).margin(1e-5));
  CHECK(physical.data().complete_mass()->mass_kg == Catch::Approx(expected * 4e-6).margin(1e-10));
  if (!bodies_hidden) {
    const auto scene = f::checked(ViewportSceneBuilder{}.build_project(p));
    CHECK(scene.empty());
  }
  const auto explicit_export = AssemblyStepExporter{}.build_posed_shape(p);
  REQUIRE_FALSE(explicit_export);
  CHECK(explicit_export.error().message().find("visible") != std::string::npos);
  CHECK(f::checked(serialize_project_to_json(normalized.value())) == original);
  CHECK(f::checked(serialize_project_to_json(p)) == original);
}
