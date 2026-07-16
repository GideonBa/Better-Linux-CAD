#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

enum class SketchConstraintIntentSource { Manual, Automatic };

[[nodiscard]] constexpr std::string_view
to_string(SketchConstraintIntentSource source) noexcept {
  switch (source) {
  case SketchConstraintIntentSource::Manual: return "manual";
  case SketchConstraintIntentSource::Automatic: return "automatic";
  }
  return "manual";
}

enum class SketchConstraintIntentTargetKind { Point, Entity };

[[nodiscard]] constexpr std::string_view
to_string(SketchConstraintIntentTargetKind kind) noexcept {
  switch (kind) {
  case SketchConstraintIntentTargetKind::Point: return "point";
  case SketchConstraintIntentTargetKind::Entity: return "entity";
  }
  return "point";
}

class SketchConstraintIntentTarget {
public:
  [[nodiscard]] static Result<SketchConstraintIntentTarget> point(SketchPointId id) {
    if (id.empty())
      return Result<SketchConstraintIntentTarget>::failure(
          Error::validation("sketch_constraint_target", "constraint point id must not be empty"));
    return Result<SketchConstraintIntentTarget>::success(
        SketchConstraintIntentTarget(SketchConstraintIntentTargetKind::Point, id.value()));
  }

  [[nodiscard]] static Result<SketchConstraintIntentTarget> entity(std::string id) {
    if (id.empty())
      return Result<SketchConstraintIntentTarget>::failure(
          Error::validation("sketch_constraint_target", "constraint entity id must not be empty"));
    return Result<SketchConstraintIntentTarget>::success(
        SketchConstraintIntentTarget(SketchConstraintIntentTargetKind::Entity, std::move(id)));
  }

  [[nodiscard]] SketchConstraintIntentTargetKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::string& id() const noexcept { return id_; }

  friend bool operator==(const SketchConstraintIntentTarget&,
                         const SketchConstraintIntentTarget&) = default;

private:
  SketchConstraintIntentTarget(SketchConstraintIntentTargetKind kind, std::string id)
      : kind_(kind), id_(std::move(id)) {}

  SketchConstraintIntentTargetKind kind_;
  std::string id_;
};

class SketchConstraintIntent {
public:
  [[nodiscard]] static Result<SketchConstraintIntent>
  create(SketchConstraintId id, SketchSolverConstraintKind kind,
         std::vector<SketchConstraintIntentTarget> targets,
         SketchConstraintIntentSource source = SketchConstraintIntentSource::Manual) {
    const std::string object_id = id.empty() ? "sketch_constraint_intent" : id.value();
    if (id.empty())
      return Result<SketchConstraintIntent>::failure(
          Error::validation(object_id, "constraint intent id must not be empty"));
    if (!geometric_kind(kind))
      return Result<SketchConstraintIntent>::failure(Error::validation(
          object_id, "Block 114 accepts geometric constraints only; dimensions belong to Block 115"));
    if (targets.size() != expected_target_count(kind))
      return Result<SketchConstraintIntent>::failure(Error::validation(
          object_id, "constraint intent has the wrong number of semantic targets"));
    if (!target_signature_valid(kind, targets))
      return Result<SketchConstraintIntent>::failure(Error::validation(
          object_id, "constraint intent target kinds are incompatible with the selected family"));
    for (std::size_t index = 0U; index < targets.size(); ++index)
      for (std::size_t previous = 0U; previous < index; ++previous)
        if (targets[index] == targets[previous] && kind != SketchSolverConstraintKind::Fixed)
          return Result<SketchConstraintIntent>::failure(
              Error::validation(object_id, "constraint intent targets must be distinct"));
    return Result<SketchConstraintIntent>::success(
        SketchConstraintIntent(std::move(id), kind, std::move(targets), source));
  }

  [[nodiscard]] const SketchConstraintId& id() const noexcept { return id_; }
  [[nodiscard]] SketchSolverConstraintKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::vector<SketchConstraintIntentTarget>& targets() const noexcept {
    return targets_;
  }
  [[nodiscard]] SketchConstraintIntentSource source() const noexcept { return source_; }

