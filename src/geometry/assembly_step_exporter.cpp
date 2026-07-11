#include "blcad/geometry/assembly_step_exporter.hpp"

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_exporter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr const char* kAssemblyStepExporterId = "geometry.assembly_step_exporter";

struct RecomputedPartShape {
  DocumentId part_document;
  ShapeCache shape_cache;
};

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

[[nodiscard]] constexpr double degrees_to_radians(double degrees) noexcept {
  return degrees * (std::numbers::pi_v<double> / 180.0);
}

[[nodiscard]] Result<TopoDS_Shape> apply_occt_transform(const TopoDS_Shape& shape,
                                                        const gp_Trsf& transform) {
  BRepBuilderAPI_Transform transformer(shape, transform, true);
  if (!transformer.IsDone()) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("could not apply component rigid transform"));
  }

  TopoDS_Shape transformed = transformer.Shape();
  if (transformed.IsNull()) {
    return Result<TopoDS_Shape>::failure(
        make_geometry_error("component rigid transform produced an empty shape"));
  }
  return Result<TopoDS_Shape>::success(std::move(transformed));
}

[[nodiscard]] Result<TopoDS_Shape> pose_shape(const TopoDS_Shape& source,
                                              const RigidTransform& transform) {
  TopoDS_Shape posed = source;

  const gp_Ax1 x_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
  const gp_Ax1 y_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
  const gp_Ax1 z_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));

  gp_Trsf rotation_x;
  rotation_x.SetRotation(x_axis, degrees_to_radians(transform.rotation_deg.x));
  auto rotated_x = apply_occt_transform(posed, rotation_x);
  if (rotated_x.has_error()) return rotated_x;
  posed = std::move(rotated_x.value());

  gp_Trsf rotation_y;
  rotation_y.SetRotation(y_axis, degrees_to_radians(transform.rotation_deg.y));
  auto rotated_y = apply_occt_transform(posed, rotation_y);
  if (rotated_y.has_error()) return rotated_y;
  posed = std::move(rotated_y.value());

  gp_Trsf rotation_z;
  rotation_z.SetRotation(z_axis, degrees_to_radians(transform.rotation_deg.z));
  auto rotated_z = apply_occt_transform(posed, rotation_z);
  if (rotated_z.has_error()) return rotated_z;
  posed = std::move(rotated_z.value());

  gp_Trsf translation;
  translation.SetTranslation(gp_Vec(transform.translation_mm.x, transform.translation_mm.y,
                                    transform.translation_mm.z));
  return apply_occt_transform(posed, translation);
}

[[nodiscard]] Result<TopoDS_Shape>
pose_shape_chain(const TopoDS_Shape& source,
                 const std::vector<RigidTransform>& transforms_inner_to_outer) {
  TopoDS_Shape posed = source;
  for (const RigidTransform& transform : transforms_inner_to_outer) {
    auto transformed = pose_shape(posed, transform);
    if (transformed.has_error()) {
      return transformed;
    }
    posed = std::move(transformed.value());
  }
  return Result<TopoDS_Shape>::success(std::move(posed));
}

[[nodiscard]] bool contains_part(const std::vector<DocumentId>& part_ids,
                                 const DocumentId& part_id) {
  return std::find(part_ids.begin(), part_ids.end(), part_id) != part_ids.end();
}

[[nodiscard]] const RecomputedPartShape*
find_recomputed_part(const std::vector<RecomputedPartShape>& parts,
                     const DocumentId& part_document) {
  const auto found = std::find_if(parts.begin(), parts.end(), [&part_document](const auto& part) {
    return part.part_document == part_document;
  });
  return found == parts.end() ? nullptr : &*found;
}

[[nodiscard]] std::string
leaf_occurrence_key(const AssemblyLeafOccurrenceDescriptor& leaf) {
  std::string key = leaf.assembly_document.value();
  for (const SubassemblyInstanceId& occurrence : leaf.subassembly_path) {
    key += "/" + occurrence.value();
  }
  key += "/" + leaf.component_instance.value();
  return key;
}

} // namespace

