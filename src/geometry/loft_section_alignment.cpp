#include "blcad/geometry/loft_section_alignment.hpp"

#include "blcad/core/part_document.hpp"
#include "loft_section_alignment_internal.hpp"

#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Curve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace blcad::geometry {
namespace {
constexpr double tolerance = 1e-8;
constexpr double improvement_epsilon = 1e-12;
constexpr std::size_t max_section_edges = 4096;
constexpr std::size_t max_total_edges = 65536;
constexpr std::size_t curve_spans = 32;
constexpr std::size_t score_samples = 64;
using Samples = std::array<Point3, score_samples>;
using Suggestions = std::vector<ProfileSectionReference>;

Error alignment_error(std::string message) {
  return Error::geometry("loft.section_alignment", std::move(message));
}

bool finite(Point3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

gp_Pnt point(Point3 p) {
  return {p.x, p.y, p.z};
}

struct Contour {
  // The bounded polyline is used only to rank seams, never to construct a Loft.
  std::vector<Point3> points;
  std::vector<double> distances;
  std::vector<double> starts;
  Point3 center;
  double perimeter = 0;
};

Result<Contour> sample_contour(const std::vector<ClosedProfileCurveSegment>& curves) {
  const auto fail = [](std::string message) {
    return Result<Contour>::failure(alignment_error(std::move(message)));
  };
  Contour result;
  result.points.push_back(curves.front().start);
  result.distances.push_back(0);
  for (std::size_t i = 0; i < curves.size(); ++i) {
    const auto& curve = curves[i];
    if (!finite(curve.start) || !finite(curve.end) || distance(curve.start, curve.end) <= tolerance)
      return fail("Loft alignment requires finite, nondegenerate edges");
    if (distance(result.points.back(), curve.start) > tolerance)
      return fail("Loft alignment requires connected profile edges");
    result.starts.push_back(result.perimeter);
    Handle(Geom_Curve) evaluator;
    if (curve.kind == ClosedProfileCurveKind::CircularArc) {
      if (!finite(curve.mid))
        return fail("Loft alignment requires finite arc controls");
      GC_MakeArcOfCircle arc(point(curve.start), point(curve.mid), point(curve.end));
      if (!arc.IsDone())
        return fail("Loft alignment encountered an invalid circular arc");
      evaluator = arc.Value().get();
    } else if (curve.kind == ClosedProfileCurveKind::CubicBezierSpline) {
      if (!finite(curve.mid) || !finite(curve.control2) ||
          std::any_of(curve.weights.begin(), curve.weights.end(),
                      [](double w) { return !std::isfinite(w) || w <= 0; }))
        return fail("Loft alignment requires finite rational spline controls");
      TColgp_Array1OfPnt poles(1, 4);
      TColStd_Array1OfReal weights(1, 4);
      const std::array controls{curve.start, curve.mid, curve.control2, curve.end};
      for (int p = 1; p <= 4; ++p) {
        poles.SetValue(p, point(controls[p - 1]));
        weights.SetValue(p, curve.weights[p - 1]);
      }
      evaluator = new Geom_BezierCurve(poles, weights);
    } else if (curve.kind != ClosedProfileCurveKind::Line) {
      return fail("Loft alignment encountered an unsupported curve kind");
    }
    const std::size_t spans = evaluator.IsNull() ? 1 : curve_spans;
    for (std::size_t span = 1; span <= spans; ++span) {
      Point3 next = curve.end;
      if (span != spans) {
        const double t = static_cast<double>(span) / static_cast<double>(spans);
        const auto value =
            evaluator->Value(std::lerp(evaluator->FirstParameter(), evaluator->LastParameter(), t));
        next = {value.X(), value.Y(), value.Z()};
      }
      const double step = distance(result.points.back(), next);
      if (!finite(next) || !std::isfinite(step))
        return fail("Loft alignment encountered a non-finite curve sample");
      result.perimeter += step;
      result.points.push_back(next);
      result.distances.push_back(result.perimeter);
    }
  }
  if (distance(result.points.back(), result.points.front()) > tolerance ||
      !std::isfinite(result.perimeter) || result.perimeter <= tolerance)
    return fail("Loft alignment requires a finite closed profile");

  // Length-weighted centroid avoids weighting each authored vertex equally.
  // Curved lengths remain approximations from the bounded sampling table. Work
  // relative to the first point to avoid summing large global coordinates.
  const auto origin = result.points.front();
  Point3 offset;
  for (std::size_t i = 1; i < result.points.size(); ++i) {
    const double weight = (result.distances[i] - result.distances[i - 1]) / result.perimeter;
    const auto a = result.points[i - 1], b = result.points[i];
    offset.x += ((a.x - origin.x) + (b.x - origin.x)) * .5 * weight;
    offset.y += ((a.y - origin.y) + (b.y - origin.y)) * .5 * weight;
    offset.z += ((a.z - origin.z) + (b.z - origin.z)) * .5 * weight;
  }
  result.center = {origin.x + offset.x, origin.y + offset.y, origin.z + offset.z};
  if (!finite(result.center))
    return fail("Loft alignment encountered a non-finite profile center");
  return Result<Contour>::success(std::move(result));
}

Samples samples(const Contour& contour, std::size_t start, bool reverse) {
  Samples result;
  for (std::size_t i = 0; i < result.size(); ++i) {
    const double fraction = static_cast<double>(i) / static_cast<double>(result.size());
    double position =
        std::fmod(contour.starts[start] + (reverse ? -fraction : fraction) * contour.perimeter,
                  contour.perimeter);
    if (position < 0)
      position += contour.perimeter;
    // Adding a tiny negative remainder to the perimeter can round up exactly
    // to the closing distance; it still identifies the first contour point.
    if (position >= contour.perimeter)
      position = 0;
    const auto upper =
        std::upper_bound(contour.distances.begin(), contour.distances.end(), position);
    const std::size_t after =
        std::min(static_cast<std::size_t>(std::distance(contour.distances.begin(), upper)),
                 contour.points.size() - 1);
    const std::size_t before = after - 1;
    const double ratio = (position - contour.distances[before]) /
                         (contour.distances[after] - contour.distances[before]);
    const auto a = contour.points[before], b = contour.points[after];
    result[i] = {(std::lerp(a.x, b.x, ratio) - contour.center.x) / contour.perimeter,
                 (std::lerp(a.y, b.y, ratio) - contour.center.y) / contour.perimeter,
                 (std::lerp(a.z, b.z, ratio) - contour.center.z) / contour.perimeter};
  }
  return result;
}

double score(const Samples& first, const Samples& second) {
  double sum = 0;
  for (std::size_t i = 0; i < first.size(); ++i) {
    const auto delta = displacement(first[i], second[i]);
    sum += dot(delta, delta);
  }
  return sum / static_cast<double>(first.size());
}
} // namespace

Result<Suggestions> suggest_loft_section_alignment(const PartDocument& document,
                                                   const Suggestions& sections,
                                                   const ShapeCache& cache) {
  if (sections.size() < 2 || std::any_of(sections.begin(), sections.end(), [](const auto& section) {
        return section.kind() != LoftSectionKind::ClosedRegion ||
               section.alignment_reference().has_value() || section.rotation_offset().has_value();
      }))
    return Result<Suggestions>::success(sections);
  if (sections.size() > max_total_edges)
    return Result<Suggestions>::failure(
        alignment_error("Loft alignment exceeds its section budget"));

  try {
    Suggestions result;
    Samples previous;
    std::size_t total_edges = 0;
    for (std::size_t index = 0; index < sections.size(); ++index) {
      const auto& original = sections[index];
      const auto& profile = std::get<ProfileRegionReference>(original.source());
      auto base = ProfileSectionReference::create_closed_region(profile);
      if (!base)
        return Result<Suggestions>::failure(base.error());
      auto curves = detail::resolve_loft_alignment_section(document, base.value(), cache);
      if (!curves)
        return Result<Suggestions>::failure(curves.error());
      if (curves.value().empty() || curves.value().size() > max_section_edges ||
          curves.value().size() > max_total_edges - total_edges)
        return Result<Suggestions>::failure(alignment_error(
            "Loft alignment requires at most 4096 edges per section and 65536 total"));
      total_edges += curves.value().size();
      const auto* sketch = document.find_sketch(profile.sketch());
      // The resolver has already established the Sketch and supported profile.
      std::vector<SketchEntityId> ids;
      if (const auto* closed = sketch->find_closed_profile(profile.profile()))
        ids = closed->line_segments();
      else if (const auto* closed = sketch->find_arc_closed_profile(profile.profile()))
        ids = closed->curve_segments();
      if (!ids.empty() && ids.size() != curves.value().size())
        return Result<Suggestions>::failure(
            alignment_error("Loft alignment boundary ids do not match"));
      auto contour = sample_contour(curves.value());
      if (!contour)
        return Result<Suggestions>::failure(contour.error());
      std::size_t best_start = 0;
      bool best_reverse = false;
      auto best_samples = samples(contour.value(), 0, false);
      if (index != 0) {
        double best_score = score(previous, best_samples);
        const std::size_t candidates = ids.empty() ? 1 : ids.size();
        // Keep authored seam/direction in a tie; then prefer authored direction
        // over a reversal. Candidate order never depends on hash iteration.
        for (const bool reverse : {false, true})
          for (std::size_t start = 0; start < candidates; ++start) {
            auto candidate = samples(contour.value(), start, reverse);
            const double candidate_score = score(previous, candidate);
            if (candidate_score + improvement_epsilon < best_score) {
              best_score = candidate_score;
              best_start = start;
              best_reverse = reverse;
              best_samples = std::move(candidate);
            }
          }
      }
      // Automatic correspondence is computed before any local user flip, so
      // flipping one row cannot silently reverse every following section.
      previous = best_samples;
      std::optional<SketchEntityId> alignment;
      if (best_start != 0)
        alignment = ids[best_start];
      auto aligned = ProfileSectionReference::create_closed_region(
          profile, std::move(alignment), best_reverse != original.flip_normal());
      if (!aligned)
        return Result<Suggestions>::failure(aligned.error());
      result.push_back(std::move(aligned.value()));
    }
    return Result<Suggestions>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    const auto* message = failure.GetMessageString();
    return Result<Suggestions>::failure(
        alignment_error(message ? message : "OCCT Loft alignment sampling failed"));
  } catch (const std::exception& failure) {
    return Result<Suggestions>::failure(alignment_error(failure.what()));
  }
}

} // namespace blcad::geometry
