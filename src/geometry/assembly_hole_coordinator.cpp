#include "blcad/geometry/assembly_hole_coordinator.hpp"
#include "../core/hole_recipe_json.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/project_evaluation.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "project_evaluation_internal.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace blcad::geometry {
namespace {
constexpr double tolerance = 1e-6;
template <class T> T take(Result<T> result) {
  if (!result)
    throw std::runtime_error(result.error().object_id() + ": " + result.error().message());
  return std::move(result.value());
}
void require(bool value, const char* message) {
  if (!value)
    throw std::runtime_error(message);
}
ShapeCache geometry(const PartDocument& part, StepImportTopologyContext* imports,
                    std::optional<std::size_t> prefix = {}) {
  auto cache = take(ShapeCache::create(ShapeCacheId("assembly.hole." + part.id().value())));
  auto result =
      prefix ? GeometryRecomputeExecutor{imports}.execute_history_prefix(part, *prefix, cache)
             : GeometryRecomputeExecutor{imports}.execute_document(part, cache);
  (void)take(std::move(result));
  return cache;
}
HoleFeature placed(const HoleFeature& recipe, std::vector<HolePlacement> placements) {
  return take(HoleFeature::create(recipe.id(), recipe.name(), recipe.kind(), recipe.target_body(),
                                  recipe.support(), std::move(placements), recipe.diameter(),
                                  recipe.extent(), recipe.direction(), recipe.tip(),
                                  recipe.manufacturing(), recipe.thread()));
}
bool same(const HoleFeature& a, const HoleFeature& b) {
  return blcad::detail::shared_hole_to_json(a) == blcad::detail::shared_hole_to_json(b);
}
void install(PartDocument& part, HoleFeature feature) {
  const auto* old = part.find_hole_feature(feature.id());
  if (old && same(*old, feature))
    return;
  (void)take(old ? part.update_hole_feature(std::move(feature))
                 : part.add_hole_feature(std::move(feature)));
}
struct LayoutPoint {
  HolePlacementId placement;
  std::optional<PatternOccurrenceIndex> occurrence;
  HoleInstanceFrame frame;
};
std::vector<LayoutPoint> layout_points(const Project& project, const AssemblyHoleLayout& layout,
                                       StepImportTopologyContext* imports) {
  const auto* definition = project.find_part_document(layout.source.part);
  require(definition, "Assembly Hole source Part is missing");
  auto part = take(definition->evaluate_model_state(layout.state));
  auto cache = geometry(part, imports);
  const auto* source = part.find_hole_feature(layout.hole);
  require(source, "Assembly Hole source Hole is missing");
  std::vector<std::optional<PatternOccurrenceIndex>> occurrences{{}};
  if (layout.pattern) {
    occurrences.clear();
    const auto* linear = part.find_linear_pattern_feature(*layout.pattern);
    const auto* circular = part.find_circular_pattern_feature(*layout.pattern);
    require(linear || circular, "Assembly Hole source pattern is missing");
    auto placements = take(linear ? evaluate_pattern_placements(part, *linear)
                                  : evaluate_pattern_placements(part, *circular));
    for (const auto& placement : placements)
      if (!placement.skipped)
        occurrences.push_back(placement.index);
  }
  require(source->placements().size() * occurrences.size() <= pattern_placement_limit,
          "Assembly Hole expanded layout exceeds the shared placement limit");
  std::vector<LayoutPoint> result;
  for (const auto& occurrence : occurrences)
    for (const auto& placement : source->placements()) {
      std::optional<HolePatternOccurrence> pattern;
      if (occurrence)
        pattern = HolePatternOccurrence{*layout.pattern, *occurrence};
      auto reference = take(HoleInstanceReference::create(layout.hole, placement.id,
                                                          HoleReferenceRole::Axis, pattern));
      result.push_back(
          {placement.id, occurrence, take(HoleInstanceGeometry::resolve(part, reference, cache))});
    }
  require(!result.empty(), "Assembly Hole layout has no active placements");
  return result;
}
std::optional<std::size_t> input_prefix(const PartDocument& part, FeatureId hole) {
  const auto history = take(part.executable_history());
  for (std::size_t i = 0; i < history.size(); ++i)
    if (history[i].node_id == hole.value()) {
      require(history[i].state == PartFeatureState::Active,
              "Assembly Hole participant feature is suppressed");
      return i;
    }
  return {};
}
std::vector<HolePlacement> map_placements(const Project& project,
                                          const AssemblyHoleFeature& feature,
                                          const AssemblyHoleParticipant& participant,
                                          StepImportTopologyContext* imports) {
  const auto* primary = project.find_part_document(participant.occurrence.part);
  require(primary, "Assembly Hole participant Part is missing");
  auto target = take(primary->evaluate_model_state(participant.state));
  auto prefix = input_prefix(target, participant.recipe.id());
  auto cache = geometry(target, imports, prefix);
  const auto plane = take(GeometryRecomputeExecutor{imports}.resolve_hole_support(
      target, participant.recipe.support(), cache, participant.recipe.id(), prefix));
  const auto mapping =
      take(project_occurrence_mapping(project, feature.layout().source, participant.occurrence));
  std::vector<HolePlacement> result;
  for (const auto& point : layout_points(project, feature.layout(), imports)) {
    const auto origin = mapping.transform_point(point.frame.frame.origin);
    const auto axis = normalized(mapping.transform_vector(point.frame.frame.normal));
    const auto denominator = dot(axis, plane.normal);
    require(std::abs(std::abs(denominator) - 1) <= 1e-8,
            "Assembly Hole axes must be normal to the explicit participant support");
    const Vector3 delta{plane.origin.x - origin.x, plane.origin.y - origin.y,
                        plane.origin.z - origin.z};
    const auto distance = dot(delta, plane.normal) / denominator;
    const Point3 intersection{origin.x + distance * axis.x, origin.y + distance * axis.y,
                              origin.z + distance * axis.z};
    const Vector3 local{intersection.x - plane.origin.x, intersection.y - plane.origin.y,
                        intersection.z - plane.origin.z};
    result.push_back({assembly_hole_placement_id(point.placement, point.occurrence),
                      Point2{dot(local, plane.x_axis), dot(local, plane.y_axis)}});
  }
  return result;
}
std::vector<DocumentId> changed_parts(const Project& before, const Project& after) {
  std::vector<DocumentId> changed;
  for (const auto& part : after.part_documents()) {
    const auto* old = before.find_part_document(part.id());
    if (!old ||
        take(serialize_part_document_to_json(*old)) != take(serialize_part_document_to_json(part)))
      changed.push_back(part.id());
  }
  return changed;
}
} // namespace

