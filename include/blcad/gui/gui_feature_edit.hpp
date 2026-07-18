#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"

#include "blcad/core/part_document.hpp"

#include <string>
#include <string_view>
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
  [[nodiscard]] Result<GuiPartFeaturePreview>
  preview(const GuiDocumentSession& session, FeatureT replacement,
          Result<std::size_t> (PartDocument::*updater)(FeatureT)) const {
    if (!active_) return preview_fail("no feature edit is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    PartDocument candidate = *part;
    auto updated = (candidate.*updater)(std::move(replacement));
    if (updated.has_error()) return Result<GuiPartFeaturePreview>::failure(updated.error());
    auto plan = candidate.create_recompute_plan();
    if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
    return Result<GuiPartFeaturePreview>::success(
        {feature_, "edit feature", plan.value().step_count()});
  }

  // Commit the edit as one transaction. The session recompute makes any
  // downstream failure fail the whole edit closed.
  template <typename FeatureT>
  [[nodiscard]] Result<std::size_t>
  commit(GuiDocumentSession& session, FeatureT replacement,
         Result<std::size_t> (PartDocument::*updater)(FeatureT)) {
    auto checked = preview(session, replacement, updater);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_part_transaction(
        "Edit feature",
        [replacement = std::move(replacement), updater](PartDocument& part) mutable {
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
