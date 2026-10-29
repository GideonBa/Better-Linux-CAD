#include "blcad/core/imported_part.hpp"
#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace blcad {
namespace {
Error invalid(std::string message) {
  return Error::validation("imported_part", std::move(message));
}
std::string body_node(const BodyId& id) {
  return "body:" + id.value();
}
} // namespace

std::string_view to_string(PartDocumentMode mode) noexcept {
  switch (mode) {
  case PartDocumentMode::Editable:
    return "editable";
  case PartDocumentMode::Reference:
    return "reference";
  }
  return "unknown";
}

ImportedPartDefinition::ImportedPartDefinition(StepImportSourceId source,
                                               ImportedTopologyCatalogId catalog,
                                               std::vector<ImportedBodyOutput> outputs)
    : source_(std::move(source)), catalog_(std::move(catalog)), outputs_(std::move(outputs)) {}

Result<ImportedPartDefinition>
ImportedPartDefinition::create(StepImportSourceId source, ImportedTopologyCatalogId catalog,
                               std::vector<ImportedBodyOutput> outputs) {
  if (source.empty() || catalog.empty() || outputs.empty())
    return Result<ImportedPartDefinition>::failure(
        invalid("imported Part requires an owned source, catalog and nonempty output mapping"));
  std::set<std::string> imported, bodies;
  for (const auto& output : outputs)
    if (output.imported_body.empty() || output.body.empty() ||
        (output.kind != BodyKind::Solid && output.kind != BodyKind::Surface) ||
        !imported.insert(output.imported_body.value()).second ||
        !bodies.insert(output.body.value()).second)
      return Result<ImportedPartDefinition>::failure(
          invalid("imported output identities must be unique and have a supported Body kind"));
  return Result<ImportedPartDefinition>::success(
      ImportedPartDefinition(std::move(source), std::move(catalog), std::move(outputs)));
}

ImportedBodyFeature::ImportedBodyFeature(FeatureId id, std::string name,
                                         ImportedPartDefinition definition)
    : id_(std::move(id)), name_(std::move(name)), definition_(std::move(definition)) {}

Result<ImportedBodyFeature> ImportedBodyFeature::create(FeatureId id, std::string name,
                                                        ImportedPartDefinition definition) {
  if (id.empty() || name.empty())
    return Result<ImportedBodyFeature>::failure(
        invalid("imported base feature requires an id and name"));
  return Result<ImportedBodyFeature>::success(
      ImportedBodyFeature(std::move(id), std::move(name), std::move(definition)));
}

Error PartDocument::reference_part_edit_error() const {
  return Error::validation(
      id_.value(), "Reference Part is read-only; convert to a new EditableBody Part to edit");
}

Result<std::size_t>
PartDocument::validate_imported_part_definition(const ImportedPartDefinition& definition) const {
  const auto* source = find_step_import_source(definition.source());
  const auto* catalog = find_imported_topology_catalog(definition.catalog());
  if (!source || !catalog || catalog->source() != source->id())
    return Result<std::size_t>::failure(
        invalid("imported Part source and catalog must belong to the same Part owner"));
  for (const auto& output : definition.outputs()) {
    const auto* entry = catalog->find(output.imported_body);
    const auto* body = find_body(output.body);
    if (!entry || entry->descriptor.kind != ImportedTopologyKind::Body ||
        entry->descriptor.body_kind != output.kind || !body || body->kind() != output.kind)
      return Result<std::size_t>::failure(
          invalid("imported output must map a catalog Body to an existing Body of the same kind"));
  }
  return Result<std::size_t>::success(definition.outputs().size());
}

