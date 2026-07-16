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
      annotations.push_back(
          {glyph.semantic_id, glyph.candidate_id, glyph.anchor, GuiSketchHitKind::Glyph});
    return Result<std::vector<GuiSketchAnnotationPrimitive>>::success(
        std::move(annotations));
  }

  [[nodiscard]] Result<std::size_t>
  commit(GuiDocumentSession& session, SketchConstraintCatalog& catalog) {
    if (!preview_.authoring.accepted || !preview_.authoring.solved_sketch.has_value())
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "conflicting, redundant, invalid, or non-convergent constraint preview cannot commit"));
    if (committed_)
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(), "constraint preview already committed"));
    if (catalog != catalog_before_)
      return Result<std::size_t>::failure(Error::dependency(
          catalog.sketch().value(), "constraint catalog changed after preview"));
    const PartDocument* document = session.part_document();
    if (document == nullptr || document->id() != document_)
      return Result<std::size_t>::failure(Error::validation(
          document_.value(), "constraint commit requires the original active Part"));
    const Sketch* current = document->find_sketch(catalog.sketch());
    if (current == nullptr)
      return Result<std::size_t>::failure(
          Error::validation(catalog.sketch().value(), "constraint target Sketch no longer exists"));
    auto current_topology = SketchTopology::migrate_legacy(*current);
    if (current_topology.has_error())
      return Result<std::size_t>::failure(current_topology.error());
    if (current_topology.value() != source_topology_)
      return Result<std::size_t>::failure(Error::dependency(
          catalog.sketch().value(), "constraint source topology changed after preview"));

    const Sketch solved = *preview_.authoring.solved_sketch;
    const SketchId sketch_id = solved.id();
    auto committed = session.commit_part_transaction(
        "Add sketch constraint",
        [sketch_id, solved](PartDocument& part) -> Result<std::size_t> {
          if (part.find_sketch(sketch_id) == nullptr)
            return Result<std::size_t>::failure(
                Error::validation(sketch_id.value(), "constraint target Sketch no longer exists"));
          return part.update_sketch(solved);
        });
    if (committed.has_error()) return committed;
    catalog = preview_.authoring.catalog_after;
    committed_ = true;
    undone_ = false;
    return committed;
  }

  [[nodiscard]] Result<std::size_t>
  undo(GuiDocumentSession& session, SketchConstraintCatalog& catalog) {
    if (!committed_ || undone_ || catalog != preview_.authoring.catalog_after ||
        session.undo_label() != "Add sketch constraint")
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "constraint undo bridge is not aligned with the document history"));
    auto undone = session.undo();
    if (undone.has_error()) return undone;
    catalog = catalog_before_;
    undone_ = true;
    return undone;
  }

  [[nodiscard]] Result<std::size_t>
  redo(GuiDocumentSession& session, SketchConstraintCatalog& catalog) {
    if (!committed_ || !undone_ || catalog != catalog_before_ ||
        session.redo_label() != "Add sketch constraint")
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "constraint redo bridge is not aligned with the document history"));
    auto redone = session.redo();
    if (redone.has_error()) return redone;
    catalog = preview_.authoring.catalog_after;
    undone_ = false;
    return redone;
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
      if (point(targets[0]) && point(targets[1]))
        result.push_back(SketchSolverConstraintKind::Coincident);
      if (line(targets[0]) && line(targets[1])) {
        result.push_back(SketchSolverConstraintKind::Parallel);
        result.push_back(SketchSolverConstraintKind::Perpendicular);
        result.push_back(SketchSolverConstraintKind::Collinear);
      }
      if (measurable(targets[0]) && measurable(targets[1]))
        result.push_back(SketchSolverConstraintKind::Equal);
      if (curve(targets[0]) && curve(targets[1]))
        result.push_back(SketchSolverConstraintKind::Tangent);
      if (centered(targets[0]) && centered(targets[1]))
        result.push_back(SketchSolverConstraintKind::Concentric);
      if (point(targets[0]) && line(targets[1]))
        result.push_back(SketchSolverConstraintKind::Midpoint);
      if (point(targets[0]) && (line(targets[1]) || arc(targets[1])))
        result.push_back(SketchSolverConstraintKind::PointOnObject);
    } else if (targets.size() == 3U && point(targets[0]) && point(targets[1]) &&
               line(targets[2])) {
      result.push_back(SketchSolverConstraintKind::Symmetric);
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
