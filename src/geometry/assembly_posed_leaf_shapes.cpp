#include "assembly_posed_leaf_shapes.hpp"

#include "assembly_occt_rigid_transform.hpp"
#include "assembly_part_shape_definitions.hpp"

#include <BRepBuilderAPI_Transform.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry::detail {
namespace {

constexpr const char* kPosedLeafShapesId = "geometry.assembly_posed_leaf_shapes";

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

  // Compose the whole canonical inner-to-outer leaf chain into one rigid
  // transform so each posed leaf costs exactly one OCCT shape transformation.
  gp_Trsf composed;
  for (const RigidTransform& transform : transforms_inner_to_outer) {
    composed = to_occt_rigid_transform(transform) * composed;
  }
  return apply_occt_transform(source, composed);
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
    referenced_part_ids.reserve(leaves.value().size());
    for (const AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
      referenced_part_ids.push_back(leaf.referenced_part_document);
    }

    const AssemblyPartShapeDefinitionBuilder part_definition_builder;
    auto part_definitions =
        part_definition_builder.build(project, std::move(referenced_part_ids));
    if (part_definitions.has_error()) {
      return Result<PosedLeafShapesBuild>::failure(part_definitions.error());
    }

    PosedLeafShapesBuild build;
    build.recomputed_part_count = part_definitions.value().recomputed_part_count;
    build.leaves.reserve(leaves.value().size());
    for (AssemblyLeafOccurrenceDescriptor& leaf : leaves.value()) {
      const AssemblyPartShapeDefinition* part = find_assembly_part_shape_definition(
          part_definitions.value().definitions, leaf.referenced_part_document);
      if (part == nullptr || part->shape.IsNull()) {
        return Result<PosedLeafShapesBuild>::failure(Error::geometry(
            leaf_occurrence_key(leaf),
            "assembly export leaf component part shape is unavailable"));
      }

      auto posed = pose_shape_chain(part->shape, leaf.transforms_inner_to_outer);
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
