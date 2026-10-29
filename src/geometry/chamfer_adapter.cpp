#include "blcad/geometry/chamfer_adapter.hpp"

#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include "geometry_shape_internal.hpp"
#include "semantic_edge_resolution.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;
constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Error chamfer_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}
[[nodiscard]] Result<Vector3> preferred_linear_face_normal(const PartDocument& document,
                                                           const SemanticEdgeReference& reference,
                                                           const FeatureId& id) {
  const Feature* source = document.find_feature(reference.source_feature());
  const Sketch* sketch = source == nullptr ? nullptr : document.find_sketch(source->input_sketch());
  if (source == nullptr || sketch == nullptr)
    return Result<Vector3>::failure(
        chamfer_error(id, "linear chamfer edge source sketch must exist"));
  auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *sketch);
  if (workplane.has_error())
    return Result<Vector3>::failure(workplane.error());
  const auto negate = [](Vector3 value) { return Vector3{-value.x, -value.y, -value.z}; };
  switch (reference.edge()) {
  case SemanticEdge::TopFront:
  case SemanticEdge::TopBack:
  case SemanticEdge::TopRight:
  case SemanticEdge::TopLeft:
    return Result<Vector3>::success(workplane.value().normal);
  case SemanticEdge::BottomFront:
  case SemanticEdge::BottomBack:
  case SemanticEdge::BottomRight:
  case SemanticEdge::BottomLeft:
    return Result<Vector3>::success(negate(workplane.value().normal));
  case SemanticEdge::FrontRight:
  case SemanticEdge::FrontLeft:
    return Result<Vector3>::success(workplane.value().y_axis);
  case SemanticEdge::BackRight:
  case SemanticEdge::BackLeft:
    return Result<Vector3>::success(negate(workplane.value().y_axis));
  }
  return Result<Vector3>::failure(chamfer_error(id, "unsupported linear chamfer edge"));
}

[[nodiscard]] Result<TopoDS_Face>
select_distance_face(const PartDocument& document, const TopoDS_Shape& target,
                     const EdgeReference& reference, const TopoDS_Edge& edge, const FeatureId& id) {
  TopTools_IndexedDataMapOfShapeListOfShape edge_faces;
  TopExp::MapShapesAndAncestors(target, TopAbs_EDGE, TopAbs_FACE, edge_faces);
  if (!edge_faces.Contains(edge))
    return Result<TopoDS_Face>::failure(
        chamfer_error(id, "chamfer edge has no adjacent target face"));

  std::vector<TopoDS_Face> planar_faces;
  for (TopTools_ListIteratorOfListOfShape iterator(edge_faces.FindFromKey(edge)); iterator.More();
       iterator.Next()) {
    const TopoDS_Face face = TopoDS::Face(iterator.Value());
    if (BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane)
      planar_faces.push_back(face);
  }
  if (std::holds_alternative<ResultEdgeReference>(reference.source())) {
    // Distance modes use a deterministic geometric side, preferring a planar
    // face. The witness prevents silently rebinding after the selected curve
    // changes. Equal-distance mode does not need a side at all.
    std::vector<TopoDS_Face> faces = planar_faces;
    if (faces.empty())
      for (TopTools_ListIteratorOfListOfShape it(edge_faces.FindFromKey(edge)); it.More();
           it.Next())
        faces.push_back(TopoDS::Face(it.Value()));
    const auto key = [](const TopoDS_Face& face) {
      GProp_GProps properties;
      BRepGProp::SurfaceProperties(face, properties);
      auto center = properties.CentreOfMass();
      return std::array<double, 4>{center.X(), center.Y(), center.Z(), properties.Mass()};
    };
    std::sort(faces.begin(), faces.end(),
              [&](const auto& a, const auto& b) { return key(a) < key(b); });
    if (faces.empty() || (faces.size() > 1 && key(faces[0]) == key(faces[1])))
      return Result<TopoDS_Face>::failure(
          chamfer_error(id, "result edge reference face is ambiguous"));
    return Result<TopoDS_Face>::success(faces.front());
  }
  if (planar_faces.empty())
    return Result<TopoDS_Face>::failure(
        chamfer_error(id, "distance chamfer requires an adjacent planar reference face"));

  if (std::holds_alternative<SemanticCircularEdgeReference>(reference.source())) {
    if (planar_faces.size() != 1U)
      return Result<TopoDS_Face>::failure(
          chamfer_error(id, "circular chamfer reference face is ambiguous"));
    return Result<TopoDS_Face>::success(planar_faces.front());
  }

  auto preferred = preferred_linear_face_normal(
      document, std::get<SemanticEdgeReference>(reference.source()), id);
  if (preferred.has_error())
    return Result<TopoDS_Face>::failure(preferred.error());
  std::vector<TopoDS_Face> matches;
  for (const TopoDS_Face& face : planar_faces) {
    const gp_Dir normal = BRepAdaptor_Surface(face).Plane().Axis().Direction();
    const Vector3 value{normal.X(), normal.Y(), normal.Z()};
    if (std::abs(std::abs(dot(value, preferred.value())) - 1.0) <= kTolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1U)
    return Result<TopoDS_Face>::failure(
        chamfer_error(id, matches.empty() ? "semantic chamfer reference face no longer exists"
                                          : "semantic chamfer reference face is ambiguous"));
  return Result<TopoDS_Face>::success(matches.front());
}
} // namespace

