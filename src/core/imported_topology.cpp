#include "blcad/core/imported_topology.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/project.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace blcad {
namespace {
Error invalid(std::string message) {
  return Error::validation("imported_topology", std::move(message));
}
bool digest(std::string_view value) {
  return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
bool finite(Point3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool valid_descriptor(const ImportedTopologyDescriptor& d) {
  if (to_string(d.kind) == "unknown" || to_string(d.geometry) == "unknown" || !finite(d.centroid) ||
      !finite(d.origin) || !std::isfinite(d.direction.x) || !std::isfinite(d.direction.y) ||
      !std::isfinite(d.direction.z) || !std::isfinite(d.measure) || d.measure < 0 ||
      !std::isfinite(d.radius_mm) || d.radius_mm < 0 || !digest(d.geometry_key) ||
      !std::all_of(d.bounds_mm.begin(), d.bounds_mm.end(),
                   [](double n) { return std::isfinite(n); }) ||
      !std::all_of(d.samples.begin(), d.samples.end(), finite) ||
      !std::is_sorted(d.boundary_keys.begin(), d.boundary_keys.end()) ||
      !std::all_of(d.boundary_keys.begin(), d.boundary_keys.end(), digest))
    return false;
  for (std::size_t axis = 0; axis < 3; ++axis)
    if (d.bounds_mm[axis] > d.bounds_mm[axis + 3])
      return false;
  if (d.kind == ImportedTopologyKind::Body) {
    if (!d.body_kind || (*d.body_kind != BodyKind::Solid && *d.body_kind != BodyKind::Surface) ||
        d.geometry != ImportedGeometryClass::Opaque || d.measure <= 0)
      return false;
  } else if (d.body_kind)
    return false;
  if (d.kind == ImportedTopologyKind::Face &&
      (d.measure <= 0 || (d.geometry != ImportedGeometryClass::Plane &&
                          d.geometry != ImportedGeometryClass::Cylinder &&
                          d.geometry != ImportedGeometryClass::Opaque)))
    return false;
  if (d.kind == ImportedTopologyKind::Edge &&
      ((d.geometry != ImportedGeometryClass::Line && d.geometry != ImportedGeometryClass::Circle &&
        d.geometry != ImportedGeometryClass::Opaque) ||
       (d.geometry != ImportedGeometryClass::Opaque && d.measure <= 0)))
    return false;
  if (d.kind == ImportedTopologyKind::Vertex &&
      (d.geometry != ImportedGeometryClass::Point || d.measure != 0))
    return false;
  const bool directed =
      d.geometry == ImportedGeometryClass::Plane || d.geometry == ImportedGeometryClass::Cylinder ||
      d.geometry == ImportedGeometryClass::Line || d.geometry == ImportedGeometryClass::Circle;
  if (directed) {
    if (std::abs(length(d.direction) - 1) > 1e-9)
      return false;
    const auto first = std::abs(d.direction.x) > 1e-12   ? d.direction.x
                       : std::abs(d.direction.y) > 1e-12 ? d.direction.y
                                                         : d.direction.z;
    if (first <= 0)
      return false;
    const double origin_length = std::hypot(d.origin.x, d.origin.y, d.origin.z);
    if (!std::isfinite(origin_length))
      return false;
    const double tolerance = 1e-6 + 1e-9 * origin_length;
    const Vector3 origin{d.origin.x, d.origin.y, d.origin.z};
    if (d.geometry == ImportedGeometryClass::Plane &&
        length(cross(origin, d.direction)) > tolerance)
      return false;
    if ((d.geometry == ImportedGeometryClass::Cylinder ||
         d.geometry == ImportedGeometryClass::Line) &&
        std::abs(dot(origin, d.direction)) > tolerance)
      return false;
  } else if (d.direction != Vector3{})
    return false;
  const bool radial =
      d.geometry == ImportedGeometryClass::Circle || d.geometry == ImportedGeometryClass::Cylinder;
  if (radial ? d.radius_mm <= 0 : d.radius_mm != 0)
    return false;
  return true;
}
bool unreserved(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
         c == '-';
}
std::string encode(std::string_view input) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : input) {
    if (unreserved(c))
      result += static_cast<char>(c);
    else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  return result;
}
std::optional<std::string> decode(std::string_view input) {
  const auto hex = [](char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
  };
  std::string result;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const auto c = static_cast<unsigned char>(input[i]);
    if (unreserved(c))
      result += static_cast<char>(c);
    else if (c == '%' && i + 2 < input.size()) {
      const int high = hex(input[i + 1]), low = hex(input[i + 2]);
      if (high < 0 || low < 0 || unreserved(static_cast<unsigned char>(high * 16 + low)))
        return {};
      result += static_cast<char>(high * 16 + low);
      i += 2;
    } else
      return {};
  }
  return result.empty() ? std::nullopt : std::optional(std::move(result));
}
template <typename Host>
const ImportedTopologyCatalog* find_catalog(const Host& host, ImportedTopologyCatalogId id) {
  const auto& values = host.imported_topology_catalogs();
  const auto found =
      std::find_if(values.begin(), values.end(), [&](const auto& c) { return c.id() == id; });
  return found == values.end() ? nullptr : &*found;
}
template <typename Host> bool source_in_use(const Host& host, StepImportSourceId id) {
  return std::any_of(host.imported_topology_catalogs().begin(),
                     host.imported_topology_catalogs().end(),
                     [&](const auto& catalog) { return catalog.source() == id; });
}
} // namespace

