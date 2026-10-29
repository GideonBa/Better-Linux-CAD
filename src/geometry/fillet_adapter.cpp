#include "blcad/geometry/fillet_adapter.hpp"

#include "blcad/geometry/shape_cache.hpp"

#include "geometry_shape_internal.hpp"
#include "semantic_edge_resolution.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {
constexpr double kTolerance = 1.0e-6;

[[nodiscard]] Error fillet_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}
} // namespace

Result<GeometryShape> FilletAdapter::apply(const PartDocument& document,
                                           const FilletFeature& feature,
                                           const GeometryShape& target,
                                           const ShapeCache& shape_cache, double radius_mm,
                                           StepImportTopologyContext* import_context) const {
  if (target.empty() || !std::isfinite(radius_mm) || radius_mm <= 0.0)
    return Result<GeometryShape>::failure(
        fillet_error(feature.id(), "fillet target and positive finite radius are required"));
  try {
    const TopoDS_Shape& target_shape = target.impl_->shape;
    BRepFilletAPI_MakeFillet builder(target_shape);
    for (const auto& reference : feature.edges()) {
      auto edge = resolve_semantic_edge(document, shape_cache, target_shape, reference,
                                        feature.id(), "fillet", import_context);
      if (edge.has_error())
        return Result<GeometryShape>::failure(edge.error());
      builder.Add(radius_mm, edge.value());
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<GeometryShape>::failure(
          fillet_error(feature.id(), "OCCT fillet did not complete; radius may be too large"));
    TopoDS_Shape result = builder.Shape();
    if (result.IsNull() || !TopExp_Explorer(result, TopAbs_SOLID).More() ||
        !BRepCheck_Analyzer(result, true).IsValid())
      return Result<GeometryShape>::failure(
          fillet_error(feature.id(), "fillet produced an invalid solid result"));
    auto backing = std::make_shared<GeometryShape::Impl>(std::move(result));
    backing->face_lineage =
        detail::map_lineage(builder, backing->shape, target.impl_->face_lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& failure) {
    const char* message = failure.GetMessageString();
    return Result<GeometryShape>::failure(fillet_error(
        feature.id(), message == nullptr ? "OCCT fillet failed" : std::string(message)));
  }
}
} // namespace blcad::geometry
