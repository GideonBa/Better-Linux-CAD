#include "curve_surface_milestone_oracle.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "curve_surface_milestone_fixture.hpp"
#include "geometry_volume_oracle.hpp"
#include "loft_section_refinement_fixture.hpp"
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>
namespace blcad::test::curve_surface_milestone {
namespace lf = loft_refinement;
namespace {
struct Face {
  TopoDS_Face shape;
  Handle(Geom_Surface) surface;
  double u0, u1, v0, v1;
  Bnd_Box box;
};
struct Frame {
  gp_Pnt point;
  gp_Vec u, v, uu, vv, uv, normal;
  double distance;
};
std::vector<Face> faces(const TopoDS_Shape& shape) {
  std::vector<Face> result;
  for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
    Face f;
    f.shape = TopoDS::Face(it.Current());
    f.surface = BRep_Tool::Surface(f.shape);
    BRepTools::UVBounds(f.shape, f.u0, f.u1, f.v0, f.v1);
    BRepBndLib::AddOptimal(f.shape, f.box, false, false);
    f.box.Enlarge(1e-5);
    result.push_back(std::move(f));
  }
  return result;
}
Frame frame(const std::vector<Face>& faces, gp_Pnt point, gp_Vec normal) {
  std::optional<Frame> best;
  double best_alignment = -1;
  for (const auto& f : faces) {
    if (f.box.IsOut(point))
      continue;
    GeomAPI_ProjectPointOnSurf projected(point, f.surface, f.u0, f.u1, f.v0, f.v1, 1e-10);
    if (projected.NbPoints() == 0 || projected.LowerDistance() > 1e-5)
      continue;
    double u, v;
    projected.LowerDistanceParameters(u, v);
    const auto state = BRepClass_FaceClassifier(f.shape, gp_Pnt2d(u, v), 1e-7).State();
    if (state != TopAbs_IN && state != TopAbs_ON)
      continue;
    Frame value;
    f.surface->D2(u, v, value.point, value.u, value.v, value.uu, value.vv, value.uv);
    const auto cross = value.u.Crossed(value.v);
    if (cross.Magnitude() < 1e-12)
      continue;
    value.normal = cross.Normalized();
    if (f.shape.Orientation() == TopAbs_REVERSED)
      value.normal.Reverse();
    const double alignment = std::abs(value.normal.Dot(normal));
    if (alignment < .9 || alignment < best_alignment)
      continue;
    value.distance = point.Distance(value.point);
    best = value;
    best_alignment = alignment;
  }
  if (!best)
    throw std::runtime_error("No native side face matches the independent boundary station");
  return *best;
}
double curvature(gp_Vec direction, gp_Vec u, gp_Vec v, gp_Vec uu, gp_Vec vv, gp_Vec uv,
                 gp_Vec normal) {
  const double E = u.Dot(u), F = u.Dot(v), G = v.Dot(v), det = E * G - F * F;
  if (det <= 1e-18)
    throw std::runtime_error("Singular native differential station");
  const double a = (G * u.Dot(direction) - F * v.Dot(direction)) / det;
  const double b = (E * v.Dot(direction) - F * u.Dot(direction)) / det;
  return (a * a * uu.Dot(normal) + 2 * a * b * uv.Dot(normal) + b * b * vv.Dot(normal)) /
         direction.SquareMagnitude();
}
Point2 second(const std::array<Point2, 4>& p, double t) {
  return {6 * ((1 - t) * (p[2].x - 2 * p[1].x + p[0].x) + t * (p[3].x - 2 * p[2].x + p[1].x)),
          6 * ((1 - t) * (p[2].y - 2 * p[1].y + p[0].y) + t * (p[3].y - 2 * p[2].y + p[1].y))};
}
bool contains(const TopoDS_Shape& shape, Point3 point) {
  for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next())
    if (BRepClass3d_SolidClassifier(it.Current(), gp_Pnt(point.x, point.y, point.z), 1e-7)
            .State() == TopAbs_IN)
      return true;
  return false;
}
} // namespace
PhysicalReport physical(const geometry::GeometryShape& shape, bool pattern, bool blend,
                        double x_shift) {
  const auto* native = geometry::OcctShapeView::get(shape);
  if (!native)
    throw std::invalid_argument("Native milestone geometry required");
  PhysicalReport report;
  report.valid = BRepCheck_Analyzer(*native, true).IsValid();
  report.volume_mm3 = integrated_solid_volume(shape).volume_mm3;
  for (TopExp_Explorer it(*native, TopAbs_SOLID); it.More(); it.Next())
    ++report.solids;
  const auto face_list = faces(*native);
  const gp_Vec axis(0, 0, 1);
  for (int copy = 0; copy < (pattern ? 2 : 1); ++copy) {
    const double shift = x_shift + 80 * copy;
    for (std::size_t station : {std::size_t(0), std::size_t(2)})
      for (bool upper : {false, true})
        for (int i = 1; i <= 4; ++i) {
          const double t = i / 5.0, angle = station == 2 ? -.08 : 0;
          const auto poles = lf::poles(station, upper);
          const auto p = lf::model(station, lf::at(poles, t));
          const auto d = lf::derivative(poles, t), dd = second(poles, t);
          const gp_Vec tangent(std::cos(angle) * d.x, d.y, std::sin(angle) * d.x),
              acceleration(std::cos(angle) * dd.x, dd.y, std::sin(angle) * dd.x);
          const auto normal = (upper ? axis.Crossed(tangent) : tangent.Crossed(axis)).Normalized();
          const auto actual = frame(face_list, {p.x + shift, p.y, p.z}, normal);
          report.boundary_error_mm = std::max(report.boundary_error_mm, actual.distance);
          report.normal_error_rad = std::max(
              report.normal_error_rad, std::acos(std::clamp(actual.normal.Dot(normal), -1.0, 1.0)));
          for (const auto& direction : {tangent.Normalized(), axis, tangent.Normalized() + axis}) {
            const double expected =
                curvature(direction, tangent, axis, acceleration, {}, {}, normal);
            const double measured = curvature(direction, actual.u, actual.v, actual.uu, actual.vv,
                                              actual.uv, actual.normal);
            report.curvature_error_per_mm =
                std::max(report.curvature_error_per_mm, std::abs(measured - expected));
          }
          ++report.stations;
        }
    auto center = lf::model(1, {0, 0});
    center.x += shift;
    report.material =
        report.material && contains(*native, center) && contains(*native, {3 + shift, 21, 20}) &&
        contains(*native, {10.5 * std::cos(-.6) + shift, 10.5 * std::sin(-.6), 10}) &&
        !contains(*native, {11.3 * std::cos(-.6) + shift, 11.3 * std::sin(-.6), 10}) &&
        !contains(*native, {9.7 * std::cos(-.6) + shift, 9.7 * std::sin(-.6), 10}) &&
        !contains(*native, {4.2 + shift, 21, 20}) && !contains(*native, {50 + shift, 0, 20});
  }
  if (blend)
    for (int i = 0; i < 8; ++i)
      for (int j = 0; j <= 4; ++j) {
        const double phi = i * std::numbers::pi / 4, theta = j * std::numbers::pi / 8;
        const double radial = .8 + .2 * std::sin(theta);
        const Point3 p{3 + x_shift + radial * std::cos(phi), 21.8 + .2 * std::cos(theta),
                       20 + radial * std::sin(phi)};
        report.blend_error_mm = std::max(report.blend_error_mm, distance_to_faces(shape, p));
      }
  return report;
}
double rim_removed_volume(const geometry::GeometryShape& before,
                          const geometry::GeometryShape& after) {
  const auto region = BRepPrimAPI_MakeBox(gp_Pnt(1, 21, 18), 4, 2, 4).Shape();
  const auto volume = [&](const geometry::GeometryShape& shape) {
    const auto* native = geometry::OcctShapeView::get(shape);
    if (!native)
      throw std::runtime_error("Native rim volume required");
    BRepAlgoAPI_Common common(*native, region);
    common.Build();
    if (!common.IsDone() || common.Shape().IsNull())
      throw std::runtime_error("Rim integration region failed");
    GProp_GProps props;
    BRepGProp::VolumeProperties(common.Shape(), props, 1e-12, true);
    return props.Mass();
  };
  return volume(before) - volume(after);
}
PartDocument connected_fixture() {
  return finished();
}
void edit_connected_section_and_guide(PartDocument& p, double shift, bool repair) {
  edit_middle_and_guide(p, shift, repair);
}
PhysicalReport reimported(const std::vector<geometry::GeometryShape>& shapes) {
  PhysicalReport result;
  std::array<bool, 2> seen{};
  if (shapes.size() != 2)
    throw std::runtime_error("STEP must contain exactly the two patterned solids");
  for (const auto& shape : shapes) {
    const auto* native = geometry::OcctShapeView::get(shape);
    if (!native)
      throw std::runtime_error("STEP has no native shape");
    Bnd_Box box;
    BRepBndLib::AddOptimal(*native, box, false, false);
    const int copy = box.CornerMin().X() < 40 ? 0 : 1;
    if (seen[copy])
      throw std::runtime_error("STEP patterned placement is duplicated");
    seen[copy] = true;
    const auto value = physical(shape, false, copy == 0, 80 * copy);
    result.solids += value.solids;
    result.stations += value.stations;
    result.volume_mm3 += value.volume_mm3;
    result.valid = result.valid && value.valid;
    result.material = result.material && value.material;
    result.boundary_error_mm = std::max(result.boundary_error_mm, value.boundary_error_mm);
    result.normal_error_rad = std::max(result.normal_error_rad, value.normal_error_rad);
    result.curvature_error_per_mm =
        std::max(result.curvature_error_per_mm, value.curvature_error_per_mm);
    result.blend_error_mm = std::max(result.blend_error_mm, value.blend_error_mm);
  }
  return result;
}
double section_error(const geometry::GeometryShape& shape, double shift) {
  double result = 0;
  for (std::size_t station = 0; station < 3; ++station)
    for (bool upper : {false, true})
      for (int i = 0; i <= 8; ++i) {
        auto poles = lf::poles(station, upper);
        if (station == 1)
          poles[0].x += shift;
        const auto p = lf::model(station, lf::at(poles, i / 8.0));
        result = std::max(result, distance_to_faces(shape, p));
      }
  return result;
}
} // namespace blcad::test::curve_surface_milestone