std::string_view to_string(ImportedTopologyKind value) noexcept {
  switch (value) {
  case ImportedTopologyKind::Body:
    return "body";
  case ImportedTopologyKind::Face:
    return "face";
  case ImportedTopologyKind::Edge:
    return "edge";
  case ImportedTopologyKind::Vertex:
    return "vertex";
  }
  return "unknown";
}
std::string_view to_string(ImportedGeometryClass value) noexcept {
  switch (value) {
  case ImportedGeometryClass::Opaque:
    return "opaque";
  case ImportedGeometryClass::Plane:
    return "plane";
  case ImportedGeometryClass::Cylinder:
    return "cylinder";
  case ImportedGeometryClass::Line:
    return "line";
  case ImportedGeometryClass::Circle:
    return "circle";
  case ImportedGeometryClass::Point:
    return "point";
  }
  return "unknown";
}
std::string_view to_string(ImportedTopologyCapability value) noexcept {
  switch (value) {
  case ImportedTopologyCapability::Body:
    return "body";
  case ImportedTopologyCapability::Face:
    return "face";
  case ImportedTopologyCapability::Edge:
    return "edge";
  case ImportedTopologyCapability::Plane:
    return "plane";
  case ImportedTopologyCapability::Cylinder:
    return "cylinder";
  case ImportedTopologyCapability::Axis:
    return "axis";
  case ImportedTopologyCapability::Line:
    return "line";
  case ImportedTopologyCapability::Circle:
    return "circle";
  case ImportedTopologyCapability::Point:
    return "point";
  }
  return "unknown";
}
std::string_view to_string(ImportedTopologyRecoveryStatus value) noexcept {
  switch (value) {
  case ImportedTopologyRecoveryStatus::Resolved:
    return "resolved";
  case ImportedTopologyRecoveryStatus::Lost:
    return "lost";
  case ImportedTopologyRecoveryStatus::Ambiguous:
    return "ambiguous";
  }
  return "unknown";
}
ImportedTopologyCatalog::ImportedTopologyCatalog(ImportedTopologyCatalogId id,
                                                 StepImportSourceId source,
                                                 std::string captured_digest,
                                                 std::string product_key,
                                                 std::vector<ImportedTopologyEntry> entries)
    : id_(std::move(id)), source_(std::move(source)), digest_(std::move(captured_digest)),
      product_key_(std::move(product_key)), entries_(std::move(entries)) {}
Result<ImportedTopologyCatalog>
ImportedTopologyCatalog::create(ImportedTopologyCatalogId id, StepImportSourceId source,
                                std::string captured_digest, std::string product_key,
                                std::vector<ImportedTopologyEntry> entries) {
  if (id.empty() || source.empty() || !digest(captured_digest) || !digest(product_key) ||
      entries.empty())
    return Result<ImportedTopologyCatalog>::failure(
        invalid("catalog requires ids, canonical capture evidence and topology entries"));
  std::set<std::string> ids, bodies;
  for (const auto& entry : entries) {
    if (entry.id.empty() || !ids.insert(entry.id.value()).second ||
        !valid_descriptor(entry.descriptor))
      return Result<ImportedTopologyCatalog>::failure(
          invalid("catalog entries require unique ids and normalized finite descriptors"));
    if (entry.descriptor.kind == ImportedTopologyKind::Body) {
      if (entry.parent_body)
        return Result<ImportedTopologyCatalog>::failure(
            invalid("imported Body must not have a parent Body"));
      bodies.insert(entry.id.value());
    }
  }
  if (bodies.empty())
    return Result<ImportedTopologyCatalog>::failure(
        invalid("catalog requires at least one imported Body"));
  for (const auto& entry : entries)
    if (entry.descriptor.kind != ImportedTopologyKind::Body &&
        (!entry.parent_body || !bodies.contains(entry.parent_body->value())))
      return Result<ImportedTopologyCatalog>::failure(
          invalid("subtopology parent must be a Body in the same catalog"));
  return Result<ImportedTopologyCatalog>::success(
      ImportedTopologyCatalog(std::move(id), std::move(source), std::move(captured_digest),
                              std::move(product_key), std::move(entries)));
}
const ImportedTopologyEntry* ImportedTopologyCatalog::find(ImportedTopologyId id) const noexcept {
  const auto found =
      std::find_if(entries_.begin(), entries_.end(), [&](const auto& e) { return e.id == id; });
  return found == entries_.end() ? nullptr : &*found;
}
std::string ImportedTopologyCatalog::node_id() const {
  return "imported-catalog:" + id_.value();
}
ImportedTopologyReference::ImportedTopologyReference(ImportedTopologyCatalogId catalog,
                                                     ImportedTopologyId entry,
                                                     ImportedTopologyCapability capability)
    : catalog_(std::move(catalog)), entry_(std::move(entry)), capability_(capability) {}
