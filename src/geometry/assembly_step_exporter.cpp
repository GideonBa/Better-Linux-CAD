#include "blcad/geometry/assembly_step_exporter.hpp"

#include "blcad/geometry/step_exporter.hpp"

#include "assembly_posed_leaf_shapes.hpp"
#include "geometry_shape_internal.hpp"

#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {

constexpr const char* kAssemblyStepExporterId = "geometry.assembly_step_exporter";

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kAssemblyStepExporterId, std::move(message));
}

[[nodiscard]] Error make_export_error(std::string message) {
  return Error::export_error(kAssemblyStepExporterId, std::move(message));
}

[[nodiscard]] std::string standard_failure_message(const Standard_Failure& failure) {
  const char* const message = failure.GetMessageString();
  if (message == nullptr || *message == '\0') {
    return "OCCT operation failed";
  }
  return message;
}

} // namespace

Result<AssemblyStepExporter::PosedAssemblyBuild>
AssemblyStepExporter::build(const Project& project) const {
  auto posed_leaves = detail::build_posed_leaf_shapes(project);
  if (posed_leaves.has_error()) {
    return Result<PosedAssemblyBuild>::failure(posed_leaves.error());
  }
  if (posed_leaves.value().leaves.empty()) {
    return Result<PosedAssemblyBuild>::failure(
        make_export_error("posed assembly export requires at least one visible active component"));
  }

  try {
    BRep_Builder compound_builder;
    TopoDS_Compound compound;
    compound_builder.MakeCompound(compound);

    for (const detail::PosedLeafShape& leaf : posed_leaves.value().leaves) {
      compound_builder.Add(compound, leaf.shape);
    }

    if (compound.IsNull()) {
      return Result<PosedAssemblyBuild>::failure(
          make_geometry_error("posed assembly compound is empty"));
    }

    GeometryShape shape(std::make_shared<GeometryShape::Impl>(TopoDS_Shape(compound)));
    return Result<PosedAssemblyBuild>::success(
        PosedAssemblyBuild{std::move(shape), posed_leaves.value().recomputed_part_count,
                           posed_leaves.value().leaves.size()});
  } catch (const Standard_Failure& failure) {
    return Result<PosedAssemblyBuild>::failure(
        make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<PosedAssemblyBuild>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<PosedAssemblyBuild>::failure(
        make_geometry_error("unknown assembly export error"));
  }
}

Result<GeometryShape> AssemblyStepExporter::build_posed_shape(const Project& project) const {
  auto built = build(project);
  if (built.has_error()) {
    return Result<GeometryShape>::failure(built.error());
  }
  return Result<GeometryShape>::success(std::move(built.value().shape));
}

Result<AssemblyStepExportSummary> AssemblyStepExporter::write_step(const Project& project,
                                                                   const std::string& path) const {
  auto built = build(project);
  if (built.has_error()) {
    return Result<AssemblyStepExportSummary>::failure(built.error());
  }

  const StepExporter step_exporter;
  const auto written = step_exporter.write_step(built.value().shape, path);
  if (written.has_error()) {
    return Result<AssemblyStepExportSummary>::failure(written.error());
  }

  return Result<AssemblyStepExportSummary>::success(
      AssemblyStepExportSummary{built.value().recomputed_part_count,
                                built.value().exported_component_count, written.value()});
}

} // namespace blcad::geometry
