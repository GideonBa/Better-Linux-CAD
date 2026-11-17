#include "blcad/geometry/sketch_driven_guides.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "emboss_mapping.hpp"
#include "part_history_evaluation.hpp"
#include "rib_native.hpp"
#include "semantic_face_resolution.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BndLib_Add2dCurve.hxx>
#include <Bnd_Box2d.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <TopoDS.hxx>
#include <numbers>
#include <sstream>
namespace blcad::geometry {
namespace {
template <class T> T take(Result<T> r) {
  if (!r)
    throw std::runtime_error(r.error().object_id() + ": " + r.error().message());
  return std::move(r.value());
}
Vector3 vector(const gp_Vec& v) {
  return {v.X(), v.Y(), v.Z()};
}
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
double value(const PartDocument& p, ParameterId id) {
  const auto* parameter = p.find_parameter(id);
  if (!parameter)
    throw std::runtime_error("Missing guide parameter: " + id.value());
  return parameter->type() == ParameterType::Angle ? parameter->value().degrees()
                                                   : parameter->value().millimeters();
}
} // namespace
Result<std::vector<SketchDrivenFaceTarget>>
SketchDrivenGuideQuery::faces(const PartDocument& p, const ShapeCache& c, BodyId body,
                              StepImportTopologyContext* context,
                              std::optional<std::size_t> prefix) {
  try {
    auto targets = take(FinishingSelectionBuilder::build(
        p, c, body, PartFeatureInputRole::SurfaceSourceFace, context, prefix));
    std::vector<SketchDrivenFaceTarget> out;
    for (auto& target : targets) {
      if (!std::holds_alternative<FaceReference>(target.reference))
        continue;
      BRepAdaptor_Surface surface(TopoDS::Face(*OcctShapeView::get(target.shape)));
      const auto frame = SurfaceSelectionGeometry::normal_frame(target.shape);
      if (!frame)
        continue;
      std::optional<EmbossMapping> mapping;
      std::string kind = "Unsupported support";
      if (surface.GetType() == GeomAbs_Plane) {
        mapping = EmbossMapping::Planar;
        kind = "Planar face";
      }
      if (surface.GetType() == GeomAbs_Cylinder) {
        mapping = EmbossMapping::Cylindrical;
        kind = "Cylindrical face";
      }
      if (surface.GetType() == GeomAbs_Cone) {
        mapping = EmbossMapping::Conical;
        kind = "Conical face";
      }
      std::ostringstream label;
      label.precision(5);
      label << kind << " at (" << frame.value().origin.x << ", " << frame.value().origin.y << ", "
            << frame.value().origin.z << ")";
      std::optional<double> center;
      const auto at =
          gp_Pnt(frame.value().origin.x, frame.value().origin.y, frame.value().origin.z);
      if (surface.GetType() == GeomAbs_Cylinder) {
        const auto axes = surface.Cylinder().Position();
        center = gp_Vec(axes.Location(), at).Dot(gp_Vec(axes.Direction()));
      }
      if (surface.GetType() == GeomAbs_Cone) {
        const auto cone = surface.Cone();
        center = gp_Vec(cone.Location(), at).Dot(gp_Vec(cone.Axis().Direction())) /
                 std::cos(cone.SemiAngle());
      }
      out.push_back({std::move(target), mapping, frame.value().origin, frame.value().normal,
                     label.str(), center});
    }
    return Result<std::vector<SketchDrivenFaceTarget>>::success(std::move(out));
  } catch (const std::exception& e) {
    return Result<std::vector<SketchDrivenFaceTarget>>::failure(
        Error::geometry(body.value(), e.what()));
  }
}
Result<std::vector<SketchDrivenGuide>>
SketchDrivenGuideQuery::guides(const PartDocument& p, const SketchDrivenFeature& feature,
                               const ShapeCache& c, StepImportTopologyContext* context,
                               std::optional<std::size_t> prefix) {
  try {
    take(detail::validate_history_modeling_scope(p, c, prefix));
    const auto& definition = feature.definition().intent;
    const SketchId sketch = std::holds_alternative<RibWebIntent>(definition)
                                ? std::get<RibWebIntent>(definition).source.sketch
                                : emboss_source_sketch(std::get<EmbossIntent>(definition));
    const auto* source = p.find_sketch(sketch);
    if (!source)
      throw std::runtime_error("Missing source Sketch");
    const auto frame = take(WorkplaneResolver{}.resolve_for_sketch(p, *source, c));
    std::vector<SketchDrivenGuide> out;
    if (const auto* r = std::get_if<RibWebIntent>(&definition)) {
      auto spines = detail::rib_spines(p, *r, frame);
      if (spines.empty())
        throw std::runtime_error("Missing Rib spine");
      BRepAdaptor_Curve curve(spines.front().edges.front().first);
      gp_Pnt at;
      gp_Vec tangent;
      curve.D1((curve.FirstParameter() + curve.LastParameter()) / 2, at, tangent);
      if (spines.front().edges.front().first.Orientation() == TopAbs_REVERSED)
        tangent.Reverse();
      gp_Vec n(frame.normal.x, frame.normal.y, frame.normal.z);
      gp_Vec side = r->plane == RibExtrusionPlane::PerpendicularToSketch
                        ? n.Crossed(tangent).Normalized()
                        : n;
      gp_Vec d = gp_Vec(frame.x_axis.x, frame.x_axis.y, frame.x_axis.z) * r->direction.x +
                 gp_Vec(frame.y_axis.x, frame.y_axis.y, frame.y_axis.z) * r->direction.y +
                 n * r->direction.z;
      d.Normalize();
      if (r->side == RibThicknessSide::Second)
        side.Reverse();
      out.push_back({"thickness", "Thickness", r->thickness, point(at), vector(side), frame.x_axis,
                     value(p, r->thickness), r->side == RibThicknessSide::Symmetric ? .5 : 1,
                     false});
      if (r->second_thickness)
        out.push_back({"second", "Second thickness", *r->second_thickness, point(at), vector(-side),
                       frame.x_axis, value(p, *r->second_thickness)});
      if (const auto* extent = std::get_if<RibDistanceExtent>(&r->extent))
        out.push_back({"distance", "Termination distance", extent->distance, point(at), vector(d),
                       frame.x_axis, value(p, extent->distance)});
      else
        out.push_back({"termination",
                       "To selected support",
                       {},
                       point(at),
                       vector(d),
                       frame.x_axis,
                       0,
                       1,
                       false});
      if (r->draft)
        out.push_back({"draft", "Draft", *r->draft, point(at), vector(tangent.Normalized()),
                       vector(d), value(p, *r->draft), 1, true});
    } else {
      const auto& e = std::get<EmbossIntent>(definition);
      const auto* body = c.find_body_shape(feature.target_body());
      if (!body)
        throw std::runtime_error("Missing target Body");
      std::vector<TopoDS_Face> supports;
      for (const auto& ref : e.supports)
        supports.push_back(take(resolve_semantic_face(p, c, *OcctShapeView::get(*body), ref,
                                                      feature.id(), "Emboss guide", context)));
      auto loops = std::holds_alternative<ProfileRegionReference>(e.source)
                       ? detail::emboss_profile_outlines(
                             p, std::get<ProfileRegionReference>(e.source), frame)
                       : detail::emboss_text_outlines(p, std::get<EmbossTextSource>(e.source));
      auto chart = detail::emboss_chart(e, supports, frame, value(p, e.depth),
                                        e.orientation ? value(p, *e.orientation) : 0);
      detail::map_emboss_outlines(loops, chart);
      Bnd_Box2d bounds;
      for (const auto& loop : loops)
        for (const auto& curve : loop.curves)
          BndLib_Add2dCurve::Add(Geom2dAdaptor_Curve(curve), 1e-9, bounds);
      double u0, v0, u1, v1;
      bounds.Get(u0, v0, u1, v1);
      double u = (u0 + u1) / 2, v = (v0 + v1) / 2;
      const auto at = chart.base->Value(u, v);
      gp_Vec normal(at, chart.depth->Value(u, v));
      normal.Normalize();
      out.push_back({"depth",
                     e.operation == EmbossOperation::Raise ? "Raised depth" : "Recess depth",
                     e.depth, point(at), vector(normal), frame.x_axis, value(p, e.depth)});
      if (e.mapping != EmbossMapping::Planar) {
        gp_Ax3 axes;
        BRepAdaptor_Surface original(supports.front());
        axes = e.mapping == EmbossMapping::Cylindrical ? original.Cylinder().Position()
                                                       : original.Cone().Position();
        out.push_back({"seam",
                       "Wrap seam",
                       {},
                       point(axes.Location().Translated(
                           gp_Vec(axes.Direction()) *
                           gp_Vec(axes.Location(), at).Dot(gp_Vec(axes.Direction())))),
                       vector(gp_Vec(axes.Direction())),
                       vector(gp_Vec(axes.XDirection())),
                       e.seam_degrees,
                       1,
                       true});
      }
    }
    return Result<std::vector<SketchDrivenGuide>>::success(std::move(out));
  } catch (const Standard_Failure& e) {
    return Result<std::vector<SketchDrivenGuide>>::failure(
        Error::geometry(feature.id().value(),
                        e.GetMessageString() ? e.GetMessageString() : "Native guide query failed"));
  } catch (const std::exception& e) {
    return Result<std::vector<SketchDrivenGuide>>::failure(
        Error::geometry(feature.id().value(), e.what()));
  }
}
} // namespace blcad::geometry
