#pragma once
#include "blcad/core/assembly_hole_feature.hpp"
#include "blcad/core/project.hpp"
#include "hole_recipe_json.hpp"
#include "project_design_reference_json.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace blcad::detail {
// Implemented by the existing Part codec, so participant recipes always use the
// identical strict Hole/Thread operation dispatch and pinned resource format.
inline nlohmann::json assembly_holes_to_json(const Project& project) {
  using J = nlohmann::json;
  J result = J::array();
  for (const auto& feature : project.assembly_hole_features()) {
    J participants = J::array();
    for (const auto& participant : feature.participants()) {
      const auto* role = participant.role == AssemblyHoleParticipantRole::Threaded ? "threaded"
                         : participant.role == AssemblyHoleParticipantRole::Seat   ? "seat"
                                                                                   : "clearance";
      participants.push_back(
          {{"id", participant.id},
           {"occurrence", design_occurrence_json(participant.occurrence)},
           {"state", participant.state.value()},
           {"role", role},
           {"definition_policy",
            participant.definition_policy == AssemblyHoleDefinitionPolicy::SingleOccurrence
                ? "single_occurrence"
                : "affect_shared_definition"},
           {"recipe", shared_hole_to_json(participant.recipe)}});
    }
    const auto& layout = feature.layout();
    J source{{"occurrence", design_occurrence_json(layout.source)},
             {"state", layout.state.value()},
             {"hole", layout.hole.value()}};
    if (layout.pattern)
      source["pattern"] = layout.pattern->value();
    result.push_back({{"version", 1},
                      {"id", feature.id().value()},
                      {"name", feature.name()},
                      {"layout", std::move(source)},
                      {"participants", std::move(participants)}});
  }
  return result;
}
inline Result<std::size_t> load_assembly_holes(const nlohmann::json& values, Project& project) {
  using J = nlohmann::json;
  const auto exact = [](const J& value, std::initializer_list<const char*> keys) {
    if (!value.is_object() || value.size() != keys.size())
      return false;
    for (const auto* key : keys)
      if (!value.contains(key))
        return false;
    return true;
  };
  const auto occurrence = [&](const J& value) {
    if (!exact(value, {"path", "component", "part"}) || !value.at("path").is_array())
      throw std::invalid_argument("Malformed Assembly Hole occurrence");
    std::vector<SubassemblyInstanceId> path;
    for (const auto& id : value.at("path"))
      path.emplace_back(id.get<std::string>());
    return ProjectDesignOccurrence{std::move(path),
                                   ComponentInstanceId(value.at("component").get<std::string>()),
                                   DocumentId(value.at("part").get<std::string>())};
  };
  try {
    if (!values.is_array() || values.empty())
      throw std::invalid_argument("Assembly Hole requires a necessary nonempty array");
    std::vector<AssemblyHoleFeature> features;
    for (const auto& v : values) {
      if (!exact(v, {"version", "id", "name", "layout", "participants"}) ||
          !v.at("version").is_number_integer() || v.at("version") != 1 ||
          !v.at("participants").is_array())
        throw std::invalid_argument("Unsupported Assembly Hole operation record");
      const auto& l = v.at("layout");
      auto base = l;
      base.erase("pattern");
      if (!exact(base, {"occurrence", "state", "hole"}))
        throw std::invalid_argument("Malformed Assembly Hole layout");
      AssemblyHoleLayout layout{occurrence(l.at("occurrence")),
                                ModelStateId(l.at("state").get<std::string>()),
                                FeatureId(l.at("hole").get<std::string>()),
                                {}};
      if (l.contains("pattern"))
        layout.pattern = FeatureId(l.at("pattern").get<std::string>());
      std::vector<AssemblyHoleParticipant> participants;
      for (const auto& p : v.at("participants")) {
        if (!exact(p, {"id", "occurrence", "state", "role", "definition_policy", "recipe"}))
          throw std::invalid_argument("Malformed Assembly Hole participant");
        AssemblyHoleParticipantRole role;
        if (p.at("role") == "clearance")
          role = AssemblyHoleParticipantRole::Clearance;
        else if (p.at("role") == "threaded")
          role = AssemblyHoleParticipantRole::Threaded;
        else if (p.at("role") == "seat")
          role = AssemblyHoleParticipantRole::Seat;
        else
          throw std::invalid_argument("Unknown Assembly Hole participant role");
        AssemblyHoleDefinitionPolicy policy;
        if (p.at("definition_policy") == "single_occurrence")
          policy = AssemblyHoleDefinitionPolicy::SingleOccurrence;
        else if (p.at("definition_policy") == "affect_shared_definition")
          policy = AssemblyHoleDefinitionPolicy::AffectSharedDefinition;
        else
          throw std::invalid_argument("Unknown Assembly Hole shared-definition policy");
        auto recipe = shared_hole_from_json(p.at("recipe"));
        if (!recipe)
          return Result<std::size_t>::failure(recipe.error());
        participants.push_back({p.at("id").get<std::string>(), occurrence(p.at("occurrence")),
                                ModelStateId(p.at("state").get<std::string>()), role, policy,
                                std::move(recipe.value())});
      }
      auto feature = AssemblyHoleFeature::create(FeatureId(v.at("id").get<std::string>()),
                                                 v.at("name").get<std::string>(), std::move(layout),
                                                 std::move(participants));
      if (!feature)
        return Result<std::size_t>::failure(feature.error());
      features.push_back(std::move(feature.value()));
    }
    return project.replace_assembly_hole_features(std::move(features));
  } catch (const std::exception& e) {
    return Result<std::size_t>::failure(Error::validation("project.assembly_holes", e.what()));
  }
}
} // namespace blcad::detail
