#include "blcad/geometry/assembly_structured_step_exporter.hpp"

#include "blcad/core/assembly_exchange_graph.hpp"

#include "assembly_occt_rigid_transform.hpp"
#include "assembly_part_shape_definitions.hpp"

#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr const char* kStructuredStepExporterId =
    "geometry.assembly_structured_step_exporter";

struct AssemblyOccurrenceLabel {
  AssemblyExchangeAssemblyOccurrenceIdentity identity;
  TDF_Label label;
};

struct PartDefinitionLabel {
  DocumentId part_document;
  TDF_Label label;
};

[[nodiscard]] Error make_export_error(std::string message) {
  return Error::export_error(kStructuredStepExporterId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }
  return message;
}

void set_label_name(const TDF_Label& label, const std::string& name) {
  TDataStd_Name::Set(label, TCollection_ExtendedString(name.c_str(), true));
}

[[nodiscard]] const TDF_Label* find_assembly_label(
    const std::vector<AssemblyOccurrenceLabel>& labels,
    const AssemblyExchangeAssemblyOccurrenceIdentity& identity) {
  const auto found = std::find_if(
      labels.begin(), labels.end(),
      [&identity](const AssemblyOccurrenceLabel& entry) {
        return entry.identity == identity;
      });
  return found == labels.end() ? nullptr : &found->label;
}

[[nodiscard]] const TDF_Label* find_part_label(
    const std::vector<PartDefinitionLabel>& labels, const DocumentId& part_document) {
  const auto found = std::find_if(
      labels.begin(), labels.end(),
      [&part_document](const PartDefinitionLabel& entry) {
        return entry.part_document == part_document;
      });
  return found == labels.end() ? nullptr : &found->label;
}

} // namespace

