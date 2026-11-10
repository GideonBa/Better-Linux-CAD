#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "geometry_shape_internal.hpp"
#include "part_history_evaluation.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
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
                                                   Vector3 normal, bool selectable) {
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
  detail::append_lineage(
      backing->face_lineage,
      {{std::move(producer), std::move(role)}, matches.front(), origin, x_axis, selectable},
      backing->shape);
  return GeometryShape(std::move(backing));
}

GeometryShape ResultFaceSupportGeometry::tag_conical_wall(const GeometryShape& shape,
                                                          FeatureId producer, std::string role,
                                                          Point3 center, Vector3 axis,
                                                          Vector3 x_axis, double first_depth,
                                                          double last_depth) {
  if (shape.empty() || length(axis) < tolerance)
    return shape;
  axis = unit(axis);
  std::vector<TopoDS_Face> matches;
  for (TopExp_Explorer it(shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Cone)
      continue;
    const auto cone = surface.Cone();
    const auto n = cone.Axis().Direction();
    const auto location = cone.Location();
    const Vector3 delta{location.X() - center.x, location.Y() - center.y, location.Z() - center.z};
    const auto mid = surface.Value((surface.FirstUParameter() + surface.LastUParameter()) / 2,
                                   (surface.FirstVParameter() + surface.LastVParameter()) / 2);
    const double z = dot({mid.X() - center.x, mid.Y() - center.y, mid.Z() - center.z}, axis);
    if (std::abs(std::abs(dot(axis, {n.X(), n.Y(), n.Z()})) - 1) <= tolerance &&
        length(cross(delta, axis)) <= tolerance && z > first_depth + tolerance &&
        z < last_depth - tolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1)
    return shape;
  auto backing = std::make_shared<GeometryShape::Impl>(*shape.impl_);
  detail::append_lineage(backing->face_lineage,
                         {{std::move(producer), std::move(role)}, matches.front(), center, x_axis},
                         backing->shape);
  return GeometryShape(std::move(backing));
}

GeometryShape ResultFaceSupportGeometry::tag_hole_wall(const GeometryShape& shape,
                                                       FeatureId producer, std::string role,
                                                       Point3 center, Vector3 axis, Vector3 x_axis,
                                                       double radius_mm) {
  if (shape.empty() || length(axis) < tolerance)
    return shape;
  axis = unit(axis);
  std::vector<TopoDS_Face> matches;
  for (TopExp_Explorer it(shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Cylinder)
      continue;
    const auto cylinder = surface.Cylinder();
    const auto n = cylinder.Axis().Direction();
    const auto p = cylinder.Location();
    const Vector3 offset{center.x - p.X(), center.y - p.Y(), center.z - p.Z()};
    if (std::abs(cylinder.Radius() - radius_mm) <= tolerance &&
        std::abs(std::abs(dot(axis, {n.X(), n.Y(), n.Z()})) - 1) <= tolerance &&
        length(cross(offset, axis)) <= tolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1)
    return shape;
  auto backing = std::make_shared<GeometryShape::Impl>(*shape.impl_);
  detail::append_lineage(backing->face_lineage,
                         {{std::move(producer), std::move(role)}, matches.front(), center, x_axis},
                         backing->shape);
  return GeometryShape(std::move(backing));
}

Result<GeometryShape>
ResultFaceSupportGeometry::resolve_face(const PartDocument& document, const ShapeCache& cache,
                                        const ResultFaceReference& reference,
                                        StepImportTopologyContext* import_context) {
  auto valid = validate_result_face_reference(document, reference);
  if (!valid)
    return Result<GeometryShape>::failure(valid.error());
  const auto history = detail::validate_history_input(document, cache);
  if (!history)
    return Result<GeometryShape>::failure(history.error());
  for (const auto& base : document.imported_body_features())
    for (const auto& output : base.definition().outputs())
      if (output.body == reference.body()) {
        auto owned = cache.validated_imported_body_result(document, base);
        if (!owned)
          return Result<GeometryShape>::failure(owned.error());
      }
  auto fresh = observe_imported_part_sources(document, import_context);
  if (!fresh)
    return Result<GeometryShape>::failure(fresh.error());
  const auto fail = [&](const char* message) {
    return Result<GeometryShape>::failure(Error::geometry(reference.producer().value(), message));
  };
  if (!detail::validate_history_source(document, reference.producer().value(), &cache))
    return fail("Lost result face: captured producer is unavailable in this history scope");
  const auto* shape = cache.find_body_shape(reference.body());
  if (!shape || shape->empty())
    return fail("Lost result face: current Body is unavailable");
  std::vector<TopoDS_Face> matches;
  for (const auto& origin : reference.origins()) {
    bool found = false;
    for (const auto& entry : shape->impl_->face_lineage) {
      if (entry.origin != origin)
        continue;
      found = true;
      if (std::none_of(matches.begin(), matches.end(),
                       [&](const auto& face) { return face.IsSame(entry.face); }))
        matches.push_back(entry.face);
    }
    if (!found)
      return fail("Lost result face: an authored contributor was deleted");
  }
  if (matches.size() != 1)
    return fail("Ambiguous result face: authored face has multiple descendants");
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(matches.front())));
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

GeometryShape ResultFaceSupportGeometry::tag_rectangular_supports(const GeometryShape& shape,
                                                                  FeatureId producer,
                                                                  const ResolvedWorkplane& frame,
                                                                  Point2 center, double width_mm,
                                                                  double height_mm) {
  if (shape.empty())
    return shape;
  std::vector<double> caps;
  for (TopExp_Explorer it(shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    BRepAdaptor_Surface surface(TopoDS::Face(it.Current()));
    if (surface.GetType() != GeomAbs_Plane)
      continue;
    const auto plane = surface.Plane();
    const auto n = plane.Axis().Direction();
    if (std::abs(std::abs(dot(frame.normal, {n.X(), n.Y(), n.Z()})) - 1) <= tolerance)
      caps.push_back(dot(between(frame.origin, project(frame.origin, plane)), frame.normal));
  }
  if (caps.size() != 2)
    return shape;
  std::sort(caps.begin(), caps.end());
  const auto point = [&](double x, double y, double z) {
    return Point3{frame.origin.x + frame.x_axis.x * x + frame.y_axis.x * y + frame.normal.x * z,
                  frame.origin.y + frame.x_axis.y * x + frame.y_axis.y * y + frame.normal.y * z,
                  frame.origin.z + frame.x_axis.z * x + frame.y_axis.z * y + frame.normal.z * z};
  };
  const auto neg = [](Vector3 v) { return Vector3{-v.x, -v.y, -v.z}; };
  const double mid = (caps.front() + caps.back()) / 2;
  GeometryShape result = shape;
  const auto tag = [&](const char* role, Point3 origin, Vector3 x, Vector3 normal) {
    result = tag_plane(result, producer, std::string("hole-support/rectangle/") + role, origin, x,
                       normal, false);
  };
  tag("top", point(center.x, center.y, caps.back()), frame.x_axis, frame.normal);
  tag("bottom", point(center.x, center.y, caps.front()), frame.x_axis, neg(frame.normal));
  tag("right", point(center.x + width_mm / 2, center.y, mid), frame.y_axis, frame.x_axis);
  tag("left", point(center.x - width_mm / 2, center.y, mid), neg(frame.y_axis), neg(frame.x_axis));
  tag("front", point(center.x, center.y + height_mm / 2, mid), neg(frame.x_axis), frame.y_axis);
  tag("back", point(center.x, center.y - height_mm / 2, mid), frame.x_axis, neg(frame.y_axis));
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
  const auto history = detail::validate_history_input(document, cache);
  if (!history)
    return Result<ResolvedWorkplane>::failure(history.error());
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

Result<ResolvedResultFace> ResultFaceSupportGeometry::resolve_current(
    const PartDocument& document, const ShapeCache& cache, const BodyId& body,
    const std::vector<ResultFaceOrigin>& origins, StepImportTopologyContext* import_context) {
  const auto history = detail::validate_history_input(document, cache);
  if (!history)
    return Result<ResolvedResultFace>::failure(history.error());
  for (const auto& base : document.imported_body_features())
    for (const auto& output : base.definition().outputs())
      if (output.body == body) {
        auto owned = cache.validated_imported_body_result(document, base);
        if (!owned)
          return Result<ResolvedResultFace>::failure(owned.error());
      }
  if (import_context) {
    auto observed = observe_imported_part_sources(document, import_context);
    if (!observed)
      return Result<ResolvedResultFace>::failure(observed.error());
  }
  const auto* shape = cache.find_body_shape(body);
  const auto fail = [&](const char* message) {
    return Result<ResolvedResultFace>::failure(Error::geometry(body.value(), message));
  };
  if (!shape || shape->empty() || origins.empty())
    return fail("Lost current face support: Body or contributors are unavailable");
  std::vector<detail::FaceLineage> matches;
  for (const auto& origin : origins) {
    bool found = false;
    for (const auto& entry : shape->impl_->face_lineage)
      if (entry.origin == origin) {
        found = true;
        if (std::none_of(matches.begin(), matches.end(),
                         [&](const auto& old) { return old.face.IsSame(entry.face); }))
          matches.push_back(entry);
      }
    if (!found)
      return fail("Lost current face support: an authored contributor was deleted");
  }
  if (matches.size() != 1)
    return fail("Ambiguous current face support: authored face has multiple descendants");
  auto frame = frame_for(DatumPlaneId(body.value()), matches.front());
  if (!frame)
    return Result<ResolvedResultFace>::failure(frame.error());
  return Result<ResolvedResultFace>::success(
      {frame.value(), GeometryShape(std::make_shared<GeometryShape::Impl>(matches.front().face))});
}

Result<std::vector<ResultFaceSelectionTarget>> ResultFaceSupportGeometry::selectable_faces(
    const PartDocument& document, const ShapeCache& cache, const BodyId& body,
    StepImportTopologyContext* import_context, std::optional<std::size_t> history_prefix) {
  const auto history = detail::validate_history_modeling_scope(document, cache, history_prefix);
  if (!history)
    return Result<std::vector<ResultFaceSelectionTarget>>::failure(history.error());
  const auto* model_body = document.find_body(body);
  const auto* result = cache.find_body_result(body);
  if (!model_body || model_body->kind() != BodyKind::Solid ||
      model_body->visibility() != BodyVisibility::Visible || !result || result->shape.empty())
    return Result<std::vector<ResultFaceSelectionTarget>>::failure(Error::validation(
        body.value(), "Face-supported sketches require a visible recomputed solid Body"));
  auto aliases = FinishingSelectionBuilder::build(document, cache, body,
                                                  PartFeatureInputRole::ShellRemovalFace,
                                                  import_context, history_prefix);
  std::vector<ResultFaceSelectionTarget> targets;
  for (TopExp_Explorer it(result->shape.impl_->shape, TopAbs_FACE); it.More(); it.Next()) {
    const auto face = TopoDS::Face(it.Current());
    if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane)
      continue;
    std::vector<ResultFaceOrigin> origins;
    for (const auto& entry : result->shape.impl_->face_lineage)
      if (entry.selectable && entry.face.IsSame(face))
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
    std::string label = "Result face / " + reference.value().origins().front().role;
    if (aliases)
      for (const auto& alias : aliases.value())
        if (alias.shape.impl_->shape.IsSame(face)) {
          semantic = alias.semantic_id;
          label = alias.label;
          break;
        }
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    const auto p = properties.CentreOfMass();
    targets.push_back({std::move(semantic),
                       std::move(label),
                       reference.value(),
                       GeometryShape(std::make_shared<GeometryShape::Impl>(face)),
                       {p.X(), p.Y(), p.Z()}});
  }
  return Result<std::vector<ResultFaceSelectionTarget>>::success(std::move(targets));
}
} // namespace blcad::geometry
