#include "blcad/geometry/loft_adapter.hpp"

#include "geometry_shape_internal.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_Shape.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

constexpr double kTolerance = 1.0e-8;
constexpr double kControlBindingTolerance = 1.0e-5;
constexpr std::size_t kControlSamplesPerSpan = 4U;

[[nodiscard]] Error loft_error(const FeatureId& id, std::string message) {
  return Error::geometry(id.value(), std::move(message));
}

[[nodiscard]] gp_Pnt point(Point3 value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] double distance(Point3 left, Point3 right) {
  return std::hypot(std::hypot(left.x - right.x, left.y - right.y), left.z - right.z);
}

[[nodiscard]] Point3 interpolate(Point3 first, Point3 second, double ratio) {
  return {first.x + (second.x - first.x) * ratio, first.y + (second.y - first.y) * ratio,
          first.z + (second.z - first.z) * ratio};
}

[[nodiscard]] std::vector<Point3> control_polyline(const std::vector<SweepPathSegment>& segments) {
  std::vector<Point3> points;
  if (segments.empty())
    return points;
  points.push_back(segments.front().start);
  for (const SweepPathSegment& segment : segments) {
    if (segment.kind == SweepPathSegmentKind::CircularArc)
      points.push_back(segment.mid);
    points.push_back(segment.end);
  }
  return points;
}

[[nodiscard]] Result<Point3> sample_control_curve(const FeatureId& id,
                                                  const std::vector<SweepPathSegment>& segments,
                                                  double ratio, std::string_view role) {
  const std::vector<Point3> points = control_polyline(segments);
  if (points.size() < 2U)
    return Result<Point3>::failure(
        loft_error(id, std::string(role) + " must contain a finite open curve"));
  std::vector<double> lengths;
  lengths.reserve(points.size() - 1U);
  double total = 0.0;
  for (std::size_t index = 1U; index < points.size(); ++index) {
    const double length = distance(points[index - 1U], points[index]);
    if (length <= kTolerance)
      continue;
    lengths.push_back(length);
    total += length;
  }
  if (lengths.size() != points.size() - 1U || total <= kTolerance)
    return Result<Point3>::failure(
        loft_error(id, std::string(role) + " contains a degenerate segment"));
  const double target = std::clamp(ratio, 0.0, 1.0) * total;
  double accumulated = 0.0;
  for (std::size_t index = 0U; index < lengths.size(); ++index) {
    if (target <= accumulated + lengths[index] || index + 1U == lengths.size())
      return Result<Point3>::success(
          interpolate(points[index], points[index + 1U],
                      std::clamp((target - accumulated) / lengths[index], 0.0, 1.0)));
    accumulated += lengths[index];
  }
  return Result<Point3>::success(points.back());
}

[[nodiscard]] Point3 closed_section_center(const std::vector<ClosedProfileCurveSegment>& section) {
  Point3 center{};
  for (const auto& segment : section) {
    center.x += segment.start.x;
    center.y += segment.start.y;
    center.z += segment.start.z;
  }
  const double count = static_cast<double>(section.size());
  return {center.x / count, center.y / count, center.z / count};
}

[[nodiscard]] Point3 open_section_center(const std::vector<SweepPathSegment>& section) {
  Point3 center{};
  for (const auto& segment : section) {
    center.x += segment.start.x;
    center.y += segment.start.y;
    center.z += segment.start.z;
  }
  center.x += section.back().end.x;
  center.y += section.back().end.y;
  center.z += section.back().end.z;
  const double count = static_cast<double>(section.size() + 1U);
  return {center.x / count, center.y / count, center.z / count};
}

void translate_closed_section(std::vector<ClosedProfileCurveSegment>& section, Vector3 delta) {
  const auto translated = [delta](Point3 value) {
    return Point3{value.x + delta.x, value.y + delta.y, value.z + delta.z};
  };
  for (auto& segment : section) {
    segment.start = translated(segment.start);
    segment.end = translated(segment.end);
    if (segment.kind != ClosedProfileCurveKind::Line)
      segment.mid = translated(segment.mid);
    if (segment.kind == ClosedProfileCurveKind::CubicBezierSpline)
      segment.control2 = translated(segment.control2);
  }
}

