#include "blcad/geometry/adaptive_project.hpp"
#include "assembly_cross_hierarchy_numeric_system.hpp"
#include "assembly_posed_leaf_shapes.hpp"
#include "blcad/core/assembly_constraint_graph.hpp"
#include "blcad/core/assembly_hierarchy.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "project_evaluation_internal.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <type_traits>

namespace blcad::geometry {
namespace {
Error error(std::string message) {
  return Error::dependency("project.adaptivity", std::move(message));
}
Result<AssemblyLeafOccurrenceDescriptor> leaf(const Project& project,
                                              const ProjectDesignOccurrence& occurrence) {
  auto leaves =
      AssemblyLeafOccurrenceResolver{}.resolve(project, AssemblyLeafParticipation::PhysicalActive);
  if (!leaves)
    return Result<AssemblyLeafOccurrenceDescriptor>::failure(leaves.error());
  for (const auto& item : leaves.value())
    if (item.subassembly_path == occurrence.path &&
        item.component_instance == occurrence.component &&
        item.referenced_part_document == occurrence.part)
      return Result<AssemblyLeafOccurrenceDescriptor>::success(item);
  return Result<AssemblyLeafOccurrenceDescriptor>::failure(
      error("Design source/driver occurrence is unavailable"));
}
GeometryAffineTransform pose(const AssemblyLeafOccurrenceDescriptor& leaf) {
  const AssemblyHierarchyTransformEvaluator evaluator;
  const auto origin = evaluator.evaluate_point(leaf.transforms_inner_to_outer, {});
  const auto x = evaluator.evaluate_vector(leaf.transforms_inner_to_outer, {1, 0, 0});
  const auto y = evaluator.evaluate_vector(leaf.transforms_inner_to_outer, {0, 1, 0});
  const auto z = evaluator.evaluate_vector(leaf.transforms_inner_to_outer, {0, 0, 1});
  GeometryAffineTransform value;
  value.matrix = {x.x, y.x, z.x, origin.x, x.y, y.y, z.y, origin.y,
                  x.z, y.z, z.z, origin.z, 0,   0,   0,   1};
  return value;
}
Result<GeometryAffineTransform> mapping(const Project& project, const ProjectDesignReference& ref) {
  auto source = leaf(project, ref.source());
  if (!source)
    return Result<GeometryAffineTransform>::failure(source.error());
  auto driver = leaf(project, ref.driver());
  if (!driver)
    return Result<GeometryAffineTransform>::failure(driver.error());
  auto inverse = pose(driver.value()).inverse_similarity();
  if (!inverse)
    return inverse;
  return Result<GeometryAffineTransform>::success(
      pose(source.value()).followed_by(inverse.value()));
}
Result<ShapeCache> geometry(const PartDocument& part, StepImportTopologyContext* imports) {
  auto cache = ShapeCache::create(ShapeCacheId("adaptive." + part.id().value()));
  if (!cache)
    return cache;
  auto built = GeometryRecomputeExecutor{imports}.execute_document(part, cache.value());
  if (!built)
    return Result<ShapeCache>::failure(built.error());
  return cache;
}
Result<AssemblyResolvedGeometricTarget> semantic(const PartDocument& part,
                                                 const ProjectDesignReference& ref,
                                                 const ShapeCache& cache,
                                                 StepImportTopologyContext* imports) {
  auto root = AssemblyDocument::create(DocumentId("adaptive.query.root"), "Adaptive source query");
  if (!root)
    return Result<AssemblyResolvedGeometricTarget>::failure(root.error());
  auto member = root.value().add_member_part(part.id());
  if (!member)
    return Result<AssemblyResolvedGeometricTarget>::failure(member.error());
  auto component = ComponentInstance::create(
      ref.source().component, "Source", part.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, identity_rigid_transform(),
      ref.source_state());
  if (!component)
    return Result<AssemblyResolvedGeometricTarget>::failure(component.error());
  auto added = root.value().add_component_instance(component.value());
  if (!added)
    return Result<AssemblyResolvedGeometricTarget>::failure(added.error());
  auto query = Project::create(DocumentId("adaptive.query"), "Adaptive query", root.value());
  if (!query)
    return Result<AssemblyResolvedGeometricTarget>::failure(query.error());
  auto owned = query.value().add_part_document(part);
  if (!owned)
    return Result<AssemblyResolvedGeometricTarget>::failure(owned.error());
  auto target = AssemblyConstraintTarget::create(
      ref.source().component,
      std::get<ProjectDesignSemanticSource>(ref.source_geometry()).reference);
  if (!target)
    return Result<AssemblyResolvedGeometricTarget>::failure(target.error());
  return AssemblyConstraintTargetResolver{imports}.resolve_geometric(query.value(), target.value(),
                                                                     &cache);
}
Result<std::size_t> copy_curve(PartDocument& target, const PartDocument& source,
                               const ProjectDesignReference& ref,
                               const GeometryAffineTransform& frame) {
  const auto& spec = std::get<ProjectDerivedPathTarget>(ref.target());
  const auto* path =
      source.find_path_curve(std::get<ProjectDesignPathSource>(ref.source_geometry()).path);
  if (!path)
    return Result<std::size_t>::failure(error("Source PathCurve is missing"));
  if (path->fixed_up_vector())
    return Result<std::size_t>::failure(
        error("Adaptive path copying currently requires a native transported "
              "frame, without a fixed-up vector"));
  auto sketch = Sketch3D::create(spec.sketch, ref.name());
  if (!sketch)
    return Result<std::size_t>::failure(sketch.error());
  std::vector<PathSegmentReference> segments;
  const Sketch3DGeometryAdapter adapter;
  std::size_t curve_index = 0;
  for (const auto& segment : path->segments()) {
    if (segment.source_kind() != PathSegmentSourceKind::Sketch3DCurve || !segment.sketch_3d() ||
        !segment.entity())
      return Result<std::size_t>::failure(
          error("Adaptive native paths currently require explicit Sketch3D "
                "lines, polylines, arcs or splines"));
    const auto* definition = source.find_sketch_3d(*segment.sketch_3d());
    if (!definition)
      return Result<std::size_t>::failure(error("Source Sketch3D is missing"));
    auto solved = adapter.convert(source, *definition);
    if (!solved)
      return Result<std::size_t>::failure(solved.error());
    const std::string prefix = "derived." + ref.id().value() + "." + std::to_string(curve_index++);
    const SketchEntityId curve(prefix + ".curve");
    std::size_t point_index = 0;
    auto add_point = [&](const SketchPointReference3D& input) -> Result<SketchPointReference3D> {
      Result<Point3> position =
          input.source() == SketchPointReference3DSource::LocalPoint ? [&]() -> Result<Point3> {
        const auto* point = solved.value().find_point(*input.local_point());
        if (!point)
          return Result<Point3>::failure(error("Solved source point is unavailable"));
        return Result<Point3>::success(point->position);
      }()
          : adapter.resolve_point(source, input);
      if (!position)
        return Result<SketchPointReference3D>::failure(position.error());
      const auto local = frame.transform_point(position.value());
      auto x = Quantity::linear_displacement_mm(local.x),
           y = Quantity::linear_displacement_mm(local.y),
           z = Quantity::linear_displacement_mm(local.z);
      if (!x || !y || !z)
        return Result<SketchPointReference3D>::failure(error("Derived point is not finite"));
      auto a = SketchCoordinate3D::create_explicit(x.value()),
           b = SketchCoordinate3D::create_explicit(y.value()),
           c = SketchCoordinate3D::create_explicit(z.value());
      if (!a || !b || !c)
        return Result<SketchPointReference3D>::failure(error("Derived coordinate is invalid"));
      const SketchEntityId id(prefix + ".point." + std::to_string(point_index++));
      auto point = SketchPoint3D::create(id, a.value(), b.value(), c.value());
      if (!point)
        return Result<SketchPointReference3D>::failure(point.error());
      auto added = sketch.value().add_point(point.value());
      if (!added)
        return Result<SketchPointReference3D>::failure(added.error());
      return SketchPointReference3D::create_local_point(id);
    };
    auto local = [&](SketchEntityId id) { return SketchPointReference3D::create_local_point(id); };
    const auto kind = *segment.sketch_3d_curve_kind();
    if (kind == Sketch3DPathCurveKind::Line) {
      const auto* input = definition->find_line(*segment.entity());
      if (!input)
        return Result<std::size_t>::failure(error("Source line is missing"));
      auto a = add_point(local(input->start_point()).value()),
           b = add_point(local(input->end_point()).value());
      if (!a || !b)
        return Result<std::size_t>::failure(error("Source line points failed"));
      auto curve_value =
          SketchLine3D::create(curve, *a.value().local_point(), *b.value().local_point());
      if (!curve_value)
        return Result<std::size_t>::failure(curve_value.error());
      auto added = sketch.value().add_line(curve_value.value());
      if (!added)
        return added;
    } else if (kind == Sketch3DPathCurveKind::Polyline) {
      const auto* input = definition->find_polyline(*segment.entity());
      if (!input)
        return Result<std::size_t>::failure(error("Source polyline is missing"));
      std::vector<SketchEntityId> points;
      for (const auto& id : input->ordered_vertices()) {
        auto copied = add_point(local(id).value());
        if (!copied)
          return Result<std::size_t>::failure(copied.error());
        points.push_back(*copied.value().local_point());
      }
      auto curve_value = SketchPolyline3D::create(curve, std::move(points));
      if (!curve_value)
        return Result<std::size_t>::failure(curve_value.error());
      auto added = sketch.value().add_polyline(curve_value.value());
      if (!added)
        return added;
    } else if (kind == Sketch3DPathCurveKind::Arc) {
      const auto* input = definition->find_arc(*segment.entity());
      if (!input)
        return Result<std::size_t>::failure(error("Source arc is missing"));
      auto a = add_point(input->start()), b = add_point(input->intermediate()),
           c = add_point(input->end());
      if (!a || !b || !c)
        return Result<std::size_t>::failure(error("Source arc points failed"));
      auto curve_value = SketchArc3D::create_three_point(curve, a.value(), b.value(), c.value());
      if (!curve_value)
        return Result<std::size_t>::failure(curve_value.error());
      auto added = sketch.value().add_arc(curve_value.value());
      if (!added)
        return added;
    } else if (kind == Sketch3DPathCurveKind::Spline) {
      const auto* input = definition->find_spline(*segment.entity());
      if (!input)
        return Result<std::size_t>::failure(error("Source spline is missing"));
      std::vector<SketchPointReference3D> points;
      for (const auto& reference : input->ordered_points()) {
        auto copied = add_point(reference);
        if (!copied)
          return Result<std::size_t>::failure(copied.error());
        points.push_back(std::move(copied.value()));
      }
      auto curve_value = SketchSpline3D::create(curve, input->representation(), std::move(points),
                                                input->degree(), input->continuity());
      if (!curve_value)
        return Result<std::size_t>::failure(curve_value.error());
      auto added = sketch.value().add_spline(curve_value.value());
      if (!added)
        return added;
    } else
      return Result<std::size_t>::failure(
          error("Helix/equation native adaptive path copying requires a "
                "separate explicit capability"));
    auto output =
        PathSegmentReference::create_sketch_3d(spec.sketch, curve, kind, segment.reversed());
    if (!output)
      return Result<std::size_t>::failure(output.error());
    segments.push_back(std::move(output.value()));
  }
  const auto* existing = target.find_path_curve(spec.path);
  if (existing) {
    if (existing->segments().size() != segments.size() || existing->closure() != path->closure() ||
        existing->orientation_rule() != path->orientation_rule() ||
        existing->execution_policy() != path->execution_policy() ||
        existing->continuity_hint() != path->continuity_hint() ||
        existing->connection_tolerance_mm() != path->connection_tolerance_mm())
      return Result<std::size_t>::failure(
          error("Adaptive PathCurve topology/policy changed; repair its "
                "explicit target"));
    for (std::size_t i = 0; i < segments.size(); ++i)
      if (existing->segments()[i].stable_key() != segments[i].stable_key())
        return Result<std::size_t>::failure(
            error("Adaptive PathCurve segment identity changed; repair explicitly"));
  }
  PartDocument proposed = target;
  auto changed = proposed.find_sketch_3d(spec.sketch) ? proposed.update_sketch_3d(sketch.value())
                                                      : proposed.add_sketch_3d(sketch.value());
  if (!changed)
    return changed;
  if (!existing) {
    auto derived = PathCurve::create(
        spec.path, ref.name(), std::move(segments), path->closure(), path->orientation_rule(),
        path->fixed_up_vector()
            ? std::optional<Vector3>(frame.transform_vector(*path->fixed_up_vector()))
            : std::nullopt,
        path->continuity_hint(), path->connection_tolerance_mm(), path->execution_policy());
    if (!derived)
      return Result<std::size_t>::failure(derived.error());
    auto added = proposed.add_path_curve(std::move(derived.value()));
    if (!added)
      return added;
  }
  auto original = serialize_part_document_to_json(target);
  auto next = serialize_part_document_to_json(proposed);
  if (!original)
    return Result<std::size_t>::failure(original.error());
  if (!next)
    return Result<std::size_t>::failure(next.error());
  if (original.value() == next.value())
    return Result<std::size_t>::success(0);
  target = std::move(proposed);
  return Result<std::size_t>::success(1);
}
// Conservative mixed shape/pose graph: a connected relationship group can
// constrain every free transform from every participating geometric definition.
// Grounded transforms remain independent authored constants. No feedback solve
// is attempted when an adaptive reference consumes one of those driven poses.
Result<std::size_t> validate_feedback(const Project& project) {
  if (project.design_references().empty() && project.assembly_hole_features().empty())
    return Result<std::size_t>::success(0);
  auto leaves =
      AssemblyLeafOccurrenceResolver{}.resolve(project, AssemblyLeafParticipation::PhysicalActive);
  if (!leaves)
    return Result<std::size_t>::failure(leaves.error());
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (!hierarchy)
    return Result<std::size_t>::failure(hierarchy.error());
  auto spelling = [](const std::vector<SubassemblyInstanceId>& path,
                     const ComponentInstanceId& component) {
    std::string key;
    for (const auto& id : path)
      key += std::to_string(id.value().size()) + ":" + id.value() + "/";
    return key + std::to_string(component.value().size()) + ":" + component.value();
  };
  std::map<std::string, std::string> occurrence_node, node_part;
  std::map<std::string, bool> free;
  for (const auto& value : leaves.value()) {
    const auto* assembly = project.find_assembly_document(value.assembly_document);
    const auto* component = assembly->find_component_instance(value.component_instance);
    const auto scope = project.flexible_occurrence(value.subassembly_path)
                           ? value.subassembly_path
                           : std::vector<SubassemblyInstanceId>{};
    const std::string node = "pose/" + std::to_string(assembly->id().value().size()) + ":" +
                             assembly->id().value() + "/" + spelling(scope, component->id());
    occurrence_node[spelling(value.subassembly_path, value.component_instance)] = node;
    node_part[node] = value.referenced_part_document.value();
    free[node] = component->grounding_state() == ComponentGroundingState::Free;
  }
  std::map<std::string, std::set<std::string>> adjacency, graph;
  std::set<std::string> constrained;
  auto relation =
      [&](const std::vector<SubassemblyInstanceId>& a_path, const ComponentInstanceId& a,
          const std::vector<SubassemblyInstanceId>& b_path, const ComponentInstanceId& b) {
        auto ia = occurrence_node.find(spelling(a_path, a)),
             ib = occurrence_node.find(spelling(b_path, b));
        if (ia == occurrence_node.end() || ib == occurrence_node.end())
          return;
        adjacency[ia->second].insert(ib->second);
        adjacency[ib->second].insert(ia->second);
        constrained.insert(ia->second);
        constrained.insert(ib->second);
      };
  for (const auto& occurrence : hierarchy.value().occurrences()) {
    if (!occurrence.active_path)
      continue;
    auto effective = project.find_assembly_document(occurrence.assembly_document)
                         ->evaluate_model_state(occurrence.selected_model_state);
    if (!effective)
      return Result<std::size_t>::failure(effective.error());
    for (const auto& constraint : effective.value().constraints())
      if (constraint.state() == AssemblyConstraintState::Active)
        relation(occurrence.occurrence_path, constraint.target_a().component_instance(),
                 occurrence.occurrence_path, constraint.target_b().component_instance());
    for (const auto& joint : effective.value().joints())
      if (joint.state() == AssemblyJointState::Active)
        relation(occurrence.occurrence_path, joint.target_a().component_instance(),
                 occurrence.occurrence_path, joint.target_b().component_instance());
  }
  for (const auto& constraint : project.cross_hierarchy_constraints())
    if (constraint.state() == AssemblyConstraintState::Active)
      relation(constraint.target_a().occurrence_path(), constraint.target_a().component_instance(),
               constraint.target_b().occurrence_path(), constraint.target_b().component_instance());
  for (const auto& joint : project.cross_hierarchy_joints())
    if (joint.state() == AssemblyJointState::Active)
      relation(joint.target_a().occurrence_path(), joint.target_a().component_instance(),
               joint.target_b().occurrence_path(), joint.target_b().component_instance());
  std::set<std::string> visited;
  for (const auto& first : constrained) {
    if (visited.contains(first))
      continue;
    std::vector<std::string> group{first};
    visited.insert(first);
    for (std::size_t i = 0; i < group.size(); ++i)
      for (const auto& next : adjacency[group[i]])
        if (visited.insert(next).second)
          group.push_back(next);
    for (const auto& input : group)
      for (const auto& output : group)
        if (free[output])
          graph["shape/" + node_part[input]].insert(output);
  }
  for (const auto& ref : project.design_references()) {
    const std::string output = "shape/" + ref.driver().part.value();
    graph["shape/" + ref.source().part.value()].insert(output);
    for (const auto* occurrence : {&ref.source(), &ref.driver()}) {
      auto found = occurrence_node.find(spelling(occurrence->path, occurrence->component));
      if (found == occurrence_node.end())
        return Result<std::size_t>::failure(error("Adaptive pose dependency is missing"));
      graph[found->second].insert(output);
    }
  }
  for (const auto& feature : project.assembly_hole_features())
    for (const auto& participant : feature.participants()) {
      // The original layout Hole is retained in its source definition. Only a
      // cross-Part participant derives shape from this layout and these poses.
      if (participant.occurrence.part == feature.layout().source.part)
        continue;
      const std::string output = "shape/" + participant.occurrence.part.value();
      graph["shape/" + feature.layout().source.part.value()].insert(output);
      for (const auto* occurrence : {&feature.layout().source, &participant.occurrence}) {
        const auto found = occurrence_node.find(spelling(occurrence->path, occurrence->component));
        if (found == occurrence_node.end())
          return Result<std::size_t>::failure(error("Assembly Hole pose dependency is missing"));
        graph[found->second].insert(output);
      }
    }
  std::map<std::string, int> marks;
  std::function<bool(const std::string&)> visit = [&](const std::string& node) {
    if (marks[node] == 1)
      return false;
    if (marks[node] == 2)
      return true;
    marks[node] = 1;
    for (const auto& next : graph[node])
      if (!visit(next))
        return false;
    marks[node] = 2;
    return true;
  };
  for (const auto& [node, _] : graph)
    if (!visit(node))
      return Result<std::size_t>::failure(
          error("Recursive directed shape/pose dependency; use an independent "
                "grounded driver or detach"));
  return Result<std::size_t>::success(graph.size());
}
Result<std::size_t> solve_assembly(Project& project, StepImportTopologyContext* imports) {
  auto hierarchy = AssemblyHierarchyTraversal::build(project);
  if (!hierarchy)
    return Result<std::size_t>::failure(hierarchy.error());
  std::size_t count = 0;
  for (const auto& occurrence : hierarchy.value().occurrences()) {
    if (!occurrence.active_path)
      continue;
    auto local = detail::make_cross_hierarchy_local_evaluation_view(
        project, occurrence.assembly_document, occurrence.occurrence_path);
    if (!local)
      return Result<std::size_t>::failure(local.error());
    auto graph = AssemblyConstraintGraph::build(local.value().assembly());
    if (!graph)
      return Result<std::size_t>::failure(graph.error());
    bool changed = false;
    for (const auto& group : graph.value().connected_components()) {
      if (group.empty())
        continue;
      auto adjacent = graph.value().adjacent_constraints(group.front());
      if (!adjacent)
        return Result<std::size_t>::failure(adjacent.error());
      if (adjacent.value().empty())
        continue;
      auto solved = AssemblyRigidBodySolver{imports}.solve(local.value(), group);
      if (!solved)
        return Result<std::size_t>::failure(solved.error());
      if (!solved.value().converged())
        return Result<std::size_t>::failure(
            error("Adaptive candidate has an unsatisfied local Assembly "
                  "relationship"));
      auto applied = AssemblySolveResultApplier{imports}.apply(local.value(), solved.value());
      if (!applied)
        return applied;
      changed = true;
    }
    const auto joints = local.value().assembly().joints();
    for (const auto& joint : joints) {
      if (joint.state() != AssemblyJointState::Active)
        continue;
      AssemblyJointDrive drive{joint.id(), {}};
      for (const auto& slot : joint.coordinate_slots())
        drive.requested_coordinates.push_back({slot.role(), slot.value()});
      auto moved = AssemblyJointMotionSolver{imports}.move(local.value(), std::move(drive));
      if (!moved)
        return Result<std::size_t>::failure(moved.error());
      if (!moved.value().converged())
        return Result<std::size_t>::failure(
            error("Adaptive candidate cannot retain a local joint coordinate"));
      auto applied = AssemblyJointMotionResultApplier{imports}.apply(local.value(), moved.value());
      if (!applied)
        return applied;
      changed = true;
    }
    if (changed)
      for (const auto& component : local.value().assembly().component_instances()) {
        if (component.grounding_state() == ComponentGroundingState::Grounded)
          continue;
        auto applied = detail::set_cross_hierarchy_authority_transform(
            project,
            {occurrence.assembly_document, component.id(),
             project.flexible_occurrence(occurrence.occurrence_path)
                 ? occurrence.occurrence_path
                 : std::vector<SubassemblyInstanceId>{}},
            component.transform());
        if (!applied)
          return applied;
        ++count;
      }
  }
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  if (!graph)
    return Result<std::size_t>::failure(graph.error());
  for (const auto& group : graph.value().solve_groups()) {
    auto solved = AssemblyCrossHierarchyConstraintSolver{imports}.solve(project, group);
    if (!solved)
      return Result<std::size_t>::failure(solved.error());
    if (!solved.value().converged())
      return Result<std::size_t>::failure(
          error("Adaptive candidate has an unsatisfied cross-hierarchy "
                "relationship"));
    auto applied = AssemblyCrossHierarchySolveResultApplier{imports}.apply(project, solved.value());
    if (!applied)
      return applied;
    count += applied.value();
  }
  const auto joints = project.cross_hierarchy_joints();
  for (const auto& joint : joints) {
    if (joint.state() != AssemblyJointState::Active)
      continue;
    AssemblyJointDrive drive{joint.id(), {}};
    for (const auto& slot : joint.coordinate_slots())
      drive.requested_coordinates.push_back({slot.role(), slot.value()});
    auto moved = AssemblyCrossHierarchyJointMotionSolver{imports}.move(project, std::move(drive));
    if (!moved)
      return Result<std::size_t>::failure(moved.error());
    if (!moved.value().converged())
      return Result<std::size_t>::failure(
          error("Adaptive candidate cannot retain a cross-hierarchy joint "
                "coordinate"));
    auto applied =
        AssemblyCrossHierarchyJointMotionResultApplier{imports}.apply(project, moved.value());
    if (!applied)
      return applied;
    count += applied.value();
  }
  return Result<std::size_t>::success(count);
}
Result<std::size_t> update(Project& project, const ProjectDesignReference& ref,
                           StepImportTopologyContext* imports) {
  auto transform = mapping(project, ref);
  if (!transform)
    return Result<std::size_t>::failure(transform.error());
  const auto* source_definition = project.find_part_document(ref.source().part);
  auto* target = project.find_part_document(ref.driver().part);
  if (!source_definition || !target || target->is_reference_part())
    return Result<std::size_t>::failure(error("Adaptive source/target definition is unavailable"));
  auto source = source_definition->evaluate_model_state(ref.source_state());
  if (!source)
    return Result<std::size_t>::failure(source.error());
  auto cache = geometry(source.value(), imports);
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  if (std::holds_alternative<ProjectDerivedPathTarget>(ref.target()))
    return copy_curve(*target, source.value(), ref, transform.value());
  auto resolved = semantic(*source_definition, ref, cache.value(), imports);
  if (!resolved)
    return Result<std::size_t>::failure(resolved.error());
  return std::visit(
      [&](const auto& destination) -> Result<std::size_t> {
        using T = std::decay_t<decltype(destination)>;
        if constexpr (std::is_same_v<T, ConstructionPointId>) {
          auto input = project_point(resolved.value());
          if (!input)
            return Result<std::size_t>::failure(input.error());
          auto value = ConstructionPoint::create_explicit(
              destination, ref.name(), transform.value().transform_point(input.value().point));
          if (!value)
            return Result<std::size_t>::failure(value.error());
          const auto* current = target->find_construction_point(destination);
          if (current && current->kind() == ConstructionPointKind::Explicit &&
              current->name() == value.value().name() &&
              current->position() == value.value().position() &&
              current->parameter_dependencies() == value.value().parameter_dependencies())
            return Result<std::size_t>::success(0);
          return current ? target->update_construction_point(value.value())
                         : target->add_construction_point(value.value());
        } else if constexpr (std::is_same_v<T, ConstructionLineId>) {
          auto projected = project_line(resolved.value());
          auto axis = project_axis(resolved.value());
          if (!projected && !axis)
            return Result<std::size_t>::failure(projected.error());
          const AssemblyAxisTargetDescriptor input =
              projected ? AssemblyAxisTargetDescriptor{projected.value().origin,
                                                       projected.value().direction}
                        : axis.value();
          auto value = ConstructionLine::create_explicit(
              destination, ref.name(), transform.value().transform_point(input.origin),
              transform.value().transform_vector(input.direction));
          if (!value)
            return Result<std::size_t>::failure(value.error());
          const auto* current = target->find_construction_line(destination);
          if (current && current->kind() == ConstructionLineKind::Explicit &&
              current->name() == value.value().name() &&
              current->point() == value.value().point() &&
              current->direction() == value.value().direction() &&
              current->parameter_dependencies() == value.value().parameter_dependencies())
            return Result<std::size_t>::success(0);
          return current ? target->update_construction_line(value.value())
                         : target->add_construction_line(value.value());
        } else if constexpr (std::is_same_v<T, ConstructionPlaneId>) {
          auto input = project_plane(resolved.value());
          if (!input)
            return Result<std::size_t>::failure(input.error());
          auto value = ConstructionPlane::create_explicit(
              destination, ref.name(), transform.value().transform_point(input.value().origin),
              transform.value().transform_vector(input.value().x_axis),
              transform.value().transform_vector(input.value().y_axis),
              transform.value().transform_vector(input.value().normal));
          if (!value)
            return Result<std::size_t>::failure(value.error());
          const auto* current = target->find_construction_plane(destination);
          if (current && current->kind() == ConstructionPlaneKind::Explicit &&
              current->name() == value.value().name() &&
              current->origin() == value.value().origin() &&
              current->x_axis() == value.value().x_axis() &&
              current->y_axis() == value.value().y_axis() &&
              current->normal() == value.value().normal() &&
              current->parameter_dependencies() == value.value().parameter_dependencies())
            return Result<std::size_t>::success(0);
          return current ? target->update_construction_plane(value.value())
                         : target->add_construction_plane(value.value());
        } else
          return Result<std::size_t>::failure(error("Invalid semantic design reference target"));
      },
      ref.target());
}
} // namespace

Result<GeometryAffineTransform>
project_occurrence_transform(const Project& project, const ProjectDesignOccurrence& occurrence) {
  auto resolved = leaf(project, occurrence);
  if (!resolved)
    return Result<GeometryAffineTransform>::failure(resolved.error());
  return Result<GeometryAffineTransform>::success(pose(resolved.value()));
}
Result<GeometryAffineTransform> project_occurrence_mapping(const Project& project,
                                                           const ProjectDesignOccurrence& source,
                                                           const ProjectDesignOccurrence& target) {
  auto from = project_occurrence_transform(project, source);
  if (!from)
    return from;
  auto to = project_occurrence_transform(project, target);
  if (!to)
    return to;
  auto inverse = to.value().inverse_similarity();
  if (!inverse)
    return inverse;
  return Result<GeometryAffineTransform>::success(from.value().followed_by(inverse.value()));
}

namespace detail {
Result<std::size_t> validate_project_feedback(const Project& project) {
  return validate_feedback(project);
}
Result<std::size_t> settle_project_pose(Project& p, StepImportTopologyContext* imports) {
  return solve_assembly(p, imports);
}
Result<Project> stage_adaptive_part(const Project& project, DocumentId part,
                                    StepImportTopologyContext* imports) {
  auto feedback = validate_feedback(project);
  if (!feedback)
    return Result<Project>::failure(feedback.error());
  Project candidate = project;
  for (const auto& ref : candidate.design_references())
    if (ref.driver().part == part) {
      auto applied = update(candidate, ref, imports);
      if (!applied)
        return Result<Project>::failure(applied.error());
    }
  return Result<Project>::success(std::move(candidate));
}
} // namespace detail

Result<AdaptiveProjectEvaluation> AdaptiveProjectEvaluator::evaluate(const Project& project) const {
  using R = Result<AdaptiveProjectEvaluation>;
  if (detail::project_evaluation_depth == 0 && !project.assembly_hole_features().empty()) {
    auto normalized = evaluate_project_geometry(project, imports_);
    if (!normalized)
      return R::failure(normalized.error());
    std::vector<DocumentId> changed;
    for (const auto& part : normalized.value().part_documents()) {
      auto before = serialize_part_document_to_json(*project.find_part_document(part.id()));
      auto after = serialize_part_document_to_json(part);
      if (!before)
        return R::failure(before.error());
      if (!after)
        return R::failure(after.error());
      if (before.value() != after.value())
        changed.push_back(part.id());
    }
    return R::success({std::move(normalized.value()), std::move(changed)});
  }
  detail::ProjectEvaluationGuard evaluation_guard;
  auto valid = project.validate_assembly_structure();
  if (!valid)
    return R::failure(valid.error());
  if (project.design_references().empty())
    return R::success({project, {}});
  auto order = project_design_dependency_order(project);
  if (!order)
    return R::failure(order.error());
  auto feedback = validate_feedback(project);
  if (!feedback)
    return R::failure(feedback.error());
  Project candidate = project;
  auto prepared_pose = solve_assembly(candidate, imports_);
  if (!prepared_pose)
    return R::failure(prepared_pose.error());
  std::map<std::string, GeometryAffineTransform> consumed_frames;
  for (const auto& ref : candidate.design_references()) {
    auto frame = mapping(candidate, ref);
    if (!frame)
      return R::failure(frame.error());
    consumed_frames.emplace(ref.id().value(), frame.value());
  }
  std::vector<DocumentId> changed;
  for (const auto& part : order.value()) {
    const auto* before = candidate.find_part_document(part);
    auto original = serialize_part_document_to_json(*before);
    if (!original)
      return R::failure(original.error());
    for (const auto& ref : candidate.design_references())
      if (ref.driver().part == part) {
        auto applied = update(candidate, ref, imports_);
        if (!applied)
          return R::failure(applied.error());
      }
    auto rebuilt = geometry(*candidate.find_part_document(part), imports_);
    if (!rebuilt)
      return R::failure(rebuilt.error());
    auto after = serialize_part_document_to_json(*candidate.find_part_document(part));
    if (!after)
      return R::failure(after.error());
    if (original.value() != after.value())
      changed.push_back(part);
  }
  auto resolved_pose = solve_assembly(candidate, imports_);
  if (!resolved_pose)
    return R::failure(resolved_pose.error());
  for (const auto& ref : candidate.design_references()) {
    auto frame = mapping(candidate, ref);
    if (!frame)
      return R::failure(frame.error());
    const auto& before = consumed_frames.at(ref.id().value()).matrix;
    for (std::size_t i = 0; i < before.size(); ++i)
      if (std::abs(before[i] - frame.value().matrix[i]) > 1e-7)
        return R::failure(error("Adaptive evaluation changed a consumed pose; no implicit "
                                "shape/pose feedback is permitted"));
  }
  // Recompute every requested visible/physical state through the same canonical
  // assembly consumers before returning any candidate to the caller.
  auto physical = BodyResultInspector{}.inspect_physical_properties(candidate, imports_);
  if (!physical)
    return R::failure(physical.error());
  auto posed = detail::build_posed_leaf_shapes(candidate, imports_,
                                               AssemblyLeafParticipation::PhysicalActive);
  if (!posed)
    return R::failure(posed.error());
  if (imports_) {
    auto fresh = imports_->validate_sources();
    if (!fresh)
      return R::failure(fresh.error());
  }
  return R::success({std::move(candidate), std::move(changed)});
}
Result<AdaptiveProjectEvaluation> AdaptiveProjectEvaluator::bind(const Project& project,
                                                                 ProjectDesignReference ref) const {
  Project candidate = project;
  auto prepared = update(candidate, ref, imports_);
  if (!prepared)
    return Result<AdaptiveProjectEvaluation>::failure(prepared.error());
  auto linked = candidate.set_design_reference(std::move(ref));
  if (!linked)
    return Result<AdaptiveProjectEvaluation>::failure(linked.error());
  auto checked = evaluate(candidate);
  if (!checked)
    return checked;
  for (const auto& part : checked.value().project.part_documents()) {
    const auto* original = project.find_part_document(part.id());
    auto old_json =
        original ? serialize_part_document_to_json(*original) : Result<std::string>::success({});
    auto new_json = serialize_part_document_to_json(part);
    if (!old_json)
      return Result<AdaptiveProjectEvaluation>::failure(old_json.error());
    if (!new_json)
      return Result<AdaptiveProjectEvaluation>::failure(new_json.error());
    if (old_json.value() != new_json.value() &&
        std::find(checked.value().changed_parts.begin(), checked.value().changed_parts.end(),
                  part.id()) == checked.value().changed_parts.end())
      checked.value().changed_parts.push_back(part.id());
  }
  return checked;
}
Result<Project> AdaptiveProjectEvaluator::detach(const Project& project,
                                                 ProjectDesignReferenceId id) const {
  auto checked = evaluate(project);
  if (!checked)
    return Result<Project>::failure(checked.error());
  auto detached = checked.value().project.detach_design_reference(std::move(id));
  if (!detached)
    return Result<Project>::failure(detached.error());
  return Result<Project>::success(std::move(checked.value().project));
}
} // namespace blcad::geometry
