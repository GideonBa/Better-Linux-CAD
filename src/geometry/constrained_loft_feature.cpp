#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/surface_adapter.hpp"
#include "constrained_loft_network.hpp"
#include "constrained_loft_patch.hpp"
#include "geometry_shape_internal.hpp"
#include "semantic_face_resolution.hpp"
#include <BOPAlgo_ArgumentAnalyzer.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <stdexcept>

namespace blcad::geometry {
namespace {
template <class T> T checked(Result<T> result) {
  if (!result)
    throw std::runtime_error(result.error().message());
  return std::move(result.value());
}
TopoDS_Face support_face(const PartDocument& part, const ShapeCache& cache,
                         const LoftBoundaryCondition& condition, FeatureId feature,
                         StepImportTopologyContext* imports) {
  if (const auto* face = std::get_if<ResultFaceReference>(&condition.support.source())) {
    const auto shape =
        checked(ResultFaceSupportGeometry::resolve_face(part, cache, *face, imports));
    const auto* native = OcctShapeView::get(shape);
    if (!native || native->ShapeType() != TopAbs_FACE)
      throw std::runtime_error("Loft support must resolve to exactly one native face");
    return TopoDS::Face(*native);
  }
  const auto* shape = condition.imported_source_body
                          ? cache.find_body_shape(*condition.imported_source_body)
                          : cache.find_feature_shape(FeatureId(condition.support.source_node_id()));
  const auto* native = shape ? OcctShapeView::get(*shape) : nullptr;
  if (!native)
    throw std::runtime_error("Loft support source has no current native geometry");
  return checked(
      resolve_semantic_face(part, cache, *native, condition.support, feature, "Loft", imports));
}
const LoftBoundaryCondition* condition(const LoftFeature& feature, bool first, std::size_t patch) {
  const auto& intent = *feature.constraints();
  const auto& whole = first ? intent.start : intent.end;
  if (whole)
    return &*whole;
  for (const auto& item : intent.patch_boundaries)
    if (item.condition.section_identity ==
            (first ? feature.sections().front() : feature.sections().back()).identity() &&
        item.from_guide == feature.guide_curves()[patch] &&
        item.to_guide == feature.guide_curves()[(patch + 1) % feature.guide_curves().size()])
      return &item.condition;
  return nullptr;
}
std::string patch_role(const LoftFeature& feature, std::size_t patch) {
  const auto& from = feature.guide_curves()[patch].value();
  const auto& to = feature.guide_curves()[(patch + 1) % feature.guide_curves().size()].value();
  return "loft/patch/" + std::to_string(from.size()) + ":" + from + "/" +
         std::to_string(to.size()) + ":" + to;
}
void check_interference(const TopoDS_Shape& shape) {
  BOPAlgo_ArgumentAnalyzer analysis;
  analysis.SetShape1(shape);
  analysis.ArgumentTypeMode() = false;
  analysis.SmallEdgeMode() = false;
  analysis.RebuildFaceMode() = false;
  analysis.TangentMode() = false;
  analysis.MergeVertexMode() = false;
  analysis.MergeEdgeMode() = false;
  analysis.ContinuityMode() = false;
  analysis.CurveOnSurfaceMode() = false;
  analysis.SelfInterMode() = true;
  analysis.StopOnFirstFaulty() = true;
  analysis.Perform();
  if (analysis.HasErrors() || analysis.HasFaulty())
    throw std::runtime_error("Verified Loft rejects native self-interference");
}
} // namespace
Result<GeometryShape> LoftAdapter::loft_constrained(const PartDocument& part,
                                                    const LoftFeature& feature,
                                                    const ShapeCache& cache,
                                                    StepImportTopologyContext* imports) const {
  const auto fail = [&](std::string message) {
    return Result<GeometryShape>::failure(
        Error::geometry(feature.id().value(), std::move(message)));
  };
  try {
    if (!feature.constraints())
      return fail("Verified Loft requires authored constraint intent");
    const auto& intent = *feature.constraints();
    const auto* parameter = part.find_parameter(intent.linear_tolerance);
    if (!parameter || parameter->type() != ParameterType::Length)
      return fail("Verified Loft requires a Length modeling tolerance");
    const double tolerance = parameter->value().millimeters();
    for (const auto* support : intent.boundary_conditions())
      if (support->tangent_scale != 1.0)
        return fail("Verified native-rail Loft currently requires tangent scale 1; rail germs own "
                    "its derivative magnitude");
    const auto network =
        checked(detail::resolve_constrained_loft_network(part, feature, cache, imports, tolerance));
    BRepBuilderAPI_Sewing sewing(tolerance);
    std::vector<detail::FaceLineage> lineage;
    struct Witness {
      std::string role;
      detail::LoftPatchBoundary boundary;
    };
    std::vector<Witness> witnesses;
    const bool reversed = feature.sections().front().flip_normal();
    for (std::size_t p = 0; p < network.sections.front().size(); ++p) {
      const auto next = (p + 1) % network.rails.size();
      const auto left = reversed ? next : p, right = reversed ? p : next;
      std::vector<detail::LoftPatchBoundary> boundaries{
          {network.sections.front()[p], {}},
          {network.rails[right], {}},
          {TopoDS::Edge(network.sections.back()[p].Reversed()), {}},
          {TopoDS::Edge(network.rails[left].Reversed()), {}}};
      for (const bool first : {true, false})
        if (const auto* support = condition(feature, first, p)) {
          auto& boundary = boundaries[first ? 0 : 2];
          boundary.support = support_face(part, cache, *support, feature.id(), imports);
          boundary.continuity = support->continuity;
          boundary.reverse_normal = support->reverse_normal;
        }
      for (const auto& boundary : boundaries)
        if (boundary.support)
          witnesses.push_back({patch_role(feature, p), boundary});
      std::vector<TopoDS_Edge> interior;
      for (std::size_t s = 1; s + 1 < network.sections.size(); ++s)
        interior.push_back(network.sections[s][p]);
      const auto built = detail::build_constrained_loft_patch(
          feature.id(), boundaries, interior, tolerance, intent.angular_tolerance_rad,
          intent.curvature_tolerance_per_mm);
      if (!built)
        return fail(patch_role(feature, p) + ": " + built.error().message());
      const auto& patch = built.value();
      sewing.Add(patch.face);
      lineage.push_back({{feature.id(), patch_role(feature, p)}, patch.face, {}, {1, 0, 0}});
    }
    if (feature.kind() != LoftFeatureKind::LoftSurface) {
      if (!network.closed)
        return fail("Verified solid Loft requires closed profiles");
      for (const std::size_t end : {std::size_t{0}, network.contours.size() - 1}) {
        BRepBuilderAPI_MakeFace cap(network.contours[end], true);
        if (!cap.IsDone())
          return fail("Verified solid Loft requires exact planar terminal caps");
        sewing.Add(cap.Face());
        const auto& source = std::get<ProfileRegionReference>(feature.sections()[end].source());
        const auto* sketch = part.find_sketch(source.sketch());
        if (!sketch)
          return fail("Verified Loft cap source Sketch is missing");
        const auto frame = checked(WorkplaneResolver{}.resolve_for_sketch(part, *sketch, cache));
        lineage.push_back({{feature.id(), "loft/cap/" + feature.sections()[end].identity()},
                           cap.Face(),
                           frame.origin,
                           frame.x_axis});
      }
    }
    sewing.Perform();
    const auto shape = sewing.SewedShape();
    if (shape.IsNull() || sewing.NbMultipleEdges() != 0 ||
        !BRepCheck_Analyzer(shape, true).IsValid())
      return fail("Verified Loft quilt failed native sewing/manifold validation");
    std::size_t faces = 0, shells = 0;
    for (TopExp_Explorer face(shape, TopAbs_FACE); face.More(); face.Next())
      ++faces;
    for (TopExp_Explorer shell(shape, TopAbs_SHELL); shell.More(); shell.Next())
      ++shells;
    const bool single_face = feature.kind() == LoftFeatureKind::LoftSurface && faces == 1 &&
                             shape.ShapeType() == TopAbs_FACE;
    if (faces != lineage.size() || (shells != 1 && !single_face))
      return fail(
          "Verified Loft patches must form one connected quilt without lost or extra faces");
    check_interference(shape);
    auto impl = std::make_shared<GeometryShape::Impl>(shape);
    for (auto entry : lineage) {
      if (sewing.IsModifiedSubShape(entry.face)) {
        const auto modified = sewing.ModifiedSubShape(entry.face);
        if (modified.ShapeType() != TopAbs_FACE)
          return fail("Verified Loft sewing changed a semantic patch ambiguously");
        entry.face = TopoDS::Face(modified);
      }
      detail::append_lineage(impl->face_lineage, std::move(entry), shape);
    }
    if (impl->face_lineage.size() != lineage.size())
      return fail("Verified Loft could not preserve every authored patch/cap origin");
    GeometryShape result(impl);
    if (feature.kind() != LoftFeatureKind::LoftSurface) {
      result = checked(SurfaceAdapter{}.close_shell_to_solid(feature.id(), result));
      // Solid closure preserves the shell faces; carry the authored origins to their
      // actual oriented uses after closed-solid orientation.
      for (auto entry : impl->face_lineage)
        detail::append_lineage(result.impl_->face_lineage, std::move(entry), result.impl_->shape);
      check_interference(result.impl_->shape);
    }
    // Sewing and closed-solid orientation must not invalidate the verified join.
    for (const auto& witness : witnesses) {
      const auto found = std::find_if(result.impl_->face_lineage.begin(),
                                      result.impl_->face_lineage.end(), [&](const auto& entry) {
                                        return entry.origin.feature == feature.id() &&
                                               entry.origin.role == witness.role;
                                      });
      if (found == result.impl_->face_lineage.end())
        return fail("Verified Loft lost a boundary witness");
      checked(detail::verify_surface_join(
          feature.id(), witness.boundary.curve, found->face, *witness.boundary.support,
          witness.boundary.continuity, tolerance, intent.angular_tolerance_rad,
          intent.curvature_tolerance_per_mm, witness.boundary.reverse_normal));
    }
    if (imports)
      checked(imports->validate_sources());
    return Result<GeometryShape>::success(std::move(result));
  } catch (const Standard_Failure& e) {
    return fail(e.GetMessageString() ? e.GetMessageString() : "Verified Loft failed");
  } catch (const std::exception& e) {
    return fail(e.what());
  }
}
} // namespace blcad::geometry
