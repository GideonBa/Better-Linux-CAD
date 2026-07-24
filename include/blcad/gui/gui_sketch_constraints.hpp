#pragma once

#include "blcad/core/sketch_constraint_authoring.hpp"
#include "blcad/geometry/sketch_constraint_glyph.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {

struct GuiSketchConstraintPreview {
  SketchConstraintAuthoringPreview authoring;
  geometry::SketchConstraintGlyph glyph;
  GuiSketchAnnotationPrimitive annotation;
};

class GuiSketchConstraintController {
public:
  [[nodiscard]] static Result<GuiSketchConstraintController>
  create(const PartDocument& document, SketchConstraintCatalog catalog,
         SketchConstraintIntent candidate) {
    if (document.id() != catalog.document())
      return Result<GuiSketchConstraintController>::failure(Error::validation(
          catalog.document().value(), "constraint controller catalog belongs to another Part"));
    const Sketch* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<GuiSketchConstraintController>::failure(
          Error::validation(catalog.sketch().value(), "constraint controller Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<GuiSketchConstraintController>::failure(topology.error());
    auto preview = SketchConstraintAuthoringService{}.preview(document, catalog, candidate);
    if (preview.has_error())
      return Result<GuiSketchConstraintController>::failure(preview.error());
    const auto state = preview.value().accepted
                           ? geometry::SketchConstraintGlyphState::Preview
                           : preview.value().solve.status == SketchSolveStatus::Redundant
                                 ? geometry::SketchConstraintGlyphState::Redundant
                                 : geometry::SketchConstraintGlyphState::Conflict;
    auto glyph = geometry::SketchConstraintGlyphLayoutResolver{}.resolve(
        topology.value(), candidate, state);
    if (glyph.has_error())
      return Result<GuiSketchConstraintController>::failure(glyph.error());
    GuiSketchAnnotationPrimitive annotation{
        glyph.value().semantic_id, glyph.value().candidate_id, glyph.value().anchor,
        GuiSketchHitKind::Glyph};
    return Result<GuiSketchConstraintController>::success(GuiSketchConstraintController(
        document.id(), std::move(topology.value()), std::move(catalog),
        {std::move(preview.value()), std::move(glyph.value()), std::move(annotation)}));
  }

  [[nodiscard]] static Result<GuiSketchConstraintController>
  create(const GuiDocumentSession& session, SketchId sketch,
         SketchConstraintIntent candidate) {
    if (session.part_document() == nullptr)
      return Result<GuiSketchConstraintController>::failure(Error::validation(
          sketch.value(), "constraint controller requires an active Part session"));
    auto catalog = session.sketch_constraint_catalog(sketch);
    if (catalog.has_error())
      return Result<GuiSketchConstraintController>::failure(catalog.error());
    return create(*session.part_document(), std::move(catalog.value()),
                  std::move(candidate));
  }

  [[nodiscard]] const GuiSketchConstraintPreview& preview() const noexcept { return preview_; }
  [[nodiscard]] const SketchConstraintCatalog& catalog_before() const noexcept {
    return catalog_before_;
  }

  [[nodiscard]] static Result<std::vector<GuiSketchAnnotationPrimitive>>
  accepted_annotations(const PartDocument& document,
                       const SketchConstraintCatalog& catalog) {
    if (document.id() != catalog.document())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(Error::validation(
          catalog.document().value(), "constraint annotation catalog belongs to another Part"));
    const Sketch* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(
          Error::validation(catalog.sketch().value(), "constraint annotation Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(topology.error());
    auto glyphs = geometry::SketchConstraintGlyphLayoutResolver{}.resolve_catalog(
        topology.value(), catalog);
    if (glyphs.has_error())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(glyphs.error());
    std::vector<GuiSketchAnnotationPrimitive> annotations;
    annotations.reserve(glyphs.value().size());
    for (const auto& glyph : glyphs.value())
      annotations.push_back({glyph.semantic_id, glyph.candidate_id, glyph.anchor,
                             GuiSketchHitKind::Glyph, glyph.token});
    return Result<std::vector<GuiSketchAnnotationPrimitive>>::success(
        std::move(annotations));
  }

  [[nodiscard]] static Result<std::vector<GuiSketchAnnotationPrimitive>>
  accepted_annotations(const GuiDocumentSession& session, SketchId sketch) {
    if (session.part_document() == nullptr)
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(Error::validation(
          sketch.value(), "constraint annotations require an active Part session"));
    auto catalog = session.sketch_constraint_catalog(sketch);
    if (catalog.has_error())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(catalog.error());
    return accepted_annotations(*session.part_document(), catalog.value());
  }

  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session) {
    if (!preview_.authoring.accepted || !preview_.authoring.solved_sketch.has_value())
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "conflicting, redundant, invalid, or non-convergent constraint preview cannot commit"));
    if (committed_)
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(), "constraint preview already committed"));
    if (session.part_document() == nullptr || session.part_document()->id() != document_)
      return Result<std::size_t>::failure(Error::validation(
          document_.value(), "constraint commit requires the original active Part"));

