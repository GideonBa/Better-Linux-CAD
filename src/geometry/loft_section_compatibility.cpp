#include "blcad/geometry/loft_section_compatibility.hpp"

#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Curve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace blcad::geometry {
namespace {
using Segment = ClosedProfileCurveSegment;
using Compatible = LoftCompatibleSections<Segment>;
constexpr double tolerance = 1e-8;
constexpr std::size_t max_section_edges = 4096;
constexpr std::size_t max_total_edges = 65536;

gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
Point3 lerp(Point3 a, Point3 b, double t) {
  return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}
bool finite(Point3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
Handle(Geom_Curve) curve(const Segment& s) {
  if (s.kind == ClosedProfileCurveKind::CircularArc) {
    GC_MakeArcOfCircle arc(point(s.start), point(s.mid), point(s.end));
    if (!arc.IsDone())
      throw std::runtime_error("invalid circular Loft section");
    return arc.Value().get();
  }
  TColgp_Array1OfPnt poles(1, 4);
  TColStd_Array1OfReal weights(1, 4);
  const std::array points{s.start, s.mid, s.control2, s.end};
  for (int i = 1; i <= 4; ++i) {
    poles.SetValue(i, point(points[i - 1]));
    weights.SetValue(i, s.weights[i - 1]);
  }
  return new Geom_BezierCurve(poles, weights);
}
Vector3 tangent(const Segment& s, bool end) {
  if (s.kind == ClosedProfileCurveKind::Line)
    return {s.end.x - s.start.x, s.end.y - s.start.y, s.end.z - s.start.z};
  auto c = curve(s);
  gp_Pnt p;
  gp_Vec v;
  c->D1(end ? c->LastParameter() : c->FirstParameter(), p, v);
  return {v.X(), v.Y(), v.Z()};
}
bool smooth(const Segment& before, const Segment& after) {
  if (before.kind != after.kind)
    return false;
  const auto a = tangent(before, true), b = tangent(after, false);
  const double na = std::hypot(a.x, a.y, a.z), nb = std::hypot(b.x, b.y, b.z);
  if (na <= tolerance || nb <= tolerance)
    return false;
  return (a.x / na * (b.x / nb) + a.y / na * (b.y / nb) + a.z / na * (b.z / nb)) >= 1 - 1e-10;
}
Segment trim(const Segment& s, double first, double last) {
  if (first == 0 && last == 1)
    return s;
  auto result = s;
  if (s.kind == ClosedProfileCurveKind::Line) {
    result.start = lerp(s.start, s.end, first);
    result.end = lerp(s.start, s.end, last);
  } else if (s.kind == ClosedProfileCurveKind::CircularArc) {
    auto c = curve(s);
    const auto at = [&](double t) {
      return point(c->Value(std::lerp(c->FirstParameter(), c->LastParameter(), t)));
    };
    result.start = at(first);
    result.mid = at((first + last) / 2);
    result.end = at(last);
  } else {
    auto c = Handle(Geom_BezierCurve)::DownCast(curve(s));
    c->Segment(first, last);
    result.start = point(c->Pole(1));
    result.mid = point(c->Pole(2));
    result.control2 = point(c->Pole(3));
    result.end = point(c->Pole(4));
    for (int i = 1; i <= 4; ++i)
      result.weights[i - 1] = c->Weight(i);
  }
  return result;
}
struct Fraction {
  std::size_t numerator, denominator;
  Fraction(std::size_t n, std::size_t d) {
    const auto divisor = std::gcd(n, d);
    numerator = n / divisor;
    denominator = d / divisor;
  }
  bool operator<(const Fraction& other) const {
    // Counts are bounded before construction, so these products cannot overflow.
    return numerator * other.denominator < other.numerator * denominator;
  }
  bool operator==(const Fraction&) const = default;
};
Result<Compatible> refine(const FeatureId& id, const std::vector<std::vector<Segment>>& sections,
                          bool closed) {
  const auto fail = [&](std::string message) {
    return Result<Compatible>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (sections.size() < 2 || sections.front().empty())
    return fail("Loft requires at least two non-empty sections");
  Compatible result;
  result.adjusted = std::any_of(sections.begin(), sections.end(),
                                [&](const auto& s) { return s.size() != sections.front().size(); });
  if (!result.adjusted) {
    result.sections = sections;
    for (const auto& s : sections) {
      std::vector<std::size_t> indices(s.size() + (closed ? 0 : 1));
      std::iota(indices.begin(), indices.end(), 0);
      result.source_vertices.push_back(std::move(indices));
    }
    return Result<Compatible>::success(std::move(result));
  }
  if (sections.size() > max_total_edges)
    return fail("Loft section refinement exceeds its budget");
  try {
    std::vector<std::vector<std::size_t>> runs;
    std::size_t input_edges = 0;
    for (const auto& section : sections) {
      if (section.empty() || section.size() > max_section_edges)
        return fail("Loft automatic refinement requires 1 to 4096 edges per section");
      if (section.size() > max_total_edges - input_edges)
        return fail("Loft automatic refinement exceeds the input edge budget");
      input_edges += section.size();
      std::vector<std::size_t> boundaries{0};
      for (std::size_t i = 0; i < section.size(); ++i) {
        const auto& s = section[i];
        if (!finite(s.start) || !finite(s.end) || distance(s.start, s.end) <= tolerance)
          return fail("Loft section has a non-finite or degenerate edge");
        if (s.kind != ClosedProfileCurveKind::Line && !finite(s.mid))
          return fail("Loft section has a non-finite curve control");
        if (s.kind == ClosedProfileCurveKind::CubicBezierSpline &&
            (!finite(s.control2) || std::any_of(s.weights.begin(), s.weights.end(), [](double w) {
              return !std::isfinite(w) || w <= 0;
            })))
          return fail("Loft section has invalid rational spline controls");
        if (s.kind != ClosedProfileCurveKind::Line &&
            s.kind != ClosedProfileCurveKind::CircularArc &&
            s.kind != ClosedProfileCurveKind::CubicBezierSpline)
          return fail("Loft section has an unsupported edge kind");
        if (i && distance(section[i - 1].end, s.start) > tolerance)
          return fail("Loft section edges must remain connected in authored order");
        if (i && !smooth(section[i - 1], s))
          boundaries.push_back(i);
      }
      const auto gap = distance(section.back().end, section.front().start);
      if ((closed && gap > tolerance) || (!closed && gap <= tolerance))
        return fail("Loft section closure does not match its solid/surface intent");
      boundaries.push_back(section.size());
      runs.push_back(std::move(boundaries));
    }
    if (std::any_of(runs.begin(), runs.end(),
                    [&](const auto& r) { return r.size() != runs.front().size(); }))
      for (std::size_t i = 0; i < runs.size(); ++i)
        runs[i] = {0, sections[i].size()};

    result.sections.resize(sections.size());
    for (const auto& s : sections)
      result.source_vertices.emplace_back(s.size() + (closed ? 0 : 1));
    for (std::size_t run = 0; run + 1 < runs.front().size(); ++run) {
      std::vector<Fraction> breaks;
      for (std::size_t j = 0; j < sections.size(); ++j) {
        const auto count = runs[j][run + 1] - runs[j][run];
        for (std::size_t k = 0; k <= count; ++k)
          breaks.emplace_back(k, count);
      }
      std::sort(breaks.begin(), breaks.end());
      breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());
      const auto count = result.sections.front().size() + breaks.size() - 1;
      if (count > max_section_edges || count > max_total_edges / sections.size())
        return fail("Loft automatic refinement exceeds the derived edge budget");
      for (std::size_t j = 0; j < sections.size(); ++j) {
        const auto offset = runs[j][run], size = runs[j][run + 1] - offset;
        for (std::size_t k = 0; k + 1 < breaks.size(); ++k) {
          const auto a = breaks[k], b = breaks[k + 1];
          const auto source = a.numerator * size / a.denominator;
          const auto first_n = a.numerator * size - source * a.denominator;
          const auto last_n = b.numerator * size - source * b.denominator;
          if (first_n == 0)
            result.source_vertices[j][offset + source] = result.sections[j].size();
          result.sections[j].push_back(trim(sections[j][offset + source],
                                            static_cast<double>(first_n) / a.denominator,
                                            static_cast<double>(last_n) / b.denominator));
        }
      }
    }
    if (!closed)
      for (std::size_t j = 0; j < sections.size(); ++j)
        result.source_vertices[j].back() = result.sections[j].size();
    return Result<Compatible>::success(std::move(result));
  } catch (const Standard_Failure& error) {
    return fail(error.GetMessageString() ? error.GetMessageString() : "Loft refinement failed");
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace

Result<LoftCompatibleSections<ClosedProfileCurveSegment>>
make_loft_sections_compatible(const FeatureId& id,
                              const std::vector<std::vector<ClosedProfileCurveSegment>>& sections) {
  return refine(id, sections, true);
}

Result<LoftCompatibleSections<SweepPathSegment>>
make_loft_sections_compatible(const FeatureId& id,
                              const std::vector<std::vector<SweepPathSegment>>& sections) {
  std::vector<std::vector<Segment>> converted;
  for (const auto& section : sections) {
    converted.emplace_back();
    for (const auto& s : section) {
      if (s.kind != SweepPathSegmentKind::Line && s.kind != SweepPathSegmentKind::CircularArc)
        return Result<LoftCompatibleSections<SweepPathSegment>>::failure(
            Error::geometry(id.value(), "Loft section has an unsupported edge kind"));
      converted.back().push_back({s.kind == SweepPathSegmentKind::Line
                                      ? ClosedProfileCurveKind::Line
                                      : ClosedProfileCurveKind::CircularArc,
                                  s.start, s.mid, s.end});
    }
  }
  auto refined = refine(id, converted, false);
  if (!refined)
    return Result<LoftCompatibleSections<SweepPathSegment>>::failure(refined.error());
  LoftCompatibleSections<SweepPathSegment> result;
  result.adjusted = refined.value().adjusted;
  result.source_vertices = std::move(refined.value().source_vertices);
  for (const auto& section : refined.value().sections) {
    result.sections.emplace_back();
    for (const auto& s : section)
      result.sections.back().push_back({s.kind == ClosedProfileCurveKind::Line
                                            ? SweepPathSegmentKind::Line
                                            : SweepPathSegmentKind::CircularArc,
                                        s.start, s.mid, s.end});
  }
  return Result<LoftCompatibleSections<SweepPathSegment>>::success(std::move(result));
}
} // namespace blcad::geometry