HolePlacementId assembly_hole_placement_id(HolePlacementId placement,
                                           std::optional<PatternOccurrenceIndex> occurrence) {
  if (!occurrence)
    return placement;
  return HolePlacementId("assembly-layout/" + std::to_string(placement.value().size()) + ":" +
                         placement.value() + "/" + pattern_occurrence_key(*occurrence));
}
namespace detail {
Result<Project> stage_assembly_holes(const Project& project, DocumentId part,
                                     StepImportTopologyContext* imports) {
  try {
    Project candidate = project;
    for (const auto& feature : candidate.assembly_hole_features())
      for (const auto& participant : feature.participants()) {
        if (participant.occurrence.part != part)
          continue;
        auto* target = candidate.find_part_document(part);
        require(target, "Assembly Hole participant Part is missing");
        if (participant.occurrence.part == feature.layout().source.part &&
            participant.recipe.id() == feature.layout().hole) {
          require(participant.occurrence == feature.layout().source,
                  "The source Hole must use its declared layout occurrence");
          const auto* source = target->find_hole_feature(feature.layout().hole);
          require(source, "Assembly Hole source recipe is missing");
          install(*target, placed(participant.recipe, source->placements()));
        } else {
          auto mapped = map_placements(candidate, feature, participant, imports);
          install(*target, placed(participant.recipe, std::move(mapped)));
        }
        auto effective = take(target->evaluate_model_state(participant.state));
        (void)geometry(effective, imports);
      }
    return Result<Project>::success(std::move(candidate));
  } catch (const std::exception& e) {
    return Result<Project>::failure(Error::geometry("assembly.hole", e.what()));
  }
}
Result<std::size_t> verify_assembly_holes(const Project& project,
                                          StepImportTopologyContext* imports) {
  try {
    std::size_t count = 0;
    for (const auto& feature : project.assembly_hole_features()) {
      const auto points = layout_points(project, feature.layout(), imports);
      for (const auto& participant : feature.participants()) {
        auto definition = take(project.find_part_document(participant.occurrence.part)
                                   ->evaluate_model_state(participant.state));
        const auto cache = geometry(definition, imports);
        const auto mapping = take(
            project_occurrence_mapping(project, feature.layout().source, participant.occurrence));
        for (const auto& point : points) {
          const bool source = participant.occurrence.part == feature.layout().source.part &&
                              participant.recipe.id() == feature.layout().hole;
          std::optional<HolePatternOccurrence> pattern;
          if (source && point.occurrence)
            pattern = HolePatternOccurrence{*feature.layout().pattern, *point.occurrence};
          const auto placement =
              source ? point.placement
                     : assembly_hole_placement_id(point.placement, point.occurrence);
          const auto reference = take(HoleInstanceReference::create(
              participant.recipe.id(), placement, HoleReferenceRole::Axis, pattern));
          const auto actual = take(HoleInstanceGeometry::resolve(definition, reference, cache));
          const auto origin = mapping.transform_point(point.frame.frame.origin);
          const auto axis = normalized(mapping.transform_vector(point.frame.frame.normal));
          const Vector3 delta{actual.frame.origin.x - origin.x, actual.frame.origin.y - origin.y,
                              actual.frame.origin.z - origin.z};
          require(length(cross(delta, axis)) <= tolerance &&
                      std::abs(std::abs(dot(axis, actual.frame.normal)) - 1) <= 1e-8,
                  "Assembly Hole participant axis moved after downstream replay or Assembly solve");
          ++count;
        }
      }
    }
    return Result<std::size_t>::success(count);
  } catch (const std::exception& e) {
    return Result<std::size_t>::failure(Error::geometry("assembly.hole", e.what()));
  }
}
} // namespace detail