    auto current_catalog = session.sketch_constraint_catalog(catalog_before_.sketch());
    if (current_catalog.has_error())
      return Result<std::size_t>::failure(current_catalog.error());
    if (current_catalog.value() != catalog_before_)
      return Result<std::size_t>::failure(Error::dependency(
          catalog_before_.sketch().value(), "constraint catalog changed after preview"));
    const Sketch* current = session.part_document()->find_sketch(catalog_before_.sketch());
    if (current == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          catalog_before_.sketch().value(), "constraint target Sketch no longer exists"));
    auto current_topology = SketchTopology::migrate_legacy(*current);
    if (current_topology.has_error())
      return Result<std::size_t>::failure(current_topology.error());
    if (current_topology.value() != source_topology_)
      return Result<std::size_t>::failure(Error::dependency(
          catalog_before_.sketch().value(), "constraint source topology changed after preview"));

    const Sketch solved = *preview_.authoring.solved_sketch;
    const SketchConstraintCatalog before = catalog_before_;
    const SketchConstraintCatalog after = preview_.authoring.catalog_after;
    auto committed = session.commit_part_constraint_transaction(
        "Add sketch constraint",
        [solved, before, after](PartDocument& part,
                                std::vector<SketchConstraintCatalog>& catalogs)
            -> Result<std::size_t> {
          const Sketch* current_sketch = part.find_sketch(before.sketch());
          if (current_sketch == nullptr)
            return Result<std::size_t>::failure(Error::validation(
                before.sketch().value(), "constraint target Sketch no longer exists"));
          auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                    [&before](const auto& catalog) {
                                      return catalog.sketch() == before.sketch();
                                    });
          SketchConstraintCatalog current_catalog = before;
          if (found != catalogs.end()) current_catalog = *found;
          if (current_catalog != before)
            return Result<std::size_t>::failure(Error::dependency(
                before.sketch().value(), "constraint catalog changed during transaction"));
          auto updated = part.update_sketch(solved);
          if (updated.has_error()) return updated;
          if (found == catalogs.end())
            catalogs.push_back(after);
          else
            *found = after;
          return updated;
        });
    if (committed.has_error()) return committed;
    committed_ = true;
    undone_ = false;
    return committed;
  }

  [[nodiscard]] Result<std::size_t> undo(GuiDocumentSession& session) {
    if (!committed_ || undone_ || session.undo_label() != "Add sketch constraint")
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "constraint undo is not aligned with the document history"));
    auto current = session.sketch_constraint_catalog(catalog_before_.sketch());
    if (current.has_error() || current.value() != preview_.authoring.catalog_after)
      return Result<std::size_t>::failure(Error::dependency(
          catalog_before_.sketch().value(), "constraint catalog is not at the committed snapshot"));
    auto result = session.undo();
    if (result.has_error()) return result;
    auto restored = session.sketch_constraint_catalog(catalog_before_.sketch());
    if (restored.has_error() || restored.value() != catalog_before_)
      return Result<std::size_t>::failure(Error::internal(
          catalog_before_.sketch().value(), "constraint undo did not restore the catalog snapshot"));
    undone_ = true;
    return result;
  }

  [[nodiscard]] Result<std::size_t> redo(GuiDocumentSession& session) {
    if (!committed_ || !undone_ || session.redo_label() != "Add sketch constraint")
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "constraint redo is not aligned with the document history"));
    auto current = session.sketch_constraint_catalog(catalog_before_.sketch());
    if (current.has_error() || current.value() != catalog_before_)
      return Result<std::size_t>::failure(Error::dependency(
          catalog_before_.sketch().value(), "constraint catalog is not at the pre-commit snapshot"));
    auto result = session.redo();
    if (result.has_error()) return result;
    auto restored = session.sketch_constraint_catalog(catalog_before_.sketch());
    if (restored.has_error() || restored.value() != preview_.authoring.catalog_after)
      return Result<std::size_t>::failure(Error::internal(
          catalog_before_.sketch().value(), "constraint redo did not restore the catalog snapshot"));
    undone_ = false;
    return result;
  }

  // Removes one accepted session-owned catalog constraint as its own atomic
  // "Remove sketch constraint" history entry. Historical embedded Sketch
  // constraint records remain Block-99 Sketch-edit intent and are not touched.
  [[nodiscard]] static Result<std::size_t>
  remove_accepted(GuiDocumentSession& session, SketchId sketch, SketchConstraintId id) {
    if (session.part_document() == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          sketch.value(), "constraint removal requires an active Part session"));
    auto catalog = session.sketch_constraint_catalog(sketch);
    if (catalog.has_error()) return Result<std::size_t>::failure(catalog.error());
    if (catalog.value().find(id) == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          id.value(), "constraint is not an accepted session catalog record"));
    const SketchConstraintCatalog before = std::move(catalog.value());
    return session.commit_part_constraint_transaction(
        "Remove sketch constraint",
        [sketch, id, before](PartDocument& part,
                             std::vector<SketchConstraintCatalog>& catalogs)
            -> Result<std::size_t> {
          if (part.find_sketch(sketch) == nullptr)
            return Result<std::size_t>::failure(Error::validation(
                sketch.value(), "constraint target Sketch no longer exists"));
          const auto found = std::find_if(
              catalogs.begin(), catalogs.end(),
              [&sketch](const auto& catalog) { return catalog.sketch() == sketch; });
          if (found == catalogs.end() || *found != before)
            return Result<std::size_t>::failure(Error::dependency(
                sketch.value(), "constraint catalog changed during removal"));
          return found->remove(id);
        });
  }

  [[nodiscard]] static std::vector<SketchSolverConstraintKind>
  compatible_kinds(const SketchTopology& topology,
                   const std::vector<SketchConstraintIntentTarget>& targets) {
    std::vector<SketchSolverConstraintKind> result;
    const auto entity = [&topology](const SketchConstraintIntentTarget& target)
        -> const SketchTopologyEntity* {
      return target.kind() == SketchConstraintIntentTargetKind::Entity
                 ? topology.find_entity(target.id())
                 : nullptr;
    };
    const auto kind = [&entity](const SketchConstraintIntentTarget& target)
        -> std::optional<SketchTopologyEntityKind> {
      const auto* found = entity(target);
      return found == nullptr ? std::nullopt : std::optional(found->kind());
    };
    const auto line = [&kind](const SketchConstraintIntentTarget& target) {
      const auto value = kind(target);
      return value && *value == SketchTopologyEntityKind::Line;
    };
    const auto arc = [&kind](const SketchConstraintIntentTarget& target) {
      const auto value = kind(target);
      return value && *value == SketchTopologyEntityKind::Arc;
    };
    const auto curve = [&kind](const SketchConstraintIntentTarget& target) {
      const auto value = kind(target);
      return value && (*value == SketchTopologyEntityKind::Line ||
                       *value == SketchTopologyEntityKind::Arc ||
                       *value == SketchTopologyEntityKind::Spline);
    };
    const auto measurable = [&kind](const SketchConstraintIntentTarget& target) {
      const auto value = kind(target);
      return value && (*value == SketchTopologyEntityKind::Line ||
                       *value == SketchTopologyEntityKind::Arc);
    };
    const auto centered = [&kind](const SketchConstraintIntentTarget& target) {
      const auto value = kind(target);
      return value && (*value == SketchTopologyEntityKind::Arc ||
                       *value == SketchTopologyEntityKind::CircleProfile ||
                       *value == SketchTopologyEntityKind::CircularHolePattern);
    };
    const auto point = [](const SketchConstraintIntentTarget& target) {
      return target.kind() == SketchConstraintIntentTargetKind::Point;
    };
    const auto shares_point = [&entity](const SketchConstraintIntentTarget& first,
                                        const SketchConstraintIntentTarget& second) {
      const auto* first_entity = entity(first);
      const auto* second_entity = entity(second);
      if (first_entity == nullptr || second_entity == nullptr) return false;
      for (const auto& first_point : first_entity->points())
        if (std::find(second_entity->points().begin(), second_entity->points().end(),
                      first_point) != second_entity->points().end())
          return true;
      return false;
    };

    if (targets.size() == 1U) {
      const auto* selected_entity = entity(targets[0]);
      if (point(targets[0]) ||
          (selected_entity != nullptr && !selected_entity->points().empty()))
        result.push_back(SketchSolverConstraintKind::Fixed);
      if (line(targets[0])) {
        result.push_back(SketchSolverConstraintKind::Horizontal);
        result.push_back(SketchSolverConstraintKind::Vertical);
      }
    } else if (targets.size() == 2U) {
      // Selection order must not matter (GUI Shell Reset MVP-9R usability):
      // every mixed-kind rule accepts both orders; intent creation normalizes.
      const auto pair = [&targets](const auto& first_predicate, const auto& second_predicate) {
        return (first_predicate(targets[0]) && second_predicate(targets[1])) ||
               (first_predicate(targets[1]) && second_predicate(targets[0]));
      };
      // Curves count as connected when they share a topology point or when two
      // of their endpoints coincide positionally (e.g. coupled via a
      // Coincident constraint rather than a shared point). The tolerance
      // matches the solver's tangent corner pairing (0.1 mm) — a coincident
      // SOLVE leaves points equal only to convergence tolerance.
      const auto connected = [&entity, &topology, &shares_point](
                                 const SketchConstraintIntentTarget& first,
                                 const SketchConstraintIntentTarget& second) {
        if (shares_point(first, second))
          return true;
        const auto* first_entity = entity(first);
        const auto* second_entity = entity(second);
        if (first_entity == nullptr || second_entity == nullptr)
          return false;
        for (const auto& first_id : first_entity->points()) {
          const auto* first_point = topology.find_point(first_id);
          if (first_point == nullptr)
            continue;
          for (const auto& second_id : second_entity->points()) {
            const auto* second_point = topology.find_point(second_id);
            if (second_point == nullptr)
              continue;
            const double dx = first_point->position().x - second_point->position().x;
            const double dy = first_point->position().y - second_point->position().y;
            if (dx * dx + dy * dy < 1.0e-2)
              return true;
          }
        }
        return false;
      };
      if (point(targets[0]) && point(targets[1]))
        result.push_back(SketchSolverConstraintKind::Coincident);
      if (line(targets[0]) && line(targets[1])) {
        result.push_back(SketchSolverConstraintKind::Parallel);
        result.push_back(SketchSolverConstraintKind::Perpendicular);
        result.push_back(SketchSolverConstraintKind::Collinear);
      }
      if (measurable(targets[0]) && measurable(targets[1]))
        result.push_back(SketchSolverConstraintKind::Equal);
      // Tangent between two straight lines is degenerate (that is Parallel);
      // require at least one non-line curve.
      if (curve(targets[0]) && curve(targets[1]) && !(line(targets[0]) && line(targets[1])) &&
          connected(targets[0], targets[1]))
        result.push_back(SketchSolverConstraintKind::Tangent);
      // Line↔circle tangency needs no shared corner (radius baked at build).
      const auto circle_profile = [&kind](const SketchConstraintIntentTarget& target) {
        const auto value = kind(target);
        return value && *value == SketchTopologyEntityKind::CircleProfile;
      };
      if (pair(line, circle_profile))
        result.push_back(SketchSolverConstraintKind::Tangent);
      if (centered(targets[0]) && centered(targets[1]))
        result.push_back(SketchSolverConstraintKind::Concentric);
      if (pair(point, line))
        result.push_back(SketchSolverConstraintKind::Midpoint);
      if (pair(point, [&](const auto& target) {
            const auto value = kind(target);
            return value && (*value == SketchTopologyEntityKind::Line ||
                             *value == SketchTopologyEntityKind::Arc ||
                             *value == SketchTopologyEntityKind::CircleProfile);
          }))
        result.push_back(SketchSolverConstraintKind::PointOnObject);
    } else if (targets.size() == 3U) {
      std::size_t points = 0;
      std::size_t lines = 0;
      for (const auto& target : targets) {
        points += point(target) ? 1U : 0U;
        lines += line(target) ? 1U : 0U;
      }
      if (points == 2U && lines == 1U)
        result.push_back(SketchSolverConstraintKind::Symmetric);
    }
    // Collinear is variable-arity across any selection size: two-plus lines, a
    // point and a line, or three-plus points, with nothing else mixed in.
    {
      std::size_t lines = 0U;
      std::size_t points = 0U;
      std::size_t others = 0U;
      for (const auto& target : targets)
        (point(target) ? points : line(target) ? lines : others) += 1U;
      if (others == 0U && (lines >= 2U || (lines == 1U && points >= 1U) ||
                           (lines == 0U && points >= 3U)))
        result.push_back(SketchSolverConstraintKind::Collinear);
    }
    std::sort(result.begin(), result.end(), [](auto left, auto right) {
      return static_cast<int>(left) < static_cast<int>(right);
    });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }

  [[nodiscard]] static Result<std::optional<SketchConstraintIntent>>
  automatic_candidate(SketchConstraintId id, GuiSketchSnapKind snap,
                      SketchConstraintIntentTarget authored,
                      std::optional<SketchConstraintIntentTarget> reference = std::nullopt) {
    std::optional<SketchSolverConstraintKind> kind;
    std::vector<SketchConstraintIntentTarget> targets;
    switch (snap) {
    case GuiSketchSnapKind::HorizontalInference:
      kind = SketchSolverConstraintKind::Horizontal;
      targets = {authored};
      break;
    case GuiSketchSnapKind::VerticalInference:
      kind = SketchSolverConstraintKind::Vertical;
      targets = {authored};
      break;
    case GuiSketchSnapKind::Endpoint:
    case GuiSketchSnapKind::Intersection:
      if (reference) {
        kind = SketchSolverConstraintKind::Coincident;
        targets = {authored, *reference};
      }
      break;
    case GuiSketchSnapKind::Midpoint:
      if (reference) {
        kind = SketchSolverConstraintKind::Midpoint;
        targets = {authored, *reference};
      }
      break;
    case GuiSketchSnapKind::Center:
      if (reference) {
        kind = SketchSolverConstraintKind::Concentric;
        targets = {authored, *reference};
      }
      break;
    case GuiSketchSnapKind::Nearest:
      if (reference) {
        kind = SketchSolverConstraintKind::PointOnObject;
        targets = {authored, *reference};
      }
      break;
    default: break;
    }
    if (!kind) return Result<std::optional<SketchConstraintIntent>>::success(std::nullopt);
    auto candidate = SketchConstraintIntent::create(
        std::move(id), *kind, std::move(targets), SketchConstraintIntentSource::Automatic);
    if (candidate.has_error())
      return Result<std::optional<SketchConstraintIntent>>::failure(candidate.error());
    return Result<std::optional<SketchConstraintIntent>>::success(
        std::move(candidate.value()));
  }

private:
  GuiSketchConstraintController(DocumentId document, SketchTopology source_topology,
                                SketchConstraintCatalog catalog_before,
                                GuiSketchConstraintPreview preview)
      : document_(std::move(document)), source_topology_(std::move(source_topology)),
        catalog_before_(std::move(catalog_before)), preview_(std::move(preview)) {}

  DocumentId document_;
  SketchTopology source_topology_;
  SketchConstraintCatalog catalog_before_;
  GuiSketchConstraintPreview preview_;
  bool committed_{false};
  bool undone_{false};
};

} // namespace blcad::gui
