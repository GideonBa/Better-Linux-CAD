#include "sweep_material_region.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/sweep_adapter.hpp"
#include "native_path_curve.hpp"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <numbers>
#include <set>
namespace blcad::geometry::detail {
namespace {
constexpr double tolerance = 1e-7;
Result<ContainedProfileHole> contour(const PartDocument& part, const Sketch& sketch,
                                     const ProfileId& id) {
  const auto fail = [&] {
    return Result<ContainedProfileHole>::failure(
        Error::geometry(id.value(), "Invalid material-region profile"));
  };
  if (const auto* rectangle = sketch.find_rectangle_profile(id)) {
    const auto* width = part.find_parameter(rectangle->width_parameter());
    const auto* height = part.find_parameter(rectangle->height_parameter());
    if (!width || !height || width->type() != ParameterType::Length ||
        height->type() != ParameterType::Length)
      return fail();
    const auto c = rectangle->center();
    const double x = width->value().millimeters() / 2, y = height->value().millimeters() / 2;
    return Result<ContainedProfileHole>::success(
        {id,
         ContainedProfileHoleKind::Polygon,
         {{c.x - x, c.y - y}, {c.x + x, c.y - y}, {c.x + x, c.y + y}, {c.x - x, c.y + y}},
         {},
         0});
  }
  if (const auto* polygon = sketch.find_closed_profile(id)) {
    auto points =
        profile_region_classifier_detail::resolve_closed_profile_vertices(part, sketch, *polygon);
    if (!points)
      return Result<ContainedProfileHole>::failure(points.error());
    return Result<ContainedProfileHole>::success(
        {id, ContainedProfileHoleKind::Polygon, points.value(), {}, 0});
  }
  if (const auto* circle = sketch.find_circle_profile(id)) {
    const auto* diameter = part.find_parameter(circle->diameter_parameter());
    if (!diameter || diameter->type() != ParameterType::Length)
      return fail();
    const double radius = diameter->value().millimeters() / 2;
    if (!std::isfinite(radius) || radius <= tolerance)
      return fail();
    return Result<ContainedProfileHole>::success(
        {id, ContainedProfileHoleKind::Circle, {}, circle->center(), radius});
  }
  if (const auto* arc = sketch.find_arc_closed_profile(id))
    return resolve_curved_profile_hole(part, sketch, id, arc->curve_segments());
  return fail();
}
Result<GeometryShape> wire(const ContainedProfileHole& hole) {
  std::vector<SweepPathSegment> segments;
  const auto point = [](Point2 p) { return Point3{p.x, p.y, 0}; };
  if (hole.kind == ContainedProfileHoleKind::Circle) {
    for (int i = 0; i < 4; ++i) {
      const auto at = [&](double a) {
        return Point3{hole.center.x + hole.radius_mm * std::cos(a),
                      hole.center.y + hole.radius_mm * std::sin(a), 0};
      };
      segments.push_back({SweepPathSegmentKind::CircularArc, at(i * std::numbers::pi / 2),
                          at((i + .5) * std::numbers::pi / 2), at((i + 1) * std::numbers::pi / 2)});
    }
  } else if (hole.kind == ContainedProfileHoleKind::Polygon) {
    for (std::size_t i = 0; i < hole.vertices.size(); ++i)
      segments.push_back({SweepPathSegmentKind::Line,
                          point(hole.vertices[i]),
                          {},
                          point(hole.vertices[(i + 1) % hole.vertices.size()])});
  } else {
    for (const auto& curve : hole.curves) {
      SweepPathSegment span{SweepPathSegmentKind::Line, curve.start, curve.mid, curve.end};
      if (curve.kind == ClosedProfileCurveKind::CircularArc)
        span.kind = SweepPathSegmentKind::CircularArc;
      else if (curve.kind == ClosedProfileCurveKind::DerivedNative) {
        span.kind = SweepPathSegmentKind::DerivedNative;
        span.derived_curve = curve.derived_curve;
      } else if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline) {
        auto native = native_rational_bezier({curve.start, curve.mid, curve.control2, curve.end},
                                             curve.weights, hole.profile.value());
        if (!native)
          return Result<GeometryShape>::failure(native.error());
        span.kind = SweepPathSegmentKind::DerivedNative;
        span.derived_curve = native.value();
      }
      segments.push_back(span);
    }
  }
  return SweepAdapter{}.path_shape(PathCurveId(hole.profile.value()), segments, true);
}
struct Boundary {
  ContainedProfileHole contour;
  GeometryShape wire;
  TopoDS_Face face;
  gp_Pnt point;
};
Result<Boundary> boundary(ContainedProfileHole value) {
  auto shape = wire(value);
  if (!shape)
    return Result<Boundary>::failure(shape.error());
  const auto* native = OcctShapeView::get(shape.value());
  if (!native || native->ShapeType() != TopAbs_WIRE)
    return Result<Boundary>::failure(
        Error::geometry(value.profile.value(), "Material-region wire is unavailable"));
  BRepBuilderAPI_MakeFace face(TopoDS::Wire(*native), true);
  if (!face.IsDone() || !BRepCheck_Analyzer(face.Face()).IsValid())
    return Result<Boundary>::failure(
        Error::geometry(value.profile.value(), "Material-region boundary is invalid"));
  const auto vertex = TopExp_Explorer(*native, TopAbs_VERTEX);
  if (!vertex.More())
    return Result<Boundary>::failure(
        Error::geometry(value.profile.value(), "Material-region boundary has no vertex"));
  const auto p = BRep_Tool::Pnt(TopoDS::Vertex(vertex.Current()));
  return Result<Boundary>::success({std::move(value), std::move(shape.value()), face.Face(), p});
}
Result<bool> separated(const Boundary& a, const Boundary& b) {
  BRepExtrema_DistShapeShape distance(*OcctShapeView::get(a.wire), *OcctShapeView::get(b.wire));
  distance.Perform();
  if (!distance.IsDone() || !std::isfinite(distance.Value()))
    return Result<bool>::failure(Error::geometry(
        b.contour.profile.value(), "Material-region boundary distance is unresolved"));
  return Result<bool>::success(distance.Value() > tolerance);
}
bool inside(const Boundary& outer, const Boundary& inner) {
  return BRepClass_FaceClassifier(outer.face, inner.point, tolerance).State() == TopAbs_IN;
}
} // namespace
Result<std::vector<ContainedProfileHole>> resolve_sweep_material_holes(const PartDocument& part,
                                                                       const Sketch& sketch,
                                                                       const ProfileId& profile) {
  const auto failed = [&](std::string message) {
    return Result<std::vector<ContainedProfileHole>>::failure(
        Error::geometry(profile.value(), std::move(message)));
  };
  try {
    auto outer_contour = contour(part, sketch, profile);
    if (!outer_contour)
      return failed(outer_contour.error().message());
    auto outer = boundary(std::move(outer_contour.value()));
    if (!outer)
      return failed(outer.error().message());
    std::vector<ContainedProfileHole> candidates;
    std::set<std::string> seen{profile.value()};
    const auto append = [&](const ProfileId& id) -> Result<std::size_t> {
      if (sketch.profile_has_construction(id) || !seen.insert(id.value()).second)
        return Result<std::size_t>::success(0);
      auto value = contour(part, sketch, id);
      if (!value)
        return Result<std::size_t>::failure(value.error());
      candidates.push_back(std::move(value.value()));
      return Result<std::size_t>::success(1);
    };
    const auto collect = [&](const auto& profiles) -> Result<std::size_t> {
      for (const auto& candidate : profiles) {
        auto added = append(candidate.id());
        if (!added)
          return added;
      }
      return Result<std::size_t>::success(profiles.size());
    };
    for (auto result : {collect(sketch.rectangle_profiles()), collect(sketch.closed_profiles()),
                        collect(sketch.circle_profiles()), collect(sketch.arc_closed_profiles())})
      if (!result)
        return failed(result.error().message());
    auto raw = SketchFinishService::analyze_unmaterialized(part, sketch);
    if (raw)
      for (const auto& region : raw.value().regions) {
        if (!seen.insert(region.id.value()).second)
          continue;
        if (!region.curve_segments.empty()) {
          auto value = resolve_curved_profile_hole(part, sketch, region.id, region.curve_segments);
          if (!value)
            return failed(value.error().message());
          candidates.push_back(std::move(value.value()));
        } else
          candidates.push_back(
              {region.id, ContainedProfileHoleKind::Polygon, region.vertices, {}, 0});
      }
    std::vector<Boundary> inner;
    for (auto& candidate : candidates) {
      auto current = boundary(std::move(candidate));
      if (!current)
        return failed(current.error().message());
      auto distinct = separated(outer.value(), current.value());
      if (!distinct)
        return failed(distinct.error().message());
      if (!distinct.value())
        return failed("Material-region boundaries touch or intersect; correct the profile");
      if (inside(outer.value(), current.value()))
        inner.push_back(std::move(current.value()));
    }
    std::vector<bool> immediate(inner.size(), true);
    for (std::size_t i = 0; i < inner.size(); ++i)
      for (std::size_t j = i + 1; j < inner.size(); ++j) {
        auto distinct = separated(inner[i], inner[j]);
        if (!distinct)
          return failed(distinct.error().message());
        if (!distinct.value())
          return failed("Material-region inner boundaries touch or intersect");
        if (inside(inner[i], inner[j]))
          immediate[j] = false;
        if (inside(inner[j], inner[i]))
          immediate[i] = false;
      }
    std::vector<ContainedProfileHole> holes;
    for (std::size_t i = 0; i < inner.size(); ++i)
      if (immediate[i])
        holes.push_back(std::move(inner[i].contour));
    return Result<std::vector<ContainedProfileHole>>::success(std::move(holes));
  } catch (const Standard_Failure& error) {
    return failed(std::string("Material-region classification failed: ") +
                  error.GetMessageString());
  }
}
} // namespace blcad::geometry::detail