  friend bool operator==(const SketchConstraintIntent&, const SketchConstraintIntent&) = default;

private:
  SketchConstraintIntent(SketchConstraintId id, SketchSolverConstraintKind kind,
                         std::vector<SketchConstraintIntentTarget> targets,
                         SketchConstraintIntentSource source)
      : id_(std::move(id)), kind_(kind), targets_(std::move(targets)), source_(source) {}

  [[nodiscard]] static bool geometric_kind(SketchSolverConstraintKind kind) noexcept {
    switch (kind) {
    case SketchSolverConstraintKind::Coincident:
    case SketchSolverConstraintKind::Fixed:
    case SketchSolverConstraintKind::Horizontal:
    case SketchSolverConstraintKind::Vertical:
    case SketchSolverConstraintKind::Parallel:
    case SketchSolverConstraintKind::Perpendicular:
    case SketchSolverConstraintKind::Collinear:
    case SketchSolverConstraintKind::Equal:
    case SketchSolverConstraintKind::Tangent:
    case SketchSolverConstraintKind::Concentric:
    case SketchSolverConstraintKind::Midpoint:
    case SketchSolverConstraintKind::Symmetric:
    case SketchSolverConstraintKind::PointOnObject: return true;
    case SketchSolverConstraintKind::HorizontalDistance:
    case SketchSolverConstraintKind::VerticalDistance:
    case SketchSolverConstraintKind::AlignedDistance:
    case SketchSolverConstraintKind::Radial:
    case SketchSolverConstraintKind::Diameter:
    case SketchSolverConstraintKind::Angular: return false;
    }
    return false;
  }

  [[nodiscard]] static std::size_t expected_target_count(
      SketchSolverConstraintKind kind) noexcept {
    switch (kind) {
    case SketchSolverConstraintKind::Fixed:
    case SketchSolverConstraintKind::Horizontal:
    case SketchSolverConstraintKind::Vertical: return 1U;
    case SketchSolverConstraintKind::Symmetric: return 3U;
    default: return 2U;
    }
  }

  [[nodiscard]] static bool target_signature_valid(
      SketchSolverConstraintKind kind,
      const std::vector<SketchConstraintIntentTarget>& targets) noexcept {
    const auto point = [&targets](std::size_t index) {
      return targets[index].kind() == SketchConstraintIntentTargetKind::Point;
    };
    const auto entity = [&targets](std::size_t index) {
      return targets[index].kind() == SketchConstraintIntentTargetKind::Entity;
    };
    switch (kind) {
    case SketchSolverConstraintKind::Coincident: return point(0U) && point(1U);
    case SketchSolverConstraintKind::Fixed: return point(0U) || entity(0U);
    case SketchSolverConstraintKind::Horizontal:
    case SketchSolverConstraintKind::Vertical: return entity(0U);
    case SketchSolverConstraintKind::Parallel:
    case SketchSolverConstraintKind::Perpendicular:
    case SketchSolverConstraintKind::Collinear:
    case SketchSolverConstraintKind::Equal:
    case SketchSolverConstraintKind::Tangent:
    case SketchSolverConstraintKind::Concentric: return entity(0U) && entity(1U);
    case SketchSolverConstraintKind::Midpoint:
    case SketchSolverConstraintKind::PointOnObject: return point(0U) && entity(1U);
    case SketchSolverConstraintKind::Symmetric:
      return point(0U) && point(1U) && entity(2U);
    default: return false;
    }
  }

  SketchConstraintId id_;
  SketchSolverConstraintKind kind_;
  std::vector<SketchConstraintIntentTarget> targets_;
  SketchConstraintIntentSource source_;
};

