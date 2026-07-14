#include "blcad/geometry/draft_adapter.hpp"

#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;
constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Error draft_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

[[nodiscard]] double dot(Vector3 a, Vector3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] double length(Vector3 value) noexcept {
  return std::sqrt(dot(value, value));
}

[[nodiscard]] Vector3 normalized(Vector3 value) noexcept {
  const double magnitude = length(value);
  return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

struct ExpectedPlane {
  Point3 origin;
  Vector3 normal;
};

[[nodiscard]] std::array<SemanticVertex, 3> face_vertices(SemanticFace face) noexcept {
  switch (face) {
  case SemanticFace::Top:
    return {SemanticVertex::TopFrontRight, SemanticVertex::TopFrontLeft,
            SemanticVertex::TopBackRight};
  case SemanticFace::Bottom:
    return {SemanticVertex::BottomFrontRight, SemanticVertex::BottomBackRight,
            SemanticVertex::BottomFrontLeft};
  case SemanticFace::Right:
    return {SemanticVertex::BottomFrontRight, SemanticVertex::TopFrontRight,
            SemanticVertex::BottomBackRight};
  case SemanticFace::Left:
    return {SemanticVertex::BottomFrontLeft, SemanticVertex::BottomBackLeft,
            SemanticVertex::TopFrontLeft};
  case SemanticFace::Front:
    return {SemanticVertex::BottomFrontRight, SemanticVertex::BottomFrontLeft,
            SemanticVertex::TopFrontRight};
  case SemanticFace::Back:
    return {SemanticVertex::BottomBackRight, SemanticVertex::TopBackRight,
            SemanticVertex::BottomBackLeft};
  }
  return {SemanticVertex::TopFrontRight, SemanticVertex::TopFrontLeft,
          SemanticVertex::TopBackRight};
}

[[nodiscard]] Result<ExpectedPlane> expected_plane(const PartDocument& document,
                                                   const ShapeCache& cache,
                                                   const SemanticFaceReference& reference,
                                                   const FeatureId& id) {
  const auto vertices = face_vertices(reference.face());
  std::array<Point3, 3> points;
  for (std::size_t index = 0; index < vertices.size(); ++index) {
    auto semantic = SemanticVertexReference::create(reference.source_feature(), vertices[index]);
    if (semantic.has_error())
      return Result<ExpectedPlane>::failure(semantic.error());
    auto resolved = SemanticReferenceEvaluator{}.resolve_vertex(document, semantic.value(), cache);
    if (resolved.has_error())
      return Result<ExpectedPlane>::failure(resolved.error());
    points[index] = resolved.value().position;
  }
  const Vector3 first{points[1].x - points[0].x, points[1].y - points[0].y,
                      points[1].z - points[0].z};
  const Vector3 second{points[2].x - points[0].x, points[2].y - points[0].y,
                       points[2].z - points[0].z};
  const Vector3 normal{first.y * second.z - first.z * second.y,
                       first.z * second.x - first.x * second.z,
                       first.x * second.y - first.y * second.x};
  if (length(normal) <= kTolerance)
    return Result<ExpectedPlane>::failure(
        draft_error(id, "semantic draft face has degenerate source geometry"));
  return Result<ExpectedPlane>::success({points[0], normalized(normal)});
}

[[nodiscard]] Result<TopoDS_Face>
resolve_planar(const PartDocument& document, const ShapeCache& cache, const TopoDS_Shape& target,
               const SemanticFaceReference& reference, const FeatureId& id) {
  auto expected = expected_plane(document, cache, reference, id);
  if (expected.has_error())
    return Result<TopoDS_Face>::failure(expected.error());
  std::vector<TopoDS_Face> matches;
  TopTools_IndexedMapOfShape faces;
  TopExp::MapShapes(target, TopAbs_FACE, faces);
  for (int index = 1; index <= faces.Extent(); ++index) {
    const TopoDS_Face face = TopoDS::Face(faces(index));
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Plane)
      continue;
    const gp_Pln plane = surface.Plane();
    const gp_Dir direction = plane.Axis().Direction();
    const Vector3 normal{direction.X(), direction.Y(), direction.Z()};
    if (std::abs(std::abs(dot(normal, expected.value().normal)) - 1.0) > kTolerance)
      continue;
    const gp_Pnt location = plane.Location();
    const Vector3 delta{expected.value().origin.x - location.X(),
                        expected.value().origin.y - location.Y(),
                        expected.value().origin.z - location.Z()};
    if (std::abs(dot(delta, normal)) <= kTolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1U)
    return Result<TopoDS_Face>::failure(
        draft_error(id, matches.empty() ? "semantic draft face no longer exists on target body"
                                        : "semantic draft face is ambiguous on target body"));
  return Result<TopoDS_Face>::success(matches.front());
}

struct ExpectedCylinder {
  Point3 center;
  Vector3 axis;
  double radius_mm;
};

[[nodiscard]] Result<ExpectedCylinder>
expected_cylinder(const PartDocument& document, const ShapeCache& cache,
                  const SemanticCylindricalFaceReference& reference, const FeatureId& id) {
  const Feature* source = document.find_feature(reference.source_feature());
  if (source == nullptr || source->type() != FeatureType::SubtractiveExtrude)
    return Result<ExpectedCylinder>::failure(
        draft_error(id, "cylindrical draft face producer must be a subtractive extrude"));
  const Sketch* sketch = document.find_sketch(source->input_sketch());
  const CircleProfile* profile =
      sketch == nullptr ? nullptr : sketch->find_circle_profile(reference.source_profile());
  if (sketch == nullptr || profile == nullptr)
    return Result<ExpectedCylinder>::failure(
        draft_error(id, "cylindrical draft face profile must exist"));
  const Parameter* diameter = document.find_parameter(profile->diameter_parameter());
  if (diameter == nullptr || diameter->type() != ParameterType::Length)
    return Result<ExpectedCylinder>::failure(
        draft_error(id, "cylindrical draft face diameter must resolve to Length"));
  auto workplane = WorkplaneResolver{}.resolve_for_sketch(document, *sketch);
  if (workplane.has_error())
    return Result<ExpectedCylinder>::failure(workplane.error());
  Vector3 axis = workplane.value().normal;
  if (source->direction() == ExtrudeDirection::OppositeSketchNormal)
    axis = {-axis.x, -axis.y, -axis.z};
  Point3 center = WorkplaneResolver{}.evaluate_point(workplane.value(), profile->center());
  double radius_mm = diameter->value().millimeters() / 2.0;
  if (source->body_result_context().has_value()) {
    const BodyId body = source->body_result_context()->effective_produced_body();
    if (const auto* state = cache.find_latest_body_transform_state(body)) {
      center = state->cumulative_transform.transform_point(center);
      axis = state->cumulative_transform.transform_vector(axis);
      const Vector3 radial{workplane.value().x_axis.x * radius_mm,
                           workplane.value().x_axis.y * radius_mm,
                           workplane.value().x_axis.z * radius_mm};
      radius_mm = length(state->cumulative_transform.transform_vector(radial));
    }
  }
  return Result<ExpectedCylinder>::success({center, normalized(axis), radius_mm});
}

[[nodiscard]] Result<TopoDS_Face>
resolve_cylindrical(const PartDocument& document, const ShapeCache& cache,
                    const TopoDS_Shape& target, const SemanticCylindricalFaceReference& reference,
                    const FeatureId& id) {
  auto expected = expected_cylinder(document, cache, reference, id);
  if (expected.has_error())
    return Result<TopoDS_Face>::failure(expected.error());
  std::vector<TopoDS_Face> matches;
  TopTools_IndexedMapOfShape faces;
  TopExp::MapShapes(target, TopAbs_FACE, faces);
  for (int index = 1; index <= faces.Extent(); ++index) {
    const TopoDS_Face face = TopoDS::Face(faces(index));
    BRepAdaptor_Surface surface(face);
    if (surface.GetType() != GeomAbs_Cylinder)
      continue;
    const gp_Cylinder cylinder = surface.Cylinder();
    if (std::abs(cylinder.Radius() - expected.value().radius_mm) > kTolerance)
      continue;
    const gp_Dir direction = cylinder.Axis().Direction();
    const Vector3 axis{direction.X(), direction.Y(), direction.Z()};
    if (std::abs(std::abs(dot(axis, expected.value().axis)) - 1.0) > kTolerance)
      continue;
    const gp_Pnt location = cylinder.Location();
    const Vector3 delta{location.X() - expected.value().center.x,
                        location.Y() - expected.value().center.y,
                        location.Z() - expected.value().center.z};
    const double along = dot(delta, expected.value().axis);
    const Vector3 radial{delta.x - expected.value().axis.x * along,
                         delta.y - expected.value().axis.y * along,
                         delta.z - expected.value().axis.z * along};
    if (length(radial) <= kTolerance)
      matches.push_back(face);
  }
  if (matches.size() != 1U)
    return Result<TopoDS_Face>::failure(draft_error(
        id, matches.empty() ? "semantic cylindrical draft face no longer exists on target body"
                            : "semantic cylindrical draft face is ambiguous on target body"));
  return Result<TopoDS_Face>::success(matches.front());
}

[[nodiscard]] Result<TopoDS_Face> resolve_face(const PartDocument& document,
                                               const ShapeCache& cache, const TopoDS_Shape& target,
                                               const FaceReference& reference,
                                               const FeatureId& id) {
  if (std::holds_alternative<SemanticFaceReference>(reference.source()))
    return resolve_planar(document, cache, target,
                          std::get<SemanticFaceReference>(reference.source()), id);
  return resolve_cylindrical(document, cache, target,
                             std::get<SemanticCylindricalFaceReference>(reference.source()), id);
}
} // namespace

