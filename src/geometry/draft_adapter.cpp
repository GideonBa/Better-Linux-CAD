#include "blcad/geometry/draft_adapter.hpp"

#include "blcad/geometry/shape_cache.hpp"

#include "geometry_shape_internal.hpp"
#include "semantic_face_resolution.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;
constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] Error draft_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
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
      auto face = resolve_semantic_face(document, shape_cache, target_shape, reference,
                                        feature.id(), "draft");
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
    auto backing = std::make_shared<GeometryShape::Impl>(std::move(result));
    backing->face_lineage =
        detail::map_lineage(builder, backing->shape, target.impl_->face_lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(
        draft_error(feature.id(), message == nullptr ? "OCCT draft failed" : std::string(message)));
  }
}

} // namespace blcad::geometry