void translate_open_section(std::vector<SweepPathSegment>& section, Vector3 delta) {
  const auto translated = [delta](Point3 value) {
    return Point3{value.x + delta.x, value.y + delta.y, value.z + delta.z};
  };
  for (auto& segment : section) {
    segment.start = translated(segment.start);
    segment.end = translated(segment.end);
    if (segment.kind == SweepPathSegmentKind::CircularArc)
      segment.mid = translated(segment.mid);
  }
}

[[nodiscard]] Result<std::vector<ClosedProfileCurveSegment>>
interpolate_closed_section(const FeatureId& id, const std::vector<ClosedProfileCurveSegment>& first,
                           const std::vector<ClosedProfileCurveSegment>& second, double ratio) {
  if (first.size() != second.size())
    return Result<std::vector<ClosedProfileCurveSegment>>::failure(
        loft_error(id, "guided loft sections require matching authored edge counts"));
  std::vector<ClosedProfileCurveSegment> result;
  result.reserve(first.size());
  for (std::size_t index = 0U; index < first.size(); ++index) {
    if (first[index].kind != second[index].kind)
      return Result<std::vector<ClosedProfileCurveSegment>>::failure(
          loft_error(id, "guided loft correspondence requires matching edge kinds"));
    result.push_back({first[index].kind,
                      interpolate(first[index].start, second[index].start, ratio),
                      interpolate(first[index].mid, second[index].mid, ratio),
                      interpolate(first[index].end, second[index].end, ratio),
                      interpolate(first[index].control2, second[index].control2, ratio)});
  }
  return Result<std::vector<ClosedProfileCurveSegment>>::success(std::move(result));
}

[[nodiscard]] Result<std::vector<SweepPathSegment>>
interpolate_open_section(const FeatureId& id, const std::vector<SweepPathSegment>& first,
                         const std::vector<SweepPathSegment>& second, double ratio) {
  if (first.size() != second.size())
    return Result<std::vector<SweepPathSegment>>::failure(
        loft_error(id, "guided open loft sections require matching authored edge counts"));
  std::vector<SweepPathSegment> result;
  result.reserve(first.size());
  for (std::size_t index = 0U; index < first.size(); ++index) {
    if (first[index].kind != second[index].kind)
      return Result<std::vector<SweepPathSegment>>::failure(
          loft_error(id, "guided open loft correspondence requires matching edge kinds"));
    result.push_back({first[index].kind,
                      interpolate(first[index].start, second[index].start, ratio),
                      interpolate(first[index].mid, second[index].mid, ratio),
                      interpolate(first[index].end, second[index].end, ratio)});
  }
  return Result<std::vector<SweepPathSegment>>::success(std::move(result));
}

