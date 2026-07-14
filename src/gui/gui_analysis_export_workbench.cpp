#include "blcad/gui/gui_analysis_export_workbench.hpp"

#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/step_exporter.hpp"

namespace blcad::gui {
namespace {
Result<const Project*> require_project(const GuiDocumentSession& session, std::string_view operation) {
  return session.project()
      ? Result<const Project*>::success(session.project())
      : Result<const Project*>::failure(Error::validation("gui.analysis", std::string(operation) + " requires an active Project"));
}
} // namespace

Result<geometry::AssemblySolveDiagnostics> GuiAnalysisExportWorkbench::inspect_dof(
    const GuiDocumentSession& session, std::vector<ComponentInstanceId> group) const {
  auto project = require_project(session, "DOF inspection");
  if (project.has_error()) return Result<geometry::AssemblySolveDiagnostics>::failure(project.error());
  return geometry::AssemblySolveDiagnosticsAnalyzer{}.analyze(*project.value(), group);
}
Result<geometry::AssemblyInterferenceAnalysis> GuiAnalysisExportWorkbench::analyze_interference(
    const GuiDocumentSession& session, geometry::AssemblyInterferenceAnalysisOptions options) const {
  auto project = require_project(session, "interference analysis");
  if (project.has_error()) return Result<geometry::AssemblyInterferenceAnalysis>::failure(project.error());
  return geometry::AssemblyInterferenceAnalyzer{}.analyze(*project.value(), options);
}
Result<geometry::AssemblyClearanceAnalysis> GuiAnalysisExportWorkbench::analyze_clearance(
    const GuiDocumentSession& session, geometry::AssemblyClearanceAnalysisOptions options) const {
  auto project = require_project(session, "clearance analysis");
  if (project.has_error()) return Result<geometry::AssemblyClearanceAnalysis>::failure(project.error());
  return geometry::AssemblyClearanceAnalyzer{}.analyze(*project.value(), options);
}
Result<geometry::AssemblyContactAnalysis> GuiAnalysisExportWorkbench::analyze_contact(
    const GuiDocumentSession& session, geometry::AssemblyContactAnalysisOptions options) const {
  auto project = require_project(session, "contact analysis");
  if (project.has_error()) return Result<geometry::AssemblyContactAnalysis>::failure(project.error());
  return geometry::AssemblyContactAnalyzer{}.analyze(*project.value(), options);
}
Result<geometry::AssemblyRevoluteSweepAnalysis> GuiAnalysisExportWorkbench::analyze_revolute_sweep(
    const GuiDocumentSession& session, const geometry::AssemblyRevoluteSweepRequest& request,
    geometry::AssemblyRevoluteSweepAnalysisOptions options) const {
  auto project = require_project(session, "motion sweep");
  if (project.has_error()) return Result<geometry::AssemblyRevoluteSweepAnalysis>::failure(project.error());
  return geometry::AssemblyRevoluteSweepAnalyzer{}.analyze(*project.value(), request, options);
}

Result<std::size_t> GuiAnalysisExportWorkbench::preflight_step_export(
    const GuiDocumentSession& session, const GuiStepExportRequest& request) const {
  if (request.output.empty())
    return Result<std::size_t>::failure(Error::export_error("gui.step_export", "output path must not be empty"));
  if (!session.derived_results_fresh())
    return Result<std::size_t>::failure(Error::export_error("gui.step_export", "recompute must be fresh before export"));
  switch (request.mode) {
  case GuiStepExportMode::PartMultiBody:
    if (!session.part_document() || !session.part_shape_cache())
      return Result<std::size_t>::failure(Error::export_error("gui.step_export", "Part multi-body export requires an active recomputed Part"));
    return Result<std::size_t>::success(session.part_shape_cache()->body_shape_count());
  case GuiStepExportMode::AssemblyFlattened:
  case GuiStepExportMode::AssemblyStructured:
    if (!session.project())
      return Result<std::size_t>::failure(Error::export_error("gui.step_export", "Assembly export requires an active Project"));
    if (auto valid = session.project()->validate_assembly_structure(); valid.has_error())
      return Result<std::size_t>::failure(valid.error());
    return Result<std::size_t>::success(session.project()->assembly().component_instance_count());
  }
  return Result<std::size_t>::failure(Error::internal("gui.step_export", "unsupported export mode"));
}

Result<GuiStepExportResult> GuiAnalysisExportWorkbench::export_step(
    const GuiDocumentSession& session, const GuiStepExportRequest& request,
    GuiExportCancellation cancelled, GuiExportProgress progress) const {
  auto preflight = preflight_step_export(session, request);
  if (preflight.has_error()) return Result<GuiStepExportResult>::failure(preflight.error());
  if (cancelled && cancelled())
    return Result<GuiStepExportResult>::failure(Error::export_error("gui.step_export", "export cancelled before commit"));
  if (progress) progress(0, 1, "writing STEP");

  GuiStepExportResult result{request.mode, preflight.value(), 0, {}};
  if (request.mode == GuiStepExportMode::PartMultiBody) {
    auto exported = geometry::StepExporter{}.write_part_step(*session.part_document(),
        *session.part_shape_cache(), request.output.string());
    if (exported.has_error()) return Result<GuiStepExportResult>::failure(exported.error());
    result.exported_item_count = exported.value().exported_body_count;
    result.written_bytes = exported.value().written_bytes;
    result.diagnostic = "visible Part Bodies exported with deterministic BodyId names";
  } else if (request.mode == GuiStepExportMode::AssemblyFlattened) {
    auto exported = geometry::AssemblyStepExporter{}.write_step(*session.project(), request.output.string());
    if (exported.has_error()) return Result<GuiStepExportResult>::failure(exported.error());
    result.exported_item_count = exported.value().exported_component_count;
    result.written_bytes = exported.value().written_bytes;
    result.diagnostic = "flattened posed Assembly exported";
  } else {
    auto exported = geometry::AssemblyStructuredStepExporter{}.write_step(*session.project(), request.output.string());
    if (exported.has_error()) return Result<GuiStepExportResult>::failure(exported.error());
    result.exported_item_count = exported.value().exported_component_occurrence_count;
    result.written_bytes = exported.value().written_bytes;
    result.diagnostic = "structured Assembly product graph exported";
  }
  if (progress) progress(1, 1, "STEP complete");
  return Result<GuiStepExportResult>::success(std::move(result));
}

} // namespace blcad::gui
