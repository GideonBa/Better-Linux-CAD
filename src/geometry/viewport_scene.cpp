#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/core/sketch_contour.hpp"

#include "assembly_posed_leaf_shapes.hpp"
#include "geometry_shape_internal.hpp"

#include "blcad/geometry/profile_region_classifier.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRep_Builder.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::geometry {
namespace {

gp_Pnt to_point(Point3 point) {
  return {point.x, point.y, point.z};
}

Point3 offset(Point3 origin, Vector3 first, double first_scale, Vector3 second = {},
              double second_scale = 0.0) {
  return {origin.x + first.x * first_scale + second.x * second_scale,
          origin.y + first.y * first_scale + second.y * second_scale,
          origin.z + first.z * first_scale + second.z * second_scale};
}

TopoDS_Shape make_plane_shape(Point3 origin, Vector3 x_axis, Vector3 y_axis) {
  constexpr double half_extent = 50.0;
  BRepBuilderAPI_MakePolygon polygon;
  polygon.Add(to_point(offset(origin, x_axis, -half_extent, y_axis, -half_extent)));
  polygon.Add(to_point(offset(origin, x_axis, half_extent, y_axis, -half_extent)));
  polygon.Add(to_point(offset(origin, x_axis, half_extent, y_axis, half_extent)));
  polygon.Add(to_point(offset(origin, x_axis, -half_extent, y_axis, half_extent)));
  polygon.Close();
  return BRepBuilderAPI_MakeFace(polygon.Wire()).Shape();
}

TopoDS_Shape make_axis_shape(Point3 origin, Vector3 direction) {
  const gp_Dir normalized(direction.x, direction.y, direction.z);
  constexpr double half_extent = 60.0;
  const gp_Vec vector(normalized);
  return BRepBuilderAPI_MakeEdge(to_point(origin).Translated(vector.Multiplied(-half_extent)),
                                 to_point(origin).Translated(vector.Multiplied(half_extent)))
      .Shape();
}

std::string sketch_entity_id(const Sketch& sketch, const SketchEntityId& entity) {
  return "sketch/" + sketch.id().value() + "/entity/" + entity.value();
}

std::string sketch_3d_entity_id(const Sketch3D& sketch, const SketchEntityId& entity) {
  return "sketch3d/" + sketch.id().value() + "/entity/" + entity.value();
}

Error scene_error(std::string message) {
  return Error::geometry("geometry.viewport_scene", std::move(message));
}

template <typename PointRange> double signed_polygon_area_twice(const PointRange& points) noexcept {
  double area_twice = 0.0;
  for (std::size_t index = 0U; index < points.size(); ++index) {
    const auto& current = points[index];
    const auto& next = points[(index + 1U) % points.size()];
    area_twice += current.x * next.y - next.x * current.y;
  }
  return area_twice;
}

template <typename PointMapper>
Result<TopoDS_Wire> make_circle_wire(Point2 center, double radius, PointMapper map_point) {
  const gp_Pnt center_3d = map_point(center);
  const gp_Pnt x_3d = map_point({center.x + radius, center.y});
  const gp_Pnt y_3d = map_point({center.x, center.y + radius});
  const gp_Vec x_vec(center_3d, x_3d);
  const gp_Vec y_vec(center_3d, y_3d);
  const gp_Vec normal = x_vec.Crossed(y_vec);
  if (x_vec.Magnitude() <= gp::Resolution() || normal.Magnitude() <= gp::Resolution()) {
    return Result<TopoDS_Wire>::failure(
        scene_error("circle profile presentation has a degenerate workplane"));
  }
  const gp_Ax2 axes(center_3d, gp_Dir(normal), gp_Dir(x_vec));
  BRepBuilderAPI_MakeEdge edge(gp_Circ(axes, x_vec.Magnitude()));
  if (!edge.IsDone()) {
    return Result<TopoDS_Wire>::failure(
        scene_error("could not build circle profile presentation edge"));
  }
  BRepBuilderAPI_MakeWire wire(edge.Edge());
  if (!wire.IsDone()) {
    return Result<TopoDS_Wire>::failure(
        scene_error("could not build circle profile presentation wire"));
  }
  return Result<TopoDS_Wire>::success(wire.Wire());
}

template <typename PointRange, typename PointMapper>
Result<TopoDS_Wire> make_polygon_wire(const PointRange& points, PointMapper map_point) {
  if (points.size() < 3U)
    return Result<TopoDS_Wire>::failure(
        scene_error("closed profile presentation requires at least three points"));
  BRepBuilderAPI_MakePolygon polygon;
  for (const auto& point : points)
    polygon.Add(map_point(point));
  polygon.Close();
  if (!polygon.IsDone())
    return Result<TopoDS_Wire>::failure(
        scene_error("could not build closed profile presentation wire"));
  return Result<TopoDS_Wire>::success(polygon.Wire());
}

template <typename PointRange, typename PointMapper>
Result<TopoDS_Shape> make_profile_face(const PointRange& points, PointMapper map_point,
                                       const std::vector<ContainedProfileHole>& holes = {}) {
  auto outer_wire = make_polygon_wire(points, map_point);
  if (outer_wire.has_error())
    return Result<TopoDS_Shape>::failure(outer_wire.error());
  const double outer_area_twice = signed_polygon_area_twice(points);
  BRepBuilderAPI_MakeFace face(outer_wire.value());
  for (const ContainedProfileHole& hole : holes) {
    Result<TopoDS_Wire> wire = hole.kind == ContainedProfileHoleKind::Circle
                                   ? make_circle_wire(hole.center, hole.radius_mm, map_point)
                                   : make_polygon_wire(hole.vertices, map_point);
    if (wire.has_error())
      return Result<TopoDS_Shape>::failure(wire.error());
    TopoDS_Wire inner_wire = wire.value();
    const bool same_orientation =
        hole.kind == ContainedProfileHoleKind::Circle
            ? outer_area_twice > 0.0
            : outer_area_twice * signed_polygon_area_twice(hole.vertices) > 0.0;
    if (same_orientation)
      inner_wire.Reverse();
    face.Add(inner_wire);
  }
  if (!face.IsDone())
    return Result<TopoDS_Shape>::failure(
        scene_error("could not build closed profile presentation face"));
  return Result<TopoDS_Shape>::success(face.Shape());
}

} // namespace

