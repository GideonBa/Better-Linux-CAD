#include "native_twist_guide.hpp"
#include "derived_curve_internal.hpp"
#include <Approx_Curve3d.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomFill_ConstantBiNormal.hxx>
#include <GeomFill_CorrectedFrenet.hxx>
#include <GeomFill_Frenet.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_DomainError.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace blcad::geometry::detail {
namespace {
// This guide is transient construction geometry. Only the pipe's ordinary native
// faces are published. Roll is evaluated on actual arc length, independently of
// curve parameter speed, display tessellation and source subdivision counts.
class TwistGuide final : public Geom_Curve {
  DEFINE_STANDARD_RTTIEXT(TwistGuide, Geom_Curve)
public:
  TwistGuide(Handle(Geom_Curve) spine, Handle(GeomFill_TrihedronLaw) frame, double phase,
             double prefix, double total, double twist, double radius)
      : spine_(std::move(spine)), frame_(std::move(frame)), phase_(phase), prefix_(prefix),
        total_(total), twist_(twist), radius_(radius) {}
  double FirstParameter() const override {
    return spine_->FirstParameter();
  }
  double LastParameter() const override {
    return spine_->LastParameter();
  }
  bool IsClosed() const override {
    return false;
  }
  bool IsPeriodic() const override {
    return false;
  }
  GeomAbs_Shape Continuity() const override {
    return GeomAbs_C1;
  }
  bool IsCN(int n) const override {
    return n >= 0 && n <= 1;
  }
  double ReversedParameter(double u) const override {
    return FirstParameter() + LastParameter() - u;
  }
  void Reverse() override {
    reverse_ = !reverse_;
  }
  void Transform(const gp_Trsf& value) override {
    transform_.PreMultiply(value);
  }
  Handle(Geom_Geometry) Copy() const override {
    auto* result = new TwistGuide(spine_, frame_->Copy(), phase_, prefix_, total_, twist_, radius_);
    result->reverse_ = reverse_;
    result->transform_ = transform_;
    return result;
  }
  void D0(double u, gp_Pnt& p) const override {
    const auto at = parameter(u);
    gp_Vec t, n, b;
    if (!frame_->D0(at, t, n, b))
      throw Standard_DomainError("Twist frame did not resolve");
    const double theta = angle(at);
    p = spine_->Value(at).Translated((n * std::cos(theta) + b * std::sin(theta)) * radius_);
    p.Transform(transform_);
  }
  void D1(double u, gp_Pnt& p, gp_Vec& v) const override {
    const auto at = parameter(u);
    gp_Vec t, dt, n, dn, b, db, dp;
    gp_Pnt base;
    if (!frame_->D1(at, t, dt, n, dn, b, db))
      throw Standard_DomainError("Twist frame derivative did not resolve");
    spine_->D1(at, base, dp);
    const double theta = angle(at), c = std::cos(theta), s = std::sin(theta);
    const double rate = twist_ * dp.Magnitude() / total_;
    p = base.Translated((n * c + b * s) * radius_);
    v = dp + (dn * c + db * s + (b * c - n * s) * rate) * radius_;
    if (reverse_)
      v.Reverse();
    p.Transform(transform_);
    v.Transform(transform_);
  }
  void D2(double u, gp_Pnt& p, gp_Vec& v, gp_Vec& a) const override {
    const auto at = parameter(u);
    gp_Vec t, dt, d2t, n, dn, d2n, b, db, d2b, dp, d2p;
    gp_Pnt base;
    if (!frame_->D2(at, t, dt, d2t, n, dn, d2n, b, db, d2b))
      throw Standard_DomainError("Twist frame second derivative did not resolve");
    spine_->D2(at, base, dp, d2p);
    const double speed = dp.Magnitude();
    if (speed <= 1e-12)
      throw Standard_DomainError("Twist path derivative is singular");
    const double theta = angle(at), c = std::cos(theta), s = std::sin(theta);
    const double rate = twist_ * speed / total_, second = twist_ * dp.Dot(d2p) / (speed * total_);
    p = base.Translated((n * c + b * s) * radius_);
    v = dp + (dn * c + db * s + (b * c - n * s) * rate) * radius_;
    a = d2p + (d2n * c + d2b * s + (db * c - dn * s) * (2 * rate) + (b * c - n * s) * second -
               (n * c + b * s) * (rate * rate)) *
                  radius_;
    if (reverse_)
      v.Reverse();
    p.Transform(transform_);
    v.Transform(transform_);
    a.Transform(transform_);
  }
  void D3(double u, gp_Pnt& p, gp_Vec& v, gp_Vec& a, gp_Vec& jerk) const override {
    D2(u, p, v, a);
    const double h = (LastParameter() - FirstParameter()) * 1e-5;
    if (h <= 64 * std::numeric_limits<double>::epsilon() * std::max(1., std::abs(u)))
      throw Standard_DomainError("Twist guide parameter resolution is insufficient");
    gp_Pnt q;
    gp_Vec w, left, right;
    if (u - h >= FirstParameter() && u + h <= LastParameter()) {
      D2(u - h, q, w, left);
      D2(u + h, q, w, right);
      jerk = (right - left) / (2 * h);
    } else if (u + h <= LastParameter()) {
      D2(u + h, q, w, left);
      D2(u + 2 * h, q, w, right);
      jerk = (-a * 3 + left * 4 - right) / (2 * h);
    } else {
      D2(u - h, q, w, left);
      D2(u - 2 * h, q, w, right);
      jerk = (a * 3 - left * 4 + right) / (2 * h);
    }
  }
  gp_Vec DN(double u, int n) const override {
    gp_Pnt p;
    gp_Vec v, a, j;
    if (n == 1) {
      D1(u, p, v);
      return v;
    }
    if (n == 2) {
      D2(u, p, v, a);
      return a;
    }
    if (n == 3) {
      D3(u, p, v, a, j);
      return j;
    }
    throw Standard_DomainError("Twist guide derivative order is unsupported");
  }

private:
  double parameter(double u) const {
    return std::clamp(reverse_ ? ReversedParameter(u) : u, FirstParameter(), LastParameter());
  }
  double angle(double u) const {
    double extent = 0;
    if (u > FirstParameter()) {
      GeomAdaptor_Curve curve(spine_);
      extent = GCPnts_AbscissaPoint::Length(curve, FirstParameter(), u, 1e-10);
    }
    return phase_ + twist_ * (prefix_ + extent) / total_;
  }
  Handle(Geom_Curve) spine_;
  Handle(GeomFill_TrihedronLaw) frame_;
  double phase_, prefix_, total_, twist_, radius_;
  bool reverse_{};
  gp_Trsf transform_;
};
IMPLEMENT_STANDARD_RTTIEXT(TwistGuide, Geom_Curve)

Handle(GeomFill_TrihedronLaw) frame_for(PathOrientationRule mode, std::optional<Vector3> up) {
  if (mode == PathOrientationRule::ProfileNormal)
    return new GeomFill_Frenet;
  if (mode == PathOrientationRule::MinimumTwist)
    return new GeomFill_CorrectedFrenet;
  if (!up || length(*up) <= 1e-12)
    throw Standard_DomainError("Twist requires a valid fixed up vector");
  return new GeomFill_ConstantBiNormal(gp_Dir(up->x, up->y, up->z));
}
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
} // namespace

