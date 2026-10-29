#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "geometry_shape_internal.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <memory>

namespace blcad::geometry {
namespace {
constexpr double tolerance = 1e-7;
Vector3 unit(Vector3 v) {
  const double n = length(v);
  return {v.x / n, v.y / n, v.z / n};
}
Vector3 between(Point3 a, Point3 b) {
  return {b.x - a.x, b.y - a.y, b.z - a.z};
}
Point3 project(Point3 origin, const gp_Pln& plane) {
  const auto n = plane.Axis().Direction();
  const auto p = plane.Location();
  const double d =
      (origin.x - p.X()) * n.X() + (origin.y - p.Y()) * n.Y() + (origin.z - p.Z()) * n.Z();
  return {origin.x - d * n.X(), origin.y - d * n.Y(), origin.z - d * n.Z()};
}
Result<ResolvedWorkplane> frame_for(const DatumPlaneId& id, const detail::FaceLineage& entry) {
  BRepAdaptor_Surface surface(entry.face);
  if (surface.GetType() != GeomAbs_Plane)
    return Result<ResolvedWorkplane>::failure(
        Error::geometry(id.value(), "Lost result-face support: descendant is no longer planar"));
  const auto plane = surface.Plane();
  const auto direction = plane.Axis().Direction();
  Vector3 normal{direction.X(), direction.Y(), direction.Z()};
  if (entry.face.Orientation() == TopAbs_REVERSED)
    normal = {-normal.x, -normal.y, -normal.z};
  const double component = dot(entry.x_axis, normal);
  Vector3 x{entry.x_axis.x - component * normal.x, entry.x_axis.y - component * normal.y,
            entry.x_axis.z - component * normal.z};
  if (length(x) < tolerance)
    return Result<ResolvedWorkplane>::failure(Error::geometry(
        id.value(), "Lost result-face support: authored orientation is degenerate"));
  x = unit(x);
  return Result<ResolvedWorkplane>::success(
      {id, project(entry.frame_origin, plane), x, cross(normal, x), normal, {}});
}
} // namespace

GeometryShape ResultFaceSupportGeometry::tag_plane(const GeometryShape& shape, FeatureId producer,
                                                   std::string role, Point3 origin, Vector3 x_axis,
                                                   Vector3 normal) {
  if (shape.empty() || length(normal) < tolerance)
    return shape;
  normal = unit(normal);
  std::vector<TopoDS_Face> matches;
  for (TopExp_Explorer it(shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane)
      continue;
    const auto plane = surface.Plane();
    const auto n = plane.Axis().Direction();
    if (std::abs(std::abs(dot(normal, {n.X(), n.Y(), n.Z()})) - 1) > tolerance ||
        length(between(origin, project(origin, plane))) > tolerance)
      continue;
    matches.push_back(face);
  }
  // A split primitive cap cannot supply a unique root in the first place.
  if (matches.size() != 1)
    return shape;
  auto backing = std::make_shared<GeometryShape::Impl>(*shape.impl_);
  detail::append_lineage(backing->face_lineage,
                         {{std::move(producer), std::move(role)}, matches.front(), origin, x_axis},
                         backing->shape);
  return GeometryShape(std::move(backing));
}

GeometryShape ResultFaceSupportGeometry::tag_prismatic_caps(const GeometryShape& shape,
                                                            FeatureId producer,
                                                            const ResolvedWorkplane& frame) {
  if (shape.empty())
    return shape;
  std::vector<double> positions;
  for (TopExp_Explorer it(shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
    if (surface.GetType() != GeomAbs_Plane)
      continue;
    const auto plane = surface.Plane();
    const auto n = plane.Axis().Direction();
    if (std::abs(std::abs(dot(frame.normal, {n.X(), n.Y(), n.Z()})) - 1) > tolerance)
      continue;
    positions.push_back(dot(between(frame.origin, project(frame.origin, plane)), frame.normal));
  }
  if (positions.size() != 2)
    return shape;
  std::sort(positions.begin(), positions.end());
  GeometryShape result = shape;
  for (std::size_t i = 0; i < 2; ++i) {
    const auto d = positions[i];
    result = tag_plane(result, producer, i == 0 ? "extrude/lower-cap" : "extrude/upper-cap",
                       {frame.origin.x + d * frame.normal.x, frame.origin.y + d * frame.normal.y,
                        frame.origin.z + d * frame.normal.z},
                       frame.x_axis, frame.normal);
  }
  return result;
}

GeometryShape ResultFaceSupportGeometry::scope_copy(const GeometryShape& shape, FeatureId producer,
                                                    std::string instance) {
  if (shape.empty())
    return shape;
  auto backing = std::make_shared<GeometryShape::Impl>(*shape.impl_);
  const auto segment = [](const std::string& value) {
    return std::to_string(value.size()) + ":" + value + "/";
  };
  for (auto& entry : backing->face_lineage) {
    entry.origin.role = "copy/" + segment(instance) + segment(entry.origin.feature.value()) +
                        segment(entry.origin.role);
    entry.origin.feature = producer;
  }
  return GeometryShape(std::move(backing));
}

Result<ResolvedWorkplane>
ResultFaceSupportGeometry::resolve(const PartDocument& document, const ResultFaceWorkplane& plane,
                                   const ShapeCache& cache,
                                   StepImportTopologyContext* import_context) {
  auto valid = validate_result_face_workplane_reference(document, plane);
  if (!valid)
    return Result<ResolvedWorkplane>::failure(valid.error());
  for (const auto& base : document.imported_body_features()) {
    const bool owns_body =
        std::any_of(base.definition().outputs().begin(), base.definition().outputs().end(),
                    [&](const auto& output) { return output.body == plane.reference().body(); });
    if (!owns_body)
      continue;
    auto owned = cache.validated_imported_body_result(document, base);
    if (!owned)
      return Result<ResolvedWorkplane>::failure(owned.error());
  }
  if (import_context) {
    auto observed = observe_imported_part_sources(document, import_context);
    if (!observed)
      return Result<ResolvedWorkplane>::failure(observed.error());
  }
  const auto* shape = cache.find_feature_shape(plane.reference().producer());
  const auto fail = [&](std::string message) {
    return Result<ResolvedWorkplane>::failure(
        Error::geometry(plane.id().value(), std::move(message)));
  };
  if (!shape || shape->empty())
    return fail("Lost result-face support: captured producer has no recomputed shape");
  std::vector<detail::FaceLineage> matches;
  for (const auto& origin : plane.reference().origins()) {
    bool found = false;
    for (const auto& entry : shape->impl_->face_lineage)
      if (entry.origin == origin) {
        found = true;
        if (std::none_of(matches.begin(), matches.end(),
                         [&](const auto& old) { return old.face.IsSame(entry.face); }))
          matches.push_back(entry);
      }
    if (!found)
      return fail("Lost result-face support: an authored face contributor was deleted");
  }
  if (matches.size() != 1)
    return fail("Ambiguous result-face support: authored face split into multiple descendants");
  return frame_for(plane.id(), matches.front());
}

Result<std::vector<ResultFaceSelectionTarget>>
ResultFaceSupportGeometry::selectable_faces(const PartDocument& document, const ShapeCache& cache,
                                            const BodyId& body,
                                            StepImportTopologyContext* import_context) {
  const auto* model_body = document.find_body(body);
  const auto* result = cache.find_body_result(body);
  if (!model_body || model_body->kind() != BodyKind::Solid ||
      model_body->visibility() != BodyVisibility::Visible || !result || result->shape.empty())
    return Result<std::vector<ResultFaceSelectionTarget>>::failure(Error::validation(
        body.value(), "Face-supported sketches require a visible recomputed solid Body"));
  auto aliases = FinishingSelectionBuilder::build(
      document, cache, body, PartFeatureInputRole::ShellRemovalFace, import_context);
  std::vector<ResultFaceSelectionTarget> targets;
  for (TopExp_Explorer it(result->shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane)
      continue;
    std::vector<ResultFaceOrigin> origins;
    for (const auto& entry : result->shape.impl_->face_lineage)
      if (entry.face.IsSame(face))
        origins.push_back(entry.origin);
    auto reference =
        ResultFaceReference::create(result->source_feature_id, body, std::move(origins));
    if (!reference)
      continue;
    auto plane =
        ResultFaceWorkplane::create(DatumPlaneId("selection.face"), "Face", reference.value());
    if (!plane || !resolve(document, plane.value(), cache, import_context))
      continue;
    std::string semantic = "support/" + reference.value().identity();
    if (aliases)
      for (const auto& alias : aliases.value())
        if (alias.shape.impl_->shape.IsSame(face)) {
          semantic = alias.semantic_id;
          break;
        }
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    const auto p = properties.CentreOfMass();
    targets.push_back({std::move(semantic),
                       "Result face / " + reference.value().origins().front().role,
                       reference.value(),
                       GeometryShape(std::make_shared<GeometryShape::Impl>(face)),
                       {p.X(), p.Y(), p.Z()}});
  }
  return Result<std::vector<ResultFaceSelectionTarget>>::success(std::move(targets));
}
} // namespace blcad::geometry
