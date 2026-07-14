#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"

#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace {
const std::filesystem::path source_dir(BLCAD_SOURCE_DIR);

nlohmann::json read_json(const std::filesystem::path& path) {
  std::ifstream stream(path);
  REQUIRE(stream.good());
  nlohmann::json value;
  stream >> value;
  return value;
}
} // namespace

TEST_CASE("Block 105 coverage manifest accounts for every implemented GUI feature family",
          "[integration][gui-feature-coverage]") {
  const auto manifest = read_json(source_dir / "docs/gui-feature-coverage-manifest-mvp7.json");
  CHECK(manifest.at("schema") == "blcad.gui-feature-coverage.v1");
  CHECK(manifest.at("implemented_through") == 105);
  const std::set<std::string> required = {
      "document-session", "viewport-selection", "browser-properties", "parameters-expressions",
      "datum-construction-workplanes", "planar-sketch-reference-repair",
      "bodies-extrude-revolve-holes", "pattern-mirror-finishing-shell-draft",
      "body-boolean-transform", "sketch3d-pathcurve", "sweep-path-extrude-loft",
      "surface-features", "assembly-project-hierarchy", "assembly-relationships-joints",
      "assembly-solve-motion", "assembly-analysis", "part-step-export", "assembly-step-export"};
  std::set<std::string> covered;
  for (const auto& family : manifest.at("families")) {
    const std::string name = family.at("family");
    CHECK(covered.insert(name).second);
    for (const char* field : {"command", "inspection", "presentation", "persistence", "test",
                              "disposition"})
      CHECK_FALSE(family.at(field).get<std::string>().empty());
    CHECK(family.at("owner").get<int>() >= 95);
    CHECK(family.at("owner").get<int>() <= 104);
    if (family.at("disposition") == "read_only") CHECK(family.at("core_read_only") == true);
  }
  CHECK(covered == required);
  for (const auto& sample : manifest.at("sample_documents"))
    CHECK(std::filesystem::is_regular_file(source_dir / sample.get<std::string>()));
}

TEST_CASE("Block 105 GUI Part sample is canonically equivalent to headless load and recompute",
          "[integration][gui-feature-coverage][integration][gui-headless-equivalence]") {
  const auto path = source_dir / "examples/bolt_circle_plate.blcad.json";
  auto headless = read_part_document_json_file(path);
  REQUIRE(headless);
  GuiDocumentSession gui;
  REQUIRE(gui.open_file(path));
  REQUIRE(gui.part_document() != nullptr);
  CHECK(serialize_part_document_to_json(*gui.part_document()).value() ==
        serialize_part_document_to_json(headless.value()).value());

  auto cache = geometry::ShapeCache::create(ShapeCacheId("acceptance.headless")).value();
  auto executed = geometry::GeometryRecomputeExecutor{}.execute_document(headless.value(), cache);
  REQUIRE(executed);
  REQUIRE(gui.recompute());
  CHECK(gui.part_shape_cache()->feature_shape_count() == cache.feature_shape_count());
  CHECK(gui.part_shape_cache()->body_shape_count() == cache.body_shape_count());
  CHECK(gui.part_shape_cache()->has_final_shape() == cache.has_final_shape());

  const std::string original = serialize_part_document_to_json(*gui.part_document()).value();
  const Parameter& parameter = gui.part_document()->parameters().front();
  REQUIRE(parameter.type() == ParameterType::Length);
  GuiPartFoundationWorkbench foundation;
  REQUIRE(foundation.set_parameter_value(gui, parameter.id(),
      Quantity::length_mm(parameter.value().millimeters() + 1.0, parameter.id().value()).value()));
  const std::string edited = serialize_part_document_to_json(*gui.part_document()).value();
  CHECK(edited != original);
  REQUIRE(gui.undo());
  CHECK(serialize_part_document_to_json(*gui.part_document()).value() == original);
  REQUIRE(gui.redo());
  CHECK(serialize_part_document_to_json(*gui.part_document()).value() == edited);
  CHECK(gui.derived_results_fresh());
}

TEST_CASE("Block 105 GUI Project sample equals headless intent and survives recompute",
          "[integration][gui-feature-coverage][integration][gui-headless-equivalence]") {
  const auto path = source_dir / "examples/nested_subassembly.blcad.project.json";
  auto headless = read_project_json_file(path);
  REQUIRE(headless);
  GuiDocumentSession gui;
  REQUIRE(gui.open_file(path));
  REQUIRE(gui.project() != nullptr);
  CHECK(serialize_project_to_json(*gui.project()).value() == serialize_project_to_json(headless.value()).value());
  REQUIRE(gui.recompute());
  CHECK(gui.project()->validate_assembly_structure());
  CHECK(gui.project()->assembly().subassembly_instance_count() > 0);
}

TEST_CASE("Block 105 integrated fail-closed paths publish no partial GUI state",
          "[integration][gui-feature-coverage]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.fail_closed"), "Fail closed"));
  GuiPartFoundationWorkbench foundation;
  REQUIRE(foundation.create_parameter(session, Parameter::create_length(ParameterId("depth"),
      "Depth", Quantity::length_mm(10).value()).value()));
  const auto before_revision = session.presentation_revision();
  auto invalid = Feature::create_additive_extrude(FeatureId("feature.invalid"), "Invalid",
      SketchId("missing.sketch"), ParameterId("depth")).value();
  CHECK(foundation.preview_extrude(session, invalid).has_error());
  CHECK(session.part_document()->feature_count() == 0);
  CHECK(session.presentation_revision() == before_revision);
  CHECK(GuiSketchWorkbench::prompt_for(GuiSketchCommand::CreateSketch).required_capability ==
        "planar_workplane");
  session.set_derived_results_fresh(false);
  GuiAnalysisExportWorkbench export_workbench;
  const auto output = std::filesystem::temp_directory_path() / "blcad-block105-invalid.step";
  std::filesystem::remove(output);
  CHECK(export_workbench.preflight_step_export(
      session, {GuiStepExportMode::PartMultiBody, output}).has_error());
  CHECK_FALSE(std::filesystem::exists(output));
  std::filesystem::remove(output);
}
