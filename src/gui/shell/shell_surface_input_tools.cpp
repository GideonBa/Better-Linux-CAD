#include "advanced_surface_controls.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <algorithm>
namespace blcad::gui {
namespace {
Result<std::size_t> fail(std::string text) {
  return Result<std::size_t>::failure(Error::validation("surface.inputs", std::move(text)));
}
std::string trim_key(const TrimmingReference& reference) {
  return std::visit(
      [](const auto& source) {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, ProfileRegionReference>)
          return "profile/" + std::to_string(source.sketch().value().size()) + ":" +
                 source.sketch().value() + source.profile().value();
        else
          return "boundary/" + source.stable_key();
      },
      reference.source());
}
} // namespace
Result<SurfaceReference> ShellFeatureTools::selected_surface_target() const {
  const auto& ui = *advanced_surface_;
  if ((command_ == Command::OffsetSurface || command_ == Command::Thicken) &&
      ui.face_set->isChecked()) {
    if (!ui.face_owner)
      return Result<SurfaceReference>::failure(
          Error::validation("surface.inputs", "Add faces of one Body to the explicit set"));
    return SurfaceReference::create_faces(*ui.face_owner, ui.face_refs);
  }
  const int index = surface_target_->currentData().toInt();
  if (index >= 0 && std::size_t(index) < surface_inputs_.size() && surface_inputs_[index].surface)
    return Result<SurfaceReference>::success(*surface_inputs_[index].surface);
  return Result<SurfaceReference>::failure(
      Error::validation("surface.inputs", "Select an owned Surface face or quilt"));
}
Result<std::size_t> ShellFeatureTools::append_surface_face() {
  auto& ui = *advanced_surface_;
  const int index = surface_target_->currentData().toInt();
  if (!surface() || (command_ != Command::OffsetSurface && command_ != Command::Thicken) ||
      index < 0 || std::size_t(index) >= surface_inputs_.size())
    return fail("Choose one owned face above");
  const auto& input = surface_inputs_[index];
  if (!input.surface || input.surface->source_kind() != SurfaceReferenceSourceKind::SelectedFaces)
    return fail("Choose an individual face; whole quilts are a separate input mode");
  const auto& selection = std::get<SurfaceFaceSelection>(input.surface->source());
  if (selection.faces.size() != 1)
    return fail("Choose one individual face to add");
  if (ui.face_owner && *ui.face_owner != selection.body)
    return fail("All selected faces must belong to the same Body");
  auto faces = ui.face_refs;
  faces.push_back(selection.faces.front());
  auto checked = SurfaceReference::create_faces(selection.body, faces);
  if (!checked)
    return Result<std::size_t>::failure(checked.error());
  ui.face_refs = std::move(faces);
  ui.face_owner = selection.body;
  {
    const QSignalBlocker a(ui.face_set), b(ui.selected_faces);
    ui.face_set->setChecked(true);
    ui.selected_faces->addItem(QString::fromStdString(input.label));
    ui.selected_faces->item(ui.selected_faces->count() - 1)
        ->setToolTip(QString::fromStdString(input.label));
    ui.selected_faces->setCurrentRow(ui.selected_faces->count() - 1);
  }
  update_surface_pick_scope();
  refresh_surface_preview();
  return Result<std::size_t>::success(ui.face_refs.size());
}
void ShellFeatureTools::build_surface_input_controls() {
  auto& ui = *advanced_surface_;
  connect(ui.include_hidden, &QCheckBox::toggled, this, [this] {
    if (!surface())
      return;
    auto& ui = *advanced_surface_;
    if (!ui.include_hidden->isChecked() && ui.face_owner) {
      const auto* owner = session_.part_document()->find_body(*ui.face_owner);
      if (!owner || owner->visibility() == BodyVisibility::Hidden ||
          !session_.presentation_visible("body/" + ui.face_owner->value())) {
        ui.face_refs.clear();
        ui.face_owner.reset();
        ui.selected_faces->clear();
      }
    }
    populate_surface_input_editors(command_);
    window_.refresh_feature_selection_scene();
    update_surface_pick_scope();
    refresh_surface_preview();
  });
  connect(ui.face_set, &QCheckBox::toggled, this, [this] {
    if (!advanced_surface())
      return;
    auto& ui = *advanced_surface_;
    if (!ui.face_set->isChecked()) {
      const int index = surface_target_->currentData().toInt();
      if (index >= 0 && std::size_t(index) < surface_inputs_.size())
        if (const auto& ref = surface_inputs_[index].surface;
            ref && ref->source_kind() == SurfaceReferenceSourceKind::SelectedFaces &&
            std::get<SurfaceFaceSelection>(ref->source()).faces.size() > 1) {
          const QSignalBlocker block(surface_target_);
          surface_target_->setCurrentIndex(0);
        }
    }
    update_surface_pick_scope();
    refresh_surface_preview();
  });
  connect(ui.face_add, &QPushButton::clicked, this, [this] {
    auto added = append_surface_face();
    if (!added)
      report(added.error());
  });
  connect(ui.face_remove, &QPushButton::clicked, this, [this] {
    auto& ui = *advanced_surface_;
    const int row = ui.selected_faces->currentRow();
    if (!surface() || row < 0 || std::size_t(row) >= ui.face_refs.size())
      return;
    ui.face_refs.erase(ui.face_refs.begin() + row);
    if (ui.face_refs.empty())
      ui.face_owner.reset();
    {
      const QSignalBlocker block(ui.selected_faces);
      delete ui.selected_faces->takeItem(row);
    }
    refresh_surface_preview();
  });
  connect(ui.selected_faces, &QListWidget::currentRowChanged, this, [this](int row) {
    const auto& ui = *advanced_surface_;
    if (!surface() || !ui.face_owner || row < 0 || std::size_t(row) >= ui.face_refs.size())
      return;
    auto chosen = SurfaceReference::create_faces(*ui.face_owner, {ui.face_refs[row]});
    if (!chosen)
      return;
    for (int i = 1; i < surface_target_->count(); ++i) {
      const auto& input = surface_inputs_[surface_target_->itemData(i).toInt()];
      if (input.surface && input.surface->stable_key() == chosen.value().stable_key()) {
        surface_target_->setCurrentIndex(i);
        break;
      }
    }
  });
}
void ShellFeatureTools::populate_surface_input_editors(Command command) {
  const auto* part = session_.part_document();
  const auto key = [](const SurfaceInput& input) {
    if (input.surface)
      return "surface/" + input.surface->stable_key();
    if (input.boundary)
      return "boundary/" + input.boundary->stable_key();
    return input.trimming ? trim_key(*input.trimming) : std::string{};
  };
  const auto selected_key = [&](QComboBox* box) {
    const int index = box->currentData().toInt();
    return index >= 0 && std::size_t(index) < surface_inputs_.size() ? key(surface_inputs_[index])
                                                                     : std::string{};
  };
  const auto target_key = selected_key(surface_target_), source_key = selected_key(surface_source_);
  std::vector<std::string> ordered;
  for (int row = 0; row < surface_order_->count(); ++row) {
    const int index = surface_order_->item(row)->data(Qt::UserRole).toInt();
    if (index >= 0 && std::size_t(index) < surface_inputs_.size())
      ordered.push_back(key(surface_inputs_[index]));
  }
  const QSignalBlocker source(surface_source_), target(surface_target_), order(surface_order_);
  surface_targets_.clear();
  surface_inputs_ = surface_inputs(&surface_targets_);
  surface_source_->clear();
  surface_target_->clear();
  surface_order_->clear();
  surface_source_->addItem("Select", -1);
  surface_target_->addItem("Select", -1);
  for (std::size_t i = 0; i < surface_inputs_.size(); ++i) {
    const auto& input = surface_inputs_[i];
    if (input.surface && (command != Command::ClosedShellToSolid ||
                          input.surface->source_kind() == SurfaceReferenceSourceKind::Body)) {
      surface_target_->addItem(QString::fromStdString(input.label), static_cast<int>(i));
      surface_target_->setItemData(surface_target_->count() - 1,
                                   QString::fromStdString(input.semantic_id), Qt::UserRole + 1);
      surface_target_->setItemData(surface_target_->count() - 1,
                                   QString::fromStdString(input.label), Qt::ToolTipRole);
    }
    bool eligible =
        command == Command::TrimSurface ? input.trimming.has_value() : input.boundary.has_value();
    if (command == Command::TrimSurface && input.boundary &&
        input.boundary->source_kind() == BoundaryCurveSourceKind::SemanticEdge)
      eligible = false;
    if (eligible && input.boundary &&
        input.boundary->source_kind() == BoundaryCurveSourceKind::PathCurve) {
      const auto id = std::get<PathCurveId>(input.boundary->source());
      const auto* path = input.pending_path ? &*input.pending_path : part->find_path_curve(id);
      if (command == Command::TrimSurface)
        eligible = path && path->closure() == PathClosure::Closed;
      if (command == Command::ExtendSurface && !input.pending_path) {
        auto segments = query_with_step_import_context(session_, [&](auto* context) {
          return geometry::GeometryRecomputeExecutor{context}.resolve_path_segments(
              *part, id,
              advanced_surface_->input_cache ? *advanced_surface_->input_cache
                                             : *session_.part_shape_cache());
        });
        eligible = segments && segments.value().size() == 1 &&
                   (segments.value().front().kind == geometry::SweepPathSegmentKind::Line ||
                    segments.value().front().kind == geometry::SweepPathSegmentKind::CircularArc ||
                    segments.value().front().kind == geometry::SweepPathSegmentKind::DerivedNative);
      }
    }
    if (eligible) {
      surface_source_->addItem(QString::fromStdString(input.label), static_cast<int>(i));
      surface_source_->setItemData(surface_source_->count() - 1,
                                   QString::fromStdString(input.semantic_id), Qt::UserRole + 1);
      surface_source_->setItemData(surface_source_->count() - 1,
                                   QString::fromStdString(input.label), Qt::ToolTipRole);
    }
  }
  for (auto* box : {surface_source_, surface_target_}) {
    const auto& saved = box == surface_source_ ? source_key : target_key;
    if (saved.empty())
      continue;
    for (int row = 1; row < box->count(); ++row)
      if (key(surface_inputs_[box->itemData(row).toInt()]) == saved) {
        box->setCurrentIndex(row);
        break;
      }
  }
  for (const auto& saved : ordered)
    for (std::size_t index = 0; index < surface_inputs_.size(); ++index)
      if (key(surface_inputs_[index]) == saved) {
        auto* item = new QListWidgetItem(QString::fromStdString(surface_inputs_[index].label),
                                         surface_order_);
        item->setData(Qt::UserRole, int(index));
        break;
      }
  // Boolean targets must exist in the same input scope as the chosen source.
  auto& ui = *advanced_surface_;
  const auto* cache = ui.input_cache ? &*ui.input_cache : session_.part_shape_cache();
  const QSignalBlocker body_block(ui.body);
  for (int row = ui.body->count() - 1; row >= 0; --row)
    if (!cache->find_body_shape(BodyId(ui.body->itemData(row).toString().toStdString())))
      ui.body->removeItem(row);
}
} // namespace blcad::gui
