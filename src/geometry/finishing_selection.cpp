#include "blcad/geometry/finishing_selection.hpp"

#include "blcad/core/part_document.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "geometry_shape_internal.hpp"
#include "imported_body_placement.hpp"
#include "part_history_evaluation.hpp"
#include "semantic_edge_resolution.hpp"
#include "semantic_face_resolution.hpp"

#include <BRepBndLib.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <memory>
#include <optional>

namespace blcad::geometry {
Result<std::vector<FinishingSelectionTarget>>
FinishingSelectionBuilder::build(const PartDocument& document, const ShapeCache& cache,
                                 const BodyId& body, PartFeatureInputRole role,
                                 StepImportTopologyContext* import_context,
                                 std::optional<std::size_t> history_prefix) {
  const auto history = detail::validate_history_modeling_scope(document, cache, history_prefix);
  if (!history)
    return Result<std::vector<FinishingSelectionTarget>>::failure(history.error());
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
  if (!document.imported_body_features().empty()) {
    auto observed = observe_imported_part_sources(document, import_context);
    if (!observed)
      return Result<std::vector<FinishingSelectionTarget>>::failure(observed.error());
    for (const auto& base : document.imported_body_features()) {
      if (!detail::validate_history_source(document, base.id().value(), &cache))
        continue;
      auto owned = cache.validated_imported_body_result(document, base);
      if (!owned)
        return Result<std::vector<FinishingSelectionTarget>>::failure(owned.error());
    }
  }
  std::vector<FinishingSelectionTarget> targets;
  std::vector<TopoDS_Shape> represented_edges;
  std::vector<TopoDS_Shape> represented_faces;
  const auto result_label = [&](const ResultFaceReference& reference) {
    for (const auto& origin : reference.origins()) {
      const auto* hole = document.find_hole_feature(origin.feature);
      if (!hole)
        continue;
      for (std::size_t index = 0; index < hole->placements().size(); ++index) {
        const auto& id = hole->placements()[index].id.value();
        const auto root = "hole/" + std::to_string(id.size()) + ":" + id + "/";
        const auto name = hole->name() + " / Bohrung " + std::to_string(index + 1);
        if (origin.role == root + "seat/wall")
          return name + " / Senkung / Mantel";
        if (origin.role == root + "seat/bottom")
          return name + " / Sitzfläche";
        if (const auto* steps = std::get_if<HoleStepped>(&hole->manufacturing()))
          for (std::size_t j = 0; j < steps->steps.size(); ++j) {
            const auto& id = steps->steps[j].id;
            const auto role = root + "step/" + std::to_string(id.size()) + ":" + id;
            if (origin.role == role + "/wall")
              return name + " / Stufe " + std::to_string(j + 1) + " / Mantel";
            if (origin.role == role + "/bottom")
              return name + " / Stufe " + std::to_string(j + 1) + " / Sitzfläche";
          }
        if (std::holds_alternative<HoleSlot>(hole->manufacturing()) &&
            origin.role.starts_with(root + "slot/"))
          return name + " / Langlochseite";
        if (origin.role == root + "wall" || origin.role == root + "bottom")
          return hole->name() + " / Bohrung " + std::to_string(index + 1) +
                 (origin.role.ends_with("/bottom") ? " / Boden" : " / Wand");
      }
    }
    return "Result face / " + reference.origins().front().role;
  };
  const auto append = [&](auto reference, const TopoDS_Shape& shape, const std::string& label,
                          const std::string& identity) {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    if (edge)
      represented_edges.push_back(shape);
    else
      represented_faces.push_back(shape);
    targets.push_back({"finishing/" + identity,
                       label,
                       reference,
                       GeometryShape(std::make_shared<GeometryShape::Impl>(shape)),
                       {(xmin + xmax) / 2, (ymin + ymax) / 2, (zmin + zmax) / 2}});
  };
  for (const auto& base : document.imported_body_features()) {
    if (!detail::validate_history_source(document, base.id().value(), &cache))
      continue;
    const auto& definition = base.definition();
    const auto output = std::find_if(definition.outputs().begin(), definition.outputs().end(),
                                     [&](const auto& value) { return value.body == body; });
    if (output == definition.outputs().end())
      continue;
    const auto* catalog = document.find_imported_topology_catalog(definition.catalog());
    for (const auto& entry : catalog->entries()) {
      if (entry.parent_body != output->imported_body ||
          entry.descriptor.kind != (edge ? ImportedTopologyKind::Edge : ImportedTopologyKind::Face))
        continue;
      auto identity = ImportedTopologyReference::create(catalog->id(), entry.id,
                                                        edge ? ImportedTopologyCapability::Edge
                                                             : ImportedTopologyCapability::Face);
      if (!identity)
        continue;
      auto current = import_context->resolve(document, identity.value());
      if (!current)
        continue;
      auto descriptor = detail::imported_descriptor_on_body(
          document, cache, identity.value(), current.value(), result->source_feature_id, true);
      if (!descriptor)
        return Result<std::vector<FinishingSelectionTarget>>::failure(descriptor.error());
      auto match = StepImportTopology{}.match_subshape(descriptor.value(), target);
      if (!match || match.value().status != ImportedTopologyRecoveryStatus::Resolved)
        continue;
      const auto spelling = make_imported_topology_target_spelling(identity.value());
      if (!spelling)
        continue;
      if (edge) {
        auto reference = EdgeReference::create_imported(role, identity.value());
        if (reference)
          append(reference.value(), match.value().shape.impl_->shape,
                 base.name() + " / Imported edge", spelling.value());
      } else {
        auto reference = FaceReference::create_imported(role, identity.value());
        if (reference)
          append(reference.value(), match.value().shape.impl_->shape,
                 base.name() + " / Imported face", spelling.value());
      }
    }
  }
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
  if (!edge) {
    for (TopExp_Explorer it(target, TopAbs_FACE); it.More(); it.Next()) {
      const auto face = TopoDS::Face(it.Current());
      if (std::any_of(represented_faces.begin(), represented_faces.end(),
                      [&](const auto& old) { return old.IsSame(face); }))
        continue;
      std::vector<ResultFaceOrigin> origins;
      for (const auto& entry : result->shape.impl_->face_lineage)
        if (entry.selectable && entry.face.IsSame(face))
          origins.push_back(entry.origin);
      auto captured =
          ResultFaceReference::create(result->source_feature_id, body, std::move(origins));
      if (!captured)
        continue;
      auto reference = FaceReference::create_result(role, captured.value());
      if (!reference || !ResultFaceSupportGeometry::resolve_face(document, cache, captured.value(),
                                                                 import_context))
        continue;
      append(reference.value(), face, result_label(captured.value()), captured.value().identity());
    }
  }
  if (edge) {
    // Keep existing producer roles where available, then expose every remaining
    // finite edge on the actual current Body (including circular/spline rims
    // and edges generated by preceding finishing or Boolean features).
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(target, TopAbs_EDGE, edges);
    // Sample each edge once. Resolving each offered witness by resampling the
    // complete Body made catalog construction quadratic in kernel evaluations.
    std::vector<std::optional<ResultEdgeReference>> witnesses;
    witnesses.reserve(edges.Extent());
    for (int i = 1; i <= edges.Extent(); ++i) {
      witnesses.emplace_back();
      try {
        auto witness =
            make_result_edge_reference(result->source_feature_id, TopoDS::Edge(edges(i)));
        if (witness)
          witnesses.back() = std::move(witness.value());
      } catch (const Standard_Failure&) {
        // Degenerate/infinite edges cannot supply a persistent witness.
      }
    }
    for (int i = 1; i <= edges.Extent(); ++i) {
      const auto shape = TopoDS::Edge(edges(i));
      if (std::any_of(represented_edges.begin(), represented_edges.end(),
                      [&](const auto& old) { return old.IsSame(shape); }))
        continue;
      try {
        const auto& witness = witnesses[static_cast<std::size_t>(i - 1)];
        if (!witness)
          continue;
        auto reference = EdgeReference::create_result(role, witness.value());
        if (!reference || !validate_part_feature_input_reference(document, reference.value()))
          continue;
        const auto matches =
            std::count_if(witnesses.begin(), witnesses.end(), [&](const auto& other) {
              return other && result_edge_witness_matches(*other, *witness);
            });
        if (matches != 1)
          continue;
        append(
            reference.value(), shape,
            "Current result / " +
                std::string(witness.value().curve_type() == 0 ? "Straight edge " : "Curved edge ") +
                std::to_string(i),
            witness.value().identity());
      } catch (const Standard_Failure&) {
        // A degenerate or infinite edge cannot supply a persistent witness.
      }
    }
  }
  if (import_context) {
    auto fresh = import_context->validate_sources();
    if (!fresh)
      return Result<std::vector<FinishingSelectionTarget>>::failure(fresh.error());
  }
  return Result<std::vector<FinishingSelectionTarget>>::success(std::move(targets));
}
} // namespace blcad::geometry
