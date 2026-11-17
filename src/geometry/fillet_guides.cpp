#include "blcad/geometry/fillet_guides.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "part_history_evaluation.hpp"
#include "semantic_face_resolution.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS_Shape.hxx>
#include <cmath>
#include <gp_Lin.hxx>
namespace blcad::geometry {
namespace detail {
Result<std::vector<FilletStationGuide>>
fillet_station_guides(const PartDocument&, const FilletFeature&, const TopoDS_Shape&,
                      const ShapeCache&, StepImportTopologyContext*);
}
Result<std::vector<FilletStationGuide>>
FilletGuideQuery::stations(const PartDocument& doc, const FilletFeature& feature,
                           const ShapeCache& cache, StepImportTopologyContext* context,
                           std::optional<std::size_t> prefix) {
  auto valid = detail::validate_history_modeling_scope(doc, cache, prefix);
  if (!valid)
    return Result<std::vector<FilletStationGuide>>::failure(valid.error());
  auto shape = cache.find_body_shape(feature.target_body());
  if (!shape || shape->empty() || !feature.advanced_intent() ||
      !std::holds_alternative<VariableFilletIntent>(feature.advanced_intent()->definition))
    return Result<std::vector<FilletStationGuide>>::failure(
        Error::geometry(feature.id().value(),
                        "Station guides require a variable Fillet and its evaluated input Body"));
  return detail::fillet_station_guides(doc, feature, *OcctShapeView::get(*shape), cache, context);
}
Result<double> FilletGuideQuery::full_round_half_width(const PartDocument& doc,
                                                       const FilletFeature& feature,
                                                       const ShapeCache& cache,
                                                       StepImportTopologyContext* context,
                                                       std::optional<std::size_t> prefix) {
  const auto fail = [&](std::string message) {
    return Result<double>::failure(Error::geometry(feature.id().value(), std::move(message)));
  };
  auto valid = detail::validate_history_modeling_scope(doc, cache, prefix);
  if (!valid)
    return Result<double>::failure(valid.error());
  auto shape = cache.find_body_shape(feature.target_body());
  if (!shape || shape->empty() || !feature.advanced_intent())
    return fail("Derived size requires an evaluated full-round source");
  auto intent = std::get_if<FullRoundIntent>(&feature.advanced_intent()->definition);
  if (!intent)
    return fail("Derived half-width belongs to full-round mode");
  auto first =
      resolve_semantic_face(doc, cache, *OcctShapeView::get(*shape), intent->first.faces.front(),
                            feature.id(), "full-round size", context);
  if (!first)
    return Result<double>::failure(first.error());
  auto second =
      resolve_semantic_face(doc, cache, *OcctShapeView::get(*shape), intent->second.faces.front(),
                            feature.id(), "full-round size", context);
  if (!second)
    return Result<double>::failure(second.error());
  BRepAdaptor_Surface a(first.value()), b(second.value());
  double half = 0;
  if (a.GetType() == GeomAbs_Plane && b.GetType() == GeomAbs_Plane) {
    if (std::abs(a.Plane().Axis().Direction().Dot(b.Plane().Axis().Direction())) < 1 - 1e-10)
      return fail("Full-round side planes must be parallel");
    half = a.Plane().Distance(b.Plane().Location()) / 2;
  } else if (a.GetType() == GeomAbs_Cylinder && b.GetType() == GeomAbs_Cylinder) {
    auto x = a.Cylinder(), y = b.Cylinder();
    if (gp_Lin(x.Axis()).Distance(y.Axis().Location()) > 1e-6 ||
        std::abs(x.Axis().Direction().Dot(y.Axis().Direction())) < 1 - 1e-10)
      return fail("Full-round side cylinders must be coaxial");
    half = std::abs(x.Radius() - y.Radius()) / 2;
  } else
    return fail("Derived full-round size requires opposed planar or coaxial cylindrical sides");
  if (!std::isfinite(half) || half <= 1e-7)
    return fail("Full-round half-width is unresolved");
  return Result<double>::success(half);
}
} // namespace blcad::geometry