[[nodiscard]] Result<std::vector<std::vector<ClosedProfileCurveSegment>>>
controlled_closed_sections(
    const FeatureId& id,
    const std::vector<std::vector<ClosedProfileCurveSegment>>& authored_sections,
    const std::vector<SweepPathSegment>* center_path,
    const std::vector<std::vector<SweepPathSegment>>& guides) {
  if (center_path == nullptr && guides.empty())
    return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::success(authored_sections);
  if (authored_sections.size() < 2U || authored_sections.front().empty())
    return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(
        loft_error(id, "guided loft requires at least two non-empty sections"));
  if (guides.size() > authored_sections.front().size())
    return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(
        loft_error(id, "ordered loft guides exceed available seam-relative boundary points"));
  const auto guide_point_index = [&guides, &authored_sections](std::size_t guide_index) {
    return guide_index * authored_sections.front().size() / guides.size();
  };

  const double span_count = static_cast<double>(authored_sections.size() - 1U);
  for (std::size_t index = 0U; index < authored_sections.size(); ++index) {
    const double ratio = static_cast<double>(index) / span_count;
    if (center_path != nullptr) {
      auto point = sample_control_curve(id, *center_path, ratio, "loft center path");
      if (point.has_error())
        return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(point.error());
      if (distance(point.value(), closed_section_center(authored_sections[index])) >
          kControlBindingTolerance)
        return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(loft_error(
            id, "loft center path must pass through every authored section center in order"));
    }
    for (std::size_t guide_index = 0U; guide_index < guides.size(); ++guide_index) {
      auto point = sample_control_curve(id, guides[guide_index], ratio, "loft guide curve");
      if (point.has_error())
        return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(point.error());
      const std::size_t point_index = guide_point_index(guide_index);
      if (distance(point.value(), authored_sections[index][point_index].start) >
          kControlBindingTolerance)
        return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(
            loft_error(id, "ordered loft guide must pass through its seam-relative section point"));
    }
  }

  std::vector<std::vector<ClosedProfileCurveSegment>> result;
  result.reserve((authored_sections.size() - 1U) * kControlSamplesPerSpan + 1U);
  for (std::size_t span = 0U; span + 1U < authored_sections.size(); ++span) {
    result.push_back(authored_sections[span]);
    for (std::size_t sample = 1U; sample < kControlSamplesPerSpan; ++sample) {
      const double local_ratio = static_cast<double>(sample) / kControlSamplesPerSpan;
      const double global_ratio = (static_cast<double>(span) + local_ratio) / span_count;
      auto section = interpolate_closed_section(id, authored_sections[span],
                                                authored_sections[span + 1U], local_ratio);
      if (section.has_error())
        return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(
            section.error());
      if (center_path != nullptr) {
        auto point = sample_control_curve(id, *center_path, global_ratio, "loft center path");
        if (point.has_error())
          return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(
              point.error());
        const Point3 center = closed_section_center(section.value());
        translate_closed_section(
            section.value(),
            {point.value().x - center.x, point.value().y - center.y, point.value().z - center.z});
      }
      for (std::size_t guide_index = 0U; guide_index < guides.size(); ++guide_index) {
        auto point =
            sample_control_curve(id, guides[guide_index], global_ratio, "loft guide curve");
        if (point.has_error())
          return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::failure(
              point.error());
        const std::size_t point_index = guide_point_index(guide_index);
        const std::size_t previous =
            (point_index + section.value().size() - 1U) % section.value().size();
        section.value()[point_index].start = point.value();
        section.value()[previous].end = point.value();
      }
      result.push_back(std::move(section.value()));
    }
  }
  result.push_back(authored_sections.back());
  return Result<std::vector<std::vector<ClosedProfileCurveSegment>>>::success(std::move(result));
}

