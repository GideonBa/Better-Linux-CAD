#pragma once

#include "blcad/core/sketch_dimension_catalog_system.hpp"
#include "blcad/core/sketch_modify.hpp"
#include "blcad/core/sketch_topology.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_corner_constraints.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

struct GuiSketchModifyPreview {
  Sketch candidate;
  std::vector<std::string> invalidated_ids;
  std::vector<std::string> invalidated_profile_ids;
  std::vector<SketchModifyTrimAttachment> attachments;
  std::vector<SketchModifyInvalidatedEndpoint> invalidated_endpoints;
};

// Block 116 headless GUI-layer controller for trim/extend/split/fillet/chamfer.
// Each operation previews a disposable candidate Sketch; commit re-derives the
// same operation from the current committed document and publishes exactly one
// atomic transaction. The Sketch and its Block-114/115 intent catalogs are
// published together: intents whose point/entity target disappeared are pruned,
// while constraints targeting an entity that survived an in-place edit remain.
class GuiSketchModifyController {
public:
  [[nodiscard]] static Result<GuiSketchModifyController> create(const GuiDocumentSession& session,
                                                                SketchId sketch) {
    if (session.part_document() == nullptr)
      return Result<GuiSketchModifyController>::failure(
          Error::validation(sketch.value(), "sketch modify requires an active Part session"));
    const Sketch* source = session.part_document()->find_sketch(sketch);
    if (source == nullptr)
      return Result<GuiSketchModifyController>::failure(
          Error::validation(sketch.value(), "sketch modify requires an existing Sketch"));
    return Result<GuiSketchModifyController>::success(
        GuiSketchModifyController(*session.part_document(), *source));
  }

  [[nodiscard]] const Sketch& source_sketch() const noexcept {
    return source_;
  }
  [[nodiscard]] const std::string& pending_label() const noexcept {
    return label_;
  }
  [[nodiscard]] bool has_preview() const noexcept {
    return static_cast<bool>(operation_);
  }

