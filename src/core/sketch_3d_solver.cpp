#include "blcad/core/sketch_3d_solver.hpp"
#include "blcad/core/part_document.hpp"
#include "dense_solver_math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>

namespace blcad {
namespace {
using Vector = std::vector<double>;
using Matrix = std::vector<Vector>;
Error error(std::string message) {
  return Error::validation("sketch3d.solver", std::move(message));
}
std::string reason(const Error& e) {
  return e.object_id() + ": " + e.message();
}
Vector3 mul(Vector3 v, double s) {
  return {v.x * s, v.y * s, v.z * s};
}
Vector3 add(Vector3 a, Vector3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
bool finite(Vector3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
Result<Vector3> unit(Vector3 v) {
  const double n = length(v);
  if (!finite(v) || !std::isfinite(n) || n <= 1e-14)
    return Result<Vector3>::failure(error("Direction or curve speed is singular"));
  return Result<Vector3>::success(mul(v, 1 / n));
}
Result<Point3> owned_point(const PartDocument& part, const SketchPoint3D& p) {
  Point3 out;
  const std::array<const SketchCoordinate3D*, 3> c{&p.x(), &p.y(), &p.z()};
  const std::array<double*, 3> v{&out.x, &out.y, &out.z};
  for (std::size_t i = 0; i < 3; ++i) {
    if (c[i]->explicit_coordinate())
      *v[i] = c[i]->explicit_coordinate()->millimeters();
    else {
      const auto* parameter = part.find_parameter(*c[i]->parameter());
      if (!parameter || parameter->type() != ParameterType::Length)
        return Result<Point3>::failure(error("Point coordinate source is unavailable"));
      *v[i] = parameter->value().millimeters();
    }
  }
  return Result<Point3>::success(out);
}
struct Context {
  const PartDocument& part;
  const Sketch3D& source;
  const SpatialSolverGeometry& geometry;
  const SpatialSolverOptions& options;
  std::vector<SpatialVariable> layout{};
  std::vector<const SpatialConstraint*> constraints{};
  Point3 origin{};
  double scale{1};
  std::size_t evaluations{0};
  std::vector<std::size_t> rows{};
  Result<Sketch3D> candidate(const Vector& values) const {
    auto s = source;
    for (const auto& p : source.points()) {
      auto resolved = owned_point(part, p);
      if (!resolved)
        return Result<Sketch3D>::failure(resolved.error());
      auto pos = resolved.value();
      std::array<double*, 3> components{&pos.x, &pos.y, &pos.z};
      const std::array<double, 3> offset{origin.x, origin.y, origin.z};
      bool changed = false;
      for (std::size_t i = 0; i < layout.size(); ++i) {
        if (layout[i].owner_id != p.id().value())
          continue;
        const auto k = layout[i].kind;
        if (k != SpatialVariableKind::PointX && k != SpatialVariableKind::PointY &&
            k != SpatialVariableKind::PointZ)
          continue;
        const auto axis = static_cast<std::size_t>(k);
        if (values[i] == (layout[i].initial_value - offset[axis]) / scale)
          continue;
        *components[axis] = offset[axis] + values[i] * scale;
        changed = true;
      }
      if (!changed)
        continue;
      auto coordinate = [](double v) -> Result<SketchCoordinate3D> {
        auto q = Quantity::linear_displacement_mm(v);
        return q ? SketchCoordinate3D::create_explicit(q.value())
                 : Result<SketchCoordinate3D>::failure(q.error());
      };
      auto x = pos.x == resolved.value().x ? Result<SketchCoordinate3D>::success(p.x())
                                           : coordinate(pos.x);
      auto y = pos.y == resolved.value().y ? Result<SketchCoordinate3D>::success(p.y())
                                           : coordinate(pos.y);
      auto z = pos.z == resolved.value().z ? Result<SketchCoordinate3D>::success(p.z())
                                           : coordinate(pos.z);
      if (!x || !y || !z)
        return Result<Sketch3D>::failure(error("Nonfinite candidate coordinate"));
      auto value = SketchPoint3D::create(p.id(), x.value(), y.value(), z.value());
      if (!value)
        return Result<Sketch3D>::failure(value.error());
      auto replaced = s.replace_point(value.value());
      if (!replaced)
        return Result<Sketch3D>::failure(replaced.error());
    }
    for (const auto& h : source.helices()) {
      auto r = h.radius_binding(), p = h.pitch_binding();
      bool changed = false;
      for (std::size_t i = 0; i < layout.size(); ++i) {
        if (layout[i].owner_id != h.id().value())
          continue;
        if (layout[i].kind != SpatialVariableKind::HelixRadius &&
            layout[i].kind != SpatialVariableKind::HelixPitch)
          continue;
        if (values[i] == layout[i].initial_value / scale)
          continue;
        auto q = Quantity::length_mm(values[i] * scale);
        if (!q)
          return Result<Sketch3D>::failure(q.error());
        auto b = SketchLength3D::create_explicit(q.value());
        if (!b)
          return Result<Sketch3D>::failure(b.error());
        (layout[i].kind == SpatialVariableKind::HelixRadius ? r : p) = b.value();
        changed = true;
      }
      if (changed) {
        auto value = SketchHelix3D::create_with_bindings(h.id(), h.axis(), r, p,
                                                         h.turns_parameter(), h.handedness());
        if (!value)
          return Result<Sketch3D>::failure(value.error());
        auto replaced = s.replace_helix(value.value());
        if (!replaced)
          return Result<Sketch3D>::failure(replaced.error());
      }
    }
    return Result<Sketch3D>::success(std::move(s));
  }
  Result<Point3> reference(const Sketch3D& s, const SketchPointReference3D& r) const {
    if (r.local_point()) {
      const auto* p = s.find_point(*r.local_point());
      return p ? owned_point(part, *p)
               : Result<Point3>::failure(error("Local point is unavailable"));
    }
    if (!geometry.external_point)
      return Result<Point3>::failure(error("External point query is unavailable"));
    return geometry.external_point(r);
  }
  Result<SpatialCurveJet> jet(const Sketch3D& s, const SpatialConstraintTarget& target,
                              double u) const {
    if (const auto* local = std::get_if<SpatialLocalCurve>(&target.source())) {
      if (const auto* line = s.find_line(local->entity)) {
        auto a = owned_point(part, *s.find_point(line->start_point()));
        auto b = owned_point(part, *s.find_point(line->end_point()));
        if (!a || !b)
          return Result<SpatialCurveJet>::failure(error("Line point is unavailable"));
        const auto d = displacement(a.value(), b.value());
        return Result<SpatialCurveJet>::success(
            {{a.value().x + u * d.x, a.value().y + u * d.y, a.value().z + u * d.z}, d, {}});
      }
    }
    if (!geometry.curve)
      return Result<SpatialCurveJet>::failure(error("Native curve query is unavailable"));
    return geometry.curve(s, target, u);
  }
  Result<Point3> point(const Sketch3D& s, const SpatialConstraintTarget& target) const {
    if (const auto* r = std::get_if<SketchPointReference3D>(&target.source()))
      return reference(s, *r);
    auto j = jet(s, target, target.curve_location() == SpatialCurveLocation::End ? 1 : 0);
    return j ? Result<Point3>::success(j.value().point) : Result<Point3>::failure(j.error());
  }
  Result<SpatialAxisFrame> axis(const Sketch3D& s, const SpatialConstraintTarget& target) const {
    if (const auto* r = std::get_if<AxisReference>(&target.source())) {
      if (!geometry.axis)
        return Result<SpatialAxisFrame>::failure(error("Axis query is unavailable"));
      auto a = geometry.axis(*r);
      if (!a)
        return a;
      auto n = unit(a.value().direction);
      return n ? Result<SpatialAxisFrame>::success({a.value().origin, n.value()})
               : Result<SpatialAxisFrame>::failure(n.error());
    }
    auto j = jet(s, target, 0);
    if (!j)
      return Result<SpatialAxisFrame>::failure(j.error());
    auto n = unit(j.value().first);
    return n ? Result<SpatialAxisFrame>::success({j.value().point, n.value()})
             : Result<SpatialAxisFrame>::failure(n.error());
  }
  Result<Vector> evaluate(const Vector& values) {
    if (evaluations >= options.maximum_evaluations)
      return Result<Vector>::failure(error("Deterministic evaluation budget exhausted"));
    ++evaluations;
    for (std::size_t i = 0; i < values.size(); ++i)
      if (!std::isfinite(values[i]) || (layout[i].kind == SpatialVariableKind::CurveParameter &&
                                        (values[i] < 0 || values[i] > 1)))
        return Result<Vector>::failure(
            error("Candidate variable lies outside its authored domain"));
    auto c = candidate(values);
    if (!c)
      return Result<Vector>::failure(c.error());
    const auto& s = c.value();
    if (geometry.prepare_candidate) {
      auto prepared = geometry.prepare_candidate(s);
      if (!prepared)
        return Result<Vector>::failure(prepared.error());
    }
    Vector residuals;
    rows.clear();
    const auto vector = [&](Vector3 v, double factor = 1.) {
      residuals.push_back(v.x * factor);
      residuals.push_back(v.y * factor);
      residuals.push_back(v.z * factor);
    };
    for (const auto* constraint : constraints) {
      const auto& t = constraint->targets();
      const auto start = residuals.size();
      const auto fail = [&](const Error& e) {
        return Result<Vector>::failure(Error::validation(constraint->id().value(), e.message()));
      };
      double dimension = 0;
      if (constraint->value()) {
        auto q = constraint->value()->resolve(part);
        if (!q)
          return fail(q.error());
        dimension = constraint->kind() == SpatialConstraintKind::Angle
                        ? q.value().degrees() * std::numbers::pi / 180
                        : q.value().millimeters();
      }
      const auto kind = constraint->kind();
      if (kind == SpatialConstraintKind::Coincident || kind == SpatialConstraintKind::Fixed ||
          kind == SpatialConstraintKind::Distance) {
        auto a = point(s, t[0]);
        if (!a)
          return fail(a.error());
        auto b = kind == SpatialConstraintKind::Fixed
                     ? Result<Point3>::success(*constraint->fixed_position())
                     : point(s, t[1]);
        if (!b)
          return fail(b.error());
        if (kind == SpatialConstraintKind::Distance)
          residuals.push_back((distance(a.value(), b.value()) - dimension) / scale);
        else
          vector(displacement(b.value(), a.value()), 1 / scale);
      } else if (kind == SpatialConstraintKind::PointOnLine) {
        auto p = point(s, t[0]);
        auto a = axis(s, t[1]);
        if (!p)
          return fail(p.error());
        if (!a)
          return fail(a.error());
        vector(cross(displacement(a.value().origin, p.value()), a.value().direction), 1 / scale);
      } else if (kind == SpatialConstraintKind::PointOnPlane ||
                 kind == SpatialConstraintKind::PlaneParallel) {
        if (!geometry.plane)
          return fail(error("Plane query is unavailable"));
        auto p = geometry.plane(std::get<PlaneReference>(t[1].source()));
        if (!p)
          return fail(p.error());
        auto n = unit(p.value().normal);
        if (!n)
          return fail(n.error());
        if (kind == SpatialConstraintKind::PointOnPlane) {
          auto a = point(s, t[0]);
          if (!a)
            return fail(a.error());
          residuals.push_back(dot(displacement(p.value().origin, a.value()), n.value()) / scale);
        } else {
          auto a = axis(s, t[0]);
          if (!a)
            return fail(a.error());
          residuals.push_back(dot(a.value().direction, n.value()));
        }
      } else if (kind == SpatialConstraintKind::PointOnCurve) {
        auto p = point(s, t[0]);
        if (!p)
          return fail(p.error());
        auto variable = std::find_if(layout.begin(), layout.end(), [&](const auto& v) {
          return v.kind == SpatialVariableKind::CurveParameter &&
                 v.owner_id == constraint->id().value();
        });
        if (variable == layout.end())
          return fail(error("Missing curve parameter variable"));
        auto j = jet(s, t[1], values[static_cast<std::size_t>(variable - layout.begin())]);
        if (!j)
          return fail(j.error());
        auto speed = unit(j.value().first);
        if (!speed)
          return fail(speed.error());
        vector(displacement(j.value().point, p.value()), 1 / scale);
      } else if (kind == SpatialConstraintKind::PointOnFace) {
        auto p = point(s, t[0]);
        if (!p)
          return fail(p.error());
        if (!geometry.project_face)
          return fail(error("Trimmed-face projection is unavailable"));
        auto q = geometry.project_face(std::get<FaceReference>(t[1].source()), p.value());
        if (!q)
          return fail(q.error());
        vector(displacement(q.value(), p.value()), 1 / scale);
      } else if (kind == SpatialConstraintKind::Tangent || kind == SpatialConstraintKind::Smooth) {
        auto a = jet(s, t[0], t[0].curve_location() == SpatialCurveLocation::End ? 1 : 0);
        auto b = jet(s, t[1], t[1].curve_location() == SpatialCurveLocation::End ? 1 : 0);
        if (!a)
          return fail(a.error());
        if (!b)
          return fail(b.error());
        auto ta = unit(a.value().first), tb = unit(b.value().first);
        if (!ta)
          return fail(ta.error());
        if (!tb)
          return fail(tb.error());
        vector(displacement(b.value().point, a.value().point), 1 / scale);
        vector(add(mul(ta.value(), t[0].curve_location() == SpatialCurveLocation::End ? -1 : 1),
                   mul(tb.value(), t[1].curve_location() == SpatialCurveLocation::End ? -1 : 1)));
        if (kind == SpatialConstraintKind::Smooth) {
          const auto curvature = [](const SpatialCurveJet& j, Vector3 tangent) {
            return mul(add(j.second, mul(tangent, -dot(tangent, j.second))),
                       1 / dot(j.first, j.first));
          };
          vector(add(curvature(a.value(), ta.value()), mul(curvature(b.value(), tb.value()), -1)),
                 scale);
        }
      } else if (kind == SpatialConstraintKind::Radius || kind == SpatialConstraintKind::Length) {
        const auto& query =
            kind == SpatialConstraintKind::Radius ? geometry.radius : geometry.curve_length;
        if (!query)
          return fail(error("Native curve measure is unavailable"));
        auto measure = query(s, t[0]);
        if (!measure)
          return fail(measure.error());
        if (!std::isfinite(measure.value()) || measure.value() <= 0)
          return fail(error("Curve measure is singular"));
        residuals.push_back((measure.value() - dimension) / scale);
      } else {
        auto a = axis(s, t[0]), b = axis(s, t[1]);
        if (!a)
          return fail(a.error());
        if (!b)
          return fail(b.error());
        const auto x = a.value().direction, y = b.value().direction;
        if (kind == SpatialConstraintKind::Perpendicular)
          residuals.push_back(dot(x, y));
        else if (kind == SpatialConstraintKind::Angle) {
          if (constraint->angle_convention() == SpatialAngleConvention::OrientedAboutAxis) {
            auto az = axis(s, t[2]);
            if (!az)
              return fail(az.error());
            const auto z = az.value().direction;
            auto px = unit(add(x, mul(z, -dot(x, z)))), py = unit(add(y, mul(z, -dot(y, z))));
            if (!px)
              return fail(px.error());
            if (!py)
              return fail(py.error());
            residuals.push_back(std::remainder(
                std::atan2(dot(z, cross(px.value(), py.value())), dot(px.value(), py.value())) -
                    dimension,
                2 * std::numbers::pi));
          } else if (dimension == 0)
            vector(add(x, mul(y, -1)));
          else if (dimension == std::numbers::pi)
            vector(add(x, y));
          else
            residuals.push_back(std::atan2(length(cross(x, y)), dot(x, y)) - dimension);
        } else
          vector(cross(x, y));
      }
      rows.push_back(residuals.size() - start);
    }
    if (!std::all_of(residuals.begin(), residuals.end(), [](double v) { return std::isfinite(v); }))
      return Result<Vector>::failure(error("Nonfinite residual"));
    return Result<Vector>::success(std::move(residuals));
  }
  Result<std::size_t> initialize_auxiliaries(Vector& values) {
    if (geometry.prepare_candidate) {
      auto prepared = geometry.prepare_candidate(source);
      if (!prepared)
        return prepared;
    }
    std::size_t initialized = 0;
    for (const auto* constraint : constraints) {
      if (constraint->kind() != SpatialConstraintKind::PointOnCurve)
        continue;
      const auto variable = std::find_if(layout.begin(), layout.end(), [&](const auto& v) {
        return v.kind == SpatialVariableKind::CurveParameter &&
               v.owner_id == constraint->id().value();
      });
      if (variable == layout.end())
        return Result<std::size_t>::failure(error("Missing curve parameter"));
      const auto index = static_cast<std::size_t>(variable - layout.begin());
      const auto target = point(source, constraint->targets()[0]);
      if (!target)
        return Result<std::size_t>::failure(target.error());
      const auto sample = [&](double u) -> Result<SpatialCurveJet> {
        if (evaluations >= options.maximum_evaluations)
          return Result<SpatialCurveJet>::failure(
              error("Deterministic evaluation budget exhausted"));
        ++evaluations;
        auto value = jet(source, constraint->targets()[1], u);
        if (!value)
          return Result<SpatialCurveJet>::failure(
              Error::validation(constraint->id().value(), value.error().message()));
        return value;
      };
      double u = values[index];
      auto current = sample(u);
      if (!current)
        return Result<std::size_t>::failure(current.error());
      for (std::size_t iteration = 0; iteration < options.maximum_curve_projection_iterations;
           ++iteration) {
        const auto delta = displacement(target.value(), current.value().point);
        const double speed2 = dot(current.value().first, current.value().first);
        if (!std::isfinite(speed2) || speed2 <= 1e-28)
          return Result<std::size_t>::failure(
              Error::validation(constraint->id().value(), "Curve branch has singular speed"));
        const double step = dot(delta, current.value().first) / speed2;
        if (!std::isfinite(step))
          return Result<std::size_t>::failure(error("Nonfinite branch projection"));
        if (std::abs(step) <= 1e-13)
          break;
        bool improved = false;
        for (std::size_t line = 0; line < options.numeric.maximum_line_search_steps; ++line) {
          const double next = std::clamp(u - std::ldexp(step, -static_cast<int>(line)), 0., 1.);
          if (next == u)
            break;
          auto trial = sample(next);
          if (!trial)
            return Result<std::size_t>::failure(trial.error());
          if (distance(trial.value().point, target.value()) <
              distance(current.value().point, target.value())) {
            current = std::move(trial);
            u = next;
            improved = true;
            break;
          }
        }
        if (!improved)
          break;
      }
      values[index] = u;
      ++initialized;
    }
    return Result<std::size_t>::success(initialized);
  }

  Result<Matrix> jacobian(const Vector& x, const Vector& center) {
    Matrix j(center.size(), Vector(x.size()));
    const auto saved_rows = rows;
    for (std::size_t col = 0; col < x.size(); ++col) {
      const double h = options.numeric.finite_difference_relative_step;
      auto plus = x, minus = x;
      plus[col] += h;
      minus[col] -= h;
      // A one-sided derivative at the authored [0,1] boundary keeps endpoint
      // branches valid.
      const bool lower =
          (layout[col].kind == SpatialVariableKind::CurveParameter && minus[col] < 0) ||
          ((layout[col].kind == SpatialVariableKind::HelixRadius ||
            layout[col].kind == SpatialVariableKind::HelixPitch) &&
           minus[col] <= 0);
      const bool upper = layout[col].kind == SpatialVariableKind::CurveParameter && plus[col] > 1;
      auto p = upper ? Result<Vector>::success(center) : evaluate(plus);
      auto m = lower ? Result<Vector>::success(center) : evaluate(minus);
      if (!p)
        return Result<Matrix>::failure(p.error());
      if (!m)
        return Result<Matrix>::failure(m.error());
      if (p.value().size() != center.size() || m.value().size() != center.size())
        return Result<Matrix>::failure(error("Residual capability changed during differentiation"));
      for (std::size_t row = 0; row < j.size(); ++row)
        j[row][col] = (p.value()[row] - m.value()[row]) / ((lower || upper) ? h : 2 * h);
    }
    for (const auto& row : j)
      for (double value : row)
        if (!std::isfinite(value))
          return Result<Matrix>::failure(error("Nonfinite normalized Jacobian"));
    rows = saved_rows;
    return Result<Matrix>::success(std::move(j));
  }
};
double norm2(const Vector& v) {
  return std::inner_product(v.begin(), v.end(), v.begin(), 0.);
}
std::size_t rank(const Matrix& m, std::size_t columns, const SpatialSolverOptions& o) {
  auto value = detail::matrix_rank(m, columns, o.numeric);
  return value ? value.value() : 0;
}
std::size_t expected_rank(const SpatialConstraint& c, const PartDocument& part) {
  switch (c.kind()) {
  case SpatialConstraintKind::Coincident:
  case SpatialConstraintKind::Fixed:
    return 3;
  case SpatialConstraintKind::AxisParallel:
  case SpatialConstraintKind::PointOnLine:
  case SpatialConstraintKind::Parallel:
  case SpatialConstraintKind::PointOnCurve:
    return 2;
  case SpatialConstraintKind::Tangent:
    return 5;
  case SpatialConstraintKind::Smooth:
    return 7;
  case SpatialConstraintKind::Angle:
    if (c.angle_convention() == SpatialAngleConvention::Unsigned && c.value())
      if (const auto q = c.value()->resolve(part);
          q && (q.value().degrees() == 0 || q.value().degrees() == 180))
        return 2;
    return 1;
  default:
    return 1;
  }
}
// Image of locally admissible rigid velocities in the actual free coordinate
// space. Bound rows constrain rigid generators; auxiliary curve parameters may
// compensate motion along fixed curves.
std::size_t gauge(Context& c, const Sketch3D& solved, const Matrix& j) {
  std::vector<std::size_t> aux;
  for (std::size_t i = 0; i < c.layout.size(); ++i)
    if (c.layout[i].kind == SpatialVariableKind::CurveParameter)
      aux.push_back(i);
  const auto width = 6 + aux.size();
  Matrix rigid(c.layout.size(), Vector(6));
  Matrix bound;
  for (const auto& p : solved.points()) {
    auto value = owned_point(c.part, p);
    if (!value)
      continue;
    auto r = mul(displacement(c.origin, value.value()), 1 / c.scale);
    const std::array<Vector, 3> rows{Vector{1, 0, 0, 0, r.z, -r.y}, Vector{0, 1, 0, -r.z, 0, r.x},
                                     Vector{0, 0, 1, r.y, -r.x, 0}};
    const std::array<const SketchCoordinate3D*, 3> coordinates{&p.x(), &p.y(), &p.z()};
    for (std::size_t axis = 0; axis < 3; ++axis) {
      if (coordinates[axis]->parameter()) {
        auto row = rows[axis];
        row.resize(width);
        bound.push_back(std::move(row));
      } else {
        auto variable = std::find_if(c.layout.begin(), c.layout.end(), [&](const auto& v) {
          return v.owner_id == p.id().value() && static_cast<std::size_t>(v.kind) == axis;
        });
        if (variable != c.layout.end())
          rigid[static_cast<std::size_t>(variable - c.layout.begin())] = rows[axis];
      }
    }
  }
  Matrix k = bound;
  for (const auto& row : j) {
    Vector v(width);
    for (std::size_t col = 0; col < c.layout.size(); ++col)
      for (std::size_t a = 0; a < 6; ++a)
        v[a] += row[col] * rigid[col][a];
    for (std::size_t a = 0; a < aux.size(); ++a)
      v[6 + a] = row[aux[a]];
    k.push_back(std::move(v));
  }
  auto gauge_options = c.options;
  double derivative_scale = 1;
  for (const auto& row : j)
    for (double value : row)
      derivative_scale = std::max(derivative_scale, std::abs(value));
  // Rigid invariance must not turn finite-difference cancellation noise into an anchor.
  gauge_options.numeric.rank_absolute_tolerance =
      std::max(c.options.numeric.rank_absolute_tolerance,
               derivative_scale * std::max(c.options.numeric.rank_relative_tolerance,
                                           64 * std::numeric_limits<double>::epsilon() /
                                               c.options.numeric.finite_difference_relative_step));
  const auto restricted = rank(k, width, gauge_options);
  for (auto row : rigid) {
    row.resize(width);
    k.push_back(std::move(row));
  }
  const auto augmented = rank(k, width, gauge_options);
  return augmented >= restricted ? augmented - restricted : 0;
}
} // namespace

SpatialSolveResult solve_spatial_sketch(const PartDocument& part, const Sketch3D& source,
                                        const SpatialSolverGeometry& geometry,
                                        const SpatialSolverOptions& options) {
  SpatialSolveResult result{source};
  Context c{part, source, geometry, options};
  const auto fail = [&](std::string message,
                        SketchSolveStatus status = SketchSolveStatus::InvalidReference) {
    result.sketch = source;
    result.status = status;
    result.evaluations = c.evaluations;
    result.diagnostic = std::move(message);
    return result;
  };
  const auto& o = options.numeric;
  for (double v : {o.convergence_rms, o.finite_difference_relative_step, o.rank_absolute_tolerance,
                   o.rank_relative_tolerance, o.initial_damping})
    if (!std::isfinite(v) || v <= 0)
      return fail("Numerical tolerances must be finite and positive");
  if (o.finite_difference_relative_step < 1e-9 || o.finite_difference_relative_step > 1e-3)
    return fail("Normalized differentiation step must lie in [1e-9,1e-3]");
  if (!o.maximum_iterations || !o.maximum_damping_attempts || !o.maximum_line_search_steps ||
      !options.maximum_evaluations || !options.maximum_curve_projection_iterations ||
      !options.maximum_variables || !options.maximum_entities || !options.maximum_constraints ||
      !std::isfinite(options.length_scale_mm) || options.length_scale_mm < 0)
    return fail("Solver bounds must be finite and positive");
  if (source.entity_count() > options.maximum_entities ||
      source.constraints().size() > options.maximum_constraints)
    return fail("Spatial owner exceeds its deterministic entity/constraint budget");
  auto layout = spatial_variable_layout(part, source);
  if (!layout)
    return fail(reason(layout.error()));
  c.layout = layout.value();
  result.variable_order = c.layout;
  if (c.layout.size() > options.maximum_variables ||
      source.constraints().size() > options.maximum_constraints)
    return fail("Spatial system exceeds its deterministic size budget");
  if (geometry.validate_sources) {
    auto valid = geometry.validate_sources();
    if (!valid)
      return fail(reason(valid.error()));
  }
  std::vector<Point3> points;
  for (const auto& p : source.points()) {
    auto value = owned_point(part, p);
    if (!value)
      return fail(reason(value.error()));
    points.push_back(value.value());
  }
  c.scale = 0;
  if (!points.empty()) {
    const auto base = points.front();
    Vector3 mean{};
    for (const auto p : points)
      mean = add(mean, mul(displacement(base, p), 1. / static_cast<double>(points.size())));
    c.origin = {base.x + mean.x, base.y + mean.y, base.z + mean.z};
    for (const auto p : points)
      c.scale = std::max(c.scale, 2 * distance(c.origin, p));
  }
  for (const auto& constraint : source.constraints()) {
    c.constraints.push_back(&constraint);
    if (constraint.value() && constraint.kind() != SpatialConstraintKind::Angle) {
      auto q = constraint.value()->resolve(part);
      if (!q)
        return fail(reason(q.error()));
      c.scale = std::max(c.scale, q.value().millimeters());
    }
  }
  std::sort(c.constraints.begin(), c.constraints.end(),
            [](const auto* a, const auto* b) { return a->id().value() < b->id().value(); });
  if (options.length_scale_mm > 0)
    c.scale = options.length_scale_mm;
  if (!std::isfinite(c.scale) || c.scale <= 0)
    c.scale = 1;
  result.length_scale_mm = c.scale;
  Vector x;
  for (const auto& v : c.layout) {
    const std::array<double, 3> origin{c.origin.x, c.origin.y, c.origin.z};
    const auto k = v.kind;
    x.push_back(k == SpatialVariableKind::CurveParameter ? v.initial_value
                : k == SpatialVariableKind::PointX || k == SpatialVariableKind::PointY ||
                        k == SpatialVariableKind::PointZ
                    ? (v.initial_value - origin[static_cast<std::size_t>(k)]) / c.scale
                    : v.initial_value / c.scale);
  }
  const auto initial = x;
  const auto initialized = c.initialize_auxiliaries(x);
  if (!initialized)
    return fail(reason(initialized.error()), SketchSolveStatus::NonConvergent);
  auto residual = c.evaluate(x);
  if (!residual)
    return fail(reason(residual.error()));
  double damping = o.initial_damping;
  const auto converged = [&](const Vector& r) {
    return std::all_of(r.begin(), r.end(),
                       [&](double v) { return std::abs(v) <= o.convergence_rms; });
  };
  bool stalled = false;
  for (; !converged(residual.value()) && result.iterations < o.maximum_iterations;
       ++result.iterations) {
    if (x.empty()) {
      stalled = true;
      break;
    }
    auto j = c.jacobian(x, residual.value());
    if (!j)
      return fail(reason(j.error()), SketchSolveStatus::NonConvergent);
    Matrix normal(x.size(), Vector(x.size()));
    Vector rhs(x.size());
    for (std::size_t r = 0; r < j.value().size(); ++r)
      for (std::size_t a = 0; a < x.size(); ++a) {
        rhs[a] -= j.value()[r][a] * residual.value()[r];
        for (std::size_t b = 0; b < x.size(); ++b)
          normal[a][b] += j.value()[r][a] * j.value()[r][b];
      }
    bool accepted = false;
    for (std::size_t attempt = 0; attempt < o.maximum_damping_attempts && !accepted; ++attempt) {
      auto matrix = normal;
      for (std::size_t i = 0; i < x.size(); ++i)
        matrix[i][i] += damping;
      Vector step;
      if (detail::solve_linear_system(std::move(matrix), rhs, step))
        for (std::size_t line = 0; line < o.maximum_line_search_steps; ++line) {
          const auto alpha = std::ldexp(1., -static_cast<int>(line));
          auto next = x;
          for (std::size_t i = 0; i < x.size(); ++i)
            next[i] += alpha * step[i];
          auto r = c.evaluate(next);
          if (r && norm2(r.value()) < norm2(residual.value())) {
            x = std::move(next);
            residual = std::move(r);
            accepted = true;
            damping = std::max(damping / 10, 1e-15);
            break;
          }
          if (c.evaluations >= options.maximum_evaluations)
            return fail("Deterministic evaluation budget exhausted",
                        SketchSolveStatus::NonConvergent);
        }
      if (!accepted)
        damping *= 10;
    }
    if (!accepted) {
      stalled = true;
      break;
    }
  }
  // Re-evaluate the accepted seed after finite differences/failed line searches
  // so diagnostics and derived Geometry caches describe exactly the final
  // candidate.
  residual = c.evaluate(x);
  if (!residual)
    return fail(reason(residual.error()), SketchSolveStatus::NonConvergent);
  result.normalized_rms =
      residual.value().empty()
          ? 0
          : std::sqrt(norm2(residual.value()) / static_cast<double>(residual.value().size()));
  auto j = c.jacobian(x, residual.value());
  if (!j)
    return fail(reason(j.error()), SketchSolveStatus::NonConvergent);
  result.jacobian_rank = rank(j.value(), x.size(), options);
  result.degrees_of_freedom = x.size() - result.jacobian_rank;
  auto solved = c.candidate(x);
  if (!solved)
    return fail(reason(solved.error()));
  result.gauge_degrees_of_freedom = gauge(c, solved.value(), j.value());
  result.gauge_degrees_of_freedom =
      std::min(result.gauge_degrees_of_freedom, result.degrees_of_freedom);
  result.internal_degrees_of_freedom = result.degrees_of_freedom - result.gauge_degrees_of_freedom;
  Matrix accumulated;
  std::size_t row = 0, previous_rank = 0;
  for (std::size_t i = 0; i < c.constraints.size(); ++i) {
    const auto count = c.rows[i];
    double n = 0;
    for (std::size_t a = row; a < row + count; ++a) {
      n += residual.value()[a] * residual.value()[a];
      accumulated.push_back(j.value()[a]);
    }
    const auto current_rank = rank(accumulated, x.size(), options),
               gain = current_rank - previous_rank;
    const auto aux = c.constraints[i]->kind() == SpatialConstraintKind::PointOnCurve ? 1u : 0u;
    const auto physical = gain >= aux ? gain - aux : 0;
    result.constraints.push_back(
        {c.constraints[i]->id(), std::sqrt(n), gain,
         (physical < expected_rank(*c.constraints[i], part)),
         (std::sqrt(n) > o.convergence_rms * std::sqrt(static_cast<double>(count)))});
    row += count;
    previous_rank = current_rank;
  }
  result.evaluations = c.evaluations;
  if (!converged(residual.value()))
    return fail(stalled ? "Spatial constraints are incompatible at the selected branch"
                        : "Spatial iteration limit reached",
                stalled ? SketchSolveStatus::Conflicting : SketchSolveStatus::NonConvergent);
  if (geometry.validate_sources) {
    auto valid = geometry.validate_sources();
    if (!valid)
      return fail(reason(valid.error()));
  }
  if (geometry.validate_candidate) {
    auto valid = geometry.validate_candidate(solved.value());
    if (!valid)
      return fail(reason(valid.error()));
  }
  bool geometry_changed = false;
  for (std::size_t i = 0; i < x.size(); ++i)
    if (c.layout[i].kind != SpatialVariableKind::CurveParameter && x[i] != initial[i])
      geometry_changed = true;
  result.sketch = geometry_changed ? std::move(solved.value()) : source;
  const bool redundant = std::any_of(result.constraints.begin(), result.constraints.end(),
                                     [](const auto& r) { return r.redundant; });
  result.status = redundant                   ? SketchSolveStatus::Redundant
                  : result.degrees_of_freedom ? SketchSolveStatus::UnderConstrained
                                              : SketchSolveStatus::FullyConstrained;
  return result;
}
} // namespace blcad
