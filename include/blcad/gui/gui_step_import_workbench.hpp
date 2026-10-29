#pragma once

#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/gui/gui_document_session.hpp"

namespace blcad::gui {

enum class GuiStepImportOperation { Register, Refresh, Remove };

// Disposable evidence from one explicit inspection. No source bytes or shapes
// are stored here or persisted in the document.
struct GuiStepImportPreview {
  GuiStepImportOperation operation;
  StepImportSource source;
  StepImportFreshnessReport report;
  std::filesystem::path document_path;
  std::string document_json;
};

struct GuiStepImportSourceStatus {
  DocumentId owner_document;
  StepImportFreshnessReport report;
  bool embedded_part{false};
};

// Read-only geometry evidence, separate from an applicable registry operation.
// Products, bodies and posed preview shapes remain disposable Geometry data.
struct GuiStepImportGeometryPreview {
  StepImportSource source;
  StepImportFreshnessReport report;
  std::filesystem::path document_path;
  std::string document_json;
  bool registered_source{false};
  geometry::StepImportGeometry geometry;
  std::vector<geometry::GeometryShape> preview_shapes;
};

class GuiStepImportWorkbench {
public:
  [[nodiscard]] Result<GuiStepImportPreview>
  prepare_register(const GuiDocumentSession&, std::string name,
                   std::string project_relative_asset_path, StepImportMode) const;
  [[nodiscard]] Result<GuiStepImportPreview> prepare_refresh(const GuiDocumentSession&,
                                                             StepImportSourceId) const;
  [[nodiscard]] Result<GuiStepImportPreview> prepare_remove(const GuiDocumentSession&,
                                                            StepImportSourceId) const;
  [[nodiscard]] Result<StepImportFreshnessReport> inspect(const GuiDocumentSession&,
                                                          StepImportSourceId) const;
  [[nodiscard]] Result<std::vector<GuiStepImportSourceStatus>>
  inspect_sources(const GuiDocumentSession&) const;
  [[nodiscard]] Result<GuiStepImportGeometryPreview> inspect_geometry(const GuiDocumentSession&,
                                                                      StepImportSourceId) const;
  [[nodiscard]] Result<GuiStepImportGeometryPreview>
  inspect_geometry(const GuiDocumentSession&, std::string name,
                   std::string project_relative_asset_path, StepImportMode) const;
  [[nodiscard]] Result<std::size_t> validate_inspection(const GuiDocumentSession&,
                                                        const GuiStepImportGeometryPreview&) const;
  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession&, const GuiStepImportPreview&) const;
};

// Source protection shared by document saving and export. Existing hardlinks
// and canonical/lexical aliases count as the same asset, including missing assets.
[[nodiscard]] Result<std::size_t>
validate_step_import_output_path(const GuiDocumentSession&, const std::filesystem::path& output);

// Save As never copies or rewrites STEP assets. Moving the document to another
// directory requires matching assets at every new document-relative location.
[[nodiscard]] Result<std::size_t>
validate_step_import_save_as(const GuiDocumentSession&, const std::filesystem::path& destination);

[[nodiscard]] Result<std::size_t>
validate_step_import_definition_sources(const GuiDocumentSession&, const PartDocument& definition);

} // namespace blcad::gui
