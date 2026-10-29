#include "blcad/geometry/reference_part_geometry.hpp"

#include "blcad/geometry/step_import_topology_context.hpp"
#include "geometry_shape_internal.hpp"
#include "occt_failure_message.hpp"

#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <exception>
#include <set>
#include <utility>

namespace blcad::geometry {
namespace {
Error invalid(const PartDocument& document, std::string message) {
  return Error::geometry(document.id().value(), std::move(message));
}
} // namespace

Result<CachedReferencePartResult> ReferencePartGeometry::resolve_definition(
    const PartDocument& document, const ImportedPartDefinition& definition,
    StepImportTopologyContext& context, std::optional<FeatureId> producer) const {
  const auto* source = document.find_step_import_source(definition.source());
  const auto* catalog = document.find_imported_topology_catalog(definition.catalog());
  if (source == nullptr || catalog == nullptr || catalog->source() != source->id() ||
      definition.outputs().empty())
    return Result<CachedReferencePartResult>::failure(invalid(
        document, "imported Part source/catalog/output intent must belong to the document"));

  auto recovery = context.recover(document, definition.catalog());
  if (!recovery)
    return Result<CachedReferencePartResult>::failure(recovery.error());

  try {
    CachedReferencePartResult result{document.id(), *source, *catalog, definition, {}, {}};
    result.bodies.reserve(definition.outputs().size());
    std::set<std::string> bodies;
    std::set<std::string> entries;
    for (const auto& output : definition.outputs()) {
      const auto* declared = document.find_body(output.body);
      if (declared == nullptr || declared->kind() != output.kind ||
          !bodies.insert(output.body.value()).second ||
          !entries.insert(output.imported_body.value()).second)
        return Result<CachedReferencePartResult>::failure(invalid(
            document, "imported Part outputs must map unique declared bodies with matching kinds"));
      auto reference = ImportedTopologyReference::create(definition.catalog(), output.imported_body,
                                                         ImportedTopologyCapability::Body);
      if (!reference)
        return Result<CachedReferencePartResult>::failure(reference.error());
      auto resolved = StepImportTopology{}.resolve(*recovery.value(), reference.value());
      if (!resolved)
        return Result<CachedReferencePartResult>::failure(resolved.error());
      if (!resolved.value().descriptor ||
          resolved.value().descriptor->kind != ImportedTopologyKind::Body ||
          resolved.value().descriptor->body_kind != output.kind)
        return Result<CachedReferencePartResult>::failure(invalid(
            document, "recovered imported body kind differs from persistent output intent"));
      GeometryShape body_shape = resolved.value().shape;
      if (producer) {
        auto backing = std::make_shared<GeometryShape::Impl>(*body_shape.impl_);
        for (const auto& entry : catalog->entries()) {
          if (entry.parent_body != output.imported_body ||
              entry.descriptor.kind != ImportedTopologyKind::Face)
            continue;
          const auto* current = recovery.value()->find(entry.id);
          if (!current || current->status != ImportedTopologyRecoveryStatus::Resolved ||
              !current->descriptor || current->descriptor->geometry != ImportedGeometryClass::Plane)
            continue;
          auto analytic = imported_topology_analytic_descriptor(*current->descriptor);
          if (!analytic)
            return Result<CachedReferencePartResult>::failure(analytic.error());
          const auto& plane = std::get<AssemblyPlanarTargetDescriptor>(analytic.value());
          auto reference = ImportedTopologyReference::create(catalog->id(), entry.id,
                                                             ImportedTopologyCapability::Face);
          if (!reference)
            return Result<CachedReferencePartResult>::failure(reference.error());
          auto role = make_imported_face_origin_role(reference.value());
          if (!role)
            return Result<CachedReferencePartResult>::failure(role.error());
          detail::append_lineage(backing->face_lineage,
                                 {{*producer, role.value()},
                                  TopoDS::Face(current->shape.impl_->shape),
                                  plane.origin,
                                  plane.x_axis},
                                 backing->shape);
        }
        body_shape = GeometryShape(std::move(backing));
      }
      result.bodies.push_back({output, std::move(body_shape)});
    }
    if (result.bodies.size() == 1) {
      result.shape = result.bodies.front().shape;
    } else {
      BRep_Builder builder;
      TopoDS_Compound compound;
      builder.MakeCompound(compound);
      for (const auto& body : result.bodies)
        builder.Add(compound, body.shape.impl_->shape);
      auto backing = std::make_shared<GeometryShape::Impl>(compound);
      for (const auto& body : result.bodies)
        for (const auto& entry : body.shape.impl_->face_lineage)
          detail::append_lineage(backing->face_lineage, entry, backing->shape);
      result.shape = GeometryShape(std::move(backing));
    }
    auto fresh = context.validate_sources();
    if (!fresh)
      return Result<CachedReferencePartResult>::failure(fresh.error());
    return Result<CachedReferencePartResult>::success(std::move(result));
  } catch (const Standard_Failure& failure) {
    return Result<CachedReferencePartResult>::failure(
        invalid(document, standard_failure_message(failure)));
  } catch (const std::exception& failure) {
    return Result<CachedReferencePartResult>::failure(invalid(document, failure.what()));
  }
}

Result<CachedReferencePartResult>
ReferencePartGeometry::resolve(const PartDocument& document,
                               StepImportTopologyContext& context) const {
  const auto* definition = document.reference_part_definition();
  if (!document.is_reference_part() || definition == nullptr)
    return Result<CachedReferencePartResult>::failure(
        invalid(document, "Reference geometry requires a Reference Part definition"));
  const auto* source = document.find_step_import_source(definition->source());
  if (source == nullptr || source->mode() != StepImportMode::Reference)
    return Result<CachedReferencePartResult>::failure(
        invalid(document, "Reference Part source must retain explicit Reference mode"));
  return resolve_definition(document, *definition, context);
}

Result<std::size_t> ReferencePartGeometry::seed_reference(const PartDocument& document,
                                                          StepImportTopologyContext& context,
                                                          ShapeCache& cache) const {
  auto resolved = resolve(document, context);
  if (!resolved)
    return Result<std::size_t>::failure(resolved.error());
  ShapeCache candidate = cache;
  auto stored = candidate.store_reference_result(std::move(resolved.value()));
  if (!stored)
    return stored;
  auto owned = candidate.validated_reference_result(document);
  if (!owned)
    return Result<std::size_t>::failure(owned.error());
  auto fresh = context.validate_sources();
  if (!fresh)
    return Result<std::size_t>::failure(fresh.error());
  cache = std::move(candidate);
  return stored;
}

Result<CachedReferencePartResult>
ReferencePartGeometry::resolve_imported_body_feature(const PartDocument& document,
                                                     const ImportedBodyFeature& feature,
                                                     StepImportTopologyContext& context) const {
  const auto& features = document.imported_body_features();
  const auto owned = std::find_if(features.begin(), features.end(),
                                  [&](const auto& value) { return value.id() == feature.id(); });
  if (document.is_reference_part() || owned == features.end() || *owned != feature)
    return Result<CachedReferencePartResult>::failure(
        invalid(document, "imported body feature must belong to this editable Part"));
  const auto* source = document.find_step_import_source(feature.definition().source());
  if (source == nullptr || source->mode() != StepImportMode::EditableBody)
    return Result<CachedReferencePartResult>::failure(
        invalid(document, "imported body feature source must retain explicit EditableBody mode"));
  return resolve_definition(document, feature.definition(), context, feature.id());
}

Result<std::size_t> ReferencePartGeometry::seed_imported_body_feature(
    const PartDocument& document, const ImportedBodyFeature& feature,
    StepImportTopologyContext& context, ShapeCache& cache) const {
  auto resolved = resolve_imported_body_feature(document, feature, context);
  if (!resolved)
    return Result<std::size_t>::failure(resolved.error());
  ShapeCache candidate = cache;
  if (candidate.reference_result() != nullptr)
    candidate.clear();
  auto stored = candidate.store_feature_shape(feature.id(), resolved.value().shape);
  if (!stored)
    return stored;
  auto base = candidate.store_imported_body_result(feature.id(), resolved.value());
  if (!base)
    return base;
  for (const auto& body : resolved.value().bodies) {
    auto result = candidate.store_body_shape(body.output.body, feature.id(), body.shape);
    if (!result)
      return result;
  }
  if (document.bodies().size() == 1 && resolved.value().bodies.size() == 1 &&
      resolved.value().bodies.front().output.kind == BodyKind::Solid) {
    auto final = candidate.set_final_shape(feature.id(), resolved.value().shape);
    if (!final)
      return final;
  } else {
    candidate.clear_final_shape();
  }
  auto fresh = context.validate_sources();
  if (!fresh)
    return Result<std::size_t>::failure(fresh.error());
  cache = std::move(candidate);
  return Result<std::size_t>::success(resolved.value().bodies.size());
}

} // namespace blcad::geometry
