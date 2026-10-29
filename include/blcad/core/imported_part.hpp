#pragma once

#include "blcad/core/body.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/imported_topology.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace blcad {

enum class PartDocumentMode { Editable, Reference };
[[nodiscard]] std::string_view to_string(PartDocumentMode mode) noexcept;

// Result-face provenance names a stable catalog Face, never a transient face
// ordinal or an invented generated-feature role.
[[nodiscard]] Result<std::string>
make_imported_face_origin_role(const ImportedTopologyReference& reference);
[[nodiscard]] Result<ImportedTopologyReference>
parse_imported_face_origin_role(std::string_view role);

// Persistent BLCAD output identities, independent of transient STEP/XDE order.
struct ImportedBodyOutput {
  ImportedTopologyId imported_body;
  BodyId body;
  BodyKind kind = BodyKind::Solid;
  friend bool operator==(const ImportedBodyOutput&, const ImportedBodyOutput&) = default;
};

// One catalog belongs to one selected foreign product. Output order is explicit
// intent and is retained through refresh, conversion and project roundtrips.
class ImportedPartDefinition {
public:
  [[nodiscard]] static Result<ImportedPartDefinition>
  create(StepImportSourceId source, ImportedTopologyCatalogId catalog,
         std::vector<ImportedBodyOutput> outputs);
  [[nodiscard]] const StepImportSourceId& source() const noexcept {
    return source_;
  }
  [[nodiscard]] const ImportedTopologyCatalogId& catalog() const noexcept {
    return catalog_;
  }
  [[nodiscard]] const std::vector<ImportedBodyOutput>& outputs() const noexcept {
    return outputs_;
  }
  friend bool operator==(const ImportedPartDefinition&, const ImportedPartDefinition&) = default;

private:
  ImportedPartDefinition(StepImportSourceId source, ImportedTopologyCatalogId catalog,
                         std::vector<ImportedBodyOutput> outputs);
  StepImportSourceId source_;
  ImportedTopologyCatalogId catalog_;
  std::vector<ImportedBodyOutput> outputs_;
};

// Immutable imported base intent, created directly or by converting a Reference
// Part. Parameters belong to downstream authored features; an explicit accepted
// source refresh retains this feature's identity and ordered Body mapping.
class ImportedBodyFeature {
public:
  [[nodiscard]] static Result<ImportedBodyFeature> create(FeatureId id, std::string name,
                                                          ImportedPartDefinition definition);
  [[nodiscard]] const FeatureId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const std::string& name() const noexcept {
    return name_;
  }
  [[nodiscard]] const ImportedPartDefinition& definition() const noexcept {
    return definition_;
  }
  friend bool operator==(const ImportedBodyFeature&, const ImportedBodyFeature&) = default;

private:
  ImportedBodyFeature(FeatureId id, std::string name, ImportedPartDefinition definition);
  FeatureId id_;
  std::string name_;
  ImportedPartDefinition definition_;
};

} // namespace blcad