Result<GeometryShape> DraftAdapter::apply(const PartDocument& document, const DraftFeature& feature,
                                          const GeometryShape& target,
                                          const ShapeCache& shape_cache, Vector3 pull_direction,
                                          Point3 neutral_origin, Vector3 neutral_normal,
                                          double angle_deg) const {
  if (target.empty() || !std::isfinite(angle_deg) || std::abs(angle_deg) <= kTolerance ||
      std::abs(angle_deg) >= 90.0 || length(pull_direction) <= kTolerance ||
      length(neutral_normal) <= kTolerance)
    return Result<GeometryShape>::failure(draft_error(
        feature.id(), "draft requires a solid target, valid references and |angle| < 90 degrees"));
  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    const Vector3 direction = normalized(pull_direction);
    const Vector3 plane_normal = normalized(neutral_normal);
    BRepOffsetAPI_DraftAngle builder(target_shape);
    for (const FaceReference& reference : feature.faces()) {
      auto face = resolve_face(document, shape_cache, target_shape, reference, feature.id());
      if (face.has_error())
        return Result<GeometryShape>::failure(face.error());
      builder.Add(face.value(), gp_Dir(direction.x, direction.y, direction.z),
                  -angle_deg * kPi / 180.0,
                  gp_Pln(gp_Pnt(neutral_origin.x, neutral_origin.y, neutral_origin.z),
                         gp_Dir(plane_normal.x, plane_normal.y, plane_normal.z)),
                  Standard_True);
      if (!builder.AddDone())
        return Result<GeometryShape>::failure(draft_error(
            feature.id(),
            "OCCT rejected a draft face; pull direction may be tangent or incompatible"));
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<GeometryShape>::failure(
          draft_error(feature.id(), "OCCT draft did not complete"));
    TopoDS_Shape result = builder.Shape();
    std::size_t solid_count = 0;
    for (TopExp_Explorer explorer(result, TopAbs_SOLID); explorer.More(); explorer.Next())
      ++solid_count;
    if (result.IsNull() || solid_count != 1U || !BRepCheck_Analyzer(result, true).IsValid())
      return Result<GeometryShape>::failure(
          draft_error(feature.id(), "draft produced an invalid or self-intersecting solid result"));
    return Result<GeometryShape>::success(
        GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(result))));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(
        draft_error(feature.id(), message == nullptr ? "OCCT draft failed" : std::string(message)));
  }
}

} // namespace blcad::geometry
