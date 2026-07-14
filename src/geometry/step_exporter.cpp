#include "blcad/geometry/step_exporter.hpp"

#include "geometry_shape_internal.hpp"

#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr const char* kStepExporterId = "geometry.step_exporter";

[[nodiscard]] Error make_export_error(std::string message) {
  return Error::export_error(kStepExporterId, std::move(message));
}

[[nodiscard]] bool is_exchange_name_unreserved(unsigned char value) noexcept {
  return (value >= static_cast<unsigned char>('A') &&
          value <= static_cast<unsigned char>('Z')) ||
         (value >= static_cast<unsigned char>('a') &&
          value <= static_cast<unsigned char>('z')) ||
         (value >= static_cast<unsigned char>('0') &&
          value <= static_cast<unsigned char>('9')) ||
         value == static_cast<unsigned char>('.') ||
         value == static_cast<unsigned char>('_') ||
         value == static_cast<unsigned char>('-');
}

[[nodiscard]] std::string body_exchange_name(const BodyId& body_id) {
  constexpr char kHexDigits[] = "0123456789ABCDEF";
  std::string result = "blcad:body-definition:";
  result.reserve(result.size() + body_id.value().size());
  for (const unsigned char byte : body_id.value()) {
    if (is_exchange_name_unreserved(byte)) {
      result.push_back(static_cast<char>(byte));
      continue;
    }
    result.push_back('%');
    result.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
    result.push_back(kHexDigits[byte & 0x0FU]);
  }
  return result;
}

[[nodiscard]] bool contains_topology(const TopoDS_Shape& shape,
                                     TopAbs_ShapeEnum topology) {
  for (TopExp_Explorer explorer(shape, topology); explorer.More(); explorer.Next()) {
    return true;
  }
  return false;
}

[[nodiscard]] Result<bool> validate_body_shape(const Body& body,
                                               const TopoDS_Shape& shape) {
  const bool has_solid = contains_topology(shape, TopAbs_SOLID);
  const bool has_face = contains_topology(shape, TopAbs_FACE);
  if (body.kind() == BodyKind::Solid && !has_solid) {
    return Result<bool>::failure(make_export_error(
        "visible solid body '" + body.id().value() + "' has no solid topology"));
  }
  if (body.kind() == BodyKind::Surface && (has_solid || !has_face)) {
    return Result<bool>::failure(make_export_error(
        "visible surface body '" + body.id().value() +
        "' must contain face topology without solid topology"));
  }
  return Result<bool>::success(true);
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }

  return message;
}

} // namespace

