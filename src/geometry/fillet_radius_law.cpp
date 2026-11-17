#include "fillet_radius_law.hpp"
#include <Standard_ConstructionError.hxx>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
struct Data {
  std::vector<RadiusKnot> knots;
  std::vector<double> slopes;
  bool cubic;
};
double harmonic(double previous, double next, double hp, double hn) {
  if (previous * next <= 0)
    return 0;
  const double w1 = 2 * hn + hp, w2 = hn + 2 * hp;
  return (w1 + w2) / (w1 / previous + w2 / next);
}
double endpoint(double h0, double h1, double d0, double d1) {
  double m = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
  if (m * d0 <= 0)
    return 0;
  if (d0 * d1 < 0 && std::abs(m) > 3 * std::abs(d0))
    m = 3 * d0;
  return m;
}
std::shared_ptr<const Data> data(std::vector<RadiusKnot> knots, bool cubic, bool periodic) {
  if (knots.size() < 2)
    throw std::invalid_argument("Radius law needs at least two stations");
  std::vector<double> h, d;
  for (std::size_t i = 0; i < knots.size(); ++i) {
    if (!std::isfinite(knots[i].length_mm) || !std::isfinite(knots[i].radius_mm) ||
        knots[i].radius_mm <= 0 || (i && knots[i].length_mm <= knots[i - 1].length_mm))
      throw std::invalid_argument(
          "Radius stations must have finite increasing lengths and positive radii");
    if (i) {
      h.push_back(knots[i].length_mm - knots[i - 1].length_mm);
      d.push_back((knots[i].radius_mm - knots[i - 1].radius_mm) / h.back());
      if (!std::isfinite(d.back()))
        throw std::invalid_argument("Radius stations are numerically unresolved");
    }
  }
  if (periodic && knots.front().radius_mm != knots.back().radius_mm)
    throw std::invalid_argument("Closed Fillet contours require equal endpoint radii");
  std::vector<double> slopes(knots.size(), d.front());
  if (knots.size() > 2) {
    for (std::size_t i = 1; i + 1 < knots.size(); ++i)
      slopes[i] = harmonic(d[i - 1], d[i], h[i - 1], h[i]);
    slopes.front() = endpoint(h[0], h[1], d[0], d[1]);
    slopes.back() = endpoint(h.back(), h[h.size() - 2], d.back(), d[d.size() - 2]);
  }
  if (periodic)
    slopes.front() = slopes.back() = harmonic(d.back(), d.front(), h.back(), h.front());
  for (std::size_t i = 0; i < h.size(); ++i)
    if (cubic) {
      const double first = knots[i].radius_mm + h[i] * slopes[i] / 3,
                   last = knots[i + 1].radius_mm - h[i] * slopes[i + 1] / 3;
      if (!std::isfinite(first) || !std::isfinite(last) || first <= 0 || last <= 0)
        throw std::invalid_argument("Monotone radius law lacks a positive control-pole bound");
    }
  return std::make_shared<const Data>(Data{std::move(knots), std::move(slopes), cubic});
}
class RadiusLaw final : public Law_Function {
public:
  RadiusLaw(std::shared_ptr<const Data> values, double first, double last)
      : values_(std::move(values)), first_(first), last_(last) {
    if (!std::isfinite(first) || !std::isfinite(last) || first >= last)
      throw std::invalid_argument("Radius law requires finite ordered bounds");
    double f, d, dd;
    D2(first, f, d, dd);
    D2(last, f, d, dd);
  }
  GeomAbs_Shape Continuity() const override {
    return values_->cubic ? GeomAbs_C1 : GeomAbs_C0;
  }
  Standard_Integer NbIntervals(GeomAbs_Shape continuity) const override {
    return Standard_Integer(intervals(continuity).size() - 1);
  }
  void Intervals(TColStd_Array1OfReal& array, GeomAbs_Shape continuity) const override {
    const auto cuts = intervals(continuity);
    if (array.Length() < int(cuts.size()))
      Standard_ConstructionError::Raise("Insufficient radius-law interval array");
    for (std::size_t i = 0; i < cuts.size(); ++i)
      array(array.Lower() + int(i)) = cuts[i];
  }
  double Value(double x) override {
    double f, d, dd;
    D2(x, f, d, dd);
    return f;
  }
  void D1(double x, double& f, double& d) override {
    double dd;
    D2(x, f, d, dd);
  }
  void D2(double x, double& f, double& d, double& dd) override {
    if (!std::isfinite(x))
      Standard_ConstructionError::Raise("Non-finite radius-law station");
    const auto& knots = values_->knots;
    if (x < knots.front().length_mm || x > knots.back().length_mm) {
      const bool before = x < knots.front().length_mm;
      const auto& end = before ? knots.front() : knots.back();
      const std::size_t i = before ? 0 : knots.size() - 2;
      d = values_->cubic ? values_->slopes[before ? 0 : knots.size() - 1]
                         : (knots[i + 1].radius_mm - knots[i].radius_mm) /
                               (knots[i + 1].length_mm - knots[i].length_mm);
      f = end.radius_mm + d * (x - end.length_mm);
      dd = 0;
    } else {
      auto found =
          std::upper_bound(knots.begin(), knots.end(), x,
                           [](double value, const auto& knot) { return value < knot.length_mm; });
      std::size_t i = found == knots.begin() ? 0 : std::size_t(found - knots.begin() - 1);
      i = std::min(i, knots.size() - 2);
      if (x == last_ && i > 0 && x == knots[i].length_mm)
        --i;
      const double h = knots[i + 1].length_mm - knots[i].length_mm,
                   t = (x - knots[i].length_mm) / h;
      const double a = knots[i].radius_mm,
                   b = values_->cubic ? h * values_->slopes[i] : knots[i + 1].radius_mm - a;
      const double c = values_->cubic ? 3 * (knots[i + 1].radius_mm - a) -
                                            h * (2 * values_->slopes[i] + values_->slopes[i + 1])
                                      : 0;
      const double e = values_->cubic ? 2 * (a - knots[i + 1].radius_mm) +
                                            h * (values_->slopes[i] + values_->slopes[i + 1])
                                      : 0;
      f = a + t * (b + t * (c + t * e));
      d = (b + t * (2 * c + 3 * t * e)) / h;
      dd = (2 * c + 6 * t * e) / (h * h);
    }
    if (!std::isfinite(f) || !std::isfinite(d) || !std::isfinite(dd) || f <= 0)
      Standard_ConstructionError::Raise(
          "Radius law or endpoint continuation is nonpositive or numerically singular");
  }
  Handle(Law_Function) Trim(double first, double last, double) const override {
    return new RadiusLaw(values_, first, last);
  }
  void Bounds(double& first, double& last) override {
    first = first_;
    last = last_;
  }
  DEFINE_STANDARD_RTTIEXT(RadiusLaw, Law_Function)
private:
  std::vector<double> intervals(GeomAbs_Shape requested) const {
    std::vector<double> cuts{first_};
    if (requested > Continuity())
      for (const auto& knot : values_->knots)
        if (knot.length_mm > first_ && knot.length_mm < last_)
          cuts.push_back(knot.length_mm);
    cuts.push_back(last_);
    return cuts;
  }
  std::shared_ptr<const Data> values_;
  double first_, last_;
};
IMPLEMENT_STANDARD_RTTIEXT(RadiusLaw, Law_Function)
} // namespace
class OrientedLaw final : public Law_Function {
public:
  OrientedLaw(Handle(Law_Function) base, std::vector<double> cuts, bool periodic, int sign,
              double offset, double first, double last, double period)
      : base_(base), cuts_(std::move(cuts)), periodic_(periodic), sign_(sign), offset_(offset),
        first_(first), last_(last), period_(period) {}
  GeomAbs_Shape Continuity() const override {
    return base_->Continuity();
  }
  int NbIntervals(GeomAbs_Shape c) const override {
    return int(intervals(c).size() - 1);
  }
  void Intervals(TColStd_Array1OfReal& a, GeomAbs_Shape c) const override {
    const auto v = intervals(c);
    if (a.Length() < int(v.size()))
      Standard_ConstructionError::Raise("Radius interval array too small");
    for (std::size_t i = 0; i < v.size(); ++i)
      a(a.Lower() + int(i)) = v[i];
  }
  double Value(double x) override {
    return base_->Value(mapped(x));
  }
  void D1(double x, double& f, double& d) override {
    base_->D1(mapped(x), f, d);
    d *= sign_;
  }
  void D2(double x, double& f, double& d, double& dd) override {
    base_->D2(mapped(x), f, d, dd);
    d *= sign_;
  }
  Handle(Law_Function) Trim(double first, double last, double) const override {
    return new OrientedLaw(base_, cuts_, periodic_, sign_, offset_, first, last, period_);
  }
  void Bounds(double& f, double& l) override {
    f = first_;
    l = last_;
  }
  DEFINE_STANDARD_RTTIEXT(OrientedLaw, Law_Function)
private:
  double mapped(double x) const {
    double y = sign_ * x + offset_;
    if (periodic_) {
      y = std::fmod(y, period_);
      if (y < 0)
        y += period_;
    }
    return y;
  }
  std::vector<double> intervals(GeomAbs_Shape c) const {
    std::vector<double> v{first_};
    if (c > Continuity()) {
      for (double cut : cuts_) {
        double x = (cut - offset_) / sign_;
        if (periodic_) {
          x = std::fmod(x, period_);
          if (x < 0)
            x += period_;
          const int begin = int(std::floor((first_ - x) / period_));
          const int end = int(std::ceil((last_ - x) / period_));
          for (int k = begin; k <= end; ++k)
            if (x + k * period_ > first_ && x + k * period_ < last_)
              v.push_back(x + k * period_);
        } else if (x > first_ && x < last_)
          v.push_back(x);
      }
    }
    v.push_back(last_);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
  }
  Handle(Law_Function) base_;
  std::vector<double> cuts_;
  bool periodic_;
  int sign_;
  double offset_, first_, last_, period_;
};
IMPLEMENT_STANDARD_RTTIEXT(OrientedLaw, Law_Function)
Handle(Law_Function)
    radius_law(std::vector<RadiusKnot> knots, bool cubic, bool periodic, int sign, double offset) {
  const auto values = data(std::move(knots), cubic, periodic);
  if ((sign != 1 && sign != -1) || !std::isfinite(offset) || values->knots.front().length_mm != 0)
    throw std::invalid_argument("Invalid radius-law coordinate mapping");
  Handle(Law_Function) base = new RadiusLaw(values, 0, values->knots.back().length_mm);
  std::vector<double> cuts;
  for (const auto& knot : values->knots)
    cuts.push_back(knot.length_mm);
  return new OrientedLaw(base, std::move(cuts), periodic, sign, offset, 0,
                         values->knots.back().length_mm, values->knots.back().length_mm);
}
} // namespace blcad::geometry::detail