class SketchConstraintCatalog {
public:
  [[nodiscard]] static Result<SketchConstraintCatalog>
  create(DocumentId document, SketchId sketch,
         std::vector<SketchConstraintIntent> constraints = {}) {
    const std::string object_id = sketch.empty() ? "sketch_constraint_catalog" : sketch.value();
    if (document.empty() || sketch.empty())
      return Result<SketchConstraintCatalog>::failure(Error::validation(
          object_id, "constraint catalog requires document and Sketch ids"));
    std::sort(constraints.begin(), constraints.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
    for (std::size_t index = 1U; index < constraints.size(); ++index)
      if (constraints[index - 1U].id() == constraints[index].id())
        return Result<SketchConstraintCatalog>::failure(Error::validation(
            constraints[index].id().value(), "constraint catalog ids must be unique"));
    return Result<SketchConstraintCatalog>::success(
        SketchConstraintCatalog(std::move(document), std::move(sketch), std::move(constraints)));
  }

  [[nodiscard]] const DocumentId& document() const noexcept { return document_; }
  [[nodiscard]] const SketchId& sketch() const noexcept { return sketch_; }
  [[nodiscard]] const std::vector<SketchConstraintIntent>& constraints() const noexcept {
    return constraints_;
  }
  [[nodiscard]] const SketchConstraintIntent* find(SketchConstraintId id) const noexcept {
    const auto found = std::lower_bound(
        constraints_.begin(), constraints_.end(), id.value(),
        [](const auto& constraint, std::string_view value) {
          return constraint.id().value() < value;
        });
    return found != constraints_.end() && found->id() == id ? &*found : nullptr;
  }
  [[nodiscard]] Result<std::size_t> add(SketchConstraintIntent constraint) {
    if (find(constraint.id()) != nullptr)
      return Result<std::size_t>::failure(
          Error::validation(constraint.id().value(), "constraint intent id already exists"));
    constraints_.push_back(std::move(constraint));
    std::sort(constraints_.begin(), constraints_.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
    return Result<std::size_t>::success(constraints_.size());
  }
  [[nodiscard]] Result<std::size_t> remove(SketchConstraintId id) {
    const auto found = std::find_if(constraints_.begin(), constraints_.end(),
                                    [&id](const auto& value) { return value.id() == id; });
    if (found == constraints_.end())
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "constraint intent does not exist"));
    constraints_.erase(found);
    return Result<std::size_t>::success(constraints_.size());
  }

  friend bool operator==(const SketchConstraintCatalog&, const SketchConstraintCatalog&) = default;

private:
  SketchConstraintCatalog(DocumentId document, SketchId sketch,
                          std::vector<SketchConstraintIntent> constraints)
      : document_(std::move(document)), sketch_(std::move(sketch)),
        constraints_(std::move(constraints)) {}

  DocumentId document_;
  SketchId sketch_;
  std::vector<SketchConstraintIntent> constraints_;
};

struct SketchConstraintAuthoringPreview {
  SketchConstraintIntent candidate;
  SketchConstraintCatalog catalog_after;
  SketchSolveResult solve;
  std::optional<Sketch> solved_sketch;
  bool accepted{false};
  std::vector<std::string> conflict_ids;
  std::vector<std::string> redundant_ids;
};

