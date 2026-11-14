#include "blcad/geometry/spatial_constraint_geometry.hpp"
#include "blcad/geometry/construction_line_resolver.hpp"
#include "blcad/geometry/construction_point_resolver.hpp"
#include "blcad/geometry/equation_curve_geometry.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "derived_curve_internal.hpp"
#include "part_history_evaluation.hpp"
#include "semantic_face_resolution.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepExtrema_SupportType.hxx>
#include <BRepGProp.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
namespace blcad::geometry {
namespace {
Error error(std::string message) {
  return Error::geometry("sketch3d.solver.geometry", std::move(message));
}
gp_Pnt native(Point3 p) {
  return {p.x, p.y, p.z};
}
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
Vector3 vector(const gp_Vec& v, double factor = 1.) {
  return {v.X() * factor, v.Y() * factor, v.Z() * factor};
}
template <class F> auto checked_native(F&& f) -> decltype(f()) {
  try {
    return f();
  } catch (const Standard_Failure& failure) {
    return decltype(f())::failure(error(failure.GetMessageString()
                                            ? failure.GetMessageString()
                                            : "OCCT curve/projection query failed"));
  } catch (const std::exception& failure) {
    return decltype(f())::failure(error(failure.what()));
  }
}
Result<TopoDS_Shape> line(Point3 a, Point3 b) {
  BRepBuilderAPI_MakeEdge edge(native(a), native(b));
  if (!edge.IsDone())
    return Result<TopoDS_Shape>::failure(error("Straight curve is singular"));
  return Result<TopoDS_Shape>::success(edge.Edge());
}
struct CurveShape {
  TopoDS_Shape shape;
  bool reversed{false};
};
struct State {
  const PartDocument& part;
  const ShapeCache& cache;
  StepImportTopologyContext* imports;
  SpatialGeometryScope scope;
  SpatialExternalSketchResolver resolve_external;
  std::optional<Sketch3DGeometryResult> candidate{};
  std::map<std::string, Sketch3DGeometryResult> external{};
  std::map<std::string, Sketch3D> external_owners{};
  Result<Sketch3D> external_owner(const Sketch3D& source) {
    if (const auto found = external_owners.find(source.id().value());
        found != external_owners.end())
      return Result<Sketch3D>::success(found->second);
    auto resolved =
        resolve_external ? resolve_external(source.id()) : Result<Sketch3D>::success(source);
    if (resolved)
      external_owners.emplace(source.id().value(), resolved.value());
    return resolved;
  }
  Result<std::size_t> fresh() const {
    auto valid = std::visit(
        [&](const auto& selected) {
          using T = std::decay_t<decltype(selected)>;
          if constexpr (std::is_same_v<T, SpatialHistoryPrefixScope>)
            return detail::validate_history_modeling_scope(part, cache, selected.count);
          else if constexpr (std::is_same_v<T, SpatialFeatureInputScope>) {
            if (!cache.has_history_scope())
              return Result<std::size_t>::failure(
                  error("Spatial feature input requires a matched history cache"));
            return cache.validate_history_input(part);
          } else
            return cache.validate_complete_geometry_result(part);
        },
        scope);
    if (!valid)
      return valid;
    auto observed = observe_imported_part_sources(part, imports, cache.history_plan());
    if (!observed)
      return observed;
    return imports ? imports->validate_sources() : Result<std::size_t>::success(0);
  }
  Result<TopoDS_Shape> spatial(const Sketch3D& s, SketchEntityId id, bool local) {
    if (local && !candidate) {
      auto converted = Sketch3DGeometryAdapter{}.convert(part, s);
      if (!converted)
        return Result<TopoDS_Shape>::failure(converted.error());
      candidate = std::move(converted.value());
    }
    if (!local && !external.contains(s.id().value())) {
      auto owner = external_owner(s);
      if (!owner)
        return Result<TopoDS_Shape>::failure(owner.error());
      auto converted = Sketch3DGeometryAdapter{}.convert(part, owner.value());
      if (!converted)
        return Result<TopoDS_Shape>::failure(converted.error());
      external.emplace(s.id().value(), std::move(converted.value()));
    }
    const auto& converted = local ? *candidate : external.at(s.id().value());
    const auto* product = converted.find_product(id);
    if (!product)
      return Result<TopoDS_Shape>::failure(error("Spatial curve source is missing"));
    const auto* shape = OcctShapeView::get(product->shape);
    if (!shape)
      return Result<TopoDS_Shape>::failure(error("Spatial curve has no native product"));
    return Result<TopoDS_Shape>::success(*shape);
  }
  Result<TopoDS_Shape> planar(const PathSegmentReference& ref) const {
    const auto* s = part.find_sketch(*ref.planar_sketch());
    if (!s)
      return Result<TopoDS_Shape>::failure(error("Planar curve source is missing"));
    WorkplaneResolver resolver;
    auto frame = resolver.resolve_for_sketch(part, *s, cache);
    if (!frame)
      return Result<TopoDS_Shape>::failure(frame.error());
    const auto map = [&](Point2 p) { return resolver.evaluate_point(frame.value(), p); };
    const auto id = *ref.entity();
    switch (*ref.planar_curve_kind()) {
    case PlanarPathCurveKind::Line: {
      const auto* v = s->find_line_segment(id);
      if (!v)
        break;
      return line(map(v->start()), map(v->end()));
    }
    case PlanarPathCurveKind::Arc: {
      const auto* v = s->find_arc_segment(id);
      if (!v)
        break;
      GC_MakeArcOfCircle arc(native(map(v->start())), native(map(v->mid())), native(map(v->end())));
      if (!arc.IsDone())
        return Result<TopoDS_Shape>::failure(error("Planar arc is singular"));
      BRepBuilderAPI_MakeEdge edge(arc.Value());
      if (!edge.IsDone())
        return Result<TopoDS_Shape>::failure(error("Planar arc construction failed"));
      return Result<TopoDS_Shape>::success(edge.Edge());
    }
    case PlanarPathCurveKind::Spline: {
      const auto* v = s->find_spline_segment(id);
      if (!v)
        break;
      TColgp_Array1OfPnt poles(1, 4);
      TColStd_Array1OfReal weights(1, 4);
      const std::array<Point2, 4> p{v->start(), v->control1(), v->control2(), v->end()};
      for (int i = 0; i < 4; ++i) {
        poles.SetValue(i + 1, native(map(p[static_cast<std::size_t>(i)])));
        weights.SetValue(i + 1, v->weights()[static_cast<std::size_t>(i)]);
      }
      Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles, weights);
      BRepBuilderAPI_MakeEdge edge(curve);
      if (!edge.IsDone())
        return Result<TopoDS_Shape>::failure(error("Planar rational spline construction failed"));
      return Result<TopoDS_Shape>::success(edge.Edge());
    }
    case PlanarPathCurveKind::EquationCurve: {
      const auto* v = s->find_equation_curve(id);
      if (!v)
        break;
      auto curve = EquationCurveGeometryAdapter{}.convert(part, *v);
      if (!curve)
        return Result<TopoDS_Shape>::failure(curve.error());
      const auto& f = frame.value();
      auto placed =
          transformed_derived_curve(curve.value().curve, f.origin, f.x_axis, f.y_axis, f.normal);
      auto edge = derived_curve_edge(placed, id.value());
      return edge ? Result<TopoDS_Shape>::success(edge.value())
                  : Result<TopoDS_Shape>::failure(edge.error());
    }
    case PlanarPathCurveKind::ProjectedLine:
    case PlanarPathCurveKind::ReferenceGeneratedLine:
      return Result<TopoDS_Shape>::failure(
          error("Derived planar line requires an explicit supported finite "
                "curve source"));
    }
    return Result<TopoDS_Shape>::failure(error("Planar curve source is unavailable"));
  }
  Result<CurveShape> curve(const Sketch3D& s, const SpatialConstraintTarget& target) {
    if (const auto* local = std::get_if<SpatialLocalCurve>(&target.source())) {
      auto shape = spatial(s, local->entity, true);
      return shape ? Result<CurveShape>::success({shape.value(), false})
                   : Result<CurveShape>::failure(shape.error());
    }
    const auto* external_ref = std::get_if<SpatialExternalCurve>(&target.source());
    if (!external_ref)
      return Result<CurveShape>::failure(error("Target is not a native curve"));
    const auto& ref = external_ref->reference;
    auto shape = Result<TopoDS_Shape>::failure(error("Unsupported external native curve source"));
    switch (ref.source_kind()) {
    case PathSegmentSourceKind::Sketch3DCurve: {
      const auto* owner = part.find_sketch_3d(*ref.sketch_3d());
      if (owner)
        shape = spatial(*owner, *ref.entity(), false);
      break;
    }
    case PathSegmentSourceKind::PlanarSketchCurve:
      shape = planar(ref);
      break;
    case PathSegmentSourceKind::ConstructionLine: {
      auto resolved = ConstructionLineResolver{}.resolve(part, *ref.construction_line());
      if (!resolved)
        return Result<CurveShape>::failure(resolved.error());
      const auto& v = resolved.value();
      shape = line(v.point, {v.point.x + v.direction.x, v.point.y + v.direction.y,
                             v.point.z + v.direction.z});
      break;
    }
    case PathSegmentSourceKind::SemanticGeneratedEdge: {
      auto resolved = SemanticReferenceEvaluator{}.resolve_edge(part, *ref.semantic_edge(), cache);
      if (!resolved)
        return Result<CurveShape>::failure(resolved.error());
      shape = line(resolved.value().start, resolved.value().end);
      break;
    }
    }
    return shape ? Result<CurveShape>::success({shape.value(), ref.reversed()})
                 : Result<CurveShape>::failure(shape.error());
  }
  Result<SpatialCurveJet> jet(const Sketch3D& s, const SpatialConstraintTarget& target, double u) {
    auto shape = curve(s, target);
    if (!shape)
      return Result<SpatialCurveJet>::failure(shape.error());
    std::vector<TopoDS_Edge> edges;
    if (shape.value().shape.ShapeType() == TopAbs_WIRE) {
      for (BRepTools_WireExplorer it(TopoDS::Wire(shape.value().shape)); it.More(); it.Next())
        edges.push_back(it.Current());
    } else {
      for (TopExp_Explorer it(shape.value().shape, TopAbs_EDGE); it.More(); it.Next())
        edges.push_back(TopoDS::Edge(it.Current()));
    }
    if (edges.empty())
      return Result<SpatialCurveJet>::failure(error("Curve contains no native edges"));
    if (shape.value().reversed)
      u = 1 - u;
    const double station = u * static_cast<double>(edges.size());
    if (edges.size() > 1 && u > 0 && u < 1 && std::abs(station - std::round(station)) < 1e-10)
      return Result<SpatialCurveJet>::failure(
          error("Polyline joint has no unique differential; choose a regular span"));
    const auto index = std::min(static_cast<std::size_t>(std::floor(station)), edges.size() - 1);
    double local = station - static_cast<double>(index);
    BRepAdaptor_Curve curve(edges[index]);
    const bool reverse = edges[index].Orientation() == TopAbs_REVERSED;
    if (reverse)
      local = 1 - local;
    const double first = curve.FirstParameter(), last = curve.LastParameter();
    if (!std::isfinite(first) || !std::isfinite(last) || last <= first)
      return Result<SpatialCurveJet>::failure(error("Curve domain is not finite and regular"));
    gp_Pnt p;
    gp_Vec d1, d2;
    curve.D2(first + local * (last - first), p, d1, d2);
    const double factor = (last - first) * static_cast<double>(edges.size()) * (reverse ? -1 : 1) *
                          (shape.value().reversed ? -1 : 1);
    return Result<SpatialCurveJet>::success(
        {point(p), vector(d1, factor), vector(d2, factor * factor)});
  }
  Result<double> length(const Sketch3D& s, const SpatialConstraintTarget& t) {
    auto native_curve = curve(s, t);
    if (!native_curve)
      return Result<double>::failure(native_curve.error());
    GProp_GProps props;
    BRepGProp::LinearProperties(native_curve.value().shape, props);
    return Result<double>::success(props.Mass());
  }
  Result<double> radius(const Sketch3D& s, const SpatialConstraintTarget& t) {
    if (const auto* local = std::get_if<SpatialLocalCurve>(&t.source()))
      if (const auto* helix = s.find_helix(local->entity)) {
        auto q = helix->radius_binding().resolve(part);
        return q ? Result<double>::success(q.value().millimeters())
                 : Result<double>::failure(q.error());
      }
    if (const auto* external_ref = std::get_if<SpatialExternalCurve>(&t.source());
        external_ref && external_ref->reference.sketch_3d()) {
      const auto* owner = part.find_sketch_3d(*external_ref->reference.sketch_3d());
      if (owner) {
        const auto resolved = external_owner(*owner);
        if (!resolved)
          return Result<double>::failure(resolved.error());
        if (const auto* helix = resolved.value().find_helix(*external_ref->reference.entity())) {
          auto q = helix->radius_binding().resolve(part);
          return q ? Result<double>::success(q.value().millimeters())
                   : Result<double>::failure(q.error());
        }
      }
    }
    auto native_curve = curve(s, t);
    if (!native_curve)
      return Result<double>::failure(native_curve.error());
    TopExp_Explorer edge(native_curve.value().shape, TopAbs_EDGE);
    if (!edge.More())
      return Result<double>::failure(error("Radius requires a native circular arc"));
    BRepAdaptor_Curve c(TopoDS::Edge(edge.Current()));
    if (c.GetType() != GeomAbs_Circle)
      return Result<double>::failure(error("Radius requires a circular arc or analytic helix"));
    return Result<double>::success(c.Circle().Radius());
  }
  Result<Point3> project(const FaceReference& reference, Point3 p) const {
    auto face = Result<TopoDS_Face>::failure(error("Face projection has no current native target"));
    if (const auto* captured = std::get_if<ResultFaceReference>(&reference.source())) {
      auto resolved = ResultFaceSupportGeometry::resolve_face(part, cache, *captured, imports);
      if (!resolved)
        return Result<Point3>::failure(resolved.error());
      const auto* native_face = OcctShapeView::get(resolved.value());
      if (!native_face)
        return Result<Point3>::failure(error("Resolved face has no native product"));
      face = Result<TopoDS_Face>::success(TopoDS::Face(*native_face));
    } else {
      std::vector<const GeometryShape*> targets;
      const auto* producer = part.find_feature(FeatureId(reference.source_node_id()));
      if (producer && producer->body_result_context()) {
        const auto* body =
            cache.find_body_shape(producer->body_result_context()->effective_produced_body());
        if (body)
          targets.push_back(body);
      } else {
        for (const auto& body : cache.body_shapes())
          targets.push_back(&body.shape);
        if (targets.empty() && cache.final_shape())
          targets.push_back(cache.final_shape());
      }
      std::vector<TopoDS_Face> matches;
      for (const auto* target : targets) {
        const auto* native_target = OcctShapeView::get(*target);
        if (!native_target)
          return Result<Point3>::failure(error("Body target has no native product"));
        auto resolved = resolve_semantic_face(part, cache, *native_target, reference, FeatureId{},
                                              "spatial constraint", imports);
        if (resolved && std::none_of(matches.begin(), matches.end(),
                                     [&](const auto& old) { return old.IsSame(resolved.value()); }))
          matches.push_back(resolved.value());
      }
      if (matches.size() != 1)
        return Result<Point3>::failure(
            error("Face source is unavailable or ambiguous on current Bodies"));
      face = Result<TopoDS_Face>::success(matches.front());
    }
    if (!face)
      return Result<Point3>::failure(face.error());
    const auto vertex = BRepBuilderAPI_MakeVertex(native(p)).Vertex();
    BRepExtrema_DistShapeShape distance(vertex, face.value());
    distance.Perform();
    if (!distance.IsDone() || distance.NbSolution() != 1)
      return Result<Point3>::failure(error("Trimmed-face projection is unavailable or ambiguous"));
    if (distance.SupportTypeShape2(1) != BRepExtrema_IsInFace)
      return Result<Point3>::failure(
          error("Trimmed-face boundary contact is not a regular interior "
                "projection"));
    return Result<Point3>::success(point(distance.PointOnShape2(1)));
  }
};
} // namespace
SpatialSolverGeometry
SpatialConstraintGeometry::queries(const PartDocument& part, const ShapeCache& cache,
                                   StepImportTopologyContext* imports, SpatialGeometryScope scope,
                                   SpatialExternalSketchResolver external) const {
  auto state =
      std::make_shared<State>(State{part, cache, imports, std::move(scope), std::move(external)});
  SpatialSolverGeometry queries;
  queries.validate_sources = [state] { return state->fresh(); };
  queries.prepare_candidate = [state](const Sketch3D&) {
    state->candidate.reset();
    return Result<std::size_t>::success(0);
  };
  queries.validate_candidate = [state](const Sketch3D& candidate) {
    return checked_native([&]() -> Result<std::size_t> {
      const auto converted = Sketch3DGeometryAdapter{}.convert(state->part, candidate);
      return converted ? Result<std::size_t>::success(converted.value().products().size())
                       : Result<std::size_t>::failure(converted.error());
    });
  };
  queries.external_point = [state](const SketchPointReference3D& ref) {
    return checked_native(
        [&] { return Sketch3DGeometryAdapter{}.resolve_point(state->part, ref); });
  };
  queries.curve = [state](const Sketch3D& s, const SpatialConstraintTarget& t, double u) {
    return checked_native([&] { return state->jet(s, t, u); });
  };
  queries.radius = [state](const Sketch3D& s, const SpatialConstraintTarget& t) {
    return checked_native([&] { return state->radius(s, t); });
  };
  queries.curve_length = [state](const Sketch3D& s, const SpatialConstraintTarget& t) {
    return checked_native([&] { return state->length(s, t); });
  };
  queries.axis = [state](const AxisReference& ref) {
    return checked_native([&]() -> Result<SpatialAxisFrame> {
      auto v = GeometryRecomputeExecutor(state->imports)
                   .resolve_feature_axis(state->part, ref, state->cache);
      return v ? Result<SpatialAxisFrame>::success({v.value().first, v.value().second})
               : Result<SpatialAxisFrame>::failure(v.error());
    });
  };
  queries.plane = [state](const PlaneReference& ref) {
    return checked_native([&]() -> Result<SpatialPlaneFrame> {
      auto v = GeometryRecomputeExecutor(state->imports)
                   .resolve_feature_plane(state->part, ref, state->cache);
      return v ? Result<SpatialPlaneFrame>::success({v.value().first, v.value().second})
               : Result<SpatialPlaneFrame>::failure(v.error());
    });
  };
  queries.project_face = [state](const FaceReference& ref, Point3 p) {
    return checked_native([&] { return state->project(ref, p); });
  };
  return queries;
}
} // namespace blcad::geometry
