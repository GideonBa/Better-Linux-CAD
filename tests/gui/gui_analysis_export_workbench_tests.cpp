#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/step_exporter.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
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
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
void seed_reference_project(GuiDocumentSession& session, const QTemporaryDir& directory) {
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(10)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(10))));
  REQUIRE(geometry::StepExporter{}.write_step(shape,
                                              directory.filePath("supplier.step").toStdString()));
  REQUIRE(session.create_project(DocumentId("reference.project"), "Reference analysis"));
  REQUIRE(session.save_as(directory.filePath("reference.blcad.project.json").toStdString()));
  const GuiStepImportWorkbench imports;
  auto registered = checked(
      imports.prepare_register(session, "Supplier", "supplier.step", StepImportMode::Reference));
  REQUIRE(imports.apply(session, registered));
  const auto inspected = checked(imports.inspect_geometry(session, registered.source.id()));
  std::optional<std::size_t> product;
  for (std::size_t i = 0; i < inspected.geometry.products.size(); ++i)
    if (!inspected.geometry.products[i].is_assembly &&
        !inspected.geometry.products[i].bodies.empty()) {
      product = i;
      break;
    }
  REQUIRE(product);
  auto topology = checked(imports.prepare_topology(session, registered.source.id(), *product));
  REQUIRE(imports.apply_topology(session, topology));
  GuiStepReferenceRequest request;
  request.catalog = topology.catalog.id();
  request.new_part = DocumentId("reference.part");
  request.name = "Reference Part";
  request.target_assembly = session.project()->assembly().id();
  request.instance = ComponentInstanceId("reference.fixed");
  REQUIRE(imports.apply_reference(session, checked(imports.prepare_reference(session, request))));
  request.operation = GuiStepReferenceOperation::Repeat;
  request.reference_part = DocumentId("reference.part");
  request.instance = ComponentInstanceId("reference.moving");
  REQUIRE(imports.apply_reference(session, checked(imports.prepare_reference(session, request))));
  REQUIRE(GuiAssemblyWorkbench{}.set_component_grounding(
      session, ComponentInstanceId("reference.fixed"), ComponentGroundingState::Grounded));
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

TEST_CASE("Reference Assembly analysis export and solve revalidate saved-root source bytes",
          "[gui][step-reference-part][analysis][step-export]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  GuiDocumentSession session;
  seed_reference_project(session, directory);
  GuiAnalysisExportWorkbench analysis;
  const auto contact = checked(analysis.analyze_contact(session));
  REQUIRE(contact.records.size() == 1);
  CHECK(contact.records[0].classification == geometry::AssemblyContactClassification::Interfering);
  CHECK(contact.records[0].overlap_volume_mm3 == Catch::Approx(1000));
  CHECK(checked(analysis.analyze_interference(session)).interferences.size() == 1);
  CHECK(checked(analysis.analyze_clearance(session)).interferences.size() == 1);
  for (const auto mode :
       {GuiStepExportMode::AssemblyFlattened, GuiStepExportMode::AssemblyStructured}) {
    const auto name =
        mode == GuiStepExportMode::AssemblyFlattened ? "flat.step" : "structured.step";
    auto exported =
        checked(analysis.export_step(session, {mode, directory.filePath(name).toStdString()}));
    CHECK(exported.exported_item_count == 2);
    CHECK(exported.written_bytes > 0);
  }
  CHECK_FALSE(analysis.export_step(session, {GuiStepExportMode::AssemblyFlattened,
                                             directory.filePath("supplier.step").toStdString()}));

  const auto* reference = session.project()->find_part_document(DocumentId("reference.part"));
  REQUIRE(reference);
  const auto standalone_path = directory.filePath("reference.part.blcad.json").toStdString();
  REQUIRE(write_part_document_json_file(*reference, standalone_path));
  GuiDocumentSession standalone;
  REQUIRE(standalone.open_file(standalone_path));
  const GuiStepExportRequest part_export{GuiStepExportMode::PartMultiBody,
                                         directory.filePath("reference.part.step").toStdString()};
  CHECK(checked(analysis.preflight_step_export(standalone, part_export)) == 1);
  CHECK(checked(analysis.export_step(standalone, part_export)).exported_item_count == 1);

  const auto spelling = checked(make_assembly_reference_target_spelling(DatumPlaneId("datum.xy")));
  auto a =
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("reference.fixed"), spelling));
  auto b =
      checked(AssemblyConstraintTarget::create(ComponentInstanceId("reference.moving"), spelling));
  GuiAssemblyWorkbench assembly;
  REQUIRE(assembly.apply_root_constraint(
      session, checked(AssemblyConstraint::create(AssemblyConstraintId("reference.parallel"),
                                                  "Parallel origin planes",
                                                  AssemblyConstraintType::Parallel, a, b))));
  const std::vector<ComponentInstanceId> group{a.component_instance(), b.component_instance()};
  CHECK(analysis.inspect_dof(session, group));
  auto solved = checked(assembly.solve_root_group(session, group));
  REQUIRE(solved.converged());
  const auto frame_spelling =
      checked(make_assembly_frame_target_spelling(DatumPlaneId("datum.xy")));
  const auto frame_a =
      checked(AssemblyConstraintTarget::create(a.component_instance(), frame_spelling));
  const auto frame_b =
      checked(AssemblyConstraintTarget::create(b.component_instance(), frame_spelling));
  REQUIRE(assembly.apply_root_joint(
      session, checked(AssemblyJoint::create(
                   AssemblyJointId("reference.revolute"), "Explicit BLCAD origin frames",
                   AssemblyJointType::Revolute, frame_a, frame_b, AssemblyJointState::Active,
                   checked(Quantity::angle_deg(-180)), checked(Quantity::angle_deg(180)),
                   checked(Quantity::angle_deg(0))))));
  auto motion = checked(assembly.preview_joint_motion(
      session, AssemblyJointId("reference.revolute"), checked(Quantity::angle_deg(30))));
  REQUIRE(motion.converged());
  // Obtain solve evidence after the joint was authored; stale-byte rejection
  // must not be masked by an unrelated model snapshot mismatch.
  solved = checked(assembly.solve_root_group(session, group));
  const auto original = checked(serialize_project_to_json(*session.project()));
  const std::string history(session.undo_label());
  QFile asset(directory.filePath("supplier.step"));
  REQUIRE(asset.open(QIODevice::Append));
  REQUIRE(asset.write("\n/* changed external bytes */\n") > 0);
  asset.close();
  REQUIRE(session.derived_results_fresh());
  CHECK_FALSE(analysis.analyze_contact(session));
  CHECK_FALSE(analysis.analyze_interference(session));
  CHECK_FALSE(analysis.analyze_clearance(session));
  CHECK_FALSE(analysis.inspect_dof(session, group));
  CHECK_FALSE(assembly.apply_solve(session, solved));
  CHECK_FALSE(assembly.apply_joint_motion(session, motion));
  CHECK_FALSE(
      analysis.preflight_step_export(session, {GuiStepExportMode::AssemblyFlattened,
                                               directory.filePath("stale.step").toStdString()}));
  CHECK_FALSE(analysis.export_step(session, {GuiStepExportMode::AssemblyStructured,
                                             directory.filePath("stale.step").toStdString()}));
  CHECK_FALSE(QFile::exists(directory.filePath("stale.step")));
  CHECK(checked(serialize_project_to_json(*session.project())) == original);
  CHECK(session.undo_label() == history);
}