Result<CoordinatedAssemblyHoleEvaluation>
AssemblyHoleCoordinator::evaluate(const Project& project) const {
  auto candidate = evaluate_project_geometry(project, imports_);
  if (!candidate)
    return Result<CoordinatedAssemblyHoleEvaluation>::failure(candidate.error());
  auto changed = changed_parts(project, candidate.value());
  return Result<CoordinatedAssemblyHoleEvaluation>::success(
      {std::move(candidate.value()), std::move(changed)});
}
Result<CoordinatedAssemblyHoleEvaluation>
AssemblyHoleCoordinator::bind(const Project& project, AssemblyHoleFeature feature) const {
  try {
    Project candidate = project;
    auto definitions = candidate.assembly_hole_features();
    const auto existing = std::find_if(definitions.begin(), definitions.end(),
                                       [&](const auto& old) { return old.id() == feature.id(); });
    if (existing != definitions.end()) {
      for (const auto& old : existing->participants()) {
        const bool retained = std::any_of(feature.participants().begin(),
                                          feature.participants().end(), [&](const auto& next) {
                                            return next.occurrence.part == old.occurrence.part &&
                                                   next.recipe.id() == old.recipe.id();
                                          });
        const bool source = old.occurrence.part == existing->layout().source.part &&
                            old.recipe.id() == existing->layout().hole;
        if (!retained && !source)
          (void)take(candidate.find_part_document(old.occurrence.part)
                         ->remove_hole_feature(old.recipe.id()));
      }
      *existing = feature;
    } else
      definitions.push_back(feature);
    // A recipe edit is explicit here. Ordinary child edits cannot silently
    // rewrite an Assembly-owned Hole; Core compares its declared recipe.
    for (const auto& participant : feature.participants()) {
      auto* part = candidate.find_part_document(participant.occurrence.part);
      require(part && !part->is_reference_part(), "Assembly Hole participant must be editable");
      const auto* old = part->find_hole_feature(participant.recipe.id());
      install(*part, placed(participant.recipe,
                            old ? old->placements() : participant.recipe.placements()));
    }
    (void)take(candidate.replace_assembly_hole_features(std::move(definitions)));
    auto evaluated = take(evaluate_project_geometry(candidate, imports_));
    auto changed = changed_parts(project, evaluated);
    return Result<CoordinatedAssemblyHoleEvaluation>::success(
        {std::move(evaluated), std::move(changed)});
  } catch (const std::exception& e) {
    return Result<CoordinatedAssemblyHoleEvaluation>::failure(
        Error::geometry("assembly.hole", e.what()));
  }
}
Result<Project> AssemblyHoleCoordinator::detach(const Project& project, FeatureId id) const {
  auto evaluated = evaluate_project_geometry(project, imports_);
  if (!evaluated)
    return Result<Project>::failure(evaluated.error());
  Project candidate = std::move(evaluated.value());
  auto features = candidate.assembly_hole_features();
  const auto erased =
      std::erase_if(features, [&](const auto& feature) { return feature.id() == id; });
  if (!erased)
    return Result<Project>::failure(Error::validation(id.value(), "Assembly Hole does not exist"));
  auto valid = candidate.replace_assembly_hole_features(std::move(features));
  if (!valid)
    return Result<Project>::failure(valid.error());
  return Result<Project>::success(std::move(candidate));
}
} // namespace blcad::geometry