Result<GeometryShape> ChamferAdapter::apply(const PartDocument& document,
                                            const ChamferFeature& feature,
                                            const GeometryShape& target,
                                            const ShapeCache& shape_cache, double first_value,
                                            std::optional<double> second_value) const {
  if (target.empty() || !std::isfinite(first_value) || first_value <= 0.0 ||
      (second_value.has_value() && (!std::isfinite(*second_value) || *second_value <= 0.0)))
    return Result<GeometryShape>::failure(
        chamfer_error(feature.id(), "chamfer target and positive finite parameters are required"));
  if ((feature.mode() == ChamferMode::EqualDistance) != !second_value.has_value())
    return Result<GeometryShape>::failure(
        chamfer_error(feature.id(), "chamfer parameter count does not match its mode"));

  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    BRepFilletAPI_MakeChamfer builder(target_shape);
    for (const EdgeReference& reference : feature.edges()) {
      auto edge = resolve_semantic_edge(document, shape_cache, target_shape, reference,
                                        feature.id(), "chamfer");
      if (edge.has_error())
        return Result<GeometryShape>::failure(edge.error());
      if (feature.mode() == ChamferMode::EqualDistance) {
        builder.Add(first_value, edge.value());
        continue;
      }
      auto face =
          select_distance_face(document, target_shape, reference, edge.value(), feature.id());
      if (face.has_error())
        return Result<GeometryShape>::failure(face.error());
      if (feature.mode() == ChamferMode::TwoDistance)
        builder.Add(first_value, *second_value, edge.value(), face.value());
      else
        builder.AddDA(first_value, *second_value * kPi / 180.0, edge.value(), face.value());
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<GeometryShape>::failure(
          chamfer_error(feature.id(), "OCCT chamfer did not complete; distance may be too large"));
    TopoDS_Shape result = builder.Shape();
    if (result.IsNull() || !TopExp_Explorer(result, TopAbs_SOLID).More() ||
        !BRepCheck_Analyzer(result, true).IsValid())
      return Result<GeometryShape>::failure(
          chamfer_error(feature.id(), "chamfer produced an invalid solid result"));
    auto backing = std::make_shared<GeometryShape::Impl>(std::move(result));
    backing->face_lineage =
        detail::map_lineage(builder, backing->shape, target.impl_->face_lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(chamfer_error(
        feature.id(), message == nullptr ? "OCCT chamfer failed" : std::string(message)));
  }
}

} // namespace blcad::geometry