[[nodiscard]] Result<std::vector<std::vector<SweepPathSegment>>>
controlled_open_sections(const FeatureId& id,
                         const std::vector<std::vector<SweepPathSegment>>& authored_sections,
                         const std::vector<SweepPathSegment>* center_path,
                         const std::vector<std::vector<SweepPathSegment>>& guides) {
  if (center_path == nullptr && guides.empty())
    return Result<std::vector<std::vector<SweepPathSegment>>>::success(authored_sections);
  if (authored_sections.size() < 2U || authored_sections.front().empty())
    return Result<std::vector<std::vector<SweepPathSegment>>>::failure(
        loft_error(id, "guided open loft requires at least two non-empty sections"));
  if (guides.size() > authored_sections.front().size() + 1U)
    return Result<std::vector<std::vector<SweepPathSegment>>>::failure(
        loft_error(id, "ordered open loft guides exceed available section points"));
  const std::size_t open_point_count = authored_sections.front().size() + 1U;
  const auto guide_point_index = [&guides, open_point_count](std::size_t guide_index) {
    return guide_index * open_point_count / guides.size();
  };

  const auto section_point = [](const std::vector<SweepPathSegment>& section, std::size_t index) {
    return index < section.size() ? section[index].start : section.back().end;
  };
  const double span_count = static_cast<double>(authored_sections.size() - 1U);
  for (std::size_t index = 0U; index < authored_sections.size(); ++index) {
    const double ratio = static_cast<double>(index) / span_count;
    if (center_path != nullptr) {
      auto point = sample_control_curve(id, *center_path, ratio, "open loft center path");
      if (point.has_error())
        return Result<std::vector<std::vector<SweepPathSegment>>>::failure(point.error());
      if (distance(point.value(), open_section_center(authored_sections[index])) >
          kControlBindingTolerance)
        return Result<std::vector<std::vector<SweepPathSegment>>>::failure(loft_error(
            id, "open loft center path must pass through every authored section center in order"));
    }
    for (std::size_t guide_index = 0U; guide_index < guides.size(); ++guide_index) {
      auto point = sample_control_curve(id, guides[guide_index], ratio, "open loft guide curve");
      if (point.has_error())
        return Result<std::vector<std::vector<SweepPathSegment>>>::failure(point.error());
      if (distance(point.value(),
                   section_point(authored_sections[index], guide_point_index(guide_index))) >
          kControlBindingTolerance)
        return Result<std::vector<std::vector<SweepPathSegment>>>::failure(
            loft_error(id, "ordered open loft guide must pass through its section point"));
    }
  }

  std::vector<std::vector<SweepPathSegment>> result;
  result.reserve((authored_sections.size() - 1U) * kControlSamplesPerSpan + 1U);
  for (std::size_t span = 0U; span + 1U < authored_sections.size(); ++span) {
    result.push_back(authored_sections[span]);
    for (std::size_t sample = 1U; sample < kControlSamplesPerSpan; ++sample) {
      const double local_ratio = static_cast<double>(sample) / kControlSamplesPerSpan;
      const double global_ratio = (static_cast<double>(span) + local_ratio) / span_count;
      auto section = interpolate_open_section(id, authored_sections[span],
                                              authored_sections[span + 1U], local_ratio);
      if (section.has_error())
        return Result<std::vector<std::vector<SweepPathSegment>>>::failure(section.error());
      if (center_path != nullptr) {
        auto point = sample_control_curve(id, *center_path, global_ratio, "open loft center path");
        if (point.has_error())
          return Result<std::vector<std::vector<SweepPathSegment>>>::failure(point.error());
        const Point3 center = open_section_center(section.value());
        translate_open_section(
            section.value(),
            {point.value().x - center.x, point.value().y - center.y, point.value().z - center.z});
      }
      for (std::size_t guide_index = 0U; guide_index < guides.size(); ++guide_index) {
        auto point =
            sample_control_curve(id, guides[guide_index], global_ratio, "open loft guide curve");
        if (point.has_error())
          return Result<std::vector<std::vector<SweepPathSegment>>>::failure(point.error());
        const std::size_t point_index = guide_point_index(guide_index);
        if (point_index < section.value().size())
          section.value()[point_index].start = point.value();
        if (point_index > 0U)
          section.value()[point_index - 1U].end = point.value();
      }
      result.push_back(std::move(section.value()));
    }
  }
  result.push_back(authored_sections.back());
  return Result<std::vector<std::vector<SweepPathSegment>>>::success(std::move(result));
}

[[nodiscard]] std::string failure_message(const Standard_Failure& failure) {
  const char* message = failure.GetMessageString();
  return message == nullptr || *message == '\0' ? "OCCT loft operation failed" : message;
}

