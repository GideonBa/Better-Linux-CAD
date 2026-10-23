#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/surface_adapter.hpp"
#include "geometry_shape_internal.hpp"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <set>

namespace blcad::geometry {
namespace {
gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}
Point3 along(Point3 p, Vector3 v, double scale) {
  return {p.x + v.x * scale, p.y + v.y * scale, p.z + v.z * scale};
}
} // namespace
Result<std::vector<AssemblyInteractionTarget>>
AssemblyInteractionGeometry::targets(const Project& project) {
  std::vector<AssemblyInteractionTarget> results;
  try {
    for (const auto& component : project.assembly().component_instances()) {
      if (component.visibility() != ComponentVisibility::Visible ||
          component.suppression_state() != ComponentSuppressionState::Active)
        continue;
      const auto* part = project.find_part_document(component.referenced_part_document());
      if (!part)
        continue;
      auto cache =
          ShapeCache::create(ShapeCacheId("assembly.interaction." + component.id().value()));
      if (!cache)
        return Result<std::vector<AssemblyInteractionTarget>>::failure(cache.error());
      auto computed = GeometryRecomputeExecutor{}.execute_document(*part, cache.value());
      if (!computed)
        return Result<std::vector<AssemblyInteractionTarget>>::failure(computed.error());
      std::set<std::string> sources;
      const auto add = [&](auto spelling) {
        if (spelling)
          sources.insert(spelling.value());
      };
      for (const auto& feature : part->features()) {
        for (const char* role : {"top", "bottom", "front", "back", "left", "right", "axis", "seat"})
          sources.insert(feature.id().value() + "." + role);
        for (int i = 0; i < 12; ++i) {
          auto edge = SemanticEdgeReference::create(feature.id(), static_cast<SemanticEdge>(i));
          if (edge)
            add(make_generated_topology_target_spelling(edge.value()));
        }
        for (int i = 0; i < 8; ++i) {
          auto vertex =
              SemanticVertexReference::create(feature.id(), static_cast<SemanticVertex>(i));
          if (vertex)
            add(make_generated_topology_target_spelling(vertex.value()));
        }
        if (const auto* sketch = part->find_sketch(feature.input_sketch()))
          for (const auto& circle : sketch->circle_profiles()) {
            for (auto role : {SemanticCircularEdge::SourceRim, SemanticCircularEdge::OppositeRim})
              add(make_generated_topology_target_spelling(
                  SemanticCircularEdgeReference::create(feature.id(), circle.id(), role).value()));
            add(make_generated_topology_target_spelling(
                SemanticCylindricalFaceReference::create(feature.id(), circle.id()).value()));
          }
      }
      for (const auto& record : part->datum_planes())
        add(make_assembly_reference_target_spelling(record.id()));
      for (const auto& record : part->datum_axes())
        add(make_assembly_reference_target_spelling(record.id()));
      for (const auto& record : part->construction_lines())
        add(make_assembly_reference_target_spelling(record.id()));
      for (const auto& record : part->construction_points())
        add(make_assembly_reference_target_spelling(record.id()));
      for (const auto& source : sources) {
        auto target = AssemblyConstraintTarget::create(component.id(), source);
        if (!target)
          continue;
        auto resolved =
            AssemblyConstraintTargetResolver{}.resolve_geometric(project, target.value());
        if (!resolved)
          continue;
        const auto& value = resolved.value();
        const AssemblyHierarchyTransformEvaluator evaluator;
        const auto world_point = [&](Point3 p) {
          return evaluator.evaluate_point(value.transforms_inner_to_outer, p);
        };
        const auto world_vector = [&](Vector3 v) {
          return evaluator.evaluate_vector(value.transforms_inner_to_outer, v);
        };
        TopoDS_Shape shape;
        Point3 anchor{};
        if (const auto* plane = std::get_if<AssemblyPlanarTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(plane->origin);
          if (value.source_kind == AssemblyGeometricTargetSourceKind::GeneratedPlanarFace &&
              value.source_metadata.source_feature && value.source_metadata.semantic_face) {
            auto face = SurfaceAdapter{}.extract_semantic_face(
                FeatureId("assembly.target"), *part, cache.value(),
                SemanticFaceReference::create(*value.source_metadata.source_feature,
                                              *value.source_metadata.semantic_face)
                    .value());
            if (!face)
              continue;
            auto transformed = face.value();
            const auto& transform = component.transform();
            for (auto [axis, angle] :
                 std::vector<std::pair<Vector3, double>>{{{1, 0, 0}, transform.rotation_deg.x},
                                                         {{0, 1, 0}, transform.rotation_deg.y},
                                                         {{0, 0, 1}, transform.rotation_deg.z}}) {
              auto changed = BodyTransformAdapter{}.rotate(transformed, {}, axis, angle);
              if (!changed)
                return Result<std::vector<AssemblyInteractionTarget>>::failure(changed.error());
              transformed = std::move(changed.value());
            }
            auto shifted = BodyTransformAdapter{}.translate(transformed, transform.translation_mm);
            if (!shifted)
              return Result<std::vector<AssemblyInteractionTarget>>::failure(shifted.error());
            shape = shifted.value().impl_->shape;
          } else {
            BRepBuilderAPI_MakePolygon polygon;
            for (auto [u, v] :
                 std::vector<std::pair<double, double>>{{-6, -6}, {6, -6}, {6, 6}, {-6, 6}})
              polygon.Add(point(
                  world_point(along(along(plane->origin, plane->x_axis, u), plane->y_axis, v))));
            polygon.Close();
            shape = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
          }
        } else if (const auto* frame =
                       std::get_if<AssemblyFrameTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(frame->origin);
          TopoDS_Compound axes;
          BRep_Builder builder;
          builder.MakeCompound(axes);
          for (auto axis : {frame->x_axis, frame->y_axis, frame->z_axis})
            builder.Add(axes, BRepBuilderAPI_MakeEdge(
                                  point(anchor), point(world_point(along(frame->origin, axis, 8))))
                                  .Edge());
          shape = axes;
        } else if (const auto* p = std::get_if<AssemblyPointTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(p->point);
          shape = BRepBuilderAPI_MakeVertex(point(anchor));
        } else if (const auto* line =
                       std::get_if<AssemblyLineTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(line->origin);
          shape =
              BRepBuilderAPI_MakeEdge(point(world_point(along(line->origin, line->direction, -10))),
                                      point(world_point(along(line->origin, line->direction, 10))));
        } else if (const auto* axis =
                       std::get_if<AssemblyAxisTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(axis->origin);
          shape =
              BRepBuilderAPI_MakeEdge(point(world_point(along(axis->origin, axis->direction, -10))),
                                      point(world_point(along(axis->origin, axis->direction, 10))));
        } else if (const auto* circle =
                       std::get_if<AssemblyCircularEdgeTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(circle->center);
          auto normal = world_vector(circle->normal);
          auto x = world_vector(circle->x_axis);
          shape = BRepBuilderAPI_MakeEdge(gp_Circ(
              gp_Ax2(point(anchor), gp_Dir(normal.x, normal.y, normal.z), gp_Dir(x.x, x.y, x.z)),
              circle->radius_mm));
        } else if (const auto* cylinder =
                       std::get_if<AssemblyCylindricalSurfaceTargetDescriptor>(&value.descriptor)) {
          anchor = world_point(cylinder->axis_origin);
          shape = BRepBuilderAPI_MakeEdge(
              point(world_point(along(cylinder->axis_origin, cylinder->axis_direction, -10))),
              point(world_point(along(cylinder->axis_origin, cylinder->axis_direction, 10))));
        }
        if (shape.IsNull())
          continue;
        const std::string identity = "assembly-target/" +
                                     std::to_string(component.id().value().size()) + ":" +
                                     component.id().value() + "/" + source;
        results.push_back({target.value(),
                           value,
                           component.name() + " / " + source,
                           {ViewportSceneKind::AssemblyTarget, identity,
                            GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape)))},
                           anchor});
      }
    }
  } catch (const Standard_Failure& failure) {
    return Result<std::vector<AssemblyInteractionTarget>>::failure(Error::geometry(
        "assembly.interaction",
        failure.GetMessageString() ? failure.GetMessageString() : "OCCT target error"));
  }
  return Result<std::vector<AssemblyInteractionTarget>>::success(std::move(results));
}
} // namespace blcad::geometry
