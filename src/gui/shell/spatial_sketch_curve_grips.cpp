#include "spatial_sketch_curve_grips.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <charconv>
#include <cmath>
namespace blcad::gui {
namespace {
Error error(std::string message) {
  return Error::validation("sketch3d.grip", std::move(message));
}
bool in_range(double v) {
  return std::isfinite(v) && v >= spatial_curve_grip_minimum && v <= spatial_curve_grip_maximum;
}
Point3 point(gp_Pnt p) {
  return {p.X(), p.Y(), p.Z()};
}
Vector3 vector(gp_Dir d) {
  return {d.X(), d.Y(), d.Z()};
}
} // namespace
Result<std::vector<SpatialCurveGrip>> spatial_curve_grips(const PartDocument& part,
                                                          const Sketch3D& owner, SketchEntityId id,
                                                          const SpatialSolverGeometry& query) {
  using Return = Result<std::vector<SpatialCurveGrip>>;
  std::vector<SpatialCurveGrip> result;
  if (query.validate_sources) {
    auto checked = query.validate_sources();
    if (!checked)
      return Return::failure(checked.error());
  }
  if (const auto* arc = owner.find_arc(id)) {
    std::vector<SketchEntityId> points;
    for (const auto& ref : arc->point_references()) {
      if (!ref.local_point())
        return Return::success({});
      const auto* p = owner.find_point(*ref.local_point());
      if (!p || !p->parameter_dependencies().empty())
        return Return::success({});
      points.push_back(p->id());
    }
    auto geometry = geometry::Sketch3DGeometryAdapter{}.convert(part, owner);
    if (!geometry)
      return Return::failure(geometry.error());
    const auto* product = geometry.value().find_product(id);
    const auto* shape = product ? geometry::OcctShapeView::get(product->shape) : nullptr;
    if (!shape)
      return Return::failure(error("Arc geometry is unavailable"));
    TopExp_Explorer edge(*shape, TopAbs_EDGE);
    if (!edge.More())
      return Return::failure(error("Arc has no modeled edge"));
    BRepAdaptor_Curve curve(TopoDS::Edge(edge.Current()));
    edge.Next();
    if (edge.More() || curve.GetType() != GeomAbs_Circle)
      return Return::failure(error("Radius handle requires one circular arc"));
    const auto circle = curve.Circle();
    const auto center = point(circle.Location());
    const auto sample = point(curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2));
    const Vector3 radial =
        normalized(Vector3{sample.x - center.x, sample.y - center.y, sample.z - center.z});
    if (!in_range(circle.Radius()))
      return Return::failure(error("Radius lies outside the interactive handle range"));
    result.push_back({owner.id(), id, SpatialCurveGripKind::ArcRadius, center, radial,
                      vector(circle.Axis().Direction()), circle.Radius(), 1, std::move(points)});
  } else if (const auto* h = owner.find_helix(id)) {
    if (!query.axis || !query.curve || !query.prepare_candidate)
      return Return::failure(error("Current curve and axis queries are required"));
    auto reference = h->axis().datum_axis()
                         ? AxisReference::create_datum_axis(PartFeatureInputRole::SpatialConstraint,
                                                            *h->axis().datum_axis())
                         : AxisReference::create_construction_line(
                               PartFeatureInputRole::SpatialConstraint,
                               *h->axis().construction_line(), PartFeatureInputCapability::Line);
    if (!reference)
      return Return::failure(reference.error());
    auto axis = query.axis(reference.value());
    if (!axis)
      return Return::failure(axis.error());
    auto target = SpatialConstraintTarget::curve(id);
    if (!target)
      return Return::failure(target.error());
    auto prepared = query.prepare_candidate(owner);
    if (!prepared)
      return Return::failure(prepared.error());
    auto start = query.curve(owner, target.value(), 0), end = query.curve(owner, target.value(), 1);
    if (!start)
      return Return::failure(start.error());
    if (!end)
      return Return::failure(end.error());
    const auto direction = normalized(axis.value().direction);
    const Vector3 offset{start.value().point.x - axis.value().origin.x,
                         start.value().point.y - axis.value().origin.y,
                         start.value().point.z - axis.value().origin.z};
    const double along = dot(offset, direction);
    const Vector3 radial{offset.x - along * direction.x, offset.y - along * direction.y,
                         offset.z - along * direction.z};
    if (dot(radial, radial) <= 1e-24)
      return Return::failure(error("Helix radius frame is singular"));
    const Point3 center{axis.value().origin.x + along * direction.x,
                        axis.value().origin.y + along * direction.y,
                        axis.value().origin.z + along * direction.z};
    if (h->radius_binding().explicit_length()) {
      const double value = h->radius_binding().explicit_length()->millimeters();
      if (in_range(value))
        result.push_back({owner.id(), id, SpatialCurveGripKind::HelixRadius, center,
                          normalized(radial), direction, value});
    }
    if (h->pitch_binding().explicit_length()) {
      const auto* turns = part.find_parameter(h->turns_parameter());
      if (!turns || !turns->value().is_valid_count())
        return Return::failure(error("Integer turn source is unavailable"));
      const double value = h->pitch_binding().explicit_length()->millimeters();
      if (in_range(value))
        result.push_back({owner.id(), id, SpatialCurveGripKind::HelixPitch, end.value().point,
                          direction, direction, value,
                          static_cast<double>(turns->value().count_value())});
    }
  }
  return Return::success(std::move(result));
}
Result<SpatialCurveGripCandidate>
move_spatial_curve_grip(const Sketch3D& source, const SpatialCurveGrip& grip, double value) {
  using Return = Result<SpatialCurveGripCandidate>;
  if (source.id() != grip.owner || !in_range(value) || !in_range(grip.value))
    return Return::failure(error("Choose a positive handle value within the supported range"));
  auto candidate = source;
  SpatialSolverOptions options;
  if (grip.kind == SpatialCurveGripKind::ArcRadius) {
    if (!source.find_arc(grip.entity) || grip.points.size() != 3)
      return Return::failure(error("Arc definition changed; select it again"));
    const double factor = value / grip.value;
    for (const auto& id : grip.points) {
      const auto* p = source.find_point(id);
      if (!p || !p->parameter_dependencies().empty())
        return Return::failure(
            error("Explicitly rebind or detach the defining point before using its radius handle"));
      const Point3 old{p->x().explicit_coordinate()->millimeters(),
                       p->y().explicit_coordinate()->millimeters(),
                       p->z().explicit_coordinate()->millimeters()};
      const Point3 next{grip.origin.x + (old.x - grip.origin.x) * factor,
                        grip.origin.y + (old.y - grip.origin.y) * factor,
                        grip.origin.z + (old.z - grip.origin.z) * factor};
      const auto coordinate = [&](const SketchCoordinate3D& old,
                                  double v) -> Result<SketchCoordinate3D> {
        if (old.explicit_coordinate()->millimeters() == v)
          return Result<SketchCoordinate3D>::success(old);
        auto q = Quantity::linear_displacement_mm(v);
        return q ? SketchCoordinate3D::create_explicit(q.value())
                 : Result<SketchCoordinate3D>::failure(q.error());
      };
      auto x = coordinate(p->x(), next.x), y = coordinate(p->y(), next.y),
           z = coordinate(p->z(), next.z);
      if (!x)
        return Return::failure(x.error());
      if (!y)
        return Return::failure(y.error());
      if (!z)
        return Return::failure(z.error());
      auto point = SketchPoint3D::create(id, x.value(), y.value(), z.value());
      if (!point)
        return Return::failure(point.error());
      auto replaced = candidate.replace_point(point.value());
      if (!replaced)
        return Return::failure(replaced.error());
      for (auto kind :
           {SpatialVariableKind::PointX, SpatialVariableKind::PointY, SpatialVariableKind::PointZ})
        options.fixed_variables.push_back({kind, id.value()});
    }
  } else {
    const auto* h = source.find_helix(grip.entity);
    if (!h)
      return Return::failure(error("Helix definition changed; select it again"));
    const bool radius = grip.kind == SpatialCurveGripKind::HelixRadius;
    if (!(radius ? h->radius_binding() : h->pitch_binding()).explicit_length())
      return Return::failure(error("Explicitly rebind the shared scalar before using its handle"));
    auto q = Quantity::length_mm(value);
    if (!q)
      return Return::failure(q.error());
    auto binding = SketchLength3D::create_explicit(q.value());
    if (!binding)
      return Return::failure(binding.error());
    auto next = SketchHelix3D::create_with_bindings(
        h->id(), h->axis(), radius ? binding.value() : h->radius_binding(),
        radius ? h->pitch_binding() : binding.value(), h->turns_parameter(), h->handedness());
    if (!next)
      return Return::failure(next.error());
    auto replaced = candidate.replace_helix(next.value());
    if (!replaced)
      return Return::failure(replaced.error());
    options.fixed_variables.push_back(
        {radius ? SpatialVariableKind::HelixRadius : SpatialVariableKind::HelixPitch,
         h->id().value()});
  }
  return Return::success({std::move(candidate), std::move(options)});
}
bool valid_spatial_curve_grip_text(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return false;
  text.remove_prefix(first);
  const auto last = text.find_last_not_of(" \t\r\n");
  text = text.substr(0, last + 1);
  double value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc() || parsed.ptr == text.data() || !in_range(value))
    return false;
  auto suffix = std::string_view(parsed.ptr,
                                 static_cast<std::size_t>(text.data() + text.size() - parsed.ptr));
  const auto start = suffix.find_first_not_of(" \t");
  if (start == std::string_view::npos)
    return true;
  suffix.remove_prefix(start);
  return suffix == "mm";
}
} // namespace blcad::gui
