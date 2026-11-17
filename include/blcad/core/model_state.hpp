#pragma once

#include "blcad/core/engineering_resource.hpp"
#include "blcad/core/id.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/result.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace blcad {

struct ModelStateIdTag;
using ModelStateId = TypedId<ModelStateIdTag>;
[[nodiscard]] inline ModelStateId primary_model_state_id() {
  return ModelStateId("primary");
}

// A literal may override only a literal parameter. A string is an explicitly
// authored replacement formula and may override only an expression parameter.
struct ModelStateParameterOverride {
  ParameterId parameter;
  std::variant<Quantity, std::string> value;
  friend bool operator==(const ModelStateParameterOverride&,
                         const ModelStateParameterOverride&) = default;
};
struct ModelStateFeatureOverride {
  std::string executable;
  bool suppressed = false;
  friend bool operator==(const ModelStateFeatureOverride&,
                         const ModelStateFeatureOverride&) = default;
};
struct ModelStateMaterialOverride {
  // An absent Body means the Part default. An absent material explicitly clears
  // that assignment; absence of the entire record inherits the Primary value.
  std::optional<BodyId> body;
  std::optional<EngineeringResourceKey> material;
  friend bool operator==(const ModelStateMaterialOverride&,
                         const ModelStateMaterialOverride&) = default;
};
struct ModelStateComponentOverride {
  ComponentInstanceId component;
  std::optional<bool> suppressed;
  std::optional<ModelStateId> selected_state;
  friend bool operator==(const ModelStateComponentOverride&,
                         const ModelStateComponentOverride&) = default;
};
struct ModelStateSubassemblyOverride {
  SubassemblyInstanceId subassembly;
  std::optional<bool> suppressed;
  std::optional<ModelStateId> selected_state;
  friend bool operator==(const ModelStateSubassemblyOverride&,
                         const ModelStateSubassemblyOverride&) = default;
};

// Primary is the document itself, with the permanent id "primary". Named states
// are sparse siblings; there is no inheritance, display or positional intent.
struct ModelState {
  ModelStateId id;
  std::string name;
  std::vector<ModelStateParameterOverride> parameters;
  std::vector<ModelStateFeatureOverride> features;
  std::vector<ModelStateMaterialOverride> materials;
  std::vector<ModelStateComponentOverride> components;
  std::vector<ModelStateSubassemblyOverride> subassemblies;
  friend bool operator==(const ModelState&, const ModelState&) = default;
};

class PartDocument;
class AssemblyDocument;
class Project;
[[nodiscard]] Result<std::size_t> validate_model_state_catalog(const std::vector<ModelState>&);
[[nodiscard]] bool has_model_state(const std::vector<ModelState>&, const ModelStateId&) noexcept;
// Includes the complete authored definition and explicit selected state. Equal
// evaluated numbers from different states never establish interchangeable scope.
[[nodiscard]] Result<std::string> model_state_source_key(const PartDocument&, ModelStateId);
[[nodiscard]] Result<std::size_t> validate_project_model_states(const Project&);

} // namespace blcad