  [[nodiscard]] Result<GuiSketchModifyPreview> trim(SketchEntityId target, Point2 pick) {
    return set_operation("Trim sketch geometry",
                         [target, pick](const PartDocument& document, const Sketch& sketch) {
                           return SketchModifyService::trim(document, sketch, target, pick);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> trim(ProfileId target, Point2 pick) {
    return set_operation("Trim sketch geometry",
                         [target, pick](const PartDocument& document, const Sketch& sketch) {
                           return SketchModifyService::trim(document, sketch, target, pick);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> extend(SketchEntityId target, Point2 pick) {
    return set_operation("Extend sketch geometry",
                         [target, pick](const PartDocument&, const Sketch& sketch) {
                           return SketchModifyService::extend(sketch, target, pick);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> split(SketchEntityId target, Point2 point) {
    return set_operation("Split sketch geometry",
                         [target, point](const PartDocument&, const Sketch& sketch) {
                           return SketchModifyService::split(sketch, target, point);
                         });
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> fillet(SketchEntityId first, SketchEntityId second,
                                                      double radius, bool keep_trim = true) {
    auto preview = set_operation("Fillet sketch corner", [first, second, radius, keep_trim](
                                                     const PartDocument&, const Sketch& sketch) {
      return SketchModifyService::fillet(sketch, first, second, radius, keep_trim);
    });
    if (preview && keep_trim) { corner_targets_ = std::pair{first, second}; corner_fillet_ = true; }
    return preview;
  }
  [[nodiscard]] Result<GuiSketchModifyPreview> chamfer(SketchEntityId first, SketchEntityId second,
                                                       double distance, bool keep_trim = true) {
    auto preview = set_operation("Chamfer sketch corner", [first, second, distance, keep_trim](
                                                      const PartDocument&, const Sketch& sketch) {
      return SketchModifyService::chamfer(sketch, first, second, distance, keep_trim);
    });
    if (preview && keep_trim) { corner_targets_ = std::pair{first, second}; corner_fillet_ = false; }
    return preview;
  }

  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session, bool constrain_corner = false) const {
    if (!operation_)
      return Result<std::size_t>::failure(
          Error::validation(source_.id().value(), "no sketch modification is prepared to commit"));
    if (session.part_document() == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          source_.id().value(), "sketch modify requires an active Part document"));
    const SketchId sketch_id = source_.id();
    const Operation operation = operation_;
    const auto corner_targets = constrain_corner ? corner_targets_ : std::nullopt;
    const bool corner_fillet = corner_fillet_;
    return session.commit_part_sketch_intent_transaction(
        label_,
        [sketch_id, operation, corner_targets, corner_fillet](
            PartDocument& document, std::vector<SketchConstraintCatalog>& constraint_catalogs,
            std::vector<SketchDimensionCatalog>& dimension_catalogs) -> Result<std::size_t> {
          const Sketch* current = document.find_sketch(sketch_id);
          if (current == nullptr)
            return Result<std::size_t>::failure(
                Error::validation(sketch_id.value(), "sketch modify target no longer exists"));
          const Sketch before = *current;
          auto modified = operation(document, before);
          if (modified.has_error())
            return Result<std::size_t>::failure(modified.error());
          const std::vector<std::string> invalidated_ids = modified.value().invalidated_ids;
          const std::vector<std::string> invalidated_profile_ids =
              modified.value().invalidated_profile_ids;
          const std::vector<SketchModifyTrimAttachment> attachments = modified.value().attachments;
          const std::vector<SketchModifyInvalidatedEndpoint> invalidated_endpoints =
              modified.value().invalidated_endpoints;
          auto source_topology = SketchTopology::migrate_legacy(before);
          if (source_topology.has_error())
            return Result<std::size_t>::failure(source_topology.error());
          auto topology = SketchTopology::migrate_legacy(modified.value().sketch);
          if (topology.has_error())
            return Result<std::size_t>::failure(topology.error());
          auto updated = document.update_sketch(std::move(modified.value().sketch));
          if (updated.has_error())
            return updated;
          const auto invalidated_points = invalidated_point_ids(
              source_topology.value(), invalidated_ids, invalidated_endpoints);
          auto pruned = prune_missing_intents(
              document.id(), sketch_id, topology.value(), invalidated_ids, invalidated_profile_ids,
              invalidated_points, constraint_catalogs, dimension_catalogs);
          if (pruned.has_error())
            return pruned;
          std::size_t automatically_constrained = 0U;
          if (!attachments.empty()) {
            auto constrained =
                add_trim_constraints(document, sketch_id, topology.value(), attachments,
                                     constraint_catalogs, dimension_catalogs);
            if (constrained.has_error())
              return constrained;
            automatically_constrained = constrained.value();
          }
          if (corner_targets) {
            auto authored = author_sketch_corner(document, before, corner_targets->first,
                corner_targets->second, corner_fillet, constraint_catalogs, dimension_catalogs);
            if (!authored) return authored;
            automatically_constrained += authored.value();
          }
          return Result<std::size_t>::success(updated.value() + pruned.value() +
                                              automatically_constrained);
        });
  }

private:
  using Operation = std::function<Result<SketchModifyResult>(const PartDocument&, const Sketch&)>;

  GuiSketchModifyController(PartDocument source_document, Sketch source)
      : source_document_(std::move(source_document)), source_(std::move(source)) {}

  [[nodiscard]] static std::optional<SketchPointId>
  endpoint_point(const SketchTopology& topology, std::string_view entity_id,
                 SketchModifyEndSelector endpoint) {
    const auto* entity = topology.find_entity(entity_id);
    if (entity == nullptr || entity->points().size() < 2U)
      return std::nullopt;
    return endpoint == SketchModifyEndSelector::Start
               ? std::optional<SketchPointId>(entity->points().front())
               : std::optional<SketchPointId>(entity->points().back());
  }

  [[nodiscard]] static std::vector<SketchPointId>
  invalidated_point_ids(const SketchTopology& source_topology,
                        const std::vector<std::string>& invalidated_entities,
                        const std::vector<SketchModifyInvalidatedEndpoint>& invalidated_endpoints) {
    std::vector<SketchPointId> result;
    const auto append = [&result](const SketchPointId& point) {
      if (std::find(result.begin(), result.end(), point) == result.end())
        result.push_back(point);
    };
    for (const std::string& id : invalidated_entities) {
      const auto* entity = source_topology.find_entity("entity/" + id);
      if (entity == nullptr)
        continue;
      for (const SketchPointId& point : entity->points())
        append(point);
    }
    for (const SketchModifyInvalidatedEndpoint& invalidated : invalidated_endpoints) {
      const auto point = endpoint_point(source_topology, "entity/" + invalidated.entity.value(),
                                        invalidated.endpoint);
      if (point)
        append(*point);
    }
    std::sort(result.begin(), result.end(),
              [](const auto& left, const auto& right) { return left.value() < right.value(); });
    return result;
  }

  [[nodiscard]] static bool
  same_relationship(const SketchConstraintIntent& existing, SketchSolverConstraintKind kind,
                    const std::vector<SketchConstraintIntentTarget>& targets) noexcept {
    if (existing.kind() != kind || existing.targets().size() != targets.size())
      return false;
    if (existing.targets() == targets)
      return true;
    return kind == SketchSolverConstraintKind::Coincident && targets.size() == 2U &&
           existing.targets()[0] == targets[1] && existing.targets()[1] == targets[0];
  }

  [[nodiscard]] static bool
  relationship_exists(const SketchConstraintCatalog& catalog, SketchSolverConstraintKind kind,
                      const std::vector<SketchConstraintIntentTarget>& targets) noexcept {
    return std::any_of(
        catalog.constraints().begin(), catalog.constraints().end(),
        [&](const auto& intent) { return same_relationship(intent, kind, targets); });
  }

  [[nodiscard]] static SketchConstraintId
  next_trim_constraint_id(const SketchConstraintCatalog& catalog) {
    constexpr std::string_view prefix = "constraint.auto.trim.";
    for (std::size_t index = 1U;; ++index) {
      SketchConstraintId candidate(std::string(prefix) + std::to_string(index));
      if (catalog.find(candidate) == nullptr)
        return candidate;
    }
  }

  [[nodiscard]] static Result<std::size_t>
  add_trim_constraints(PartDocument& document, const SketchId& sketch,
                       const SketchTopology& trimmed_topology,
                       std::vector<SketchModifyTrimAttachment> attachments,
                       std::vector<SketchConstraintCatalog>& constraint_catalogs,
                       const std::vector<SketchDimensionCatalog>& dimension_catalogs) {
    auto constraint_entry =
        std::find_if(constraint_catalogs.begin(), constraint_catalogs.end(),
                     [&](const auto& catalog) { return catalog.sketch() == sketch; });
    auto empty_constraints = SketchConstraintCatalog::create(document.id(), sketch);
    if (empty_constraints.has_error())
      return Result<std::size_t>::failure(empty_constraints.error());
    SketchConstraintCatalog candidate_catalog = constraint_entry == constraint_catalogs.end()
                                                    ? std::move(empty_constraints.value())
                                                    : *constraint_entry;

    const auto dimension_entry =
        std::find_if(dimension_catalogs.begin(), dimension_catalogs.end(),
                     [&](const auto& catalog) { return catalog.sketch() == sketch; });
    auto empty_dimensions = SketchDimensionCatalog::create(document.id(), sketch);
    if (empty_dimensions.has_error())
      return Result<std::size_t>::failure(empty_dimensions.error());
    const SketchDimensionCatalog dimensions = dimension_entry == dimension_catalogs.end()
                                                  ? std::move(empty_dimensions.value())
                                                  : *dimension_entry;

    std::sort(attachments.begin(), attachments.end(), [](const auto& left, const auto& right) {
      if (left.result_entity != right.result_entity)
        return left.result_entity.value() < right.result_entity.value();
      if (left.result_endpoint != right.result_endpoint)
        return static_cast<int>(left.result_endpoint) < static_cast<int>(right.result_endpoint);
      if (left.cutter_entity != right.cutter_entity)
        return left.cutter_entity < right.cutter_entity;
      if (left.cutter_endpoint.has_value() != right.cutter_endpoint.has_value())
        return left.cutter_endpoint.has_value();
      return static_cast<int>(left.cutter_endpoint.value_or(SketchModifyEndSelector::End)) <
             static_cast<int>(right.cutter_endpoint.value_or(SketchModifyEndSelector::End));
    });
    attachments.erase(std::unique(attachments.begin(), attachments.end()), attachments.end());

    std::size_t added_count = 0U;
    for (const SketchModifyTrimAttachment& attachment : attachments) {
      const std::string result_entity = "entity/" + attachment.result_entity.value();
      const auto result_point =
          endpoint_point(trimmed_topology, result_entity, attachment.result_endpoint);
      if (!result_point)
        return Result<std::size_t>::failure(Error::internal(
            result_entity, "trim attachment references an unknown result endpoint"));
      if (trimmed_topology.find_entity(attachment.cutter_entity) == nullptr)
        return Result<std::size_t>::failure(Error::internal(
            attachment.cutter_entity, "trim attachment references an unknown cutter entity"));
      if (attachment.cutter_entity == result_entity)
        continue;

      auto authored = SketchConstraintIntentTarget::point(*result_point);
      if (authored.has_error())
        return Result<std::size_t>::failure(authored.error());
      SketchSolverConstraintKind kind = SketchSolverConstraintKind::PointOnObject;
      Result<SketchConstraintIntentTarget> reference =
          SketchConstraintIntentTarget::entity(attachment.cutter_entity);
      if (attachment.cutter_endpoint) {
        const auto cutter_point =
            endpoint_point(trimmed_topology, attachment.cutter_entity, *attachment.cutter_endpoint);
        if (!cutter_point)
          return Result<std::size_t>::failure(Error::internal(
              attachment.cutter_entity, "trim attachment references an unknown cutter endpoint"));
        if (*result_point == *cutter_point)
          continue;
        kind = SketchSolverConstraintKind::Coincident;
        reference = SketchConstraintIntentTarget::point(*cutter_point);
      }
      if (reference.has_error())
        return Result<std::size_t>::failure(reference.error());
      std::vector<SketchConstraintIntentTarget> targets{authored.value(), reference.value()};
      if (relationship_exists(candidate_catalog, kind, targets))
        continue;

      auto intent =
          SketchConstraintIntent::create(next_trim_constraint_id(candidate_catalog), kind, targets,
                                         SketchConstraintIntentSource::Automatic);
      if (intent.has_error())
        return Result<std::size_t>::failure(intent.error());

      // Author each inferred relation through the same preview authority used
      // by the interactive constraint UI. Redundant/conflicting candidates are
      // intentionally skipped; a trim must never publish an invalid catalog.
      auto preview =
          SketchConstraintAuthoringService{}.preview(document, candidate_catalog, intent.value());
      if (preview.has_error())
        return Result<std::size_t>::failure(preview.error());
      if (!preview.value().accepted || !preview.value().solved_sketch)
        continue;

      // The Block-114 preview knows existing geometric constraints. Stage its
      // materialized result, then solve the combined Block-114/115 system so
      // driving dimensions participate before this inferred relation is kept.
      PartDocument staged = document;
      auto staged_update = staged.update_sketch(*preview.value().solved_sketch);
      if (staged_update.has_error())
        return Result<std::size_t>::failure(staged_update.error());
      auto combined = SketchDimensionCatalogSystemBuilder::solve(
          staged, preview.value().catalog_after, dimensions);
      if (combined.has_error())
        return Result<std::size_t>::failure(combined.error());
      if (!combined.value().accepted || !combined.value().solved_sketch)
        continue;

      auto materialized = document.update_sketch(*combined.value().solved_sketch);
      if (materialized.has_error())
        return Result<std::size_t>::failure(materialized.error());
      candidate_catalog = std::move(preview.value().catalog_after);
      ++added_count;
    }

    if (added_count != 0U) {
      if (constraint_entry == constraint_catalogs.end())
        constraint_catalogs.push_back(std::move(candidate_catalog));
      else
        *constraint_entry = std::move(candidate_catalog);
    }
    return Result<std::size_t>::success(added_count);
  }

  [[nodiscard]] static bool target_survives(const SketchTopology& topology,
                                            const SketchConstraintIntentTarget& target,
                                            const std::vector<std::string>& invalidated_ids,
                                            const std::vector<std::string>& invalidated_profile_ids,
                                            const std::vector<SketchPointId>& invalidated_points) {
    if (target.kind() == SketchConstraintIntentTargetKind::Point) {
      if (std::find(invalidated_points.begin(), invalidated_points.end(),
                    SketchPointId(target.id())) != invalidated_points.end())
        return false;
      const bool invalidated_entity_point =
          std::any_of(invalidated_ids.begin(), invalidated_ids.end(), [&](const auto& id) {
            const std::string prefix = "entity/" + id + "/";
            return target.id().starts_with(prefix);
          });
      const bool invalidated_profile_point = std::any_of(
          invalidated_profile_ids.begin(), invalidated_profile_ids.end(), [&](const auto& id) {
            const std::string prefix = "profile/" + id + "/";
            return target.id().starts_with(prefix);
          });
      if (invalidated_entity_point || invalidated_profile_point)
        return false;
      return topology.find_point(SketchPointId(target.id())) != nullptr;
    }
    const bool invalidated_entity =
        std::any_of(invalidated_ids.begin(), invalidated_ids.end(),
                    [&](const auto& id) { return target.id() == "entity/" + id; });
    const bool invalidated_profile =
        std::any_of(invalidated_profile_ids.begin(), invalidated_profile_ids.end(),
                    [&](const auto& id) { return target.id() == "profile/" + id; });
    if (invalidated_entity || invalidated_profile)
      return false;
    return topology.find_entity(target.id()) != nullptr;
  }

  [[nodiscard]] static Result<std::size_t>
  prune_missing_intents(const DocumentId& document, const SketchId& sketch,
                        const SketchTopology& topology,
                        const std::vector<std::string>& invalidated_ids,
                        const std::vector<std::string>& invalidated_profile_ids,
                        const std::vector<SketchPointId>& invalidated_points,
                        std::vector<SketchConstraintCatalog>& constraint_catalogs,
                        std::vector<SketchDimensionCatalog>& dimension_catalogs) {
    std::size_t removed = 0U;
    for (auto& catalog : constraint_catalogs) {
      if (catalog.sketch() != sketch)
        continue;
      std::vector<SketchConstraintIntent> surviving;
      surviving.reserve(catalog.constraints().size());
      for (const auto& intent : catalog.constraints()) {
        const bool valid =
            std::all_of(intent.targets().begin(), intent.targets().end(), [&](const auto& target) {
              return target_survives(topology, target, invalidated_ids, invalidated_profile_ids,
                                     invalidated_points);
            });
        if (valid)
          surviving.push_back(intent);
        else
          ++removed;
      }
      auto rebuilt = SketchConstraintCatalog::create(document, sketch, std::move(surviving));
      if (rebuilt.has_error())
        return Result<std::size_t>::failure(rebuilt.error());
      catalog = std::move(rebuilt.value());
    }
    for (auto& catalog : dimension_catalogs) {
      if (catalog.sketch() != sketch)
        continue;
      std::vector<SketchDimensionIntent> surviving;
      surviving.reserve(catalog.dimensions().size());
      for (const auto& intent : catalog.dimensions()) {
        const bool valid =
            std::all_of(intent.targets().begin(), intent.targets().end(), [&](const auto& target) {
              return target_survives(topology, target, invalidated_ids, invalidated_profile_ids,
                                     invalidated_points);
            });
        if (valid)
          surviving.push_back(intent);
        else
          ++removed;
      }
      auto rebuilt = SketchDimensionCatalog::create(document, sketch, std::move(surviving));
      if (rebuilt.has_error())
        return Result<std::size_t>::failure(rebuilt.error());
      catalog = std::move(rebuilt.value());
    }
    return Result<std::size_t>::success(removed);
  }

  [[nodiscard]] Result<GuiSketchModifyPreview> set_operation(std::string label,
                                                             Operation operation) {
    corner_targets_.reset();
    auto preview = operation(source_document_, source_);
    if (preview.has_error()) {
      operation_ = nullptr;
      label_.clear();
      return Result<GuiSketchModifyPreview>::failure(preview.error());
    }
    operation_ = std::move(operation);
    label_ = std::move(label);
    return Result<GuiSketchModifyPreview>::success(
        {std::move(preview.value().sketch), std::move(preview.value().invalidated_ids),
         std::move(preview.value().invalidated_profile_ids), std::move(preview.value().attachments),
         std::move(preview.value().invalidated_endpoints)});
  }

  PartDocument source_document_;
  Sketch source_;
  Operation operation_;
  std::string label_;
  std::optional<std::pair<SketchEntityId, SketchEntityId>> corner_targets_;
  bool corner_fillet_{false};
};

} // namespace blcad::gui
