#include "blcad/geometry/viewport_scene.hpp"

#include "assembly_posed_leaf_shapes.hpp"
#include "geometry_shape_internal.hpp"

#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

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
        const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
        items.push_back({ViewportSceneKind::SketchEntity, sketch_entity_id(sketch, spline.id()),
                         wrap(BRepBuilderAPI_MakeEdge(curve).Shape())});
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
