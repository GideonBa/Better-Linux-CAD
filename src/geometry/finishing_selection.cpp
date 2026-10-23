#include "blcad/geometry/finishing_selection.hpp"

#include "blcad/core/part_document.hpp"
#include "geometry_shape_internal.hpp"
#include "semantic_edge_resolution.hpp"
#include "semantic_face_resolution.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <memory>

namespace blcad::geometry {
Result<std::vector<FinishingSelectionTarget>>
FinishingSelectionBuilder::build(const PartDocument& document, const ShapeCache& cache,
                                 const BodyId& body, PartFeatureInputRole role) {
  const bool edge =
      role == PartFeatureInputRole::FilletEdge || role == PartFeatureInputRole::ChamferEdge;
  if (!edge && role != PartFeatureInputRole::ShellRemovalFace &&
      role != PartFeatureInputRole::DraftFace)
    return Result<std::vector<FinishingSelectionTarget>>::failure(
        Error::validation(body.value(), "unsupported finishing selection role"));
  const auto* result = cache.find_body_result(body);
  const auto* target_body = document.find_body(body);
  if (!result || !target_body || target_body->kind() != BodyKind::Solid ||
      target_body->visibility() != BodyVisibility::Visible || result->shape.empty())
    return Result<std::vector<FinishingSelectionTarget>>::failure(
        Error::validation(body.value(), "finishing requires a visible, recomputed solid Body"));
  const auto& target = result->shape.impl_->shape;
  std::vector<FinishingSelectionTarget> targets;
  const auto append = [&](auto reference, const TopoDS_Shape& shape, const std::string& label,
                          const std::string& identity) {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    targets.push_back({"finishing/" + identity,
                       label,
                       reference,
                       GeometryShape(std::make_shared<GeometryShape::Impl>(shape)),
                       {(xmin + xmax) / 2, (ymin + ymax) / 2, (zmin + zmax) / 2}});
  };
  for (const auto& source : document.features()) {
    if (!source.body_result_context() ||
        source.body_result_context()->effective_produced_body() != body)
      continue;
    if (edge) {
      for (int i = 0; i < 12; ++i) {
        auto semantic = SemanticEdgeReference::create(source.id(), static_cast<SemanticEdge>(i));
        if (!semantic)
          continue;
        auto reference = EdgeReference::create_linear(role, semantic.value());
        if (!reference || !validate_part_feature_input_reference(document, reference.value()))
          continue;
        auto shape = resolve_semantic_edge(document, cache, target, reference.value(), source.id(),
                                           "selection");
        if (shape)
          append(reference.value(), shape.value(),
                 source.name() + " / " + std::string(to_string(semantic.value().edge())),
                 make_generated_topology_target_spelling(semantic.value()).value());
      }
    } else {
      for (int i = 0; i < 6; ++i) {
        auto semantic = SemanticFaceReference::create(source.id(), static_cast<SemanticFace>(i));
        if (!semantic)
          continue;
        auto reference = FaceReference::create_planar(role, semantic.value());
        if (!reference)
          continue;
        auto shape = resolve_semantic_face(document, cache, target, reference.value(), source.id(),
                                           "selection");
        if (shape)
          append(reference.value(), shape.value(),
                 source.name() + " / " + std::string(to_string(semantic.value().face())),
                 "face/" + std::to_string(source.id().value().size()) + ":" + source.id().value() +
                     "/" + std::string(to_string(semantic.value().face())));
      }
    }
  }
  return Result<std::vector<FinishingSelectionTarget>>::success(std::move(targets));
}
} // namespace blcad::geometry