Result<std::size_t> PartDocument::seal_reference_part(ImportedPartDefinition definition) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  auto valid = validate_imported_part_definition(definition);
  if (!valid)
    return valid;
  if (find_step_import_source(definition.source())->mode() != StepImportMode::Reference ||
      step_import_source_count() != 1 || imported_topology_catalogs_.size() != 1 ||
      bodies_.size() != definition.outputs().size() || !imported_body_features_.empty() ||
      !parameters_.empty() || datum_planes_.size() != 3 || !datum_axes_.empty() ||
      !construction_points_.empty() || !construction_lines_.empty() ||
      !construction_planes_.empty() || !derived_workplanes_.empty() ||
      !result_face_workplanes_.empty() || !sketches_.empty() || !sketches_3d_.empty() ||
      !path_curves_.empty() || !features_.empty() || !revolve_features_.empty() ||
      !sweep_features_.empty() || !loft_features_.empty() || !surface_features_.empty() ||
      !linear_pattern_features_.empty() || !circular_pattern_features_.empty() ||
      !mirror_features_.empty() || !fillet_features_.empty() || !chamfer_features_.empty() ||
      !shell_features_.empty() || !draft_features_.empty() || !body_boolean_features_.empty() ||
      !sketch_ownerships_.empty() || !body_transforms_.empty() || !reference_statuses_.empty() ||
      !reference_remaps_.empty() || !sketch_origin_overrides_.empty())
    return Result<std::size_t>::failure(
        invalid("Reference Part requires exactly its bound source, catalog and mapped Bodies, "
                "with fixed BLCAD origin planes and without authored intent"));
  std::size_t plane_index = 0;
  for (const auto& expected : {DatumPlane::xy(), DatumPlane::xz(), DatumPlane::yz()}) {
    if (!expected)
      return Result<std::size_t>::failure(expected.error());
    const auto& origin = expected.value();
    const auto& actual = datum_planes_[plane_index++];
    if (actual.id() != origin.id() || actual.name() != origin.name() ||
        actual.origin() != origin.origin() || actual.x_axis() != origin.x_axis() ||
        actual.y_axis() != origin.y_axis() || actual.normal() != origin.normal())
      return Result<std::size_t>::failure(
          invalid("Reference Part requires the exact immutable BLCAD XY, XZ and YZ origin planes"));
  }
  auto candidate = *this;
  const auto* catalog = candidate.find_imported_topology_catalog(definition.catalog());
  for (const auto& output : definition.outputs()) {
    auto edge =
        candidate.dependency_graph_.add_dependency(catalog->node_id(), body_node(output.body));
    if (!edge)
      return edge;
  }
  auto synced = candidate.invalidation_state_.sync_from_graph(candidate.dependency_graph_);
  if (!synced)
    return synced;
  candidate.reference_part_definition_ = std::move(definition);
  *this = std::move(candidate);
  return valid;
}

