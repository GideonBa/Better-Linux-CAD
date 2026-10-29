#include "blcad/geometry/step_structured_import_commands.hpp"

#include "blcad/core/step_import_freshness.hpp"
#include "blcad/core/structured_step_import.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"

#include "assembly_occt_rigid_transform.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>

namespace blcad::geometry {
namespace {
Error invalid(std::string message) {
  return Error::validation("step.structured", std::move(message));
}

Result<RigidTransform> rigid_transform(const StepImportTransform& direct) {
  const auto& m = direct.matrix;
  if (!std::all_of(m.begin(), m.end(), [](double value) { return std::isfinite(value); }))
    return Result<RigidTransform>::failure(invalid("Imported occurrence placement must be finite"));
  constexpr double tolerance = 1e-10;
  for (std::size_t row = 0; row < 3; ++row)
    for (std::size_t other = 0; other < 3; ++other) {
      double product = 0;
      for (std::size_t column = 0; column < 3; ++column)
        product += m[row * 4 + column] * m[other * 4 + column];
      if (std::abs(product - (row == other ? 1.0 : 0.0)) > tolerance)
        return Result<RigidTransform>::failure(invalid("Imported occurrence is not rigid"));
    }
  const double determinant = m[0] * (m[5] * m[10] - m[6] * m[9]) -
                             m[1] * (m[4] * m[10] - m[6] * m[8]) +
                             m[2] * (m[4] * m[9] - m[5] * m[8]);
  if (std::abs(determinant - 1) > tolerance)
    return Result<RigidTransform>::failure(invalid("Imported occurrence must be right-handed"));

  // The existing Core contract is active X, then Y, then Z (Rz * Ry * Rx).
  // At either gimbal pole choose the deterministic equivalent z=0 solution.
  const double cosine_y = std::hypot(m[0], m[4]);
  const double y = std::atan2(-m[8], cosine_y);
  const double x = cosine_y > 1e-12 ? std::atan2(m[9], m[10]) : std::atan2(-m[6], m[5]);
  const double z = cosine_y > 1e-12 ? std::atan2(m[4], m[0]) : 0;
  const auto degrees = [](double radians) {
    const double value = radians * 180 / std::numbers::pi;
    return value == 0 ? 0.0 : value;
  };
  RigidTransform result{{m[3], m[7], m[11]}, {degrees(x), degrees(y), degrees(z)}};
  const auto reconstructed = detail::to_occt_rigid_transform(result);
  for (int row = 1; row <= 3; ++row)
    for (int column = 1; column <= 4; ++column) {
      const auto index = static_cast<std::size_t>((row - 1) * 4 + column - 1);
      const double value = reconstructed.Value(row, column);
      if (!std::isfinite(value) ||
          std::abs(value - m[index]) > tolerance * std::max(1.0, std::abs(m[index])))
        return Result<RigidTransform>::failure(
            invalid("Imported direct placement cannot be represented by the Core rigid transform"));
    }
  return Result<RigidTransform>::success(result);
}

std::string supported_name(const std::string& name, const std::string& fallback) {
  return name.empty() ? fallback : name;
}

Result<std::vector<std::size_t>> canonical_products(const StepImportGeometry& imported) {
  // Assembly definitions have no persistent topology correspondence to recover.
  // Keep equal-geometry definitions distinct and order their initial BLCAD IDs
  // by literal metadata and rooted contexts. Names never become source identity;
  // Part/Body catalog ambiguity retains the stricter geometry-only boundary.
  std::vector<std::vector<std::size_t>> children(imported.products.size());
  struct Pending {
    std::size_t occurrence;
    std::string parent_context;
  };
  std::vector<Pending> pending;
  for (std::size_t index = 0; index < imported.occurrences.size(); ++index) {
    const auto& occurrence = imported.occurrences[index];
    if (occurrence.product_definition >= imported.products.size() ||
        (occurrence.parent_definition &&
         (*occurrence.parent_definition >= imported.products.size() ||
          !imported.products[*occurrence.parent_definition].is_assembly)))
      return Result<std::vector<std::size_t>>::failure(
          invalid("Imported occurrence has a missing definition"));
    if (occurrence.parent_definition)
      children[*occurrence.parent_definition].push_back(index);
    else
      pending.push_back({index, "root"});
  }
  std::vector<std::vector<std::string>> incoming(imported.products.size());
  for (std::size_t index = 0; index < pending.size(); ++index) {
    if (pending.size() > 100000)
      return Result<std::vector<std::size_t>>::failure(
          invalid("Structured occurrence expansion exceeds the import limit"));
    const auto& occurrence = imported.occurrences[pending[index].occurrence];
    const auto& product = imported.products[occurrence.product_definition];
    std::ostringstream context;
    context.imbue(std::locale::classic());
    context << std::setprecision(std::numeric_limits<double>::max_digits10)
            << pending[index].parent_context << '/' << product.definition_key << '/'
            << product.name.size() << ':' << product.name << '/' << occurrence.name.size() << ':'
            << occurrence.name;
    for (const double value : occurrence.transform.matrix)
      context << '/' << (value == 0 ? 0.0 : value);
    auto key = step_import_sha256(context.str());
    if (!key)
      return Result<std::vector<std::size_t>>::failure(key.error());
    incoming[occurrence.product_definition].push_back(key.value());
    for (const auto child : children[occurrence.product_definition])
      pending.push_back({child, key.value()});
  }
  for (auto& contexts : incoming)
    std::sort(contexts.begin(), contexts.end());
  std::vector<std::size_t> products(imported.products.size());
  for (std::size_t index = 0; index < products.size(); ++index) {
    products[index] = index;
    const auto& product = imported.products[index];
    if (incoming[index].empty() || product.definition_key.empty() ||
        (!product.is_assembly && product.ambiguous_order) ||
        std::any_of(product.bodies.begin(), product.bodies.end(),
                    [](const auto& body) { return body.ambiguous_order; }))
      return Result<std::vector<std::size_t>>::failure(
          invalid("Indistinguishable imported Part/Body definitions cannot receive deterministic "
                  "structured-import identities"));
  }
  const auto key = [&](std::size_t index) {
    const auto& product = imported.products[index];
    return std::tie(product.definition_key, product.name, incoming[index]);
  };
  std::sort(products.begin(), products.end(),
            [&](auto first, auto second) { return key(first) < key(second); });
  for (std::size_t index = 1; index < products.size(); ++index)
    if (key(products[index - 1]) == key(products[index]))
      return Result<std::vector<std::size_t>>::failure(
          invalid("Indistinguishable imported Assembly definitions have no unique canonical "
                  "occurrence context"));
  return Result<std::vector<std::size_t>>::success(std::move(products));
}

Result<PartDocument> imported_part(const StepImportGeometry& imported, std::size_t product_index,
                                   const DocumentId& part_id, const std::string& catalog_id,
                                   const std::string& body_prefix, const FeatureId& feature_id) {
  const auto& product = imported.products[product_index];
  auto catalog = StepImportTopology{}.create_catalog(ImportedTopologyCatalogId(catalog_id),
                                                     imported, product_index);
  if (!catalog)
    return Result<PartDocument>::failure(catalog.error());
  auto recovery = StepImportTopology{}.recover(catalog.value(), imported);
  if (!recovery)
    return Result<PartDocument>::failure(recovery.error());
  std::vector<ImportedBodyOutput> outputs;
  for (const auto& entry : catalog.value().entries()) {
    if (entry.descriptor.kind != ImportedTopologyKind::Body)
      continue;
    if (!entry.descriptor.body_kind)
      return Result<PartDocument>::failure(invalid("Imported Body kind is unavailable"));
    auto identity = ImportedTopologyReference::create(catalog.value().id(), entry.id,
                                                      ImportedTopologyCapability::Body);
    if (!identity)
      return Result<PartDocument>::failure(identity.error());
    auto resolved = StepImportTopology{}.resolve(recovery.value(), identity.value());
    if (!resolved)
      return Result<PartDocument>::failure(resolved.error());
    if (resolved.value().product_index != product_index)
      return Result<PartDocument>::failure(
          invalid("Imported product does not have a unique matching Body owner"));
    outputs.push_back({entry.id, BodyId(body_prefix + std::to_string(outputs.size() + 1)),
                       *entry.descriptor.body_kind});
  }
  auto definition = ImportedPartDefinition::create(imported.source.id(), catalog.value().id(),
                                                   std::move(outputs));
  if (!definition)
    return Result<PartDocument>::failure(definition.error());
  const auto name = supported_name(product.name, "Imported Part " + part_id.value());
  if (imported.source.mode() == StepImportMode::Reference)
    return PartDocument::create_reference(part_id, name, imported.source, catalog.value(),
                                          definition.value());
  auto feature = ImportedBodyFeature::create(feature_id, "Imported body", definition.value());
  if (!feature)
    return Result<PartDocument>::failure(feature.error());
  return PartDocument::create_editable_import(part_id, name, imported.source, catalog.value(),
                                              feature.value());
}
} // namespace

Result<StepStructuredImportCandidate>
StepStructuredImportCommands::create_project(const StepImportSource& source, DocumentId project_id,
                                             std::string name,
                                             const std::filesystem::path& asset_root) const {
  if (project_id.empty() || name.empty() || asset_root.empty())
    return Result<StepStructuredImportCandidate>::failure(
        invalid("Structured import requires a new Project identity, name and explicit asset root"));
  try {
    const auto root_path = std::filesystem::absolute(asset_root).lexically_normal();
    auto imported = StepImportReader{}.read(source, root_path);
    if (!imported)
      return Result<StepStructuredImportCandidate>::failure(imported.error());
    auto ordered = canonical_products(imported.value());
    if (!ordered)
      return Result<StepStructuredImportCandidate>::failure(ordered.error());
    const auto& products = ordered.value();
    std::vector<std::size_t> canonical_rank(products.size());
    for (std::size_t rank = 0; rank < products.size(); ++rank) {
      canonical_rank[products[rank]] = rank;
      const auto& product = imported.value().products[products[rank]];
      if (product.is_assembly ? !product.bodies.empty() : product.bodies.empty())
        return Result<StepStructuredImportCandidate>::failure(
            invalid("Imported definition must be an Assembly or a nonempty leaf Part"));
    }

    const auto prefix = project_id.value() + ".step.";
    auto root = AssemblyDocument::create(DocumentId(prefix + "root"), name + " / Imported roots");
    if (!root)
      return Result<StepStructuredImportCandidate>::failure(root.error());
    std::vector<AssemblyDocument> assemblies;
    std::vector<PartDocument> parts;
    std::vector<std::optional<std::size_t>> assembly_index(products.size());
    std::vector<std::optional<DocumentId>> part_id(products.size());
    for (const auto index : products) {
      const auto& product = imported.value().products[index];
      if (product.is_assembly) {
        const auto ordinal = std::to_string(assemblies.size() + 1);
        auto assembly =
            AssemblyDocument::create(DocumentId(prefix + "assembly." + ordinal),
                                     supported_name(product.name, "Imported Assembly " + ordinal));
        if (!assembly)
          return Result<StepStructuredImportCandidate>::failure(assembly.error());
        assembly_index[index] = assemblies.size();
        assemblies.push_back(std::move(assembly.value()));
      } else {
        const auto ordinal = std::to_string(parts.size() + 1);
        part_id[index] = DocumentId(prefix + "part." + ordinal);
        auto part =
            imported_part(imported.value(), index, *part_id[index], prefix + "catalog." + ordinal,
                          prefix + "body." + ordinal + ".", FeatureId(prefix + "base." + ordinal));
        if (!part)
          return Result<StepStructuredImportCandidate>::failure(part.error());
        parts.push_back(std::move(part.value()));
      }
    }
    auto occurrences = imported.value().occurrences;
    std::sort(occurrences.begin(), occurrences.end(), [&](const auto& first, const auto& second) {
      const auto parent = [&](const auto& occurrence) -> std::optional<std::size_t> {
        return occurrence.parent_definition
                   ? std::optional(canonical_rank[*occurrence.parent_definition])
                   : std::nullopt;
      };
      return std::tuple(parent(first), canonical_rank[first.product_definition],
                        first.transform.matrix, first.name) <
             std::tuple(parent(second), canonical_rank[second.product_definition],
                        second.transform.matrix, second.name);
    });
    std::size_t roots = 0;
    for (std::size_t index = 0; index < occurrences.size(); ++index) {
      const auto& occurrence = occurrences[index];
      if (occurrence.product_definition >= products.size() ||
          (occurrence.parent_definition && (*occurrence.parent_definition >= products.size() ||
                                            !assembly_index[*occurrence.parent_definition])))
        return Result<StepStructuredImportCandidate>::failure(
            invalid("Imported occurrence has a missing or non-Assembly parent definition"));
      auto transform = rigid_transform(occurrence.transform);
      if (!transform)
        return Result<StepStructuredImportCandidate>::failure(transform.error());
      AssemblyDocument& parent = occurrence.parent_definition
                                     ? assemblies[*assembly_index[*occurrence.parent_definition]]
                                     : root.value();
      if (!occurrence.parent_definition)
        ++roots;
      const auto child = occurrence.product_definition;
      const auto occurrence_id = prefix + "occurrence." + std::to_string(index + 1);
      const auto occurrence_name = supported_name(
          occurrence.name, supported_name(imported.value().products[child].name, "Occurrence"));
      if (part_id[child]) {
        if (std::find(parent.member_parts().begin(), parent.member_parts().end(),
                      *part_id[child]) == parent.member_parts().end()) {
          auto added = parent.add_member_part(*part_id[child]);
          if (!added)
            return Result<StepStructuredImportCandidate>::failure(added.error());
        }
        auto component = ComponentInstance::create(
            ComponentInstanceId(occurrence_id), occurrence_name, *part_id[child],
            ComponentVisibility::Visible, ComponentSuppressionState::Active,
            ComponentGroundingState::Free, transform.value());
        if (!component)
          return Result<StepStructuredImportCandidate>::failure(component.error());
        auto added = parent.add_component_instance(component.value());
        if (!added)
          return Result<StepStructuredImportCandidate>::failure(added.error());
      } else {
        auto instance = SubassemblyInstance::create(
            SubassemblyInstanceId(occurrence_id), occurrence_name,
            assemblies[*assembly_index[child]].id(), ComponentVisibility::Visible,
            ComponentSuppressionState::Active, transform.value());
        if (!instance)
          return Result<StepStructuredImportCandidate>::failure(instance.error());
        auto added = parent.add_subassembly_instance(instance.value());
        if (!added)
          return Result<StepStructuredImportCandidate>::failure(added.error());
      }
    }
    if (!roots)
      return Result<StepStructuredImportCandidate>::failure(invalid("Imported graph has no roots"));
    auto project = create_structured_step_project(project_id, name, source, std::move(root.value()),
                                                  std::move(assemblies), std::move(parts));
    if (!project)
      return Result<StepStructuredImportCandidate>::failure(project.error());
    auto fresh = inspect_step_import_source(source, root_path);
    if (!fresh)
      return Result<StepStructuredImportCandidate>::failure(fresh.error());
    if (fresh.value().state() != StepImportFreshness::Available)
      return Result<StepStructuredImportCandidate>::failure(invalid(fresh.value().diagnostic()));
    std::vector<DocumentId> owners{project.value().id(), project.value().assembly().id()};
    for (const auto& part : project.value().part_documents())
      owners.push_back(part.id());
    for (const auto& assembly : project.value().child_assembly_documents())
      owners.push_back(assembly.id());
    std::sort(owners.begin(), owners.end(),
              [](const auto& a, const auto& b) { return a.value() < b.value(); });
    auto diagnostics = std::move(imported.value().diagnostics);
    diagnostics.push_back("Created shared imported definitions with exact direct placements; "
                          "the synthetic BLCAD root preserves every foreign root occurrence.");
    return Result<StepStructuredImportCandidate>::success(
        {std::move(project.value()), std::move(owners), std::move(diagnostics)});
  } catch (const std::exception& failure) {
    return Result<StepStructuredImportCandidate>::failure(invalid(failure.what()));
  }
}
} // namespace blcad::geometry