Result<std::vector<ViewportSceneItem>>
ViewportSceneBuilder::build_part(const PartDocument& document,
                                 const ShapeCache& shape_cache) const {
  try {
    std::vector<ViewportSceneItem> items;
    items.reserve(shape_cache.body_shape_count() + document.datum_plane_count() +
                  document.datum_axis_count() + document.sketch_3d_count());
    for (const CachedBodyShape& cached : shape_cache.body_shapes()) {
      const Body* body = document.find_body(cached.body_id);
      if (body == nullptr || body->visibility() == BodyVisibility::Hidden || cached.shape.empty())
        continue;
      const auto kind = body->kind() == BodyKind::Surface ? ViewportSceneKind::SurfaceBody
                                                          : ViewportSceneKind::SolidBody;
      items.push_back({kind, "body/" + cached.body_id.value(), cached.shape});
    }

    if (items.empty() && shape_cache.has_final_shape() && shape_cache.final_shape() != nullptr &&
        !shape_cache.final_shape()->empty()) {
      items.push_back({ViewportSceneKind::SolidBody,
                       "feature/" + shape_cache.final_feature_id().value(),
                       *shape_cache.final_shape()});
    }

    const auto wrap = [](TopoDS_Shape shape) {
      return GeometryShape(std::make_shared<GeometryShape::Impl>(std::move(shape)));
    };
    for (const DatumPlane& plane : document.datum_planes()) {
      items.push_back({ViewportSceneKind::Datum, "datum-plane/" + plane.id().value(),
                       wrap(make_plane_shape(plane.origin(), plane.x_axis(), plane.y_axis()))});
    }
    for (const DatumAxis& axis : document.datum_axes()) {
      if (axis.kind() == DatumAxisKind::Explicit) {
        items.push_back({ViewportSceneKind::Datum, "datum-axis/" + axis.id().value(),
                         wrap(make_axis_shape(axis.origin(), axis.direction()))});
      }
    }
    for (const ConstructionLine& line : document.construction_lines()) {
      if (line.kind() == ConstructionLineKind::Explicit) {
        items.push_back({ViewportSceneKind::Datum, "construction-line/" + line.id().value(),
                         wrap(make_axis_shape(line.point(), line.direction()))});
      }
    }
    for (const ConstructionPlane& plane : document.construction_planes()) {
      if (plane.kind() == ConstructionPlaneKind::Explicit) {
        items.push_back({ViewportSceneKind::Datum, "construction-plane/" + plane.id().value(),
                         wrap(make_plane_shape(plane.origin(), plane.x_axis(), plane.y_axis()))});
      }
    }

    const WorkplaneResolver workplane_resolver;
    for (const Sketch& sketch : document.sketches()) {
      auto workplane = workplane_resolver.resolve_for_sketch(document, sketch, shape_cache);
      if (workplane.has_error())
        return Result<std::vector<ViewportSceneItem>>::failure(workplane.error());
      const auto point = [&workplane_resolver, &resolved = workplane.value()](Point2 local) {
        return to_point(workplane_resolver.evaluate_point(resolved, local));
      };
      for (const LineSegment& line : sketch.line_segments()) {
        items.push_back(
            {ViewportSceneKind::SketchEntity, sketch_entity_id(sketch, line.id()),
             wrap(BRepBuilderAPI_MakeEdge(point(line.start()), point(line.end())).Shape())});
      }
      for (const ArcSegment& arc : sketch.arc_segments()) {
        GC_MakeArcOfCircle curve(point(arc.start()), point(arc.mid()), point(arc.end()));
        if (!curve.IsDone())
          return Result<std::vector<ViewportSceneItem>>::failure(
              scene_error("could not build planar sketch arc presentation"));
        items.push_back({ViewportSceneKind::SketchEntity, sketch_entity_id(sketch, arc.id()),
                         wrap(BRepBuilderAPI_MakeEdge(curve.Value()).Shape())});
      }
      for (const SplineSegment& spline : sketch.spline_segments()) {
        TColgp_Array1OfPnt poles(1, 4);
        poles.SetValue(1, point(spline.start()));
        poles.SetValue(2, point(spline.control1()));
        poles.SetValue(3, point(spline.control2()));
        poles.SetValue(4, point(spline.end()));
        TColStd_Array1OfReal weights(1, 4);
        for (int i = 1; i <= 4; ++i) weights.SetValue(i, spline.weights()[i-1]);
        const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles, weights);
        items.push_back({ViewportSceneKind::SketchEntity, sketch_entity_id(sketch, spline.id()),
                         wrap(BRepBuilderAPI_MakeEdge(curve).Shape())});
      }

      // Dimension-driven sketch profiles (rectangle/circle) are authored
      // geometry too; without these items the interactive circle and
      // rectangle tools would commit invisible results.
      const auto parameter_mm = [&document](const ParameterId& id) -> std::optional<double> {
        const Parameter* parameter = document.find_parameter(id);
        if (parameter == nullptr)
          return std::nullopt;
        return parameter->value().millimeters();
      };
      const std::string profile_prefix = "sketch/" + sketch.id().value() + "/profile/";
      for (const RectangleProfile& profile : sketch.rectangle_profiles()) {
        const auto profile_width = parameter_mm(profile.width_parameter());
        const auto profile_height = parameter_mm(profile.height_parameter());
        if (!profile_width || !profile_height)
          continue;
        const Point2 center = profile.center();
        const double half_width = *profile_width * 0.5;
        const double half_height = *profile_height * 0.5;
        const std::array<Point2, 4> corners{{{center.x - half_width, center.y - half_height},
                                             {center.x + half_width, center.y - half_height},
                                             {center.x + half_width, center.y + half_height},
                                             {center.x - half_width, center.y + half_height}}};
        const std::vector<Point2> outer_vertices(corners.begin(), corners.end());
        auto holes =
            classify_contained_profile_holes(document, sketch, profile.id(), outer_vertices);
        if (holes.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(holes.error());
        auto face = make_profile_face(corners, point, holes.value());
        if (face.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(face.error());
        items.push_back({ViewportSceneKind::SketchEntity, profile_prefix + profile.id().value(),
                         wrap(std::move(face.value()))});
      }
      for (const CircleProfile& profile : sketch.circle_profiles()) {
        const auto diameter = parameter_mm(profile.diameter_parameter());
        if (!diameter || *diameter <= 0.0)
          continue;
        auto wire = make_circle_wire(profile.center(), *diameter * 0.5, point);
        if (wire.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(wire.error());
        BRepBuilderAPI_MakeFace face(wire.value());
        if (!face.IsDone())
          return Result<std::vector<ViewportSceneItem>>::failure(
              scene_error("could not build circle profile presentation face"));
        items.push_back({ViewportSceneKind::SketchEntity, profile_prefix + profile.id().value(),
                         wrap(face.Shape())});
      }

      // Topology-authored closed profiles reuse their persistent source
      // curves, but still need a dedicated compound presentation so the
      // finished profile region can be selected as one feature input. The
      // individual curves remain visible/selectable under their entity ids.
      const auto curve_edge = [&sketch,
                               &point](const SketchEntityId& entity) -> Result<TopoDS_Edge> {
        if (const auto* line = sketch.find_line_segment(entity))
          return Result<TopoDS_Edge>::success(
              BRepBuilderAPI_MakeEdge(point(line->start()), point(line->end())).Edge());
        if (const auto* arc = sketch.find_arc_segment(entity)) {
          GC_MakeArcOfCircle curve(point(arc->start()), point(arc->mid()), point(arc->end()));
          if (!curve.IsDone())
            return Result<TopoDS_Edge>::failure(
                scene_error("could not build closed-profile arc presentation"));
          return Result<TopoDS_Edge>::success(BRepBuilderAPI_MakeEdge(curve.Value()).Edge());
        }
        if (const auto* spline = sketch.find_spline_segment(entity)) {
          TColgp_Array1OfPnt poles(1, 4);
          poles.SetValue(1, point(spline->start()));
          poles.SetValue(2, point(spline->control1()));
          poles.SetValue(3, point(spline->control2()));
          poles.SetValue(4, point(spline->end()));
          TColStd_Array1OfReal weights(1, 4);
          for (int i = 1; i <= 4; ++i) weights.SetValue(i, spline->weights()[i-1]);
          const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles, weights);
          return Result<TopoDS_Edge>::success(BRepBuilderAPI_MakeEdge(curve).Edge());
        }
        return Result<TopoDS_Edge>::failure(
            scene_error("closed profile references an unavailable sketch curve"));
      };
      const auto contour_is_counter_clockwise =
          [&sketch](const std::vector<SketchEntityId>& contour) -> std::optional<bool> {
        std::vector<Point2> vertices;
        vertices.reserve(contour.size());
        for (const SketchEntityId& entity : contour) {
          const LineSegment* line = sketch.find_line_segment(entity);
          if (line == nullptr)
            return std::nullopt;
          vertices.push_back(line->start());
        }
        const double area_twice = signed_polygon_area_twice(vertices);
        if (std::abs(area_twice) <= 1.0e-12)
          return std::nullopt;
        return area_twice > 0.0;
      };
      const std::vector<std::vector<SketchEntityId>> no_inner_contours;
      const std::vector<ContainedProfileHole> no_profile_holes;
      const auto append_curve_profile =
          [&items, &profile_prefix, &wrap, &curve_edge, &contour_is_counter_clockwise,
           &point, &sketch](const ProfileId& id, const std::vector<SketchEntityId>& outer,
                   const std::vector<std::vector<SketchEntityId>>& inner,
                   const std::vector<ContainedProfileHole>& holes) -> Result<std::size_t> {
        const auto make_wire =
            [&curve_edge, &sketch](const std::vector<SketchEntityId>& contour) -> Result<TopoDS_Wire> {
          BRepBuilderAPI_MakeWire wire;
          auto traversal = resolve_sketch_contour(sketch, contour);
          for (std::size_t index = 0; index < contour.size(); ++index) {
            const auto& entity = contour[index];
            auto edge = curve_edge(entity);
            if (edge.has_error())
              return Result<TopoDS_Wire>::failure(edge.error());
            if (traversal && traversal.value()[index].reversed)
              edge.value().Reverse();
            wire.Add(edge.value());
          }
          if (!wire.IsDone())
            return Result<TopoDS_Wire>::failure(
                scene_error("could not build closed-profile presentation wire"));
          return Result<TopoDS_Wire>::success(wire.Wire());
        };

        auto outer_wire = make_wire(outer);
        if (outer_wire.has_error())
          return Result<std::size_t>::failure(outer_wire.error());
        const std::optional<bool> outer_is_counter_clockwise = contour_is_counter_clockwise(outer);
        BRepBuilderAPI_MakeFace face(outer_wire.value());
        for (const auto& contour : inner) {
          auto inner_wire = make_wire(contour);
          if (inner_wire.has_error())
            return Result<std::size_t>::failure(inner_wire.error());
          TopoDS_Wire oriented_inner = inner_wire.value();
          const std::optional<bool> inner_is_counter_clockwise =
              contour_is_counter_clockwise(contour);
          if (!outer_is_counter_clockwise || !inner_is_counter_clockwise ||
              *outer_is_counter_clockwise == *inner_is_counter_clockwise)
            oriented_inner.Reverse();
          face.Add(oriented_inner);
        }
        for (const ContainedProfileHole& hole : holes) {
          Result<TopoDS_Wire> inner_wire =
              hole.kind == ContainedProfileHoleKind::Circle
                  ? make_circle_wire(hole.center, hole.radius_mm, point)
                  : make_polygon_wire(hole.vertices, point);
          if (inner_wire.has_error())
            return Result<std::size_t>::failure(inner_wire.error());
          TopoDS_Wire oriented_inner = inner_wire.value();
          const std::optional<bool> hole_is_counter_clockwise =
              hole.kind == ContainedProfileHoleKind::Circle ? std::optional<bool>(true)
                                                            : [&hole]() -> std::optional<bool> {
            const double area_twice = signed_polygon_area_twice(hole.vertices);
            if (std::abs(area_twice) <= 1.0e-12)
              return std::nullopt;
            return area_twice > 0.0;
          }();
          if (!outer_is_counter_clockwise || !hole_is_counter_clockwise ||
              *outer_is_counter_clockwise == *hole_is_counter_clockwise)
            oriented_inner.Reverse();
          face.Add(oriented_inner);
        }
        if (!face.IsDone())
          return Result<std::size_t>::failure(
              scene_error("could not build closed-profile presentation face"));
        items.push_back(
            {ViewportSceneKind::SketchEntity, profile_prefix + id.value(), wrap(face.Shape())});
        return Result<std::size_t>::success(1U + inner.size() + holes.size());
      };
      for (const ClosedProfile& profile : sketch.closed_profiles()) {
        auto vertices = sketch.closed_profile_vertices(profile);
        if (vertices.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(vertices.error());
        auto holes =
            classify_contained_profile_holes(document, sketch, profile.id(), vertices.value());
        if (holes.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(holes.error());
        auto appended = append_curve_profile(profile.id(), profile.line_segments(),
                                             no_inner_contours, holes.value());
        if (appended.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(appended.error());
      }
      for (const ArcClosedProfile& profile : sketch.arc_closed_profiles()) {
        auto appended = append_curve_profile(profile.id(), profile.curve_segments(),
                                             no_inner_contours, no_profile_holes);
        if (appended.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(appended.error());
      }
      for (const CompositeClosedProfile& profile : sketch.composite_closed_profiles()) {
        auto appended = append_curve_profile(profile.id(), profile.outer_contour(),
                                             profile.inner_contours(), no_profile_holes);
        if (appended.has_error())
          return Result<std::vector<ViewportSceneItem>>::failure(appended.error());
      }

      // Legacy/raw line sketches have no persistent profile presentation yet,
      // but a bounded region is still a valid selection-first feature input.
      // Publish a transient face under the same deterministic generated id
      // used by SketchFinishService and the interactive materialization path.
      // Existing materialized contours are suppressed so their coplanar face
      // does not compete with a duplicate generated presentation.
      auto detected_regions = SketchFinishService::analyze_unmaterialized(document, sketch);
      if (detected_regions) {
        for (const auto& region : detected_regions.value().regions) {
          auto holes =
              classify_contained_profile_holes(document, sketch, region.id, region.vertices);
          if (holes.has_error())
            continue;
          auto face = make_profile_face(region.vertices, point, holes.value());
          if (face.has_error())
            continue;
          items.push_back({ViewportSceneKind::SketchEntity, profile_prefix + region.id.value(),
                           wrap(std::move(face.value()))});
        }
      }

      for (const CircularHolePattern& pattern : sketch.circular_hole_patterns()) {
        const Parameter* radius = document.find_parameter(pattern.radius_parameter());
        const Parameter* count = document.find_parameter(pattern.count_parameter());
        const Parameter* diameter = document.find_parameter(pattern.hole_diameter_parameter());
        if (radius == nullptr || radius->type() != ParameterType::Length || count == nullptr ||
            count->type() != ParameterType::Count || diameter == nullptr ||
            diameter->type() != ParameterType::Length || diameter->value().millimeters() <= 0.0)
          continue;
        BRep_Builder compound_builder;
        TopoDS_Compound compound;
        compound_builder.MakeCompound(compound);
        const std::size_t hole_count = count->value().count_value();
        for (std::size_t index = 0U; index < hole_count; ++index) {
          const double angle =
              pattern.angle_offset_deg() * std::numbers::pi / 180.0 +
              2.0 * std::numbers::pi * static_cast<double>(index) / static_cast<double>(hole_count);
          const Point2 center{pattern.center().x + radius->value().millimeters() * std::cos(angle),
                              pattern.center().y + radius->value().millimeters() * std::sin(angle)};
          const gp_Pnt center_3d = point(center);
          const double hole_radius = diameter->value().millimeters() * 0.5;
          const gp_Pnt x_3d = point({center.x + hole_radius, center.y});
          const gp_Pnt y_3d = point({center.x, center.y + hole_radius});
          const gp_Vec x_vec(center_3d, x_3d);
          const gp_Vec y_vec(center_3d, y_3d);
          const gp_Vec normal = x_vec.Crossed(y_vec);
          if (x_vec.Magnitude() <= gp::Resolution() || normal.Magnitude() <= gp::Resolution())
            continue;
          const gp_Ax2 axes(center_3d, gp_Dir(normal), gp_Dir(x_vec));
          compound_builder.Add(compound,
                               BRepBuilderAPI_MakeEdge(gp_Circ(axes, x_vec.Magnitude())).Shape());
        }
        items.push_back({ViewportSceneKind::SketchEntity, profile_prefix + pattern.id().value(),
                         wrap(compound)});
      }
    }

    const Sketch3DGeometryAdapter sketch_3d_adapter;
    for (const Sketch3D& sketch : document.sketches_3d()) {
      auto converted = sketch_3d_adapter.convert(document, sketch.id());
      if (converted.has_error())
        return Result<std::vector<ViewportSceneItem>>::failure(converted.error());
      for (const Sketch3DGeometryProduct& product : converted.value().products()) {
        if (!product.shape.empty()) {
          items.push_back({ViewportSceneKind::SketchEntity,
                           sketch_3d_entity_id(sketch, product.entity_id), product.shape});
        }
      }
    }

    for (const PathCurve& path : document.path_curves()) {
      for (std::size_t index = 0; index < path.segments().size(); ++index) {
        const PathSegmentReference& segment = path.segments()[index];
        if (!segment.entity().has_value())
          continue;
        std::string source_id;
        if (segment.planar_sketch().has_value()) {
          source_id =
              "sketch/" + segment.planar_sketch()->value() + "/entity/" + segment.entity()->value();
        } else if (segment.sketch_3d().has_value()) {
          source_id =
              "sketch3d/" + segment.sketch_3d()->value() + "/entity/" + segment.entity()->value();
        }
        const auto source =
            std::find_if(items.begin(), items.end(),
                         [&source_id](const auto& item) { return item.semantic_id == source_id; });
        if (source != items.end()) {
          items.push_back({ViewportSceneKind::Path,
                           "path/" + path.id().value() + "/segment/" + std::to_string(index),
                           source->shape});
        }
      }
    }
    return Result<std::vector<ViewportSceneItem>>::success(std::move(items));
  } catch (const Standard_Failure& failure) {
    return Result<std::vector<ViewportSceneItem>>::failure(scene_error(
        failure.GetMessageString() != nullptr ? failure.GetMessageString() : "OCCT scene error"));
  }
}

Result<std::vector<ViewportSceneItem>>
ViewportSceneBuilder::build_project(const Project& project) const {
  auto posed = detail::build_posed_leaf_shapes(project);
  if (posed.has_error())
    return Result<std::vector<ViewportSceneItem>>::failure(posed.error());

  std::vector<ViewportSceneItem> items;
  items.reserve(posed.value().leaves.size());
  for (auto& leaf : posed.value().leaves) {
    const std::string semantic_id = "component/" + detail::leaf_occurrence_key(leaf.leaf);
    GeometryShape shape(std::make_shared<GeometryShape::Impl>(std::move(leaf.shape)));
    items.push_back({ViewportSceneKind::Component, semantic_id, std::move(shape)});
  }
  return Result<std::vector<ViewportSceneItem>>::success(std::move(items));
}

} // namespace blcad::geometry