Result<AssemblyStructuredStepExportSummary> AssemblyStructuredStepExporter::write_step(
    const Project& project, const std::string& path) const {
  if (path.empty()) {
    return Result<AssemblyStructuredStepExportSummary>::failure(
        make_export_error("structured step export path must not be empty"));
  }

  auto exchange_graph = AssemblyExchangeGraph::build(project);
  if (exchange_graph.has_error()) {
    return Result<AssemblyStructuredStepExportSummary>::failure(exchange_graph.error());
  }
  if (exchange_graph.value().component_occurrences().empty()) {
    return Result<AssemblyStructuredStepExportSummary>::failure(make_export_error(
        "structured step export requires at least one visible active component"));
  }

  std::vector<DocumentId> part_documents;
  part_documents.reserve(exchange_graph.value().part_product_definitions().size());
  for (const AssemblyExchangePartProductDefinition& definition :
       exchange_graph.value().part_product_definitions()) {
    part_documents.push_back(definition.identity.part_document);
  }

  const detail::AssemblyPartShapeDefinitionBuilder part_definition_builder;
  auto part_shapes = part_definition_builder.build(project, std::move(part_documents));
  if (part_shapes.has_error()) {
    return Result<AssemblyStructuredStepExportSummary>::failure(part_shapes.error());
  }

  try {
    Handle(TDocStd_Document) document = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(document->Main());
    Handle(XCAFDoc_ShapeTool) shape_tool =
        XCAFDoc_DocumentTool::ShapeTool(document->Main());
    if (shape_tool.IsNull()) {
      return Result<AssemblyStructuredStepExportSummary>::failure(
          make_export_error("could not create XDE shape tool"));
    }

    std::vector<PartDefinitionLabel> part_labels;
    part_labels.reserve(exchange_graph.value().part_product_definitions().size());
    for (const AssemblyExchangePartProductDefinition& definition :
         exchange_graph.value().part_product_definitions()) {
      const detail::AssemblyPartShapeDefinition* part_shape =
          detail::find_assembly_part_shape_definition(
              part_shapes.value().definitions, definition.identity.part_document);
      if (part_shape == nullptr || part_shape->shape.IsNull()) {
        return Result<AssemblyStructuredStepExportSummary>::failure(make_export_error(
            "structured step part product definition shape is unavailable"));
      }

      TDF_Label label = shape_tool->AddShape(part_shape->shape, false, false);
      if (label.IsNull()) {
        return Result<AssemblyStructuredStepExportSummary>::failure(
            make_export_error("could not create XDE part product definition"));
      }
      set_label_name(label, definition.product_name);
      part_labels.push_back(
          PartDefinitionLabel{definition.identity.part_document, label});
    }

    std::vector<AssemblyOccurrenceLabel> assembly_labels;
    assembly_labels.reserve(exchange_graph.value().assembly_occurrences().size());
    for (const AssemblyExchangeAssemblyOccurrence& occurrence :
         exchange_graph.value().assembly_occurrences()) {
      TDF_Label label = shape_tool->NewShape();
      if (label.IsNull()) {
        return Result<AssemblyStructuredStepExportSummary>::failure(
            make_export_error("could not create XDE assembly occurrence product"));
      }
      set_label_name(label, occurrence.product_name);
      assembly_labels.push_back(AssemblyOccurrenceLabel{occurrence.identity, label});
    }

    for (const AssemblyExchangeComponentOccurrence& occurrence :
         exchange_graph.value().component_occurrences()) {
      const TDF_Label* assembly_label =
          find_assembly_label(assembly_labels,
                              occurrence.identity.containing_assembly_occurrence);
      const TDF_Label* part_label =
          find_part_label(part_labels, occurrence.part_product.part_document);
      if (assembly_label == nullptr || part_label == nullptr ||
          occurrence.transforms_inner_to_outer.empty()) {
        return Result<AssemblyStructuredStepExportSummary>::failure(make_export_error(
            "structured step component occurrence exchange mapping is incomplete"));
      }

      TDF_Label component_label = shape_tool->AddComponent(
          *assembly_label, *part_label,
          detail::to_occt_location(occurrence.transforms_inner_to_outer.front()));
      if (component_label.IsNull()) {
        return Result<AssemblyStructuredStepExportSummary>::failure(
            make_export_error("could not add XDE part component occurrence"));
      }
      set_label_name(component_label, occurrence.occurrence_name);
    }

    for (const AssemblyExchangeAssemblyOccurrence& occurrence :
         exchange_graph.value().assembly_occurrences()) {
      if (!occurrence.parent_identity.has_value()) {
        continue;
      }
      const TDF_Label* parent_label =
          find_assembly_label(assembly_labels, *occurrence.parent_identity);
      const TDF_Label* child_label =
          find_assembly_label(assembly_labels, occurrence.identity);
      if (parent_label == nullptr || child_label == nullptr) {
        return Result<AssemblyStructuredStepExportSummary>::failure(make_export_error(
            "structured step assembly occurrence exchange mapping is incomplete"));
      }

      TDF_Label component_label = shape_tool->AddComponent(
          *parent_label, *child_label,
          detail::to_occt_location(occurrence.transform_from_parent));
      if (component_label.IsNull()) {
        return Result<AssemblyStructuredStepExportSummary>::failure(
            make_export_error("could not add nested XDE assembly occurrence"));
      }
      set_label_name(component_label, occurrence.product_name);
    }

    shape_tool->UpdateAssemblies();

    const AssemblyExchangeAssemblyOccurrenceIdentity root_identity{};
    const TDF_Label* root_label = find_assembly_label(assembly_labels, root_identity);
    if (root_label == nullptr) {
      return Result<AssemblyStructuredStepExportSummary>::failure(
          make_export_error("structured step exchange graph has no explicit root assembly"));
    }

    STEPCAFControl_Writer writer;
    writer.SetNameMode(true);
    Interface_Static::SetCVal("write.step.schema", "AP214");
    if (!writer.Transfer(*root_label, STEPControl_AsIs)) {
      return Result<AssemblyStructuredStepExportSummary>::failure(
          make_export_error("could not transfer structured XDE assembly to step model"));
    }

    const IFSelect_ReturnStatus write_status = writer.Write(path.c_str());
    if (write_status != IFSelect_RetDone) {
      return Result<AssemblyStructuredStepExportSummary>::failure(
          make_export_error("could not write structured step file"));
    }

    std::error_code error_code;
    const std::uintmax_t size = std::filesystem::file_size(path, error_code);
    if (error_code) {
      return Result<AssemblyStructuredStepExportSummary>::failure(
          make_export_error("could not read back written structured step file"));
    }
    if (size == 0U) {
      return Result<AssemblyStructuredStepExportSummary>::failure(
          make_export_error("structured step export produced an empty file"));
    }

    return Result<AssemblyStructuredStepExportSummary>::success(
        AssemblyStructuredStepExportSummary{
            part_shapes.value().recomputed_part_count,
            exchange_graph.value().assembly_occurrence_count(),
            exchange_graph.value().component_occurrence_count(),
            exchange_graph.value().part_product_definition_count(),
            static_cast<std::size_t>(size)});
  } catch (const Standard_Failure& failure) {
    return Result<AssemblyStructuredStepExportSummary>::failure(
        make_export_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<AssemblyStructuredStepExportSummary>::failure(
        make_export_error(exception.what()));
  } catch (...) {
    return Result<AssemblyStructuredStepExportSummary>::failure(
        make_export_error("unknown structured step export error"));
  }
}

} // namespace blcad::geometry