Result<ImportedTopologyReference>
ImportedTopologyReference::create(ImportedTopologyCatalogId catalog, ImportedTopologyId entry,
                                  ImportedTopologyCapability capability) {
  if (catalog.empty() || entry.empty() || to_string(capability) == "unknown")
    return Result<ImportedTopologyReference>::failure(
        invalid("imported reference requires catalog, entry and explicit capability"));
  return Result<ImportedTopologyReference>::success(
      ImportedTopologyReference(std::move(catalog), std::move(entry), capability));
}
std::string ImportedTopologyReference::source_node_id() const {
  return "imported-catalog:" + catalog_.value();
}
std::vector<ImportedTopologyCapability>
imported_topology_capabilities(const ImportedTopologyDescriptor& d) {
  using C = ImportedTopologyCapability;
  if (d.kind == ImportedTopologyKind::Body)
    return {C::Body};
  switch (d.geometry) {
  case ImportedGeometryClass::Plane:
    return {C::Plane, C::Face};
  case ImportedGeometryClass::Cylinder:
    return {C::Cylinder, C::Axis, C::Face};
  case ImportedGeometryClass::Line:
    return {C::Line, C::Edge};
  case ImportedGeometryClass::Circle:
    return {C::Circle, C::Axis, C::Point, C::Edge};
  case ImportedGeometryClass::Point:
    return {C::Point};
  case ImportedGeometryClass::Opaque:
    if (d.kind == ImportedTopologyKind::Face)
      return {C::Face};
    if (d.kind == ImportedTopologyKind::Edge)
      return {C::Edge};
    return {};
  }
  return {};
}
Result<std::size_t> validate_imported_topology_reference(const PartDocument& part,
                                                         const ImportedTopologyReference& ref) {
  const auto* catalog = part.find_imported_topology_catalog(ref.catalog());
  const auto* entry = catalog ? catalog->find(ref.entry()) : nullptr;
  if (!catalog || !part.find_step_import_source(catalog->source()) || !entry)
    return Result<std::size_t>::failure(
        invalid("imported reference catalog, source and entry must belong to this Part"));
  const auto capabilities = imported_topology_capabilities(entry->descriptor);
  if (std::find(capabilities.begin(), capabilities.end(), ref.capability()) == capabilities.end())
    return Result<std::size_t>::failure(
        invalid("imported topology does not provide the requested capability"));
  return Result<std::size_t>::success(1);
}
Result<std::string> make_imported_topology_target_spelling(const ImportedTopologyReference& ref) {
  return Result<std::string>::success("imported:" + encode(ref.catalog().value()) + ':' +
                                      encode(ref.entry().value()) + ':' +
                                      std::string(to_string(ref.capability())));
}
bool is_imported_topology_target_spelling(std::string_view value) noexcept {
  return value.starts_with("imported:");
}
Result<ImportedTopologyReference> parse_imported_topology_target_spelling(std::string_view value) {
  const auto fail = [] {
    return Result<ImportedTopologyReference>::failure(
        invalid("invalid canonical imported topology target spelling"));
  };
  if (!is_imported_topology_target_spelling(value))
    return fail();
  value.remove_prefix(9);
  const auto first = value.find(':');
  if (first == std::string_view::npos)
    return fail();
  const auto second = value.find(':', first + 1);
  if (second == std::string_view::npos || value.find(':', second + 1) != std::string_view::npos)
    return fail();
  const auto catalog = decode(value.substr(0, first)),
             entry = decode(value.substr(first + 1, second - first - 1));
  if (!catalog || !entry)
    return fail();
  for (auto capability : {ImportedTopologyCapability::Body, ImportedTopologyCapability::Face,
                          ImportedTopologyCapability::Edge, ImportedTopologyCapability::Plane,
                          ImportedTopologyCapability::Cylinder, ImportedTopologyCapability::Axis,
                          ImportedTopologyCapability::Line, ImportedTopologyCapability::Circle,
                          ImportedTopologyCapability::Point})
    if (value.substr(second + 1) == to_string(capability))
      return ImportedTopologyReference::create(ImportedTopologyCatalogId(*catalog),
                                               ImportedTopologyId(*entry), capability);
  return fail();
}