Result<std::size_t> StepExporter::write_step(const GeometryShape& shape,
                                             const std::string& path) const {
  if (path.empty()) {
    return Result<std::size_t>::failure(make_export_error("step export path must not be empty"));
  }

  if (shape.empty()) {
    return Result<std::size_t>::failure(
        make_export_error("step export requires a non-empty shape"));
  }

  try {
    STEPControl_Writer writer;
    Interface_Static::SetCVal("write.step.schema", "AP214");

    const IFSelect_ReturnStatus transfer = writer.Transfer(shape.impl_->shape, STEPControl_AsIs);
    if (transfer != IFSelect_RetDone) {
      return Result<std::size_t>::failure(
          make_export_error("could not transfer shape to step model"));
    }

    const IFSelect_ReturnStatus write = writer.Write(path.c_str());
    if (write != IFSelect_RetDone) {
      return Result<std::size_t>::failure(make_export_error("could not write step file"));
    }

    std::error_code error_code;
    const std::uintmax_t size = std::filesystem::file_size(path, error_code);
    if (error_code) {
      return Result<std::size_t>::failure(
          make_export_error("could not read back written step file"));
    }

    if (size == 0U) {
      return Result<std::size_t>::failure(make_export_error("step export produced an empty file"));
    }

    return Result<std::size_t>::success(static_cast<std::size_t>(size));
  } catch (const Standard_Failure& failure) {
    return Result<std::size_t>::failure(make_export_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<std::size_t>::failure(make_export_error(exception.what()));
  } catch (...) {
    return Result<std::size_t>::failure(make_export_error("unknown export error"));
  }
}

Result<PartStepExportSummary>
StepExporter::write_part_step(const PartDocument& document,
                              const ShapeCache& shape_cache,
                              const std::string& path) const {
  if (path.empty()) {
    return Result<PartStepExportSummary>::failure(
        make_export_error("part step export path must not be empty"));
  }

  std::vector<const Body*> visible_bodies;
  for (const Body& body : document.bodies()) {
    if (body.visibility() == BodyVisibility::Visible) {
      visible_bodies.push_back(&body);
    }
  }
  std::sort(visible_bodies.begin(), visible_bodies.end(),
            [](const Body* left, const Body* right) {
              return left->id().value() < right->id().value();
            });
  if (visible_bodies.empty()) {
    return Result<PartStepExportSummary>::failure(
        make_export_error("part step export requires at least one visible body"));
  }

  PartStepExportSummary summary;
  summary.bodies.reserve(visible_bodies.size());

  try {
    Handle(TDocStd_Document) xde_document = new TDocStd_Document("BinXCAF");
    XCAFDoc_DocumentTool::Set(xde_document->Main());
    Handle(XCAFDoc_ShapeTool) shape_tool =
        XCAFDoc_DocumentTool::ShapeTool(xde_document->Main());
    if (shape_tool.IsNull()) {
      return Result<PartStepExportSummary>::failure(
          make_export_error("could not create XDE shape tool"));
    }

    for (const Body* body : visible_bodies) {
      const GeometryShape* shape = shape_cache.find_body_shape(body->id());
      if (shape == nullptr) {
        return Result<PartStepExportSummary>::failure(make_export_error(
            "visible body '" + body->id().value() + "' has no cached result"));
      }
      if (shape->empty()) {
        return Result<PartStepExportSummary>::failure(make_export_error(
            "visible body '" + body->id().value() + "' has an empty shape"));
      }
      const auto valid = validate_body_shape(*body, shape->impl_->shape);
      if (valid.has_error()) {
        return Result<PartStepExportSummary>::failure(valid.error());
      }

      TDF_Label label = shape_tool->AddShape(shape->impl_->shape, false, false);
      if (label.IsNull()) {
        return Result<PartStepExportSummary>::failure(make_export_error(
            "could not create XDE definition for body '" + body->id().value() + "'"));
      }
      const std::string exchange_name = body_exchange_name(body->id());
      TDataStd_Name::Set(label, TCollection_ExtendedString(exchange_name.c_str(), true));
      summary.bodies.push_back(
          PartStepExportBodySummary{body->id(), exchange_name, body->kind()});
      if (body->kind() == BodyKind::Solid) {
        ++summary.exported_solid_body_count;
      } else {
        ++summary.exported_surface_body_count;
      }
    }

    STEPCAFControl_Writer writer;
    writer.SetNameMode(true);
    Interface_Static::SetCVal("write.step.schema", "AP214");
    if (!writer.Transfer(xde_document, STEPControl_AsIs)) {
      return Result<PartStepExportSummary>::failure(
          make_export_error("could not transfer multi-body XDE document to step model"));
    }
    if (writer.Write(path.c_str()) != IFSelect_RetDone) {
      return Result<PartStepExportSummary>::failure(
          make_export_error("could not write part step file"));
    }

    std::error_code error_code;
    const std::uintmax_t size = std::filesystem::file_size(path, error_code);
    if (error_code || size == 0U) {
      return Result<PartStepExportSummary>::failure(make_export_error(
          error_code ? "could not read back written part step file"
                     : "part step export produced an empty file"));
    }
    summary.exported_body_count = summary.bodies.size();
    summary.written_bytes = static_cast<std::size_t>(size);
    return Result<PartStepExportSummary>::success(std::move(summary));
  } catch (const Standard_Failure& failure) {
    return Result<PartStepExportSummary>::failure(
        make_export_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<PartStepExportSummary>::failure(make_export_error(exception.what()));
  } catch (...) {
    return Result<PartStepExportSummary>::failure(
        make_export_error("unknown part step export error"));
  }
}

} // namespace blcad::geometry