Result<std::vector<SweepPathSegment>> native_twist_guide(const FeatureId& id,
                                                         const std::vector<DerivedCurveRef>& inputs,
                                                         PathOrientationRule mode,
                                                         std::optional<Vector3> up, double degrees,
                                                         double tolerance) {
  const auto fail = [&](std::string text) {
    return Result<std::vector<SweepPathSegment>>::failure(
        Error::geometry(id.value(), std::move(text)));
  };
  if (inputs.empty() || !std::isfinite(degrees) || std::abs(degrees) > 1440 ||
      !std::isfinite(tolerance) || tolerance < 1e-9 || tolerance > 1)
    return fail(
        "Curve-preserving twist requires a valid modeling tolerance and at most four turns");
  try {
    std::vector<Handle(Geom_Curve)> curves;
    std::vector<double> lengths;
    double total = 0;
    for (const auto& input : inputs) {
      if (!input || input->curve.IsNull())
        return fail("Twist path is missing its native curve");
      if (input->closure_proven)
        return fail("Curve-preserving twist requires an open spine");
      GeomAdaptor_Curve source(input->curve);
      const int count = source.NbIntervals(GeomAbs_C3);
      if (count < 1 || count > 512 - static_cast<int>(curves.size()))
        return fail("Curve-preserving twist exceeds its 512-span frame budget");
      TColStd_Array1OfReal boundaries(1, count + 1);
      source.Intervals(boundaries, GeomAbs_C3);
      for (int i = boundaries.Lower(); i < boundaries.Upper(); ++i) {
        if (curves.size() >= 512)
          return fail("Curve-preserving twist exceeds its 512-span frame budget");
        Handle(Geom_Curve) basis = input->curve;
        if (auto trim = Handle(Geom_TrimmedCurve)::DownCast(basis); !trim.IsNull())
          basis = trim->BasisCurve();
        Handle(Geom_Curve) c;
        if (auto spline = Handle(Geom_BSplineCurve)::DownCast(basis); !spline.IsNull()) {
          auto copy = Handle(Geom_BSplineCurve)::DownCast(spline->Copy());
          copy->Segment(boundaries(i), boundaries(i + 1));
          c = copy;
        } else
          c = new Geom_TrimmedCurve(basis, boundaries(i), boundaries(i + 1));
        GeomAdaptor_Curve adapted(c);
        const double extent =
            GCPnts_AbscissaPoint::Length(adapted, c->FirstParameter(), c->LastParameter(), 1e-10);
        if (!std::isfinite(extent) || extent <= 1e-8)
          return fail("Twist path has a degenerate native span");
        curves.push_back(c);
        lengths.push_back(extent);
        total += extent;
      }
    }
    if (!std::isfinite(total) || total <= 1e-8)
      return fail("Native twist path length is invalid");
    // A fixed world-space offset can cross the curvature center after scaling,
    // making the normal-plane/guide correspondence ambiguous. Keep the guide
    // local to every native span and well inside its sampled curvature radius.
    double radius = std::min(1., .01 * *std::min_element(lengths.begin(), lengths.end()));
    for (const auto& c : curves)
      for (int station = 0; station <= 64; ++station) {
        gp_Pnt p;
        gp_Vec v, a;
        c->D2(std::lerp(c->FirstParameter(), c->LastParameter(), station / 64.), p, v, a);
        const double speed = v.Magnitude();
        if (!std::isfinite(speed) || speed <= 1e-12)
          return fail("Native twist frame has a singular derivative");
        if (mode == PathOrientationRule::FixedUpVector) {
          if (!up || length(*up) <= 1e-12)
            return fail("Native twist requires a nonzero fixed-up vector");
          const auto unit = gp_Vec(up->x, up->y, up->z).Normalized();
          if ((v / speed).Crossed(unit).Magnitude() <= 1e-8)
            return fail("Fixed-up vector is parallel to a native path tangent");
        }
        const double curvature = v.Crossed(a).Magnitude() / (speed * speed * speed);
        if (!std::isfinite(curvature))
          return fail("Native twist curvature is not resolvable");
        if (curvature > 0)
          radius = std::min(radius, .05 / curvature);
      }
    if (!std::isfinite(radius) || radius <= 0)
      return fail("Native twist guide scale is not resolvable");
    std::vector<SweepPathSegment> guide;
    double prefix = 0;
    gp_Vec previous_normal, previous_tangent;
    gp_Pnt previous_point;
    for (std::size_t i = 0; i < curves.size(); ++i) {
      auto frame = frame_for(mode, up);
      frame->SetCurve(new GeomAdaptor_Curve(curves[i]));
      gp_Vec t, n, b;
      if (!frame->D0(curves[i]->FirstParameter(), t, n, b) || t.Magnitude() <= 1e-12 ||
          n.Magnitude() <= 1e-12)
        return fail("Native twist frame is singular");
      t.Normalize();
      n.Normalize();
      b.Normalize();
      double phase = 0;
      const auto start = curves[i]->Value(curves[i]->FirstParameter());
      if (i) {
        if (previous_point.Distance(start) > 1e-8 || previous_tangent.Dot(t) < 1 - 1e-8)
          return fail("Curve-preserving twist requires connected tangent-continuous native spans");
        phase = std::atan2(t.Dot(n.Crossed(previous_normal)), n.Dot(previous_normal));
      }
      Handle(Geom_Curve) product = new TwistGuide(curves[i], frame, phase, prefix, total,
                                                  degrees * std::numbers::pi / 180, radius);
      const double guide_tolerance = std::min({1e-9, tolerance * .1, radius * 1e-8});
      Approx_Curve3d approximation(new GeomAdaptor_Curve(product), guide_tolerance, GeomAbs_C1, 256,
                                   14);
      if (!approximation.IsDone() || !std::isfinite(approximation.MaxError()) ||
          approximation.MaxError() > guide_tolerance)
        return fail(
            "Native twist guide did not meet modeling tolerance within its approximation budget");
      product = approximation.Curve();
      auto derived = std::make_shared<const DerivedCurve>(product, false);
      auto edge = derived_curve_edge(derived, id.value());
      if (!edge)
        return fail(edge.error().message());
      guide.push_back({SweepPathSegmentKind::DerivedNative,
                       point(product->Value(product->FirstParameter())),
                       {},
                       point(product->Value(product->LastParameter())),
                       derived,
                       tolerance});
      if (!frame->D0(curves[i]->LastParameter(), t, n, b))
        return fail("Native twist frame did not resolve its end");
      previous_normal = n * std::cos(phase) + b * std::sin(phase);
      previous_normal.Normalize();
      previous_tangent = t.Normalized();
      previous_point = curves[i]->Value(curves[i]->LastParameter());
      prefix += lengths[i];
    }
    return Result<std::vector<SweepPathSegment>>::success(std::move(guide));
  } catch (const Standard_Failure& failure) {
    return fail(failure.GetMessageString() ? failure.GetMessageString()
                                           : "Native twist guide construction failed");
  }
}
} // namespace blcad::geometry::detail
