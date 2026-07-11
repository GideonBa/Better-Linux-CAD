#include "assembly_posed_leaf_shapes.hpp"

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_Transform.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <numbers>
#include <string>
#include <utility>

namespace blcad::geometry::detail {
namespace {

constexpr const char* kPosedLeafShapesId = "geometry.assembly_posed_leaf_shapes";

struct RecomputedPartShape {
  DocumentId part_document;
  ShapeCache shape_cache;
};

[[nodiscard]] Error make_geometry_error(std::string message) {
  return Error::geometry(kPosedLeafShapesId, std::move(message));
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

// One composed OCCT transform per authored RigidTransform: rotate about the
// fixed X, then Y, then Z axis, then translate — exactly the semantics of
// AssemblyTransformEvaluator. gp_Trsf composition applies the right-hand
// factor first, so the product is translation * Rz * Ry * Rx.
[[nodiscard]] gp_Trsf to_occt_transform(const RigidTransform& transform) {
  const gp_Ax1 x_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
  const gp_Ax1 y_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
  const gp_Ax1 z_axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));

  gp_Trsf rotation_x;
  rotation_x.SetRotation(x_axis, degrees_to_radians(transform.rotation_deg.x));
  gp_Trsf rotation_y;
  rotation_y.SetRotation(y_axis, degrees_to_radians(transform.rotation_deg.y));
  gp_Trsf rotation_z;
  rotation_z.SetRotation(z_axis, degrees_to_radians(transform.rotation_deg.z));
  gp_Trsf translation;
  translation.SetTranslation(
      gp_Vec(transform.translation_mm.x, transform.translation_mm.y, transform.translation_mm.z));

  return translation * rotation_z * rotation_y * rotation_x;
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

[[nodiscard]] Result<TopoDS_Shape>
pose_shape_chain(const TopoDS_Shape& source,
                 const std::vector<RigidTransform>& transforms_inner_to_outer) {
  if (transforms_inner_to_outer.empty()) {
    return Result<TopoDS_Shape>::success(source);
  }

  // Compose the whole inner-to-outer chain into one rigid transform so each
  // leaf occurrence costs exactly one OCCT shape transformation.
  gp_Trsf composed;
  for (const RigidTransform& transform : transforms_inner_to_outer) {
    composed = to_occt_transform(transform) * composed;
  }
  return apply_occt_transform(source, composed);
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

} // namespace

std::string leaf_occurrence_key(const AssemblyLeafOccurrenceDescriptor& leaf) {
  std::string key = leaf.assembly_document.value();
  for (const SubassemblyInstanceId& occurrence : leaf.subassembly_path) {
    key += "/" + occurrence.value();
  }
  key += "/" + leaf.component_instance.value();
  return key;
}

Result<PosedLeafShapesBuild> build_posed_leaf_shapes(const Project& project) {
  const AssemblyPosedLeafShapeBuilder builder;
  return builder.build(project);
}

Result<PosedLeafShapesBuild> AssemblyPosedLeafShapeBuilder::build(const Project& project) const {
  const auto valid_structure = project.validate_assembly_structure();
  if (valid_structure.has_error()) {
    return Result<PosedLeafShapesBuild>::failure(valid_structure.error());
  }

  try {
    const AssemblyLeafOccurrenceResolver leaf_resolver;
    auto leaves = leaf_resolver.resolve(project);
    if (leaves.has_error()) {
      return Result<PosedLeafShapesBuild>::failure(leaves.error());
    }

    std::vector<DocumentId> referenced_part_ids;
    for (const AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
      if (!contains_part(referenced_part_ids, leaf.referenced_part_document)) {
        referenced_part_ids.push_back(leaf.referenced_part_document);
      }
    }
    std::sort(
        referenced_part_ids.begin(), referenced_part_ids.end(),
        [](const DocumentId& lhs, const DocumentId& rhs) { return lhs.value() < rhs.value(); });

    const GeometryRecomputeExecutor recompute_executor;
    std::vector<RecomputedPartShape> recomputed_parts;
    recomputed_parts.reserve(referenced_part_ids.size());
    for (const DocumentId& part_id : referenced_part_ids) {
      const PartDocument* part = project.find_part_document(part_id);
      if (part == nullptr) {
        return Result<PosedLeafShapesBuild>::failure(Error::validation(
            part_id.value(), "assembly export referenced part must resolve to a project part"));
      }

      auto cache =
          ShapeCache::create(ShapeCacheId("shape_cache.assembly_step_export." + part_id.value()));
      if (cache.has_error()) {
        return Result<PosedLeafShapesBuild>::failure(cache.error());
      }

      const auto recomputed = recompute_executor.execute_document(*part, cache.value());
      if (recomputed.has_error()) {
        return Result<PosedLeafShapesBuild>::failure(recomputed.error());
      }
      if (cache.value().final_shape() == nullptr) {
        return Result<PosedLeafShapesBuild>::failure(Error::geometry(
            part_id.value(), "assembly export part recompute produced no final shape"));
      }

      recomputed_parts.push_back(RecomputedPartShape{part_id, std::move(cache.value())});
    }

    PosedLeafShapesBuild build;
    build.recomputed_part_count = recomputed_parts.size();
    build.leaves.reserve(leaves.value().size());
    for (AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
      const RecomputedPartShape* part =
          find_recomputed_part(recomputed_parts, leaf.referenced_part_document);
      if (part == nullptr || part->shape_cache.final_shape() == nullptr) {
        return Result<PosedLeafShapesBuild>::failure(Error::geometry(
            leaf_occurrence_key(leaf), "assembly export leaf component part shape is unavailable"));
      }

      const GeometryShape* final_shape = part->shape_cache.final_shape();
      auto posed = pose_shape_chain(final_shape->impl_->shape, leaf.transforms_inner_to_outer);
      if (posed.has_error()) {
        return Result<PosedLeafShapesBuild>::failure(posed.error());
      }
      build.leaves.push_back(PosedLeafShape{std::move(leaf), std::move(posed.value())});
    }

    return Result<PosedLeafShapesBuild>::success(std::move(build));
  } catch (const Standard_Failure& failure) {
    return Result<PosedLeafShapesBuild>::failure(
        make_geometry_error(standard_failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<PosedLeafShapesBuild>::failure(make_geometry_error(exception.what()));
  } catch (...) {
    return Result<PosedLeafShapesBuild>::failure(
        make_geometry_error("unknown posed leaf shape error"));
  }
}

} // namespace blcad::geometry::detail
