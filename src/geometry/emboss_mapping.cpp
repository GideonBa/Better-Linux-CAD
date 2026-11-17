#include "emboss_mapping.hpp"
#include "rib_native.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <BndLib_Add2dCurve.hxx>
#include <Bnd_Box2d.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomAPI.hxx>
#include <GeomLib.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <gp_Lin.hxx>
#include <numbers>
#include <stdexcept>
namespace blcad::geometry::detail {
namespace {
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
gp_Vec vec(Vector3 v) {
  return {v.x, v.y, v.z};
}
void turn(gp_Ax3& axes, double phase) {
  const auto x =
      gp_Vec(axes.XDirection()) * std::cos(phase) + gp_Vec(axes.YDirection()) * std::sin(phase);
  axes.SetXDirection(gp_Dir(x));
}
} // namespace
std::vector<EmbossOutlineLoop> emboss_profile_outlines(const PartDocument& p,
                                                       const ProfileRegionReference& profile,
                                                       const ResolvedWorkplane& frame) {
  RibWebIntent source{RibWebKind::Rib,
                      {profile.sketch(), {}, profile.profile()},
                      RibExtrusionPlane::PerpendicularToSketch,
                      {0, 0, 1},
                      RibThicknessSide::Symmetric,
                      ParameterId("outline"),
                      std::nullopt,
                      std::nullopt,
                      RibDistanceExtent{ParameterId("outline")},
                      RibCrossingPolicy::Union};
  const auto spines = rib_spines(p, source, frame);
  const gp_Pln plane(gp_Ax3(gp_Pnt(frame.origin.x, frame.origin.y, frame.origin.z),
                            gp_Dir(vec(frame.normal)), gp_Dir(vec(frame.x_axis))));
  std::vector<EmbossOutlineLoop> result;
  for (const auto& spine : spines) {
    require(spine.closed, "Emboss profile is not a closed region");
    EmbossOutlineLoop loop{spine.edges.front().second, {}};
    for (const auto& [edge, name] : spine.edges) {
      double first, last;
      const auto curve = BRep_Tool::Curve(edge, first, last);
      require(!curve.IsNull(), "Emboss source curve has no native geometry");
      auto local = GeomAPI::To2d(curve, plane);
      require(!local.IsNull(), "Emboss source curve is not planar");
      Handle(Geom2d_Curve) trimmed = new Geom2d_TrimmedCurve(local, first, last);
      if (edge.Orientation() == TopAbs_REVERSED)
        trimmed->Reverse();
      loop.curves.push_back(trimmed);
    }
    result.push_back(std::move(loop));
  }
  return result;
}
EmbossChart emboss_chart(const EmbossIntent& e, const std::vector<TopoDS_Face>& supports,
                         const ResolvedWorkplane& source, double depth, double orientation) {
  require(!supports.empty(), "Emboss has no target support");
  BRepAdaptor_Surface surface(supports.front());
  const auto kind = surface.GetType();
  require((e.mapping == EmbossMapping::Planar && kind == GeomAbs_Plane) ||
              (e.mapping == EmbossMapping::Cylindrical && kind == GeomAbs_Cylinder) ||
              (e.mapping == EmbossMapping::Conical && kind == GeomAbs_Cone),
          "Selected support does not match the explicit Emboss mapping");
  const auto carrier_axes = kind == GeomAbs_Plane      ? surface.Plane().Position()
                            : kind == GeomAbs_Cylinder ? surface.Cylinder().Position()
                                                       : surface.Cone().Position();
  const double signed_depth = (e.operation == EmbossOperation::Raise ? 1. : -1.) *
                              (supports.front().Orientation() == TopAbs_REVERSED ? -1. : 1.) *
                              (carrier_axes.Direct() ? 1. : -1.) * depth;
  EmbossChart chart;
  gp_Ax3 axes;
  if (kind == GeomAbs_Plane) {
    const auto plane = surface.Plane();
    axes = plane.Position();
    chart.base = new Geom_Plane(plane);
    auto offset = plane;
    offset.Translate(gp_Vec(axes.Direction()) * signed_depth);
    chart.depth = new Geom_Plane(offset);
    chart.periodic = false;
    chart.reference_radius = 1;
  } else if (kind == GeomAbs_Cylinder) {
    const auto cylinder = surface.Cylinder();
    axes = cylinder.Position();
    chart.reference_radius = cylinder.Radius();
    require(chart.reference_radius + signed_depth > 1e-6,
            "Emboss normal depth collapses the cylindrical support");
    chart.base = new Geom_CylindricalSurface(cylinder);
    chart.depth =
        new Geom_CylindricalSurface(gp_Cylinder(axes, chart.reference_radius + signed_depth));
    chart.periodic = true;
  } else {
    const auto cone = surface.Cone();
    axes = cone.Position();
    const double angle = cone.SemiAngle();
    chart.reference_radius = cone.RefRadius() + e.anchor_mm.y * std::sin(angle);
    require(chart.reference_radius > 1e-6 &&
                cone.RefRadius() + signed_depth * std::cos(angle) > 1e-6,
            "Emboss cone mapping reaches its singular apex");
    chart.base = new Geom_ConicalSurface(cone);
    auto offset_axes = axes;
    offset_axes.Translate(gp_Vec(axes.Direction()) * (-signed_depth * std::sin(angle)));
    chart.depth = new Geom_ConicalSurface(
        gp_Cone(offset_axes, angle, cone.RefRadius() + signed_depth * std::cos(angle)));
    chart.periodic = true;
  }
  for (const auto& face : supports) {
    BRepAdaptor_Surface other(face);
    require(other.GetType() == kind,
            "Emboss supports require one consistently oriented analytic carrier");
    const auto other_axes = kind == GeomAbs_Plane      ? other.Plane().Position()
                            : kind == GeomAbs_Cylinder ? other.Cylinder().Position()
                                                       : other.Cone().Position();
    const double first_sign =
        (supports.front().Orientation() == TopAbs_REVERSED ? -1. : 1.) * (axes.Direct() ? 1. : -1.);
    const double other_sign =
        (face.Orientation() == TopAbs_REVERSED ? -1. : 1.) * (other_axes.Direct() ? 1. : -1.);
    if (kind == GeomAbs_Plane)
      require(
          (gp_Vec(axes.Direction()) * first_sign).Dot(gp_Vec(other_axes.Direction()) * other_sign) >
              1 - 1e-9,
          "Emboss planar supports have opposing material normals");
    else
      require(first_sign == other_sign, "Emboss supports have opposing material normals");
    if (kind == GeomAbs_Plane)
      require(other.Plane().Distance(axes.Location()) < 1e-6 &&
                  other.Plane().Axis().Direction().IsParallel(axes.Direction(), 1e-9),
              "Emboss planar supports are not coplanar");
    if (kind == GeomAbs_Cylinder)
      require(std::abs(other.Cylinder().Radius() - chart.reference_radius) < 1e-6 &&
                  other.Cylinder().Axis().Direction().IsParallel(axes.Direction(), 1e-9) &&
                  gp_Lin(other.Cylinder().Axis()).Distance(axes.Location()) < 1e-6,
              "Emboss cylindrical supports do not share a carrier");
    if (kind == GeomAbs_Cone)
      require(other.Cone().Apex().Distance(surface.Cone().Apex()) < 1e-6 &&
                  other.Cone().Axis().Direction().IsParallel(axes.Direction(), 1e-9) &&
                  std::abs(other.Cone().SemiAngle() - surface.Cone().SemiAngle()) < 1e-9,
              "Emboss conical supports do not share a carrier");
    if (kind == GeomAbs_Cone)
      require(other.Cone().Apex().Distance(surface.Cone().Apex()) < 1e-6 &&
                  other.Cone().Axis().Direction().IsEqual(axes.Direction(), 1e-9) &&
                  std::abs(other.Cone().SemiAngle() - surface.Cone().SemiAngle()) < 1e-9,
              "Emboss conical supports do not share an oriented carrier");
  }
  const double a = orientation * std::numbers::pi / 180, c = std::cos(a), s = std::sin(a);
  double xx = c, xy = -s, yx = s, yy = c, tx = e.anchor_mm.x, ty = e.anchor_mm.y;
  if (chart.periodic) {
    xx /= chart.reference_radius;
    xy /= chart.reference_radius;
    tx /= chart.reference_radius;
    tx += std::remainder(e.seam_degrees, 360.) * std::numbers::pi / 180;
  } else {
    const gp_Vec sx = vec(source.x_axis) * c + vec(source.y_axis) * s,
                 sy = vec(source.x_axis) * (-s) + vec(source.y_axis) * c;
    const gp_Vec origin(axes.Location(), gp_Pnt(source.origin.x, source.origin.y, source.origin.z));
    xx = sx.Dot(gp_Vec(axes.XDirection()));
    xy = sy.Dot(gp_Vec(axes.XDirection()));
    yx = sx.Dot(gp_Vec(axes.YDirection()));
    yy = sy.Dot(gp_Vec(axes.YDirection()));
    tx += origin.Dot(gp_Vec(axes.XDirection()));
    ty += origin.Dot(gp_Vec(axes.YDirection()));
    require(std::abs(xx * yy - xy * yx) > 1e-9,
            "Emboss planar projection collapses the source region");
  }
  chart.transform.SetValue(1, 1, xx);
  chart.transform.SetValue(1, 2, xy);
  chart.transform.SetValue(2, 1, yx);
  chart.transform.SetValue(2, 2, yy);
  chart.transform.SetValue(1, 3, tx);
  chart.transform.SetValue(2, 3, ty);
  return chart;
}
void map_emboss_outlines(std::vector<EmbossOutlineLoop>& loops, EmbossChart& chart) {
  Bnd_Box2d bounds;
  for (auto& loop : loops)
    for (auto& curve : loop.curves) {
      curve = GeomLib::GTransform(curve, chart.transform);
      require(!curve.IsNull(), "Emboss source curve cannot map into the support chart");
      BndLib_Add2dCurve::Add(Geom2dAdaptor_Curve(curve), 1e-9, bounds);
    }
  double u0, v0, u1, v1;
  bounds.Get(u0, v0, u1, v1);
  if (!chart.periodic)
    return;
  require(u1 - u0 < 2 * std::numbers::pi - 1e-7, "Emboss wrap overlaps a complete circumference");
  const double phase = (u0 + u1) / 2 - std::numbers::pi;
  gp_GTrsf2d shift;
  shift.SetTranslationPart(gp_XY(-phase, 0));
  for (auto& loop : loops)
    for (auto& curve : loop.curves)
      curve = GeomLib::GTransform(curve, shift);
  for (auto* surface : {&chart.base, &chart.depth}) {
    if (auto cylinder = Handle(Geom_CylindricalSurface)::DownCast(*surface); !cylinder.IsNull()) {
      auto axes = cylinder->Position();
      turn(axes, phase);
      cylinder->SetPosition(axes);
    } else {
      auto cone = Handle(Geom_ConicalSurface)::DownCast(*surface);
      require(!cone.IsNull(), "Unknown Emboss periodic carrier");
      const double angle = cone->SemiAngle();
      require(std::min(cone->RefRadius() + v0 * std::sin(angle),
                       cone->RefRadius() + v1 * std::sin(angle)) > 1e-6,
              "Emboss wrap crosses the conical apex");
      auto axes = cone->Position();
      turn(axes, phase);
      cone->SetPosition(axes);
    }
  }
}
} // namespace blcad::geometry::detail
