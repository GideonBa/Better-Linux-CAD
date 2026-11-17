#include "fillet_spine_law.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Standard_ConstructionError.hxx>
#include <algorithm>
#include <cmath>
#include <vector>
namespace blcad::geometry::detail {
namespace {
struct Coordinate {
  double value, first, second;
};
class SpineLaw final : public Law_Function {
public:
  SpineLaw(Handle(Law_Function) law, Handle(ChFiDS_Spine) source, Handle(ChFiDS_ElSpine) guide,
           double first, double last, std::shared_ptr<std::string> failure)
      : law_(law), source_(source), guide_(guide), first_(first), last_(last),
        failure_(std::move(failure)) {}
  GeomAbs_Shape Continuity() const override {
    return law_->Continuity();
  }
  int NbIntervals(GeomAbs_Shape c) const override {
    return int(intervals(c).size() - 1);
  }
  void Intervals(TColStd_Array1OfReal& a, GeomAbs_Shape c) const override {
    const auto cuts = intervals(c);
    if (a.Length() < int(cuts.size()))
      Standard_ConstructionError::Raise("Insufficient physical radius-law interval array");
    for (std::size_t i = 0; i < cuts.size(); ++i)
      a(a.Lower() + int(i)) = cuts[i];
  }
  double Value(double x) override {
    return law_->Value(coordinate(x).value);
  }
  void D1(double x, double& f, double& d) override {
    auto s = coordinate(x);
    law_->D1(s.value, f, d);
    d *= s.first;
  }
  void D2(double x, double& f, double& d, double& dd) override {
    auto s = coordinate(x);
    law_->D2(s.value, f, d, dd);
    dd = dd * s.first * s.first + d * s.second;
    d *= s.first;
  }
  void Bounds(double& f, double& l) override {
    f = first_;
    l = last_;
  }
  Handle(Law_Function) Trim(double f, double l, double) const override {
    return new SpineLaw(law_, source_, guide_, f, l, failure_);
  }
  DEFINE_STANDARD_RTTIEXT(SpineLaw, Law_Function)
private:
  Coordinate coordinate(double x) const {
    try {
      return project(x);
    } catch (const Standard_Failure& ex) {
      *failure_ =
          ex.GetMessageString() ? ex.GetMessageString() : "Physical radius station mapping failed";
      throw;
    }
  }
  Coordinate project(double x) const {
    // The kernel asks for acquisition bounds before assigning an elementary curve.
    if (guide_->GetType() == GeomAbs_OtherCurve)
      return {x, 1, 0};
    const double total = source_->LastParameter();
    double hint = x, period_shift = 0;
    if (source_->IsClosed()) {
      period_shift = std::floor(x / total) * total;
      hint = x - period_shift;
    }
    if (!source_->IsClosed() && (x < 0 || x > total)) {
      const bool before = x < 0;
      const int edge_index = before ? 1 : source_->NbEdges();
      const auto& edge = source_->Edges(edge_index);
      BRepAdaptor_Curve curve(edge);
      const bool reversed = edge.Orientation() == TopAbs_REVERSED;
      const double parameter =
          (before != reversed) ? curve.FirstParameter() : curve.LastParameter();
      gp_Pnt point, guide_point;
      gp_Vec tangent, guide_first, guide_second;
      curve.D1(parameter, point, tangent);
      tangent.Normalize();
      if (reversed)
        tangent.Reverse();
      const double end = before ? 0 : total;
      guide_->D2(end, guide_point, guide_first, guide_second);
      const double slope = guide_first.Dot(tangent);
      if (!std::isfinite(slope) || slope <= 1e-8)
        Standard_ConstructionError::Raise("Singular Fillet endpoint station continuation");
      return {end + slope * (x - end), slope, 0};
    }
    int index = 1;
    while (index < source_->NbEdges() && hint > source_->LastParameter(index))
      ++index;
    const auto& edge = source_->Edges(index);
    BRepAdaptor_Curve curve(edge);
    const bool reversed = edge.Orientation() == TopAbs_REVERSED;
    const double sign = reversed ? -1 : 1, begin = source_->FirstParameter(index),
                 length = source_->Length(index);
    const double f = curve.FirstParameter(), l = curve.LastParameter();
    gp_Pnt g;
    gp_Vec g1, g2;
    guide_->D2(x, g, g1, g2);
    double v = f + (l - f) * std::clamp((hint - begin) / length, 0., 1.);
    if (reversed)
      v = f + l - v;
    gp_Pnt c;
    gp_Vec c1, c2, c3;
    for (int iteration = 0; iteration < 30; ++iteration) {
      curve.D3(v, c, c1, c2, c3);
      const gp_Vec delta(g, c);
      const double denominator = c1.SquareMagnitude() + delta.Dot(c2);
      if (denominator <= 1e-18)
        Standard_ConstructionError::Raise(
            "Singular source projection in variable Fillet radius law");
      const double next = std::clamp(v - delta.Dot(c1) / denominator, f, l);
      if (std::abs(next - v) < 1e-13 * std::max(1., l - f)) {
        v = next;
        break;
      }
      v = next;
      if (iteration == 29)
        Standard_ConstructionError::Raise(
            "Unresolved source projection in variable Fillet radius law");
    }
    curve.D3(v, c, c1, c2, c3);
    const double speed = c1.Magnitude();
    if (speed <= 1e-12)
      Standard_ConstructionError::Raise("Singular source tangent in variable Fillet radius law");
    const gp_Vec delta(g, c);
    // Physical tangent continuation at open endpoints is measured in millimetres as well.
    if ((v == f || v == l) && std::abs(delta.Dot(c1)) > 1e-10) {
      const auto tangent = c1 * (sign / speed);
      const double origin = begin + ((reversed ? v == f : v == l) ? length : 0);
      return {origin + gp_Vec(c, g).Dot(tangent) + period_shift, g1.Dot(tangent), g2.Dot(tangent)};
    }
    const double denominator = c1.SquareMagnitude() + delta.Dot(c2);
    const double vp = g1.Dot(c1) / denominator;
    const double dp = (3 * c1.Dot(c2) + delta.Dot(c3)) * vp - g1.Dot(c2);
    const double vpp = (g2.Dot(c1) + g1.Dot(c2) * vp - vp * dp) / denominator;
    double s = GCPnts_AbscissaPoint::Length(curve, f, v, 1e-9);
    if (reversed)
      s = length - s;
    const double first = sign * speed * vp;
    const double second = sign * (speed * vpp + c1.Dot(c2) / speed * vp * vp);
    if (!std::isfinite(s) || !std::isfinite(first) || !std::isfinite(second) || first <= 1e-8)
      Standard_ConstructionError::Raise("Nonmonotone physical station mapping in variable Fillet");
    return {begin + s + period_shift, first, second};
  }
  std::vector<double> intervals(GeomAbs_Shape c) const {
    std::vector<double> cuts{first_};
    if (c > Continuity() && guide_->GetType() != GeomAbs_OtherCurve) {
      const double sf = coordinate(first_).value, sl = coordinate(last_).value;
      if (sl <= sf)
        Standard_ConstructionError::Raise("Unordered physical radius-law bounds");
      auto trimmed = law_->Trim(sf, sl, 1e-9);
      TColStd_Array1OfReal authored(1, trimmed->NbIntervals(c) + 1);
      trimmed->Intervals(authored, c);
      for (int i = 2; i < authored.Upper(); ++i) {
        double lo = first_, hi = last_;
        for (int j = 0; j < 42; ++j) {
          const double mid = (lo + hi) / 2;
          if (coordinate(mid).value < authored(i))
            lo = mid;
          else
            hi = mid;
        }
        cuts.push_back((lo + hi) / 2);
      }
    }
    cuts.push_back(last_);
    return cuts;
  }
  Handle(Law_Function) law_;
  Handle(ChFiDS_Spine) source_;
  Handle(ChFiDS_ElSpine) guide_;
  double first_, last_;
  std::shared_ptr<std::string> failure_;
};
IMPLEMENT_STANDARD_RTTIEXT(SpineLaw, Law_Function)
} // namespace
Handle(Law_Function)
    fillet_spine_law(const Handle(Law_Function) & law, const Handle(ChFiDS_Spine) & source,
                     const Handle(ChFiDS_ElSpine) & guide, std::shared_ptr<std::string> failure) {
  return new SpineLaw(law, source, guide, guide->FirstParameter(), guide->LastParameter(),
                      std::move(failure));
}
} // namespace blcad::geometry::detail