Result<PartDocument> PartDocument::create_reference(DocumentId id, std::string name,
                                                    StepImportSource source,
                                                    ImportedTopologyCatalog catalog,
                                                    ImportedPartDefinition definition) {
  auto document = create(std::move(id), std::move(name));
  if (!document)
    return document;
  for (const auto& plane : {DatumPlane::xy(), DatumPlane::xz(), DatumPlane::yz()}) {
    if (!plane)
      return Result<PartDocument>::failure(plane.error());
    auto added = document.value().add_datum_plane(plane.value());
    if (!added)
      return Result<PartDocument>::failure(added.error());
  }
  auto source_added = document.value().add_step_import_source(std::move(source));
  if (!source_added)
    return Result<PartDocument>::failure(source_added.error());
  auto catalog_added = document.value().add_imported_topology_catalog(std::move(catalog));
  if (!catalog_added)
    return Result<PartDocument>::failure(catalog_added.error());
  for (const auto& output : definition.outputs()) {
    auto body = Body::create(output.body, output.body.value(), output.kind);
    if (!body)
      return Result<PartDocument>::failure(body.error());
    auto added = document.value().add_body(std::move(body.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
  }
  auto sealed = document.value().seal_reference_part(std::move(definition));
  if (!sealed)
    return Result<PartDocument>::failure(sealed.error());
  return document;
}

Result<std::size_t> PartDocument::add_imported_body_feature(ImportedBodyFeature feature) {
  if (is_reference_part())
    return Result<std::size_t>::failure(reference_part_edit_error());
  auto valid = validate_imported_part_definition(feature.definition());
  if (!valid)
    return valid;
  if (find_step_import_source(feature.definition().source())->mode() !=
      StepImportMode::EditableBody)
    return Result<std::size_t>::failure(
        invalid("imported base feature requires an EditableBody source"));
  if (has_any_feature_id(feature.id()) || dependency_graph_.has_node(feature.id().value()))
    return Result<std::size_t>::failure(invalid("imported base feature id must be unique"));
  for (const auto& output : feature.definition().outputs())
    for (const auto& [producer, dependent] : dependency_graph_.dependencies())
      if (dependent == body_node(output.body))
        return Result<std::size_t>::failure(invalid("imported output Body already has a producer"));
  auto candidate = *this;
  auto added = candidate.dependency_graph_.add_node(feature.id().value());
  if (!added)
    return added;
  auto edge = candidate.dependency_graph_.add_dependency(
      find_imported_topology_catalog(feature.definition().catalog())->node_id(),
      feature.id().value());
  if (!edge)
    return edge;
  for (const auto& output : feature.definition().outputs()) {
    auto output_edge =
        candidate.dependency_graph_.add_dependency(feature.id().value(), body_node(output.body));
    if (!output_edge)
      return output_edge;
  }
  if (candidate.dependency_graph_.has_cycle())
    return Result<std::size_t>::failure(
        invalid("imported base feature creates a dependency cycle"));
  auto synced = candidate.invalidation_state_.sync_from_graph(candidate.dependency_graph_);
  if (!synced)
    return synced;
  candidate.imported_body_features_.push_back(std::move(feature));
  const auto index = candidate.imported_body_features_.size() - 1;
  *this = std::move(candidate);
  return Result<std::size_t>::success(index);
}

Result<PartDocument>
PartDocument::convert_reference_to_editable(DocumentId new_id, std::string new_name,
                                            FeatureId base_feature_id,
                                            std::vector<BodyId> new_body_ids) const {
  if (!is_reference_part() || new_id == id_ ||
      new_body_ids.size() != reference_part_definition_->outputs().size())
    return Result<PartDocument>::failure(invalid(
        "conversion requires a Reference Part, a new document identity and exact output mapping"));
  auto converted = create(std::move(new_id), std::move(new_name));
  if (!converted)
    return converted;
  for (const auto& plane : datum_planes_) {
    auto added = converted.value().add_datum_plane(plane);
    if (!added)
      return Result<PartDocument>::failure(added.error());
  }
  const auto& definition = *reference_part_definition_;
  const auto& source = *find_step_import_source(definition.source());
  auto editable_source =
      StepImportSource::create(source.id(), source.name(), source.project_relative_asset_path(),
                               source.sha256_digest(), StepImportMode::EditableBody);
  if (!editable_source)
    return Result<PartDocument>::failure(editable_source.error());
  auto source_added = converted.value().add_step_import_source(std::move(editable_source.value()));
  if (!source_added)
    return Result<PartDocument>::failure(source_added.error());
  auto catalog_added = converted.value().add_imported_topology_catalog(
      *find_imported_topology_catalog(definition.catalog()));
  if (!catalog_added)
    return Result<PartDocument>::failure(catalog_added.error());
  std::vector<ImportedBodyOutput> outputs;
  for (std::size_t index = 0; index < new_body_ids.size(); ++index) {
    const auto& old_output = definition.outputs()[index];
    const auto* old_body = find_body(old_output.body);
    auto body = Body::create(new_body_ids[index], old_body->name(), old_output.kind,
                             old_body->visibility());
    if (!body)
      return Result<PartDocument>::failure(body.error());
    auto added = converted.value().add_body(std::move(body.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
    outputs.push_back({old_output.imported_body, std::move(new_body_ids[index]), old_output.kind});
  }
  auto editable_definition =
      ImportedPartDefinition::create(definition.source(), definition.catalog(), std::move(outputs));
  if (!editable_definition)
    return Result<PartDocument>::failure(editable_definition.error());
  auto feature = ImportedBodyFeature::create(std::move(base_feature_id), "Imported body",
                                             std::move(editable_definition.value()));
  if (!feature)
    return Result<PartDocument>::failure(feature.error());
  auto added = converted.value().add_imported_body_feature(std::move(feature.value()));
  if (!added)
    return Result<PartDocument>::failure(added.error());
  return converted;
}
} // namespace blcad
