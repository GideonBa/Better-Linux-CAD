#include "blcad/geometry/shell_adapter.hpp"

#include "blcad/geometry/shape_cache.hpp"

#include "geometry_shape_internal.hpp"
#include "semantic_face_resolution.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Face.hxx>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;

[[nodiscard]] Error shell_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

} // namespace

Result<GeometryShape> ShellAdapter::apply(const PartDocument& document, const ShellFeature& feature,
                                          const GeometryShape& target,
                                          const ShapeCache& shape_cache, double thickness_mm,
                                          StepImportTopologyContext* import_context) const {
  if (target.empty() || !std::isfinite(thickness_mm) || thickness_mm <= 0.0)
    return Result<GeometryShape>::failure(
        shell_error(feature.id(), "shell target and positive finite thickness are required"));
  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    TopTools_ListOfShape closing_faces;
    for (const FaceReference& reference : feature.removed_faces()) {
      auto face = resolve_semantic_face(document, shape_cache, target_shape, reference,
                                        feature.id(), "shell", import_context);
      if (face.has_error())
        return Result<GeometryShape>::failure(face.error());
      closing_faces.Append(face.value());
    }

    const double offset =
        feature.direction() == ShellDirection::Inward ? -thickness_mm : thickness_mm;
    BRepOffsetAPI_MakeThickSolid builder;
    builder.MakeThickSolidByJoin(target_shape, closing_faces, offset, kTolerance, BRepOffset_Skin,
                                 Standard_False, Standard_False, GeomAbs_Arc, Standard_True);
    if (!builder.IsDone())
      return Result<GeometryShape>::failure(shell_error(
          feature.id(), "OCCT shell did not complete; thickness may exceed local geometry"));
    TopoDS_Shape result = builder.Shape();
    std::size_t solid_count = 0;
    for (TopExp_Explorer explorer(result, TopAbs_SOLID); explorer.More(); explorer.Next())
      ++solid_count;
    if (result.IsNull() || solid_count != 1U || !BRepCheck_Analyzer(result, true).IsValid())
      return Result<GeometryShape>::failure(
          shell_error(feature.id(), "shell produced an invalid or non-manifold solid result"));
    auto backing = std::make_shared<GeometryShape::Impl>(std::move(result));
    backing->face_lineage =
        detail::map_lineage(builder, backing->shape, target.impl_->face_lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(
        shell_error(feature.id(), message == nullptr ? "OCCT shell failed" : std::string(message)));
  }
}

} // namespace blcad::geometry