const ImportedTopologyCatalog*
PartDocument::find_imported_topology_catalog(ImportedTopologyCatalogId id) const noexcept {
  return find_catalog(*this, std::move(id));
}
const ImportedTopologyCatalog*
Project::find_imported_topology_catalog(ImportedTopologyCatalogId id) const noexcept {
  return find_catalog(*this, std::move(id));
}
Result<std::size_t> PartDocument::add_imported_topology_catalog(ImportedTopologyCatalog catalog) {
  if (!find_step_import_source(catalog.source()) || find_imported_topology_catalog(catalog.id()) ||
      dependency_graph_.has_node(catalog.node_id()))
    return Result<std::size_t>::failure(
        invalid("catalog requires an owned source and unique catalog/dependency identity"));
  auto candidate = *this;
  auto added = candidate.dependency_graph_.add_node(catalog.node_id());
  if (!added)
    return added;
  auto synced = candidate.invalidation_state_.sync_from_graph(candidate.dependency_graph_);
  if (!synced)
    return synced;
  candidate.imported_topology_catalogs_.push_back(std::move(catalog));
  const auto index = candidate.imported_topology_catalogs_.size() - 1;
  *this = std::move(candidate);
  return Result<std::size_t>::success(index);
}
Result<std::size_t> Project::add_imported_topology_catalog(ImportedTopologyCatalog catalog) {
  if (!find_step_import_source(catalog.source()) || find_imported_topology_catalog(catalog.id()))
    return Result<std::size_t>::failure(
        invalid("catalog requires an owned source and unique catalog identity"));
  imported_topology_catalogs_.push_back(std::move(catalog));
  return Result<std::size_t>::success(imported_topology_catalogs_.size() - 1);
}
Result<std::size_t> PartDocument::remove_imported_topology_catalog(ImportedTopologyCatalogId id) {
  const auto* catalog = find_imported_topology_catalog(id);
  if (!catalog || !dependency_graph_.direct_dependents(catalog->node_id()).empty())
    return Result<std::size_t>::failure(
        invalid("referenced or absent imported catalog cannot be removed"));
  auto candidate = *this;
  const auto node = catalog->node_id();
  auto removed = candidate.dependency_graph_.remove_node(node);
  if (!removed)
    return removed;
  candidate.invalidation_state_.untrack_node(node);
  std::erase_if(candidate.imported_topology_catalogs_, [&](const auto& c) { return c.id() == id; });
  *this = std::move(candidate);
  return Result<std::size_t>::success(1);
}
Result<std::size_t> Project::remove_imported_topology_catalog(ImportedTopologyCatalogId id) {
  if (!find_imported_topology_catalog(id))
    return Result<std::size_t>::failure(invalid("imported catalog does not exist"));
  std::erase_if(imported_topology_catalogs_, [&](const auto& c) { return c.id() == id; });
  return Result<std::size_t>::success(1);
}
Result<std::size_t> PartDocument::remove_step_import_source(StepImportSourceId id) {
  return source_in_use(*this, id) ? Result<std::size_t>::failure(invalid(
                                        "STEP source is owned by an imported topology catalog"))
                                  : step_import_sources_.remove(std::move(id));
}
Result<std::size_t> PartDocument::remove_step_import_source(const StepImportSource& expected) {
  return source_in_use(*this, expected.id())
             ? Result<std::size_t>::failure(
                   invalid("STEP source is owned by an imported topology catalog"))
             : step_import_sources_.remove(expected);
}
Result<std::size_t> Project::remove_step_import_source(StepImportSourceId id) {
  return source_in_use(*this, id) ? Result<std::size_t>::failure(invalid(
                                        "STEP source is owned by an imported topology catalog"))
                                  : step_import_sources_.remove(std::move(id));
}
Result<std::size_t> Project::remove_step_import_source(const StepImportSource& expected) {
  return source_in_use(*this, expected.id())
             ? Result<std::size_t>::failure(
                   invalid("STEP source is owned by an imported topology catalog"))
             : step_import_sources_.remove(expected);
}
Result<std::size_t> PartDocument::refresh_step_import_source(const StepImportSource& expected,
                                                             std::string hash) {
  auto candidate = *this;
  auto changed = candidate.step_import_sources_.refresh(expected, std::move(hash));
  if (!changed || changed.value() == 0)
    return changed;
  for (const auto& catalog : candidate.imported_topology_catalogs_)
    if (catalog.source() == expected.id()) {
      auto invalidated = candidate.invalidation_state_.mark_changed(candidate.dependency_graph_,
                                                                    catalog.node_id());
      if (!invalidated)
        return Result<std::size_t>::failure(invalidated.error());
    }
  *this = std::move(candidate);
  return changed;
}
} // namespace blcad
