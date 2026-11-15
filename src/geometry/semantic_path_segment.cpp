#include "semantic_path_segment.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "derived_curve_internal.hpp"
#include "semantic_edge_resolution.hpp"
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax2.hxx>
#include <numbers>
namespace blcad::geometry::detail {
namespace {
constexpr double tolerance = 1e-6;
Point3 point(const gp_Pnt& p) {
  return {p.X(), p.Y(), p.Z()};
}
Result<SweepPathSegment> failed(const PathSegmentReference& ref, std::string message) {
  return Result<SweepPathSegment>::failure(
      Error::geometry(ref.source_node_id(), std::move(message)));
}
} // namespace
Result<SweepPathSegment> resolve_semantic_path_segment(const PartDocument& document,
                                                       const PathSegmentReference& ref,
                                                       const ShapeCache& cache,
                                                       StepImportTopologyContext* context) {
  try {
    auto typed =
        ref.feature_edge()
            ? Result<EdgeReference>::success(*ref.feature_edge())
            : EdgeReference::create_linear(PartFeatureInputRole::PathSegment, *ref.semantic_edge());
    if (!typed)
      return Result<SweepPathSegment>::failure(typed.error());
    const auto& reference = typed.value();
    Result<TopoDS_Edge> recovered = Result<TopoDS_Edge>::failure(
        Error::geometry(ref.source_node_id(), "Path edge has no source shape"));
    std::vector<Point3> samples;
    std::optional<ImportedTopologyDescriptor> imported_descriptor;
    if (const auto* imported = std::get_if<ImportedTopologyReference>(&reference.source())) {
      if (!context || !ref.imported_source_body())
        return failed(ref, "Imported path requires a verified source context and explicit Body");
      const ImportedBodyFeature* owner = nullptr;
      const auto* catalog = document.find_imported_topology_catalog(imported->catalog());
      const auto* entry = catalog ? catalog->find(imported->entry()) : nullptr;
      for (const auto& feature : document.imported_body_features())
        if (feature.definition().catalog() == imported->catalog())
          for (const auto& output : feature.definition().outputs())
            if (entry && entry->parent_body && output.imported_body == *entry->parent_body &&
                output.body == *ref.imported_source_body()) {
              if (owner)
                return failed(ref, "Imported path source Body is ambiguous");
              owner = &feature;
            }
      if (!owner)
        return failed(ref, "Imported path source Body does not own the selected edge");
      const auto valid = cache.validated_imported_body_result(document, *owner);
      if (!valid)
        return Result<SweepPathSegment>::failure(valid.error());
      const auto* base = cache.find_imported_body_shape(owner->id(), *ref.imported_source_body());
      const auto* target = base ? OcctShapeView::get(*base) : nullptr;
      if (!target)
        return failed(ref, "Imported path base output is unavailable");
      auto current = context->resolve(document, *imported);
      if (!current)
        return Result<SweepPathSegment>::failure(current.error());
      if (!current.value().descriptor)
        return failed(ref, "Imported path edge is not resolved");
      auto match = StepImportTopology{}.match_subshape(*current.value().descriptor, *target);
      if (!match)
        return Result<SweepPathSegment>::failure(match.error());
      if (match.value().status != ImportedTopologyRecoveryStatus::Resolved)
        return failed(ref, "Imported path edge is lost or ambiguous on its source Body");
      const auto* shape = OcctShapeView::get(match.value().shape);
      if (!shape || shape->ShapeType() != TopAbs_EDGE)
        return failed(ref, "Imported path reference did not recover an edge");
      recovered = Result<TopoDS_Edge>::success(TopoDS::Edge(*shape));
      imported_descriptor = current.value().descriptor;

    } else {
      const auto* source = cache.find_feature_shape(FeatureId(reference.source_node_id()));
      const auto* shape = source ? OcctShapeView::get(*source) : nullptr;
      if (!shape)
        return failed(ref, "Semantic path producer result is unavailable");
      recovered = resolve_semantic_edge(document, cache, *shape, reference,
                                        FeatureId(ref.source_node_id()), "path", context);
      if (const auto* witness = std::get_if<ResultEdgeReference>(&reference.source()))
        samples.assign(witness->samples().begin(), witness->samples().end());
      else if (const auto* linear = std::get_if<SemanticEdgeReference>(&reference.source())) {
        auto endpoints = SemanticReferenceEvaluator{}.resolve_edge(document, *linear, cache);
        if (!endpoints)
          return Result<SweepPathSegment>::failure(endpoints.error());
        samples = {endpoints.value().start, endpoints.value().end};
      }
    }
    if (!recovered)
      return Result<SweepPathSegment>::failure(recovered.error());
    if (BRep_Tool::Degenerated(recovered.value()))
      return failed(ref, "Degenerate path edge");
    double first = 0, last = 0;
    // This overload applies the edge's location. Trim before reversal; never alter the source.
    Handle(Geom_Curve) native = BRep_Tool::Curve(recovered.value(), first, last);
    if (native.IsNull() || !std::isfinite(first) || !std::isfinite(last) || last <= first)
      return failed(ref, "Path edge has no finite bounded native curve");
    const bool closed_edge = BRep_Tool::IsClosed(recovered.value());
    Handle(Geom_Curve) curve = new Geom_TrimmedCurve(native, first, last);
    if (imported_descriptor && !closed_edge) {
      // Catalog samples are an unordered recovery set. Open imported edges use the
      // lexicographically smaller actual endpoint as their positive direction.
      samples = {point(curve->Value(first)), point(curve->Value(last))};
      const auto less = [](Point3 a, Point3 b) {
        if (a.x != b.x)
          return a.x < b.x;
        if (a.y != b.y)
          return a.y < b.y;
        return a.z < b.z;
      };
      if (less(samples[1], samples[0]))
        std::reverse(samples.begin(), samples.end());
    }
    if (!samples.empty()) {
      if (samples.size() < 2)
        return failed(ref, "Path edge orientation evidence is incomplete");
      bool forward = true, backward = true;
      for (std::size_t i = 0; i < samples.size(); ++i) {
        const double fraction = double(i) / double(samples.size() - 1);
        forward = forward && distance(point(curve->Value(first + (last - first) * fraction)),
                                      samples[i]) <= tolerance;
        backward = backward && distance(point(curve->Value(last - (last - first) * fraction)),
                                        samples[i]) <= tolerance;
      }
      if (forward == backward)
        return failed(ref, "Path edge orientation is lost or ambiguous");
      if (backward)
        curve->Reverse();
    } else {
      // Complete analytic circles have no endpoint orientation. Freeze the phase and positive
      // direction to the source sketch frame or the normalized imported analytic frame.
      BRepAdaptor_Curve adaptor(recovered.value());
      if (!closed_edge || adaptor.GetType() != GeomAbs_Circle ||
          std::abs((last - first) - 2 * std::numbers::pi) > 1e-12)
        return failed(ref, "Path edge requires ordered samples or a complete analytic circle");
      const auto actual = adaptor.Circle();
      gp_Dir desired_axis, desired_x;
      if (imported_descriptor) {
        const auto d = imported_descriptor->direction;
        desired_axis = gp_Dir(d.x, d.y, d.z);
        desired_x = gp_Ax2(actual.Location(), desired_axis).XDirection();
      } else {
        const auto* circular = std::get_if<SemanticCircularEdgeReference>(&reference.source());
        if (!circular)
          return failed(ref, "Circular path has no authored orientation frame");
        const auto* producer = document.find_feature(circular->source_feature());
        const auto* sketch = producer ? document.find_sketch(producer->input_sketch()) : nullptr;
        if (!sketch)
          return failed(ref, "Circular path source sketch is unavailable");
        auto frame = WorkplaneResolver{}.resolve_for_sketch(document, *sketch, cache);
        if (!frame)
          return Result<SweepPathSegment>::failure(frame.error());
        const auto n = frame.value().normal, a = frame.value().x_axis;
        desired_axis = gp_Dir(n.x, n.y, n.z);
        desired_x = gp_Dir(a.x, a.y, a.z);
      }
      // The normalized descriptor/Sketch frame defines phase and sense, never a replacement
      // plane. Project its phase direction onto the actual recovered circle's native plane.
      auto axis = actual.Axis().Direction();
      if (axis.Dot(desired_axis) < 0)
        axis.Reverse();
      const gp_Vec requested(desired_x), normal(axis);
      const auto x = requested - normal * requested.Dot(normal);
      if (x.Magnitude() <= 1e-12)
        return failed(ref, "Circular path phase is singular on its native plane");
      curve = new Geom_Circle(gp_Ax2(actual.Location(), axis, gp_Dir(x)), actual.Radius());
    }
    if (closed_edge && !curve->IsClosed()) {
      BRepAdaptor_Curve adaptor(recovered.value());
      if (!closed_edge || adaptor.GetType() != GeomAbs_Circle ||
          std::abs((last - first) - 2 * std::numbers::pi) > 1e-12)
        return failed(ref, "Closed path requires a complete analytic circle with a defined phase");
      const auto circle = adaptor.Circle();
      gp_Pnt p;
      gp_Vec tangent;
      curve->D1(curve->FirstParameter(), p, tangent);
      const gp_Vec radial(circle.Location(), p);
      auto axis = circle.Axis().Direction();
      if (gp_Vec(axis).Dot(radial.Crossed(tangent)) < 0)
        axis.Reverse();
      const auto x = radial - gp_Vec(axis) * radial.Dot(gp_Vec(axis));
      curve = new Geom_Circle(gp_Ax2(circle.Location(), axis, gp_Dir(x)), circle.Radius());
    }
    const auto a = point(curve->Value(curve->FirstParameter()));
    const auto b = point(curve->Value(curve->LastParameter()));
    const bool closed = curve->IsClosed();
    auto derived = std::make_shared<DerivedCurve>(curve, closed);
    auto extent = derived_curve_length(derived, ref.source_node_id());
    if (!extent || extent.value() <= tolerance)
      return failed(ref, "Path edge has no positive length");
    return Result<SweepPathSegment>::success(
        {SweepPathSegmentKind::DerivedNative, a, {}, b, derived});
  } catch (const Standard_Failure& error) {
    return failed(ref,
                  std::string("Native path edge recovery failed: ") + error.GetMessageString());
  }
}
} // namespace blcad::geometry::detail
