#include "blcad/geometry/sketch_driven_adapter.hpp"
#include "blcad/geometry/closed_profile_adapter.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "geometry_shape_internal.hpp"
#include "rib_native.hpp"
#include "semantic_face_resolution.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
namespace blcad::geometry {
namespace {
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <class T> T take(Result<T> v) {
  if (!v)
    throw std::runtime_error(v.error().object_id() + ": " + v.error().message());
  return std::move(v.value());
}
int solids(const TopoDS_Shape& s) {
  int n = 0;
  for (TopExp_Explorer it(s, TopAbs_SOLID); it.More(); it.Next())
    ++n;
  return n;
}
} // namespace
Result<GeometryShape> SketchDrivenAdapter::apply(const PartDocument& p,
                                                 const SketchDrivenFeature& feature,
                                                 const GeometryShape& target,
                                                 const ShapeCache& cache,
                                                 StepImportTopologyContext* imports) const {
  try {
    require(!target.empty(), "Sketch-driven target Body is missing");
    if (std::holds_alternative<EmbossIntent>(feature.definition().intent))
      return apply_emboss(p, feature, target, cache, imports);
    const auto& r = std::get<RibWebIntent>(feature.definition().intent);
    const auto* sketch = p.find_sketch(r.source.sketch);
    require(sketch, "Rib source Sketch is missing");
    const auto frame = take(WorkplaneResolver{}.resolve_for_sketch(p, *sketch, cache));
    gp_Vec d(frame.x_axis.x * r.direction.x + frame.y_axis.x * r.direction.y +
                 frame.normal.x * r.direction.z,
             frame.x_axis.y * r.direction.x + frame.y_axis.y * r.direction.y +
                 frame.normal.y * r.direction.z,
             frame.x_axis.z * r.direction.x + frame.y_axis.z * r.direction.y +
                 frame.normal.z * r.direction.z);
    d.Normalize();
    const gp_Vec n(frame.normal.x, frame.normal.y, frame.normal.z);
    require(r.plane == RibExtrusionPlane::PerpendicularToSketch
                ? std::abs(std::abs(n.Dot(d)) - 1) < 1e-9
                : std::abs(n.Dot(d)) < 1e-9,
            "Rib direction does not match the selected Sketch extrusion mode");
    const auto value = [&](ParameterId id) {
      const auto* v = p.find_parameter(id);
      require(v && v->type() == ParameterType::Length && v->value().millimeters() > 1e-6,
              "Rib size must exceed numerical thickness tolerance");
      return v->value().millimeters();
    };
    const double t = value(r.thickness);
    double a = t / 2, b = -t / 2;
    if (r.side == RibThicknessSide::First) {
      a = t;
      b = 0;
    } else if (r.side == RibThicknessSide::Second) {
      a = 0;
      b = -t;
    } else if (r.side == RibThicknessSide::Asymmetric) {
      a = t;
      b = -value(*r.second_thickness);
    }
    double draft = 0;
    if (r.draft) {
      const auto* v = p.find_parameter(*r.draft);
      require(v && v->type() == ParameterType::Angle, "Rib draft binding is unavailable");
      draft = std::tan(v->value().degrees() * std::numbers::pi / 180.);
    }
    const auto spines = detail::rib_spines(p, r, frame);
    double length = 0;
    std::vector<TopoDS_Face> limits;
    if (const auto* distance = std::get_if<RibDistanceExtent>(&r.extent))
      length = value(distance->distance);
    else {
      Bnd_Box bounds;
      BRepBndLib::Add(target.impl_->shape, bounds);
      for (const auto& body : cache.body_shapes())
        if (!body.shape.empty())
          BRepBndLib::Add(body.shape.impl_->shape, bounds);
      double x0, y0, z0, x1, y1, z1;
      bounds.Get(x0, y0, z0, x1, y1, z1);
      length = 2 * std::hypot(x1 - x0, y1 - y0, z1 - z0) +
               std::hypot(frame.origin.x - x0, frame.origin.y - y0, frame.origin.z - z0) + t + 1;
      if (const auto* body = std::get_if<RibBodyExtent>(&r.extent)) {
        require(body->body != feature.target_body(),
                "Rib termination by its own current target is ambiguous; select an earlier face");
        const auto* limit = cache.find_body_shape(body->body);
        require(limit, "Rib termination Body is unavailable in this history prefix");
        for (TopExp_Explorer it(limit->impl_->shape, TopAbs_FACE); it.More(); it.Next())
          limits.push_back(TopoDS::Face(it.Current()));
      } else {
        const auto& ref = std::get<RibFaceExtent>(r.extent).face;
        if (const auto* result = std::get_if<ResultFaceReference>(&ref.source())) {
          auto face = take(ResultFaceSupportGeometry::resolve_face(p, cache, *result, imports));
          limits.push_back(TopoDS::Face(face.impl_->shape));
        } else {
          std::optional<BodyId> owner;
          if (const auto* semantic = std::get_if<SemanticFaceReference>(&ref.source())) {
            const auto* source = p.find_feature(semantic->source_feature());
            if (source && source->body_result_context())
              owner = source->body_result_context()->effective_produced_body();
          } else if (const auto* imported = std::get_if<ImportedTopologyReference>(&ref.source())) {
            const auto* catalog = p.find_imported_topology_catalog(imported->catalog());
            const auto* entry = catalog ? catalog->find(imported->entry()) : nullptr;
            if (entry)
              for (const auto& base : p.imported_body_features())
                if (base.definition().catalog() == catalog->id())
                  for (const auto& out : base.definition().outputs())
                    if (entry->parent_body == out.imported_body)
                      owner = out.body;
          }
          require(owner.has_value(), "Rib face termination has no unambiguous owned Body");
          const auto* body = cache.find_body_shape(*owner);
          require(body, "Rib limit Body is unavailable");
          limits.push_back(take(resolve_semantic_face(p, cache, body->impl_->shape, ref,
                                                      feature.id(), "Rib limit", imports)));
        }
      }
    }
    std::optional<detail::RibNative> tools;
    std::vector<detail::FaceLineage> tool_witnesses;
    for (const auto& spine : spines) {
      auto native = detail::rib_tool(spine, r, feature.id(), frame, a, b, draft, length, d);
      if (!limits.empty()) {
        auto backing = std::make_shared<GeometryShape::Impl>(native.shape);
        backing->face_lineage = native.lineage;
        const GeometryShape tool(backing);
        std::optional<TopoDS_Face> selected;
        double nearest = std::numeric_limits<double>::infinity();
        for (const auto& face : limits) {
          BRepAlgoAPI_Common overlap;
          TopTools_ListOfShape args, cutters;
          args.Append(native.shape);
          cutters.Append(face);
          overlap.SetArguments(args);
          overlap.SetTools(cutters);
          overlap.SetNonDestructive(true);
          overlap.Build();
          require(overlap.IsDone() && !overlap.HasErrors(),
                  "Cannot check actual Rib limit coverage");
          GProp_GProps area;
          BRepGProp::SurfaceProperties(overlap.Shape(), area);
          if (area.Mass() < 1e-9)
            continue;
          BRepAdaptor_Surface surface(face);
          require(surface.GetType() == GeomAbs_Plane,
                  "Rib Body limit intersects an unsupported curved support");
          const auto plane = surface.Plane();
          const auto normal = gp_Vec(plane.Axis().Direction());
          const double denominator = normal.Dot(d);
          if (std::abs(denominator) < 1e-9)
            continue;
          if (std::holds_alternative<RibBodyExtent>(r.extent))
            require(std::abs(std::abs(denominator) - 1) < 1e-9,
                    "Rib Body termination requires a planar first support normal to its direction");
          const gp_Pnt origin(frame.origin.x, frame.origin.y, frame.origin.z);
          const double distance = gp_Vec(origin, plane.Location()).Dot(normal) / denominator;
          if (distance > 1e-6 && distance < nearest) {
            nearest = distance;
            selected = face;
          }
        }
        require(selected.has_value(), "Rib termination requires an actual forward support");
        const GeometryShape support(std::make_shared<GeometryShape::Impl>(*selected));
        auto clipped = ClosedProfileAdapter{}.terminate_at_planar_face(tool, support, frame.origin);
        require(bool(clipped), "The first Rib support does not cover its complete section");
        auto terminated = std::move(clipped.value());
        native = {terminated.impl_->shape, terminated.impl_->face_lineage};
        const gp_Pnt cap(frame.origin.x + d.X() * nearest, frame.origin.y + d.Y() * nearest,
                         frame.origin.z + d.Z() * nearest);
        auto tagged = ResultFaceSupportGeometry::tag_plane(
            terminated, feature.id(), spine.edges.front().second + "/cap",
            {cap.X(), cap.Y(), cap.Z()}, frame.x_axis, {d.X(), d.Y(), d.Z()});
        native.lineage = tagged.impl_->face_lineage;
      }
      require(detail::rib_volume(native.shape) > 1e-8, "Rib tool has no material");
      tool_witnesses.insert(tool_witnesses.end(), native.lineage.begin(), native.lineage.end());
      tools = tools ? detail::join_rib(*tools, native, feature.id(),
                                       r.crossings == RibCrossingPolicy::Reject)
                    : std::move(native);
    }
    require(tools.has_value(), "Rib has no material tool");
    BRepBuilderAPI_Copy copy(target.impl_->shape, true, false);
    detail::RibNative source{copy.Shape(),
                             detail::map_lineage(copy, copy.Shape(), target.impl_->face_lineage)};
    auto result = detail::join_rib(source, *tools, feature.id());
    require(solids(result.shape) == solids(target.impl_->shape),
            "Rib/Web must join every thin volume to the target Body");
    require(detail::rib_volume(result.shape) > detail::rib_volume(target.impl_->shape) + 1e-7,
            "Rib/Web does not add material to the target Body");
    auto witnesses = source.lineage;
    witnesses.insert(witnesses.end(), tool_witnesses.begin(), tool_witnesses.end());
    detail::scope_rib_fragments(result, witnesses, feature.id());
    auto backing = std::make_shared<GeometryShape::Impl>(std::move(result.shape));
    backing->face_lineage = std::move(result.lineage);
    return Result<GeometryShape>::success(GeometryShape(std::move(backing)));
  } catch (const Standard_Failure& e) {
    return Result<GeometryShape>::failure(Error::geometry(
        feature.id().value(),
        e.GetMessageString() ? e.GetMessageString() : "Native Rib/Web construction failed"));
  } catch (const std::exception& e) {
    return Result<GeometryShape>::failure(Error::geometry(feature.id().value(), e.what()));
  }
}
} // namespace blcad::geometry
