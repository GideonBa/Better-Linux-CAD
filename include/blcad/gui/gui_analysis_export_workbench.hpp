#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"
#include "blcad/geometry/assembly_interference_analyzer.hpp"
#include "blcad/geometry/assembly_revolute_sweep_analyzer.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace blcad::gui {

enum class GuiStepExportMode { PartMultiBody, AssemblyFlattened, AssemblyStructured };

struct GuiStepExportRequest {
  GuiStepExportMode mode{GuiStepExportMode::PartMultiBody};
  std::filesystem::path output;
};

struct GuiStepExportResult {
  GuiStepExportMode mode{GuiStepExportMode::PartMultiBody};
  std::size_t exported_item_count{0};
  std::size_t written_bytes{0};
  std::string diagnostic;
};

using GuiExportCancellation = std::function<bool()>;
using GuiExportProgress = std::function<void(std::size_t completed, std::size_t total,
                                             std::string_view stage)>;

class GuiAnalysisExportWorkbench {
public:
  [[nodiscard]] Result<geometry::AssemblySolveDiagnostics>
  inspect_dof(const GuiDocumentSession&, std::vector<ComponentInstanceId>) const;
  [[nodiscard]] Result<geometry::AssemblyInterferenceAnalysis>
  analyze_interference(const GuiDocumentSession&, geometry::AssemblyInterferenceAnalysisOptions = {}) const;
  [[nodiscard]] Result<geometry::AssemblyClearanceAnalysis>
  analyze_clearance(const GuiDocumentSession&, geometry::AssemblyClearanceAnalysisOptions = {}) const;
  [[nodiscard]] Result<geometry::AssemblyContactAnalysis>
  analyze_contact(const GuiDocumentSession&, geometry::AssemblyContactAnalysisOptions = {}) const;
  [[nodiscard]] Result<geometry::AssemblyRevoluteSweepAnalysis>
  analyze_revolute_sweep(const GuiDocumentSession&, const geometry::AssemblyRevoluteSweepRequest&,
                         geometry::AssemblyRevoluteSweepAnalysisOptions = {}) const;

  [[nodiscard]] Result<std::size_t> preflight_step_export(const GuiDocumentSession&,
                                                          const GuiStepExportRequest&) const;
  [[nodiscard]] Result<GuiStepExportResult>
  export_step(const GuiDocumentSession&, const GuiStepExportRequest&,
              GuiExportCancellation cancelled = {}, GuiExportProgress progress = {}) const;
};

} // namespace blcad::gui
