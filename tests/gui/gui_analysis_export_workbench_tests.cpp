#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace blcad;
using namespace blcad::gui;

namespace {
Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
PartDocument block_part() {
  auto part = PartDocument::create(DocumentId("part.analysis"), "Analysis block").value();
  REQUIRE(part.add_parameter(length("width", 10)));
  REQUIRE(part.add_parameter(length("height", 10)));
  REQUIRE(part.add_parameter(length("depth", 10)));
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
      ParameterId("width"), ParameterId("height")).value()));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(Body::create(BodyId("body.main"), "Main", BodyKind::Solid).value()));
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                   BodyId("body.main")).value();
  REQUIRE(part.add_feature(Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
      SketchId("sketch.base"), ParameterId("depth")).value().with_body_result_context(context).value()));
  return part;
}
void seed_project(GuiDocumentSession& session) {
  GuiAssemblyWorkbench assembly;
  REQUIRE(session.create_project(DocumentId("project.analysis"), "Analysis"));
  REQUIRE(assembly.add_part(session, block_part()));
  REQUIRE(assembly.add_root_member_part(session, DocumentId("part.analysis")));
  REQUIRE(assembly.add_root_component(session, ComponentInstance::create(
      ComponentInstanceId("component.a"), "A", DocumentId("part.analysis"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Grounded).value()));
  REQUIRE(assembly.add_root_component(session, ComponentInstance::create(
      ComponentInstanceId("component.b"), "B", DocumentId("part.analysis")).value()));
  auto first = AssemblyConstraintTarget::create(ComponentInstanceId("component.a"),
                                                 "feature.base.top").value();
  auto second = AssemblyConstraintTarget::create(ComponentInstanceId("component.b"),
                                                  "feature.base.bottom").value();
  REQUIRE(assembly.apply_root_constraint(session, AssemblyConstraint::create(
      AssemblyConstraintId("constraint.mate"), "Mate", AssemblyConstraintType::Mate,
      first, second).value()));
}
} // namespace

TEST_CASE("Block 104 GUI exposes deterministic interference clearance contact and DOF results",
          "[gui][analysis]") {
  GuiDocumentSession session;
  seed_project(session);
  GuiAnalysisExportWorkbench workbench;
  auto interference = workbench.analyze_interference(session);
  REQUIRE(interference);
  REQUIRE(interference.value().interferences.size() == 1);
  CHECK(interference.value().interferences.front().leaf_a.occurrence_key <
        interference.value().interferences.front().leaf_b.occurrence_key);
  CHECK(interference.value().interferences.front().overlap_volume_mm3 > 0);
  auto clearance = workbench.analyze_clearance(session);
  REQUIRE(clearance);
  CHECK(clearance.value().interferences.size() == 1);
  CHECK(clearance.value().clearance_violations.empty());
  auto contact = workbench.analyze_contact(session);
  REQUIRE(contact);
  REQUIRE(contact.value().records.size() == 1);
  CHECK(contact.value().records.front().classification ==
        geometry::AssemblyContactClassification::Interfering);
  auto dof = workbench.inspect_dof(session,
      {ComponentInstanceId("component.a"), ComponentInstanceId("component.b")});
  REQUIRE(dof);
  CHECK(dof.value().rank_summary.remaining_dof > 0);
  CHECK(dof.value().rank_summary.remaining_dof < 6);
  CHECK(session.project()->assembly().find_component_instance(ComponentInstanceId("component.a"))
            ->transform() == identity_rigid_transform());
}

TEST_CASE("Block 104 GUI exports flattened and structured Assembly STEP with cancellation preflight",
          "[gui][step-export]") {
  GuiDocumentSession session;
  seed_project(session);
  GuiAnalysisExportWorkbench workbench;
  const auto flat = std::filesystem::temp_directory_path() / "blcad-block104-flat.step";
  const auto structured = std::filesystem::temp_directory_path() / "blcad-block104-structured.step";
  const auto cancelled = std::filesystem::temp_directory_path() / "blcad-block104-cancelled.step";
  std::vector<std::string> stages;
  auto flat_result = workbench.export_step(session, {GuiStepExportMode::AssemblyFlattened, flat}, {},
      [&stages](std::size_t, std::size_t, std::string_view stage) { stages.emplace_back(stage); });
  REQUIRE(flat_result);
  CHECK(flat_result.value().exported_item_count == 2);
  CHECK(flat_result.value().written_bytes > 0);
  CHECK(stages == std::vector<std::string>{"writing STEP", "STEP complete"});
  auto structured_result = workbench.export_step(
      session, {GuiStepExportMode::AssemblyStructured, structured});
  REQUIRE(structured_result);
  CHECK(structured_result.value().exported_item_count == 2);
  CHECK(structured_result.value().written_bytes > 0);
  auto cancelled_result = workbench.export_step(
      session, {GuiStepExportMode::AssemblyFlattened, cancelled}, [] { return true; });
  CHECK(cancelled_result.has_error());
  CHECK_FALSE(std::filesystem::exists(cancelled));
  std::filesystem::remove(flat);
  std::filesystem::remove(structured);
}

TEST_CASE("Block 104 STEP preflight rejects stale results and wrong output mode",
          "[gui][step-export]") {
  GuiDocumentSession session;
  seed_project(session);
  GuiAnalysisExportWorkbench workbench;
  session.set_derived_results_fresh(false);
  CHECK(workbench.preflight_step_export(session,
      {GuiStepExportMode::AssemblyFlattened, "/tmp/stale.step"}).has_error());
  session.set_derived_results_fresh(true);
  CHECK(workbench.preflight_step_export(session,
      {GuiStepExportMode::PartMultiBody, "/tmp/wrong.step"}).has_error());
  CHECK(workbench.preflight_step_export(session,
      {GuiStepExportMode::AssemblyStructured, {}}).has_error());
}