Result<AssemblyStepExporter::PosedAssemblyBuild>
AssemblyStepExporter::build(const Project& project) const {
  const auto valid_structure = project.validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<PosedAssemblyBuild>::failure(valid_structure.error());
  }

  try {
    const AssemblyLeafOccurrenceResolver leaf_resolver;
    auto leaves = leaf_resolver.resolve(project);
    if (leaves.has_error()) {
      return Result<PosedAssemblyBuild>::failure(leaves.error());
    }
    if (leaves.value().empty()) {
      return Result<PosedAssemblyBuild>::failure(
          make_export_error("posed assembly export requires at least one visible active component"));
    }

    std::vector<DocumentId> referenced_part_ids;
    for (const AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
      if (!contains_part(referenced_part_ids, leaf.referenced_part_document)) {
        referenced_part_ids.push_back(leaf.referenced_part_document);
      }
    }
    std::sort(referenced_part_ids.begin(), referenced_part_ids.end(),
              [](const DocumentId& lhs, const DocumentId& rhs) {
                return lhs.value() < rhs.value();
              });

    const GeometryRecomputeExecutor recompute_executor;
    std::vector<RecomputedPartShape> recomputed_parts;
    recomputed_parts.reserve(referenced_part_ids.size());
    for (const DocumentId& part_id : referenced_part_ids) {
      const PartDocument* part = project.find_part_document(part_id);
      if (part == nullptr) {
        return Result<PosedAssemblyBuild>::failure(Error::validation(
            part_id.value(), "assembly export referenced part must resolve to a project part"));
      }

      auto cache = ShapeCache::create(
          ShapeCacheId("shape_cache.assembly_step_export." + part_id.value()));
      if (cache.has_error()) {
        return Result<PosedAssemblyBuild>::failure(cache.error());
      }

      const auto recomputed = recompute_executor.execute_document(*part, cache.value());
      if (recomputed.has_error()) {
        return Result<PosedAssemblyBuild>::failure(recomputed.error());
      }
      if (cache.value().final_shape() == nullptr) {
        return Result<PosedAssemblyBuild>::failure(Error::geometry(
            part_id.value(), "assembly export part recompute produced no final shape"));
      }

      recomputed_parts.push_back(RecomputedPartShape{part_id, std::move(cache.value())});
    }

    BRep_Builder compound_builder;
    TopoDS_Compound compound;
    compound_builder.MakeCompound(compound);

    for (const AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
      const RecomputedPartShape* part =
          find_recomputed_part(recomputed_parts, leaf.referenced_part_document);
      if (part == nullptr || part->shape_cache.final_shape() == nullptr) {
        return Result<PosedAssemblyBuild>::failure(Error::geometry(
            leaf_occurrence_key(leaf), "assembly export leaf component part shape is unavailable"));
      }

      const GeometryShape* final_shape = part->shape_cache.final_shape();
      auto posed = pose_shape_chain(final_shape->impl_->shape, leaf.transforms_inner_to_outer);
      if (posed.has_error()) {
        return Result<PosedAssemblyBuild>::failure(posed.error());
      }
      compound_builder.Add(compound, posed.value());
    }

    if (compound.IsNull()) {
      return Result<PosedAssemblyBuild>::failure(
          make_geometry_error("posed assembly compound is empty"));
    }

    GeometryShape shape(std::make_shared<GeometryShape::Impl>(TopoDS_Shape(compound)));
    return Result<PosedAssemblyBuild>::success(
        PosedAssemblyBuild{std::move(shape), recomputed_parts.size(), leaves.value().size()});
  } catch (const Standard_Failure& failure) {
    return Result<PosedAssemblyBuild>::failure(
        make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<PosedAssemblyBuild>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<PosedAssemblyBuild>::failure(make_geometry_error("unknown assembly export error"));
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
