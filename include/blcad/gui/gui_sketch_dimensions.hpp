#pragma once

#include "blcad/core/sketch_dimension_catalog_system.hpp"
#include "blcad/geometry/sketch_dimension_glyph.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_sketch_interaction.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {

struct GuiSketchDimensionPreview {
  SketchDimensionAuthoringPreview authoring;
  geometry::SketchDimensionGlyph glyph;
  GuiSketchAnnotationPrimitive annotation;
};

class GuiSketchDimensionController {
public:
  [[nodiscard]] static Result<GuiSketchDimensionController>
  create(const GuiDocumentSession& session, SketchId sketch,
         SketchDimensionIntent candidate) {
    if (session.part_document() == nullptr)
      return Result<GuiSketchDimensionController>::failure(Error::validation(
          sketch.value(), "dimension controller requires an active Part session"));
    auto constraints = session.sketch_constraint_catalog(sketch);
    if (constraints.has_error())
      return Result<GuiSketchDimensionController>::failure(constraints.error());
    auto dimensions = session.sketch_dimension_catalog(sketch);
    if (dimensions.has_error())
      return Result<GuiSketchDimensionController>::failure(dimensions.error());
    const Sketch* source = session.part_document()->find_sketch(sketch);
    if (source == nullptr)
      return Result<GuiSketchDimensionController>::failure(
          Error::validation(sketch.value(), "dimension controller Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*source);
    if (topology.has_error())
      return Result<GuiSketchDimensionController>::failure(topology.error());
    auto preview = SketchDimensionAuthoringService{}.preview(
        *session.part_document(), constraints.value(), dimensions.value(), candidate);
    if (preview.has_error())
      return Result<GuiSketchDimensionController>::failure(preview.error());

    SketchDimensionMeasurement measurement = [&]() {
      if (preview.value().measurement) return *preview.value().measurement;
      auto source_measurement = SketchDimensionMeasurementEvaluator::measure(
          topology.value(), candidate);
      return source_measurement.has_error()
                 ? fallback_measurement(candidate)
                 : source_measurement.value();
    }();
    const auto state = preview.value().accepted
                           ? geometry::SketchDimensionGlyphState::Preview
                           : geometry::SketchDimensionGlyphState::Conflict;
    auto glyph = geometry::SketchDimensionGlyphLayoutResolver{}.resolve(
        preview.value().accepted ? preview.value().solve.topology : topology.value(),
        candidate, measurement, state);
    if (glyph.has_error())
      return Result<GuiSketchDimensionController>::failure(glyph.error());
    GuiSketchAnnotationPrimitive annotation{
        glyph.value().semantic_id, glyph.value().candidate_id, glyph.value().anchor,
        GuiSketchHitKind::Dimension};
    return Result<GuiSketchDimensionController>::success(GuiSketchDimensionController(
        session.part_document()->id(), std::move(topology.value()),
        std::move(constraints.value()), std::move(dimensions.value()),
        {std::move(preview.value()), std::move(glyph.value()), std::move(annotation)}));
  }

  [[nodiscard]] const GuiSketchDimensionPreview& preview() const noexcept { return preview_; }
  [[nodiscard]] const SketchDimensionCatalog& catalog_before() const noexcept {
    return dimensions_before_;
  }

  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session) {
    if (!preview_.authoring.accepted || !preview_.authoring.solved_sketch)
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "conflicting, redundant, invalid, or non-convergent dimension preview cannot commit"));
    if (committed_)
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(), "dimension preview already committed"));
    auto freshness = validate_freshness(session);
    if (freshness.has_error()) return freshness;

    const Sketch solved = *preview_.authoring.solved_sketch;
    const SketchConstraintCatalog expected_constraints = constraints_before_;
    const SketchDimensionCatalog before = dimensions_before_;
    const SketchDimensionCatalog after = preview_.authoring.catalog_after;
    auto committed = session.commit_part_sketch_intent_transaction(
        "Add sketch dimension",
        [solved, expected_constraints, before, after](
            PartDocument& part, std::vector<SketchConstraintCatalog>& constraints,
            std::vector<SketchDimensionCatalog>& dimensions) -> Result<std::size_t> {
          if (catalog_for(constraints, before.sketch(), part.id()) != expected_constraints)
            return Result<std::size_t>::failure(Error::dependency(
                before.sketch().value(), "constraint catalog changed during dimension transaction"));
          SketchDimensionCatalog current = catalog_for(dimensions, before.sketch(), part.id());
          if (current != before)
            return Result<std::size_t>::failure(Error::dependency(
                before.sketch().value(), "dimension catalog changed during transaction"));
          auto updated = part.update_sketch(solved);
          if (updated.has_error()) return updated;
          replace_catalog(dimensions, after);
          return updated;
        });
    if (committed.has_error()) return committed;
    committed_ = true;
    undone_ = false;
    return committed;
  }

  [[nodiscard]] Result<std::size_t> undo(GuiDocumentSession& session) {
    if (!committed_ || undone_ || session.undo_label() != "Add sketch dimension")
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "dimension undo is not aligned with document history"));
    auto current = session.sketch_dimension_catalog(dimensions_before_.sketch());
    if (current.has_error() || current.value() != preview_.authoring.catalog_after)
      return Result<std::size_t>::failure(Error::dependency(
          dimensions_before_.sketch().value(),
          "dimension catalog is not at the committed snapshot"));
    auto result = session.undo();
    if (result.has_error()) return result;
    auto restored = session.sketch_dimension_catalog(dimensions_before_.sketch());
    if (restored.has_error() || restored.value() != dimensions_before_)
      return Result<std::size_t>::failure(Error::internal(
          dimensions_before_.sketch().value(),
          "dimension undo did not restore the catalog snapshot"));
    undone_ = true;
    return result;
  }

  [[nodiscard]] Result<std::size_t> redo(GuiDocumentSession& session) {
    if (!committed_ || !undone_ || session.redo_label() != "Add sketch dimension")
      return Result<std::size_t>::failure(Error::validation(
          preview_.authoring.candidate.id().value(),
          "dimension redo is not aligned with document history"));
    auto current = session.sketch_dimension_catalog(dimensions_before_.sketch());
    if (current.has_error() || current.value() != dimensions_before_)
      return Result<std::size_t>::failure(Error::dependency(
          dimensions_before_.sketch().value(),
          "dimension catalog is not at the pre-commit snapshot"));
    auto result = session.redo();
    if (result.has_error()) return result;
    auto restored = session.sketch_dimension_catalog(dimensions_before_.sketch());
    if (restored.has_error() || restored.value() != preview_.authoring.catalog_after)
      return Result<std::size_t>::failure(Error::internal(
          dimensions_before_.sketch().value(),
          "dimension redo did not restore the catalog snapshot"));
    undone_ = false;
    return result;
  }

  [[nodiscard]] static std::vector<SketchDimensionKind>
  compatible_kinds(const SketchTopology& topology,
                   const std::vector<SketchConstraintIntentTarget>& targets) {
    std::vector<SketchDimensionKind> result;
    const auto point = [](const auto& target) {
      return target.kind() == SketchConstraintIntentTargetKind::Point;
    };
    const auto entity_kind = [&topology](const auto& target)
        -> std::optional<SketchTopologyEntityKind> {
      if (target.kind() != SketchConstraintIntentTargetKind::Entity) return std::nullopt;
      const auto* entity = topology.find_entity(target.id());
      return entity == nullptr ? std::nullopt : std::optional(entity->kind());
    };
    if (targets.size() == 1U) {
      const auto kind = entity_kind(targets.front());
      if (kind == SketchTopologyEntityKind::Line)
        result.push_back(SketchDimensionKind::Length);
      if (kind == SketchTopologyEntityKind::Arc) {
        result.push_back(SketchDimensionKind::Radius);
        result.push_back(SketchDimensionKind::Diameter);
        result.push_back(SketchDimensionKind::ArcLength);
      }
    } else if (targets.size() == 2U) {
      if (point(targets[0]) && point(targets[1])) {
        result.push_back(SketchDimensionKind::HorizontalDistance);
        result.push_back(SketchDimensionKind::VerticalDistance);
        result.push_back(SketchDimensionKind::AlignedDistance);
        result.push_back(SketchDimensionKind::PointToPointDistance);
      }
      if (entity_kind(targets[0]) == SketchTopologyEntityKind::Line &&
          entity_kind(targets[1]) == SketchTopologyEntityKind::Line)
        result.push_back(SketchDimensionKind::Angle);
    }
    std::sort(result.begin(), result.end(), [](auto left, auto right) {
      return static_cast<int>(left) < static_cast<int>(right);
    });
    return result;
  }

  [[nodiscard]] static std::vector<ParameterId>
  compatible_parameters(const PartDocument& document, SketchDimensionKind kind) {
    const ParameterType expected = kind == SketchDimensionKind::Angle
                                       ? ParameterType::Angle
                                       : ParameterType::Length;
    std::vector<ParameterId> result;
    for (const auto& parameter : document.parameters())
      if (parameter.type() == expected) result.push_back(parameter.id());
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
      return left.value() < right.value();
    });
    return result;
  }

  [[nodiscard]] static Result<std::vector<GuiSketchAnnotationPrimitive>>
  accepted_annotations(const GuiDocumentSession& session, SketchId sketch) {
    if (session.part_document() == nullptr)
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(Error::validation(
          sketch.value(), "dimension annotations require an active Part session"));
    auto catalog = session.sketch_dimension_catalog(sketch);
    if (catalog.has_error())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(catalog.error());
    const Sketch* source = session.part_document()->find_sketch(sketch);
    if (source == nullptr)
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(
          Error::validation(sketch.value(), "dimension annotation Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*source);
    if (topology.has_error())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(topology.error());
    auto glyphs = geometry::SketchDimensionGlyphLayoutResolver{}.resolve_catalog(
        *session.part_document(), topology.value(), catalog.value());
    if (glyphs.has_error())
      return Result<std::vector<GuiSketchAnnotationPrimitive>>::failure(glyphs.error());
    std::vector<GuiSketchAnnotationPrimitive> annotations;
    annotations.reserve(glyphs.value().size());
    for (const auto& glyph : glyphs.value())
      annotations.push_back({glyph.semantic_id, glyph.candidate_id, glyph.anchor,
                             GuiSketchHitKind::Dimension, glyph.value_text, glyph.leader});
    return Result<std::vector<GuiSketchAnnotationPrimitive>>::success(
        std::move(annotations));
  }

  [[nodiscard]] static Result<std::size_t>
  remove_accepted(GuiDocumentSession& session, SketchId sketch, SketchDimensionId dimension) {
    return edit_catalog(session, sketch, "Remove sketch dimension",
                        [dimension](PartDocument&, SketchDimensionCatalog& catalog) {
                          return catalog.remove(dimension);
                        });
  }

  [[nodiscard]] static Result<std::size_t>
  edit_literal(GuiDocumentSession& session, SketchId sketch,
               SketchDimensionId dimension, Quantity value) {
    return edit_catalog(session, sketch, "Edit sketch dimension",
                        [dimension, value](PartDocument& part,
                                           SketchDimensionCatalog& catalog) {
                          return SketchDimensionEditService::set_literal(
                              part, catalog, dimension, value);
                        });
  }

  [[nodiscard]] static Result<std::size_t>
  edit_formula(GuiDocumentSession& session, SketchId sketch,
               SketchDimensionId dimension, std::string formula,
               std::optional<ParameterId> replacement_parameter = std::nullopt,
               std::string replacement_name = {}) {
    return edit_catalog(
        session, sketch, "Edit sketch dimension expression",
        [dimension, formula = std::move(formula),
         replacement_parameter = std::move(replacement_parameter),
         replacement_name = std::move(replacement_name)](
            PartDocument& part, SketchDimensionCatalog& catalog) mutable {
          return SketchDimensionEditService::set_formula(
              part, catalog, dimension, std::move(formula),
              std::move(replacement_parameter), std::move(replacement_name));
        });
  }

  [[nodiscard]] static Result<std::size_t>
  rebind_parameter(GuiDocumentSession& session, SketchId sketch,
                   SketchDimensionId dimension, ParameterId parameter) {
    return edit_catalog(
        session, sketch, "Rebind sketch dimension",
        [dimension, parameter = std::move(parameter)](
            PartDocument& part, SketchDimensionCatalog& catalog) mutable {
          return SketchDimensionEditService::rebind_parameter(
              part, catalog, dimension, std::move(parameter));
        });
  }

  [[nodiscard]] static Result<std::size_t>
  set_mode(GuiDocumentSession& session, SketchId sketch,
           SketchDimensionId dimension_id, SketchDimensionMode mode,
           std::optional<ParameterId> parameter = std::nullopt) {
    return edit_catalog(
        session, sketch, "Change sketch dimension mode",
        [dimension_id, mode, parameter = std::move(parameter)](
            PartDocument&, SketchDimensionCatalog& catalog) mutable {
          const SketchDimensionIntent* current = catalog.find(dimension_id);
          if (current == nullptr)
            return Result<std::size_t>::failure(Error::validation(
                dimension_id.value(), "dimension does not exist"));
          auto changed = current->with_mode(mode, std::move(parameter));
          if (changed.has_error())
            return Result<std::size_t>::failure(changed.error());
          return catalog.replace(std::move(changed.value()));
        });
  }

private:
  using CatalogEditor =
      std::function<Result<std::size_t>(PartDocument&, SketchDimensionCatalog&)>;

  GuiSketchDimensionController(DocumentId document, SketchTopology source_topology,
                               SketchConstraintCatalog constraints_before,
                               SketchDimensionCatalog dimensions_before,
                               GuiSketchDimensionPreview preview)
      : document_(std::move(document)), source_topology_(std::move(source_topology)),
        constraints_before_(std::move(constraints_before)),
        dimensions_before_(std::move(dimensions_before)),
        preview_(std::move(preview)) {}

  [[nodiscard]] Result<std::size_t>
  validate_freshness(const GuiDocumentSession& session) const {
    if (session.part_document() == nullptr || session.part_document()->id() != document_)
      return Result<std::size_t>::failure(Error::dependency(
          document_.value(), "dimension commit requires the original active Part"));
    auto constraints = session.sketch_constraint_catalog(dimensions_before_.sketch());
    if (constraints.has_error()) return Result<std::size_t>::failure(constraints.error());
    if (constraints.value() != constraints_before_)
      return Result<std::size_t>::failure(Error::dependency(
          dimensions_before_.sketch().value(),
          "constraint catalog changed after dimension preview"));
    auto dimensions = session.sketch_dimension_catalog(dimensions_before_.sketch());
    if (dimensions.has_error()) return Result<std::size_t>::failure(dimensions.error());
    if (dimensions.value() != dimensions_before_)
      return Result<std::size_t>::failure(Error::dependency(
          dimensions_before_.sketch().value(),
          "dimension catalog changed after preview"));
    const Sketch* current = session.part_document()->find_sketch(dimensions_before_.sketch());
    if (current == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          dimensions_before_.sketch().value(), "dimension Sketch no longer exists"));
    auto topology = SketchTopology::migrate_legacy(*current);
    if (topology.has_error()) return Result<std::size_t>::failure(topology.error());
    if (topology.value() != source_topology_)
      return Result<std::size_t>::failure(Error::dependency(
          dimensions_before_.sketch().value(),
          "dimension source topology changed after preview"));
    return Result<std::size_t>::success(0U);
  }

  [[nodiscard]] static Result<std::size_t>
  edit_catalog(GuiDocumentSession& session, SketchId sketch, std::string label,
               CatalogEditor editor) {
    if (session.part_document() == nullptr || !editor)
      return Result<std::size_t>::failure(Error::validation(
          sketch.value(), "dimension edit requires an active Part session"));
    auto constraints = session.sketch_constraint_catalog(sketch);
    if (constraints.has_error()) return Result<std::size_t>::failure(constraints.error());
    auto before = session.sketch_dimension_catalog(sketch);
    if (before.has_error()) return Result<std::size_t>::failure(before.error());
    const SketchConstraintCatalog expected_constraints = constraints.value();
    const SketchDimensionCatalog expected_dimensions = before.value();
    return session.commit_part_sketch_intent_transaction(
        std::move(label),
        [sketch, expected_constraints, expected_dimensions,
         editor = std::move(editor)](
            PartDocument& part, std::vector<SketchConstraintCatalog>& constraint_catalogs,
            std::vector<SketchDimensionCatalog>& dimension_catalogs) mutable
            -> Result<std::size_t> {
          if (catalog_for(constraint_catalogs, sketch, part.id()) != expected_constraints)
            return Result<std::size_t>::failure(Error::dependency(
                sketch.value(), "constraint catalog changed before dimension edit"));
          SketchDimensionCatalog current =
              catalog_for(dimension_catalogs, sketch, part.id());
          if (current != expected_dimensions)
            return Result<std::size_t>::failure(Error::dependency(
                sketch.value(), "dimension catalog changed before edit"));
          auto edited = editor(part, current);
          if (edited.has_error()) return edited;
          auto solved = SketchDimensionCatalogSystemBuilder::solve(
              part, expected_constraints, current);
          if (solved.has_error())
            return Result<std::size_t>::failure(solved.error());
          if (!solved.value().accepted || !solved.value().solved_sketch)
            return Result<std::size_t>::failure(Error::validation(
                sketch.value(), "dimension edit creates a conflicting or non-convergent Sketch"));
          auto updated = part.update_sketch(*solved.value().solved_sketch);
          if (updated.has_error()) return updated;
          replace_catalog(dimension_catalogs, current);
          return Result<std::size_t>::success(edited.value() + updated.value());
        });
  }

  template <typename Catalog>
  [[nodiscard]] static Catalog catalog_for(const std::vector<Catalog>& catalogs,
                                           const SketchId& sketch,
                                           const DocumentId& document) {
    const auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                    [&sketch](const auto& catalog) {
                                      return catalog.sketch() == sketch;
                                    });
    if (found != catalogs.end()) return *found;
    return Catalog::create(document, sketch).value();
  }

  template <typename Catalog>
  static void replace_catalog(std::vector<Catalog>& catalogs, const Catalog& catalog) {
    const auto found = std::find_if(catalogs.begin(), catalogs.end(),
                                    [&catalog](const auto& value) {
                                      return value.sketch() == catalog.sketch();
                                    });
    if (found == catalogs.end())
      catalogs.push_back(catalog);
    else
      *found = catalog;
  }

  [[nodiscard]] static SketchDimensionMeasurement
  fallback_measurement(const SketchDimensionIntent& dimension) {
    if (dimension.kind() == SketchDimensionKind::Angle)
      return {dimension.id(), dimension.kind(),
              Quantity::angle_deg(0.0, dimension.id().value()).value()};
    return {dimension.id(), dimension.kind(),
            Quantity::length_mm(1.0e-9, dimension.id().value()).value()};
  }

  DocumentId document_;
  SketchTopology source_topology_;
  SketchConstraintCatalog constraints_before_;
  SketchDimensionCatalog dimensions_before_;
  GuiSketchDimensionPreview preview_;
  bool committed_{false};
  bool undone_{false};
};

} // namespace blcad::gui
