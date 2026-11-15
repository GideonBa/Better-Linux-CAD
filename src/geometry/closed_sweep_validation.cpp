#include "closed_sweep_validation.hpp"
#include "blcad/geometry/body_boolean_adapter.hpp"
#include "derived_curve_internal.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Tool.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomFill_ConstantBiNormal.hxx>
#include <GeomFill_CorrectedFrenet.hxx>
#include <GeomFill_Frenet.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax1.hxx>
#include <numbers>
namespace blcad::geometry::detail {
Result<CircularSweepWires> validate_circular_closed_sweep(const FeatureId& id,
                                                          const TopoDS_Wire& profile,
                                                          const TopoDS_Wire& spine,
                                                          PathOrientationRule mode,
                                                          std::optional<Vector3> up, double degrees,
                                                          bool user_guide, double tolerance) {
  const auto fail = [&](std::string message) {
    return Result<CircularSweepWires>::failure(Error::geometry(id.value(), std::move(message)));
  };
  if (user_guide)
    return fail("Closed Sweep does not support an authored guide; use a cyclic frame and "
                "integer-turn twist");
  if (!std::isfinite(degrees) || std::abs(degrees) > 1440 ||
      std::abs(degrees / 360 - std::round(degrees / 360)) > 1e-10)
    return fail(
        "Closed Sweep twist must contain an integer number of complete turns, at most four");
  try {
    TopExp_Explorer first(spine, TopAbs_EDGE);
    if (!first.More())
      return fail("Closed Sweep has no native spine");
    BRepAdaptor_Curve initial(TopoDS::Edge(first.Current()));
    if (initial.GetType() != GeomAbs_Circle)
      return fail("Closed Sweep currently requires an exact circular spine");
    const auto circle = initial.Circle();
    gp_Pnt start;
    gp_Vec tangent;
    initial.D1(initial.FirstParameter(), start, tangent);
    const gp_Vec radial(circle.Location(), start);
    const gp_Dir axis(radial.Crossed(tangent));
    const double radius = circle.Radius(), margin = std::max(1e-7, tolerance);
    const double precision = std::min(1e-9, tolerance * .05);
    const double plane_tolerance = std::min(1e-7, tolerance * .05);
    double sweep_angle = 0;
    for (TopExp_Explorer edge(spine, TopAbs_EDGE); edge.More(); edge.Next()) {
      BRepAdaptor_Curve curve(TopoDS::Edge(edge.Current()));
      if (curve.GetType() != GeomAbs_Circle)
        return fail("Closed Sweep requires only exact circular spans");
      const auto actual = curve.Circle();
      gp_Pnt p;
      gp_Vec t;
      curve.D1(curve.FirstParameter(), p, t);
      const gp_Dir direction(gp_Vec(actual.Location(), p).Crossed(t));
      if (actual.Location().Distance(circle.Location()) > precision ||
          std::abs(actual.Radius() - radius) > precision || direction.Dot(axis) <= 0 ||
          gp_Vec(direction).Crossed(gp_Vec(axis)).Magnitude() > precision / std::max(1., radius))
        return fail("Closed Sweep spans must traverse the same circle consistently");
      sweep_angle += curve.LastParameter() - curve.FirstParameter();
    }
    if (std::abs(sweep_angle - 2 * std::numbers::pi) > 1e-9)
      return fail("Closed Sweep must traverse its circular spine exactly once");
    if (mode == PathOrientationRule::FixedUpVector) {
      if (!up || length(*up) <= 1e-12 ||
          std::abs(gp_Dir(up->x, up->y, up->z).Dot(axis)) < 1 - 1e-10)
        return fail("Closed fixed-up Sweep requires an up vector parallel to the circle axis");
    }
    Handle(GeomFill_TrihedronLaw) frame;
    if (mode == PathOrientationRule::ProfileNormal)
      frame = new GeomFill_Frenet;
    else if (mode == PathOrientationRule::MinimumTwist)
      frame = new GeomFill_CorrectedFrenet;
    else
      frame = new GeomFill_ConstantBiNormal(gp_Dir(up->x, up->y, up->z));
    frame->SetCurve(new GeomAdaptor_Curve(
        new Geom_Circle(gp_Ax2(circle.Location(), axis, gp_Dir(radial)), radius)));
    gp_Vec t0, n0, b0, t1, n1, b1;
    if (!frame->D0(0, t0, n0, b0) || !frame->D0(2 * std::numbers::pi, t1, n1, b1) ||
        t0.Normalized().Dot(t1.Normalized()) < 1 - 1e-10 ||
        n0.Normalized().Dot(n1.Normalized()) < 1 - 1e-10 ||
        b0.Normalized().Dot(b1.Normalized()) < 1 - 1e-10)
      return fail("Closed Sweep frame does not close cyclically");
    const gp_Vec normal = tangent.Normalized();
    double bound = 0;
    const auto pole = [&](const gp_Pnt& p) {
      const gp_Vec offset(start, p);
      if (std::abs(offset.Dot(normal)) > plane_tolerance)
        return false;
      bound = std::max(bound, offset.Magnitude());
      return true;
    };
    for (TopExp_Explorer edge(profile, TopAbs_EDGE); edge.More(); edge.Next()) {
      BRepAdaptor_Curve curve(TopoDS::Edge(edge.Current()));
      if (curve.GetType() == GeomAbs_Line) {
        if (!pole(curve.Value(curve.FirstParameter())) || !pole(curve.Value(curve.LastParameter())))
          return fail("Closed Sweep profile must lie in the spine start-normal plane");
      } else if (curve.GetType() == GeomAbs_Circle) {
        const auto c = curve.Circle();
        if (std::abs(c.Axis().Direction().Dot(gp_Dir(normal))) < 1 - 1e-10 || !pole(c.Location()))
          return fail("Closed Sweep circular profile must lie in the start-normal plane");
        bound = std::max(bound, start.Distance(c.Location()) + c.Radius());
      } else if (curve.GetType() == GeomAbs_BezierCurve) {
        const auto c = curve.Bezier();
        for (int i = 1; i <= c->NbPoles(); ++i)
          if (c->Weight(i) <= 0 || !pole(c->Pole(i)))
            return fail("Closed Sweep requires a positive planar profile control hull");
      } else if (curve.GetType() == GeomAbs_BSplineCurve) {
        const auto c = curve.BSpline();
        for (int i = 1; i <= c->NbPoles(); ++i)
          if (c->Weight(i) <= 0 || !pole(c->Pole(i)))
            return fail("Closed Sweep requires a positive planar profile control hull");
      } else
        return fail("Closed Sweep cannot prove this profile's exact extent");
    }
    // OCCT validity and Boolean self-interference checks can accept an intra-face spindle fold.
    // A strict positive radial normal-bundle bound rules it out analytically, including roll.
    if (!std::isfinite(bound) || bound <= 0 || bound + margin >= radius)
      return fail("Closed Sweep profile extent reaches its circle axis and can self-intersect");
    const auto circle_wire = [](const gp_Circ& c, const gp_Pnt& p, const gp_Vec& t) {
      const gp_Vec r(c.Location(), p);
      return BRepBuilderAPI_MakeWire(
                 BRepBuilderAPI_MakeEdge(
                     gp_Circ(gp_Ax2(c.Location(), gp_Dir(r.Crossed(t)), gp_Dir(r)), c.Radius()))
                     .Edge())
          .Wire();
    };
    // Avoid a multi-span periodic approximation when the verified geometry is exactly a circle.
    // This changes only transient construction, preserving logical segment/section identities.
    auto native_spine = circle_wire(circle, start, tangent);
    auto native_profile = profile;
    TopExp_Explorer first_profile(profile, TopAbs_EDGE);
    if (first_profile.More()) {
      BRepAdaptor_Curve c0(TopoDS::Edge(first_profile.Current()));
      if (c0.GetType() == GeomAbs_Circle) {
        const auto base = c0.Circle();
        bool same = true;
        double total = 0;
        for (TopExp_Explorer e(profile, TopAbs_EDGE); e.More(); e.Next()) {
          BRepAdaptor_Curve c(TopoDS::Edge(e.Current()));
          if (c.GetType() != GeomAbs_Circle) {
            same = false;
            break;
          }
          const auto current = c.Circle();
          if (current.Location().Distance(base.Location()) > 1e-9 ||
              std::abs(current.Radius() - base.Radius()) > 1e-9 ||
              std::abs(current.Axis().Direction().Dot(base.Axis().Direction())) < 1 - 1e-12) {
            same = false;
            break;
          }
          total += c.LastParameter() - c.FirstParameter();
        }
        if (same && std::abs(total - 2 * std::numbers::pi) < 1e-10) {
          gp_Pnt p;
          gp_Vec t;
          c0.D1(c0.FirstParameter(), p, t);
          native_profile = circle_wire(base, p, t);
        }
      }
    }
    return Result<CircularSweepWires>::success({native_profile, native_spine});
  } catch (const Standard_Failure& error) {
    return fail(std::string("Closed Sweep capability verification failed: ") +
                error.GetMessageString());
  }
}
Result<GeometryShape>
sweep_circular_closed_twist(const FeatureId& id,
                            const std::vector<ClosedProfileCurveSegment>& profile,
                            const TopoDS_Wire& spine, PathOrientationRule mode,
                            std::optional<Vector3> up, double degrees, double tolerance) {
  try {
    TopExp_Explorer edge(spine, TopAbs_EDGE);
    BRepAdaptor_Curve source(TopoDS::Edge(edge.Current()));
    const auto circle = source.Circle();
    Handle(Geom_Curve) native = new Geom_Circle(circle);
    std::vector<GeometryShape> pieces;
    for (int quarter = 0; quarter < 4; ++quarter) {
      const double first = quarter * std::numbers::pi / 2,
                   last = (quarter + 1) * std::numbers::pi / 2;
      gp_Pnt start;
      gp_Vec tangent;
      native->D1(first, start, tangent);
      gp_Trsf orbit, roll;
      orbit.SetRotation(circle.Axis(), first);
      roll.SetRotation(gp_Ax1(start, gp_Dir(tangent)),
                       degrees * std::numbers::pi / 180 * quarter / 4);
      const auto placement = roll * orbit;
      const auto transform = [&](Point3 p) {
        const auto mapped = gp_Pnt(p.x, p.y, p.z).Transformed(placement);
        return Point3{mapped.X(), mapped.Y(), mapped.Z()};
      };
      auto section = profile;
      for (auto& curve : section) {
        curve.start = transform(curve.start);
        curve.mid = transform(curve.mid);
        curve.end = transform(curve.end);
        curve.control2 = transform(curve.control2);
        if (curve.derived_curve)
          curve.derived_curve = std::make_shared<const DerivedCurve>(
              Handle(Geom_Curve)::DownCast(curve.derived_curve->curve->Transformed(placement)),
              curve.derived_curve->closure_proven);
      }
      auto span =
          std::make_shared<const DerivedCurve>(new Geom_TrimmedCurve(native, first, last), false);
      const auto end = native->Value(last);
      const std::vector<SweepPathSegment> path{{SweepPathSegmentKind::DerivedNative,
                                                {start.X(), start.Y(), start.Z()},
                                                {},
                                                {end.X(), end.Y(), end.Z()},
                                                span,
                                                tolerance}};
      auto piece = SweepAdapter{}.sweep_closed_profile(id, section, path, mode, up, degrees / 4);
      if (!piece)
        return piece;
      pieces.push_back(std::move(piece.value()));
    }
    auto result = pieces.front();
    pieces.erase(pieces.begin());
    // Each section is an exact rigid placement at the authored global roll phase. Checked
    // non-destructive union removes internal caps; no gap healing or frame reset is permitted.
    auto united = BodyBooleanAdapter{}.combine(id, BodyBooleanOperation::Add, result, pieces);
    if (!united)
      return united;
    if (RectangleExtrusionAdapter{}.summarize(united.value()).solid_count != 1)
      return Result<GeometryShape>::failure(
          Error::geometry(id.value(), "Cyclic Sweep seams leave disconnected solid components"));
    return united;
  } catch (const Standard_Failure& error) {
    return Result<GeometryShape>::failure(Error::geometry(
        id.value(), std::string("Cyclic Sweep construction failed: ") + error.GetMessageString()));
  }
}
} // namespace blcad::geometry::detail
