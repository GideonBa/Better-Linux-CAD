#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
#include "blcad/gui/gui_step_import_context.hpp"

#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/step_exporter.hpp"

#include <algorithm>

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
  return query_with_step_import_context(session, [&](auto* context) {
    return geometry::AssemblySolveDiagnosticsAnalyzer{context}.analyze(*project.value(), group);
  });
}
Result<geometry::AssemblyInterferenceAnalysis> GuiAnalysisExportWorkbench::analyze_interference(
    const GuiDocumentSession& session, geometry::AssemblyInterferenceAnalysisOptions options) const {
  auto project = require_project(session, "interference analysis");
  if (project.has_error()) return Result<geometry::AssemblyInterferenceAnalysis>::failure(project.error());
  return query_with_step_import_context(session, [&](auto* context) {
    return geometry::AssemblyInterferenceAnalyzer{context}.analyze(*project.value(), options);
  });
}
Result<geometry::AssemblyClearanceAnalysis> GuiAnalysisExportWorkbench::analyze_clearance(
    const GuiDocumentSession& session, geometry::AssemblyClearanceAnalysisOptions options) const {
  auto project = require_project(session, "clearance analysis");
  if (project.has_error()) return Result<geometry::AssemblyClearanceAnalysis>::failure(project.error());
  return query_with_step_import_context(session, [&](auto* context) {
    return geometry::AssemblyClearanceAnalyzer{context}.analyze(*project.value(), options);
  });
}
Result<geometry::AssemblyContactAnalysis> GuiAnalysisExportWorkbench::analyze_contact(
    const GuiDocumentSession& session, geometry::AssemblyContactAnalysisOptions options) const {
  auto project = require_project(session, "contact analysis");
  if (project.has_error()) return Result<geometry::AssemblyContactAnalysis>::failure(project.error());
  return query_with_step_import_context(session, [&](auto* context) {
    return geometry::AssemblyContactAnalyzer{context}.analyze(*project.value(), options);
  });
}
Result<geometry::AssemblyRevoluteSweepAnalysis> GuiAnalysisExportWorkbench::analyze_revolute_sweep(
    const GuiDocumentSession& session, const geometry::AssemblyRevoluteSweepRequest& request,
    geometry::AssemblyRevoluteSweepAnalysisOptions options) const {
  auto project = require_project(session, "motion sweep");
  if (project.has_error()) return Result<geometry::AssemblyRevoluteSweepAnalysis>::failure(project.error());
  return query_with_step_import_context(session, [&](auto* context) {
    return geometry::AssemblyRevoluteSweepAnalyzer{context}.analyze(*project.value(), request,
                                                                    options);
  });
}

Result<std::size_t> GuiAnalysisExportWorkbench::preflight_step_export(
    const GuiDocumentSession& session, const GuiStepExportRequest& request) const {
  if (session.part_workflow_active())
    return Result<std::size_t>::failure(Error::export_error(
        "gui.step_export", "Apply or cancel the Part workflow before STEP export."));
  if (request.output.empty())
    return Result<std::size_t>::failure(Error::export_error("gui.step_export", "output path must not be empty"));
  const auto protected_source = validate_step_import_output_path(session, request.output);
  if (!protected_source)
    return protected_source;
  if (!session.derived_results_fresh())
    return Result<std::size_t>::failure(Error::export_error("gui.step_export", "recompute must be fresh before export"));
  return query_with_step_import_context(session, [&](auto* context) -> Result<std::size_t> {
    switch (request.mode) {
    case GuiStepExportMode::PartMultiBody: {
      if (!session.part_document() || !session.part_shape_cache())
        return Result<std::size_t>::failure(Error::export_error(
            "gui.step_export", "Part multi-body export requires an active recomputed Part"));
      if (auto valid = geometry::observe_imported_part_sources(*session.part_document(), context);
          !valid)
        return valid;
      const auto inspection = geometry::BodyResultInspector{}.inspect(*session.part_document(),
                                                                      *session.part_shape_cache());
      if (!inspection)
        return Result<std::size_t>::failure(inspection.error());
      const auto visible = std::count_if(
          inspection.value().bodies.begin(), inspection.value().bodies.end(), [](const auto& body) {
            return body.visibility == BodyVisibility::Visible && body.shape_summary.has_value();
          });
      if (visible == 0)
        return Result<std::size_t>::failure(Error::export_error(
            "gui.step_export", "STEP export requires an available visible Body result"));
      return Result<std::size_t>::success(static_cast<std::size_t>(visible));
    }
    case GuiStepExportMode::AssemblyFlattened:
    case GuiStepExportMode::AssemblyStructured:
      if (!session.project())
        return Result<std::size_t>::failure(
            Error::export_error("gui.step_export", "Assembly export requires an active Project"));
      if (auto valid = session.project()->validate_assembly_structure(); valid.has_error())
        return Result<std::size_t>::failure(valid.error());
      if (auto valid = validate_visible_step_imports(*session.project(), context); !valid)
        return valid;
      return Result<std::size_t>::success(session.project()->assembly().component_instance_count());
    }
    return Result<std::size_t>::failure(
        Error::internal("gui.step_export", "unsupported export mode"));
  });
}

Result<GuiStepExportResult> GuiAnalysisExportWorkbench::export_step(
    const GuiDocumentSession& session, const GuiStepExportRequest& request,
    GuiExportCancellation cancelled, GuiExportProgress progress) const {
  auto preflight = preflight_step_export(session, request);
  if (preflight.has_error()) return Result<GuiStepExportResult>::failure(preflight.error());
  if (cancelled && cancelled())
    return Result<GuiStepExportResult>::failure(Error::export_error("gui.step_export", "export cancelled before commit"));
  auto context = make_step_import_context(session);
  if (!context)
    return Result<GuiStepExportResult>::failure(context.error());
  if (progress) progress(0, 1, "writing STEP");

  GuiStepExportResult result{request.mode, preflight.value(), 0, {}};
  if (request.mode == GuiStepExportMode::PartMultiBody) {
    auto exported = geometry::StepExporter{context.value().get()}.write_part_step(
        *session.part_document(), *session.part_shape_cache(), request.output.string());
    if (exported.has_error()) return Result<GuiStepExportResult>::failure(exported.error());
    result.exported_item_count = exported.value().exported_body_count;
    result.written_bytes = exported.value().written_bytes;
    result.diagnostic = "visible Part Bodies exported with deterministic BodyId names";
  } else if (request.mode == GuiStepExportMode::AssemblyFlattened) {
    auto exported = geometry::AssemblyStepExporter{context.value().get()}.write_step(
        *session.project(), request.output.string());
    if (exported.has_error()) return Result<GuiStepExportResult>::failure(exported.error());
    result.exported_item_count = exported.value().exported_component_count;
    result.written_bytes = exported.value().written_bytes;
    result.diagnostic = "flattened posed Assembly exported";
  } else {
    auto exported = geometry::AssemblyStructuredStepExporter{context.value().get()}.write_step(
        *session.project(), request.output.string());
    if (exported.has_error()) return Result<GuiStepExportResult>::failure(exported.error());
    result.exported_item_count = exported.value().exported_component_occurrence_count;
    result.written_bytes = exported.value().written_bytes;
    result.diagnostic = "structured Assembly product graph exported";
  }
  if (progress) progress(1, 1, "STEP complete");
  return Result<GuiStepExportResult>::success(std::move(result));
}

} // namespace blcad::gui
