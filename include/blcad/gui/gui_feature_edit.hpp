#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"

#include "blcad/core/part_document.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace blcad::gui {

// The feature family a double-clicked feature belongs to, so the edit UI can
// preload the matching Block-124..128 controller with the persistent intent.
enum class GuiFeatureEditKind {
  Extrude,
  Revolve,
  Sweep,
  Loft,
  Surface,
  LinearPattern,
  CircularPattern,
  Mirror,
  Fillet,
  Chamfer,
  Shell,
  Draft,
  BodyBoolean,
};

// Block 129 feature edit lifecycle. Double-clicking a feature begins an edit:
// the controller identifies the family so the UI can preload the persistent
// intent into the matching authoring controller, previews a disposable edited
// candidate, and commits exactly one document transaction through the Core
// update-* command. Because the transaction recomputes, a downstream feature
// that fails after the edit fails the whole transaction closed (last-valid);
// there is no partial commit and no silent suppression.
class GuiFeatureEditController {
public:
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession& session, FeatureId feature) {
    const auto* part = session.part_document();
    if (part == nullptr)
      return fail("feature edit requires an active Part document");
    if (part->find_imported_body_feature(feature))
      return fail(
          "Imported Body base is immutable; use STEP Sources to explicitly refresh its source");
    if (part->find_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Extrude;
    else if (part->find_revolve_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Revolve;
    else if (part->find_sweep_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Sweep;
    else if (part->find_loft_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Loft;
    else if (part->find_surface_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Surface;
    else if (part->find_linear_pattern_feature(feature) != nullptr)
      kind_ = GuiFeatureEditKind::LinearPattern;
    else if (part->find_circular_pattern_feature(feature) != nullptr)
      kind_ = GuiFeatureEditKind::CircularPattern;
    else if (part->find_mirror_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Mirror;
    else if (part->find_fillet_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Fillet;
    else if (part->find_chamfer_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Chamfer;
    else if (part->find_shell_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Shell;
    else if (part->find_draft_feature(feature) != nullptr) kind_ = GuiFeatureEditKind::Draft;
    else if (part->find_body_boolean_feature(feature) != nullptr)
      kind_ = GuiFeatureEditKind::BodyBoolean;
    else
      return fail("feature to edit does not exist in the active Part document");
    feature_ = std::move(feature);
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] GuiFeatureEditKind kind() const noexcept { return kind_; }
  [[nodiscard]] const FeatureId& feature() const noexcept { return feature_; }

  // Preview the edited replacement on a disposable clone via the Core update
  // command, validating the whole feature graph without mutating the document.
  template <typename FeatureT>
  [[nodiscard]] Result<PartDocument>
  candidate(const GuiDocumentSession& session, FeatureT replacement,
            Result<std::size_t> (PartDocument::*updater)(FeatureT),
            const GuiPartMutation& prepare = {}) const {
    if (!active_ || !session.part_document())
      return Result<PartDocument>::failure(
          Error::validation("gui.feature_edit", "no active Part feature edit"));
    const auto id = [&] {
      if constexpr (std::is_same_v<FeatureT, SurfaceFeature>)
        return surface_feature_id(replacement);
      else
        return replacement.id();
    }();
    if (id != feature_)
      return Result<PartDocument>::failure(
          Error::validation("gui.feature_edit", "replacement must retain the edited feature id"));
    PartDocument draft = *session.part_document();
    if (prepare) {
      auto prepared = prepare(draft);
      if (!prepared)
        return Result<PartDocument>::failure(prepared.error());
    }
    auto updated = (draft.*updater)(std::move(replacement));
    if (!updated)
      return Result<PartDocument>::failure(updated.error());
    auto plan = draft.create_recompute_plan();
    if (!plan)
      return Result<PartDocument>::failure(plan.error());
    return Result<PartDocument>::success(std::move(draft));
  }

  template <typename FeatureT>
  [[nodiscard]] Result<GuiPartFeaturePreview>
  preview(const GuiDocumentSession& session, FeatureT replacement,
          Result<std::size_t> (PartDocument::*updater)(FeatureT),
          const GuiPartMutation& prepare = {}) const {
    auto draft = candidate(session, std::move(replacement), updater, prepare);
    if (!draft)
      return Result<GuiPartFeaturePreview>::failure(draft.error());
    auto plan = draft.value().create_recompute_plan();
    if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
    return Result<GuiPartFeaturePreview>::success(
        {feature_, "edit feature", plan.value().step_count()});
  }

  // Commit the edit as one transaction. The session recompute makes any
  // downstream failure fail the whole edit closed.
  template <typename FeatureT>
  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session, FeatureT replacement,
                                           Result<std::size_t> (PartDocument::*updater)(FeatureT),
                                           const GuiPartMutation& prepare = {}) {
    auto checked = preview(session, replacement, updater, prepare);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_part_transaction(
        "Edit feature",
        [replacement = std::move(replacement), updater, prepare](PartDocument& part) mutable {
          if (prepare) {
            auto prepared = prepare(part);
            if (!prepared)
              return prepared;
          }
          return (part.*updater)(std::move(replacement));
        });
    if (!committed.has_error()) active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.feature_edit", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.feature_edit", std::move(message)));
  }

  bool active_{false};
  GuiFeatureEditKind kind_{GuiFeatureEditKind::Extrude};
  FeatureId feature_{""};
};

} // namespace blcad::gui