[[nodiscard]] Result<TopoDS_Edge> closed_edge(const FeatureId& id,
                                              const ClosedProfileCurveSegment& segment) {
  if (distance(segment.start, segment.end) <= kTolerance)
    return Result<TopoDS_Edge>::failure(loft_error(id, "loft section edge is degenerate"));
  if (segment.kind == ClosedProfileCurveKind::Line) {
    BRepBuilderAPI_MakeEdge edge(point(segment.start), point(segment.end));
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft line edge"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  if (segment.kind == ClosedProfileCurveKind::CircularArc) {
    GC_MakeArcOfCircle arc(point(segment.start), point(segment.mid), point(segment.end));
    if (!arc.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft arc"));
    BRepBuilderAPI_MakeEdge edge(arc.Value());
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft arc edge"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  TColgp_Array1OfPnt poles(1, 4);
  poles.SetValue(1, point(segment.start));
  poles.SetValue(2, point(segment.mid));
  poles.SetValue(3, point(segment.control2));
  poles.SetValue(4, point(segment.end));
  Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
  BRepBuilderAPI_MakeEdge edge(curve);
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(loft_error(id, "could not build loft spline edge"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

[[nodiscard]] Result<TopoDS_Edge> open_edge(const FeatureId& id, const SweepPathSegment& segment) {
  if (distance(segment.start, segment.end) <= kTolerance)
    return Result<TopoDS_Edge>::failure(loft_error(id, "open loft section edge is degenerate"));
  if (segment.kind == SweepPathSegmentKind::Line) {
    BRepBuilderAPI_MakeEdge edge(point(segment.start), point(segment.end));
    if (!edge.IsDone())
      return Result<TopoDS_Edge>::failure(loft_error(id, "could not build open loft line"));
    return Result<TopoDS_Edge>::success(edge.Edge());
  }
  GC_MakeArcOfCircle arc(point(segment.start), point(segment.mid), point(segment.end));
  if (!arc.IsDone())
    return Result<TopoDS_Edge>::failure(loft_error(id, "could not build open loft arc"));
  BRepBuilderAPI_MakeEdge edge(arc.Value());
  if (!edge.IsDone())
    return Result<TopoDS_Edge>::failure(loft_error(id, "could not build open loft arc edge"));
  return Result<TopoDS_Edge>::success(edge.Edge());
}

template <typename Segment, typename Factory>
[[nodiscard]] Result<TopoDS_Wire> make_wire(const FeatureId& id,
                                            const std::vector<Segment>& section, bool closed,
                                            Factory&& factory) {
  if (section.empty())
    return Result<TopoDS_Wire>::failure(loft_error(id, "loft section must not be empty"));
  for (std::size_t index = 1U; index < section.size(); ++index)
    if (distance(section[index - 1U].end, section[index].start) > kTolerance)
      return Result<TopoDS_Wire>::failure(
          loft_error(id, "loft section edges must remain connected in authored order"));
  if (closed && distance(section.back().end, section.front().start) > kTolerance)
    return Result<TopoDS_Wire>::failure(loft_error(id, "solid loft section must be closed"));
  if (!closed && distance(section.back().end, section.front().start) <= kTolerance)
    return Result<TopoDS_Wire>::failure(loft_error(id, "open loft section must remain open"));
  BRepBuilderAPI_MakeWire wire;
  for (const auto& segment : section) {
    auto edge = factory(segment);
    if (edge.has_error())
      return Result<TopoDS_Wire>::failure(edge.error());
    wire.Add(edge.value());
  }
  if (!wire.IsDone())
    return Result<TopoDS_Wire>::failure(loft_error(id, "could not build loft section wire"));
  return Result<TopoDS_Wire>::success(wire.Wire());
}

template <typename Segment, typename Factory>
[[nodiscard]] Result<TopoDS_Shape>
execute_loft(const FeatureId& id, const std::vector<std::vector<Segment>>& sections, bool closed,
             bool solid, LoftContinuity continuity, Factory&& factory) {
  if (sections.size() < 2U)
    return Result<TopoDS_Shape>::failure(
        loft_error(id, "loft requires at least two ordered sections"));
  const std::size_t authored_edge_count = sections.front().size();
  for (const auto& section : sections)
    if (section.size() != authored_edge_count)
      return Result<TopoDS_Shape>::failure(
          loft_error(id, "all loft sections require matching authored edge counts"));
  try {
    BRepOffsetAPI_ThruSections builder(solid, Standard_False, kTolerance);
    builder.CheckCompatibility(Standard_False);
    if (continuity == LoftContinuity::G1)
      builder.SetContinuity(GeomAbs_C1);
    for (const auto& section : sections) {
      auto wire = make_wire(id, section, closed, factory);
      if (wire.has_error())
        return Result<TopoDS_Shape>::failure(wire.error());
      builder.AddWire(wire.value());
    }
    builder.Build();
    if (!builder.IsDone())
      return Result<TopoDS_Shape>::failure(loft_error(id, "OCCT loft did not complete"));
    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull() || !BRepCheck_Analyzer(shape, true).IsValid())
      return Result<TopoDS_Shape>::failure(loft_error(id, "loft result is invalid"));
    if (solid) {
      if (!TopExp_Explorer(shape, TopAbs_SOLID).More())
        return Result<TopoDS_Shape>::failure(loft_error(id, "loft produced no solid"));
      GProp_GProps properties;
      BRepGProp::VolumeProperties(shape, properties);
      if (!std::isfinite(properties.Mass()) || properties.Mass() <= kTolerance)
        return Result<TopoDS_Shape>::failure(loft_error(id, "loft has no positive volume"));
    } else if (!TopExp_Explorer(shape, TopAbs_FACE).More() ||
               TopExp_Explorer(shape, TopAbs_SOLID).More()) {
      return Result<TopoDS_Shape>::failure(
          loft_error(id, "surface loft must produce faces without a solid"));
    }
    if (continuity == LoftContinuity::G1) {
      for (TopExp_Explorer face(shape, TopAbs_FACE); face.More(); face.Next()) {
        Handle(Geom_Surface) surface = BRep_Tool::Surface(TopoDS::Face(face.Current()));
        if (surface.IsNull() || surface->Continuity() < GeomAbs_C1)
          return Result<TopoDS_Shape>::failure(
              loft_error(id, "loft result does not provide the requested G1/C1 continuity"));
      }
    }
    return Result<TopoDS_Shape>::success(std::move(shape));
  } catch (const Standard_Failure& failure) {
    return Result<TopoDS_Shape>::failure(loft_error(id, failure_message(failure)));
  } catch (const std::exception& exception) {
    return Result<TopoDS_Shape>::failure(loft_error(id, exception.what()));
  } catch (...) {
    return Result<TopoDS_Shape>::failure(loft_error(id, "unknown loft geometry error"));
  }
}

} // namespace

Result<GeometryShape> LoftAdapter::loft_closed_sections(
    FeatureId feature_id,
    const std::vector<std::vector<ClosedProfileCurveSegment>>& ordered_sections, bool make_solid,
    const std::vector<SweepPathSegment>* center_path,
    const std::vector<std::vector<SweepPathSegment>>& guide_curves,
    LoftContinuity continuity) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("loft", "feature id must not be empty"));
  if (continuity == LoftContinuity::G2)
    return Result<GeometryShape>::failure(loft_error(
        feature_id, "G2 loft continuity is unsupported without a verified curvature guarantee"));
  auto sections =
      controlled_closed_sections(feature_id, ordered_sections, center_path, guide_curves);
  if (sections.has_error())
    return Result<GeometryShape>::failure(sections.error());
  auto shape =
      execute_loft(feature_id, sections.value(), true, make_solid, continuity,
                   [&feature_id](const auto& segment) { return closed_edge(feature_id, segment); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

Result<GeometryShape>
LoftAdapter::loft_open_sections(FeatureId feature_id,
                                const std::vector<std::vector<SweepPathSegment>>& ordered_sections,
                                const std::vector<SweepPathSegment>* center_path,
                                const std::vector<std::vector<SweepPathSegment>>& guide_curves,
                                LoftContinuity continuity) const {
  if (feature_id.empty())
    return Result<GeometryShape>::failure(
        Error::validation("loft", "feature id must not be empty"));
  if (continuity == LoftContinuity::G2)
    return Result<GeometryShape>::failure(loft_error(
        feature_id, "G2 loft continuity is unsupported without a verified curvature guarantee"));
  auto sections = controlled_open_sections(feature_id, ordered_sections, center_path, guide_curves);
  if (sections.has_error())
    return Result<GeometryShape>::failure(sections.error());
  auto shape =
      execute_loft(feature_id, sections.value(), false, false, continuity,
                   [&feature_id](const auto& segment) { return open_edge(feature_id, segment); });
  if (shape.has_error())
    return Result<GeometryShape>::failure(shape.error());
  return Result<GeometryShape>::success(
      GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape.value()))));
}

} // namespace blcad::geometry
