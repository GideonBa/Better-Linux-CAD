#include "blcad/geometry/step_exporter.hpp"

#include "geometry_shape_internal.hpp"

#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>

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

} // namespace blcad::geometry
