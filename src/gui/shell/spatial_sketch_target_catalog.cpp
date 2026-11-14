#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "spatial_sketch_task_context.hpp"
#include <algorithm>
#include <map>
namespace blcad::gui {
Result<std::vector<SpatialSketchTargetChoice>>
SpatialSketchTaskContext::constraint_targets(const Sketch3D& owner) const {
  std::vector<SpatialSketchTargetChoice> result;
  auto query = queries();
  auto ready = query.prepare_candidate(owner);
  if (!ready)
    return Result<std::vector<SpatialSketchTargetChoice>>::failure(ready.error());
  const auto descendants = part_.dependency_graph().transitive_dependents(owner.id().value());
  const auto allowed = [&](const auto& target) {
    auto node = target.referenced_node_id();
    return !node || (*node != owner.id().value() &&
                     std::find(descendants.begin(), descendants.end(), *node) == descendants.end());
  };
  std::map<std::string, geometry::GeometryShape> shapes;
  auto scene = geometry::ViewportSceneBuilder{imports_.get()}.build_part(part_, cache_, prefix_);
  if (!scene)
    return Result<std::vector<SpatialSketchTargetChoice>>::failure(scene.error());
  for (auto& item : scene.value())
    shapes.emplace(item.semantic_id, item.shape);
  auto local = geometry::Sketch3DGeometryAdapter{}.convert(part_, owner);
  if (!local)
    return Result<std::vector<SpatialSketchTargetChoice>>::failure(local.error());
  for (const auto& product : local.value().products())
    shapes.insert_or_assign(
        "sketch3d/" + owner.id().value() + "/entity/" + product.entity_id.value(), product.shape);
  const auto add = [&](Result<SpatialConstraintTarget> target, std::string label,
                       std::string semantic, bool line = false, bool radius = false,
                       geometry::GeometryShape shape = {}) {
    if (!target || !allowed(target.value()))
      return;
    const auto& t = target.value();
    if (const auto* p = std::get_if<SketchPointReference3D>(&t.source()); p && !p->local_point()) {
      if (!query.external_point(*p))
        return;
    }
    if (const auto* axis = std::get_if<AxisReference>(&t.source()); axis && !query.axis(*axis))
      return;
    if (const auto* plane = std::get_if<PlaneReference>(&t.source()); plane && !query.plane(*plane))
      return;
    if (t.kind() == SpatialTargetKind::Curve) {
      const double u = t.curve_location() == SpatialCurveLocation::Start ? 0
                       : t.curve_location() == SpatialCurveLocation::End ? 1
                                                                         : .37;
      if (!query.curve(owner, t, u))
        return;
    }
    if (shape.empty()) {
      const auto found = shapes.find(semantic);
      if (found != shapes.end())
        shape = found->second;
    }
    result.push_back({std::move(label), std::move(semantic), std::move(target.value()), line,
                      radius, std::move(shape)});
  };
  for (const auto& p : owner.points())
    add(SpatialConstraintTarget::local_point(p.id()), "Local point / " + p.id().value(),
        "sketch3d/" + owner.id().value() + "/entity/" + p.id().value());
  for (const auto& p : part_.construction_points()) {
    auto ref = SketchPointReference3D::create_construction_point(p.id());
    if (ref)
      add(SpatialConstraintTarget::point(ref.value()), "Linked point / " + p.name(),
          "construction-point/" + p.id().value());
  }
  const auto curve = [&](const Sketch3D& s, const auto& records, Sketch3DPathCurveKind kind) {
    for (const auto& c : records)
      for (auto location :
           {SpatialCurveLocation::Whole, SpatialCurveLocation::Start, SpatialCurveLocation::End}) {
        const auto semantic = "sketch3d/" + s.id().value() + "/entity/" + c.id().value();
        const auto label =
            (s.id() == owner.id() ? "Local curve / " : "Linked curve / " + s.id().value() + " / ") +
            c.id().value() + " / " + std::string(to_string(location));
        if (s.id() == owner.id())
          add(SpatialConstraintTarget::curve(c.id(), location), label, semantic,
              kind == Sketch3DPathCurveKind::Line,
              kind == Sketch3DPathCurveKind::Arc || kind == Sketch3DPathCurveKind::Helix);
        else {
          auto ref = PathSegmentReference::create_sketch_3d(s.id(), c.id(), kind);
          if (ref)
            add(SpatialConstraintTarget::external_curve(ref.value(), location), label, semantic,
                kind == Sketch3DPathCurveKind::Line,
                kind == Sketch3DPathCurveKind::Arc || kind == Sketch3DPathCurveKind::Helix);
        }
      }
  };
  const auto owner_curves = [&](const Sketch3D& s) {
    curve(s, s.lines(), Sketch3DPathCurveKind::Line);
    curve(s, s.polylines(), Sketch3DPathCurveKind::Polyline);
    curve(s, s.arcs(), Sketch3DPathCurveKind::Arc);
    curve(s, s.splines(), Sketch3DPathCurveKind::Spline);
    curve(s, s.helices(), Sketch3DPathCurveKind::Helix);
    curve(s, s.equation_curves(), Sketch3DPathCurveKind::EquationCurve);
  };
  owner_curves(owner);
  for (const auto& s : part_.sketches_3d())
    if (s.id() != owner.id())
      owner_curves(s);
  for (const auto& s : part_.sketches()) {
    const auto planar = [&](const auto& records, PlanarPathCurveKind kind) {
      for (const auto& c : records)
        for (auto location : {SpatialCurveLocation::Whole, SpatialCurveLocation::Start,
                              SpatialCurveLocation::End}) {
          auto ref = PathSegmentReference::create_planar(s.id(), c.id(), kind);
          if (ref)
            add(SpatialConstraintTarget::external_curve(ref.value(), location),
                "Linked planar / " + s.id().value() + " / " + c.id().value() + " / " +
                    std::string(to_string(location)),
                "sketch/" + s.id().value() + "/entity/" + c.id().value(),
                kind == PlanarPathCurveKind::Line, kind == PlanarPathCurveKind::Arc);
        }
    };
    planar(s.line_segments(), PlanarPathCurveKind::Line);
    planar(s.arc_segments(), PlanarPathCurveKind::Arc);
    planar(s.spline_segments(), PlanarPathCurveKind::Spline);
    planar(s.equation_curves(), PlanarPathCurveKind::EquationCurve);
    for (const auto& p : s.projected_points()) {
      auto target = SketchReferenceTarget::create_projected_point(p.id());
      if (!target)
        continue;
      auto ref = SketchPointReference3D::create_planar_sketch_point(s.id(), target.value());
      if (ref)
        add(SpatialConstraintTarget::point(ref.value()),
            "Linked planar point / " + s.id().value() + " / " + p.id().value(),
            "sketch/" + s.id().value() + "/entity/" + p.id().value());
    }
  }
  for (const auto& a : part_.datum_axes()) {
    auto ref = AxisReference::create_datum_axis(PartFeatureInputRole::SpatialConstraint, a.id());
    if (ref)
      add(SpatialConstraintTarget::axis(ref.value()), "Axis / " + a.name(),
          "datum-axis/" + a.id().value(), true);
  }
  for (const auto& a : part_.construction_lines()) {
    auto ref = AxisReference::create_construction_line(PartFeatureInputRole::SpatialConstraint,
                                                       a.id(), PartFeatureInputCapability::Line);
    if (ref)
      add(SpatialConstraintTarget::axis(ref.value()), "Axis / " + a.name(),
          "construction-line/" + a.id().value(), true);
  }
  const auto planes = [&](const auto& records) {
    for (const auto& p : records) {
      auto ref =
          PlaneReference::create_datum_plane(PartFeatureInputRole::SpatialConstraint, p.id());
      if (ref)
        add(SpatialConstraintTarget::plane(ref.value()), "Plane / " + p.name(),
            "datum-plane/" + p.id().value());
    }
  };
  planes(part_.datum_planes());
  planes(part_.derived_workplanes());
  for (const auto& p : part_.construction_planes()) {
    auto ref =
        PlaneReference::create_construction_plane(PartFeatureInputRole::SpatialConstraint, p.id());
    if (ref)
      add(SpatialConstraintTarget::plane(ref.value()), "Plane / " + p.name(),
          "construction-plane/" + p.id().value());
  }
  for (const auto& body : part_.bodies()) {
    if (!cache_.find_body_shape(body.id()))
      continue;
    const auto mapped = geometry::FinishingSelectionBuilder::build(
        part_, cache_, body.id(), PartFeatureInputRole::ShellRemovalFace, imports_.get(), prefix_,
        geometry::FinishingSelectionVisibility::IncludeHiddenInputs);
    if (mapped)
      for (const auto& item : mapped.value())
        if (const auto* face = std::get_if<FaceReference>(&item.reference)) {
          const auto retagged = std::visit(
              [](const auto& source) -> Result<FaceReference> {
                using T = std::decay_t<decltype(source)>;
                if constexpr (std::is_same_v<T, SemanticFaceReference>)
                  return FaceReference::create_planar(PartFeatureInputRole::SpatialConstraint,
                                                      source);
                else if constexpr (std::is_same_v<T, SemanticCylindricalFaceReference>)
                  return FaceReference::create_cylindrical(PartFeatureInputRole::SpatialConstraint,
                                                           source);
                else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
                  return FaceReference::create_imported(PartFeatureInputRole::SpatialConstraint,
                                                        source);
                else
                  return FaceReference::create_result(PartFeatureInputRole::SpatialConstraint,
                                                      source);
              },
              face->source());
          if (retagged)
            add(SpatialConstraintTarget::face(retagged.value()),
                "Face / " + body.name() + " / " + item.label, item.semantic_id, false, false,
                item.shape);
        }
    if (body.kind() == BodyKind::Surface) {
      auto faces = geometry::ResultFaceSupportGeometry::selectable_faces(part_, cache_, body.id(),
                                                                         imports_.get(), prefix_);
      if (faces)
        for (const auto& item : faces.value()) {
          auto ref =
              FaceReference::create_result(PartFeatureInputRole::SpatialConstraint, item.reference);
          if (ref)
            add(SpatialConstraintTarget::face(ref.value()),
                "Surface face / " + body.name() + " / " + item.label, item.semantic_id, false,
                false, item.shape);
        }
    }
  }
  return Result<std::vector<SpatialSketchTargetChoice>>::success(std::move(result));
}
} // namespace blcad::gui