class SketchConstraintAuthoringService {
public:
  [[nodiscard]] Result<SketchConstraintAuthoringPreview>
  preview(const PartDocument& document, const SketchConstraintCatalog& catalog,
          const SketchConstraintIntent& candidate) const {
    if (document.id() != catalog.document())
      return Result<SketchConstraintAuthoringPreview>::failure(Error::validation(
          catalog.document().value(), "constraint catalog belongs to another Part document"));
    const Sketch* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<SketchConstraintAuthoringPreview>::failure(
          Error::validation(catalog.sketch().value(), "constraint catalog Sketch does not exist"));
    if (catalog.find(candidate.id()) != nullptr)
      return Result<SketchConstraintAuthoringPreview>::failure(
          Error::validation(candidate.id().value(), "candidate constraint id already exists"));

    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<SketchConstraintAuthoringPreview>::failure(topology.error());
    auto legacy = SketchConstraintSystemBuilder::from_legacy(topology.value(), *sketch, document);
    if (legacy.has_error())
      return Result<SketchConstraintAuthoringPreview>::failure(legacy.error());

    std::vector<SketchSolverConstraint> constraints = legacy.value().constraints();
    for (const auto& intent : catalog.constraints()) {
      auto converted = solver_constraint(intent, topology.value());
      if (converted.has_error())
        return Result<SketchConstraintAuthoringPreview>::failure(converted.error());
      constraints.push_back(std::move(converted.value()));
    }
    auto converted_candidate = solver_constraint(candidate, topology.value());
    if (converted_candidate.has_error())
      return Result<SketchConstraintAuthoringPreview>::failure(converted_candidate.error());
    constraints.push_back(std::move(converted_candidate.value()));

    auto system = SketchConstraintSystem::create(sketch->id(), std::move(constraints));
    if (system.has_error())
      return Result<SketchConstraintAuthoringPreview>::failure(system.error());
    auto solved = SketchConstraintSolver{}.solve(topology.value(), system.value());
    if (solved.has_error())
      return Result<SketchConstraintAuthoringPreview>::failure(solved.error());

    SketchConstraintCatalog after = catalog;
    auto added = after.add(candidate);
    if (added.has_error())
      return Result<SketchConstraintAuthoringPreview>::failure(added.error());

    const bool accepted = solved.value().status == SketchSolveStatus::UnderConstrained ||
                          solved.value().status == SketchSolveStatus::FullyConstrained;
    std::optional<Sketch> solved_sketch;
    if (accepted) {
      auto materialized =
          SketchTopologyLegacyMaterializer{}.materialize(*sketch, solved.value().topology);
      if (materialized.has_error())
        return Result<SketchConstraintAuthoringPreview>::failure(materialized.error());
      solved_sketch = std::move(materialized.value());
    }
    auto conflicts = strip_intent_prefixes(solved.value().conflicting_constraint_ids);
    auto redundant = strip_intent_prefixes(solved.value().redundant_constraint_ids);
    SketchSolveResult solve_result = std::move(solved.value());

    return Result<SketchConstraintAuthoringPreview>::success(
        {candidate, std::move(after), std::move(solve_result), std::move(solved_sketch), accepted,
         std::move(conflicts), std::move(redundant)});
  }

private:
  [[nodiscard]] static std::string solver_id(const SketchConstraintId& id) {
    return "intent/" + id.value();
  }

  [[nodiscard]] static Result<SketchSolverConstraint>
  solver_constraint(const SketchConstraintIntent& intent, const SketchTopology& topology) {
    std::vector<SketchSolverTarget> targets;
    targets.reserve(intent.targets().size());
    for (const auto& target : intent.targets()) {
      if (target.kind() == SketchConstraintIntentTargetKind::Point) {
        if (topology.find_point(SketchPointId(target.id())) == nullptr)
          return Result<SketchSolverConstraint>::failure(Error::validation(
              intent.id().value(), "constraint intent references an unknown Sketch point"));
        auto converted = SketchSolverTarget::point(SketchPointId(target.id()));
        if (converted.has_error())
          return Result<SketchSolverConstraint>::failure(converted.error());
        targets.push_back(std::move(converted.value()));
      } else {
        if (topology.find_entity(target.id()) == nullptr)
          return Result<SketchSolverConstraint>::failure(Error::validation(
              intent.id().value(), "constraint intent references an unknown Sketch entity"));
        auto converted = SketchSolverTarget::entity(target.id());
        if (converted.has_error())
          return Result<SketchSolverConstraint>::failure(converted.error());
        targets.push_back(std::move(converted.value()));
      }
    }
    return SketchSolverConstraint::create(solver_id(intent.id()), intent.kind(), std::move(targets));
  }

  [[nodiscard]] static std::vector<std::string>
  strip_intent_prefixes(const std::vector<std::string>& ids) {
    std::vector<std::string> result;
    result.reserve(ids.size());
    constexpr std::string_view prefix = "intent/";
    for (const auto& id : ids)
      result.push_back(std::string_view(id).starts_with(prefix)
                           ? id.substr(prefix.size())
                           : id);
    return result;
  }
};

} // namespace blcad
