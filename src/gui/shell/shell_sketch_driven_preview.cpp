#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "sketch_driven_tools_impl.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
namespace blcad::gui {
namespace {
Error error(const std::string& s) {
  return Error::validation("part.sketch_driven", s);
}
} // namespace
void ShellSketchDrivenTools::Impl::overlay_refresh() {
  auto mapping = GuiViewportManipulatorMapping::create(
      [this](GuiSketchScreenPoint p) { return window.viewport().screen_to_view_ray(p); },
      [this](Point3 p) { return window.viewport().model_to_screen(p); });
  if (mapping)
    (void)layer.set_mapping(std::move(mapping.value()));
  overlay->setGeometry(window.viewport().rect());
  overlay->setVisible(active && !source_edit && !layer.handles().empty());
  overlay->raise();
  overlay->update();
}
void ShellSketchDrivenTools::Impl::guides(const PartDocument& candidate,
                                          const SketchDrivenFeature& record) {
  if (!layer.active_handle_id().empty())
    return;
  const auto history = candidate.executable_history();
  if (!history)
    return;
  auto found = std::find_if(history.value().begin(), history.value().end(),
                            [&](const auto& row) { return row.node_id == feature.value(); });
  if (found == history.value().end())
    return;
  auto prepared = prepare_inputs(candidate, std::size_t(found - history.value().begin()));
  if (!prepared)
    return;
  auto queried = query_with_step_import_context(window.session(), [&](auto* context) {
    return geometry::SketchDrivenGuideQuery::guides(*inputs, record, *input_cache, context, prefix);
  });
  if (!queried)
    return;
  std::vector<GuiViewportManipulatorHandle> handles;
  grip_fields.clear();
  for (const auto& guide : queried.value()) {
    QLineEdit* field = guide.id == "thickness" || guide.id == "depth" ? ui->size.value
                       : guide.id == "second"                         ? ui->second.value
                       : guide.id == "distance"                       ? ui->distance.value
                       : guide.id == "draft"                          ? ui->draft.value
                       : guide.id == "seam"                           ? ui->seam
                                                                      : nullptr;
    GuiViewportManipulatorHandle handle;
    handle.id = "sketch_driven/" + guide.id;
    handle.origin = guide.origin;
    handle.axis = guide.axis;
    handle.reference_direction = guide.reference_direction;
    handle.reference_value = guide.value;
    handle.model_units_per_value = guide.model_units_per_value;
    handle.label = guide.label;
    handle.kind =
        guide.angular ? GuiViewportManipulatorKind::Angular : GuiViewportManipulatorKind::Linear;
    handle.minimum_value = guide.angular ? -1e9 : 1e-6;
    handle.interactive = field && field->isEnabled() && !field->isReadOnly();
    if (guide.binding) {
      const auto* p = candidate.find_parameter(*guide.binding);
      if (p && p->is_expression())
        handle.interactive = false;
    }
    if (field)
      grip_fields[handle.id] = field;
    handles.push_back(std::move(handle));
  }
  (void)layer.set_handles(std::move(handles));
  overlay_refresh();
}
void ShellSketchDrivenTools::Impl::fail(const Error& e) {
  valid = false;
  ui->apply->setEnabled(false);
  ui->status->setText(QString::fromStdString(e.object_id() + ": " + e.message()));
  window.viewport().clear_feature_preview();
  if (!layer.active_handle_id().empty())
    return;
  std::vector<GuiViewportManipulatorHandle> markers;
  for (const auto& target : targets) {
    const auto key = interactive_face_identity(std::get<FaceReference>(target.selection.reference));
    bool selected = ui->extent->currentIndex() == 1 &&
                    ui->limit_face->currentData().toString().toStdString() == key;
    for (int row = 0; row < ui->supports->count(); ++row)
      selected |= ui->supports->item(row)->data(Qt::UserRole).toString().toStdString() == key;
    if (selected) {
      GuiViewportManipulatorHandle marker;
      marker.id = "sketch_driven/failure/" + key;
      marker.origin = target.anchor;
      marker.axis = target.normal;
      marker.interactive = false;
      marker.label = "Failure: selected support";
      markers.push_back(std::move(marker));
    }
  }
  for (auto handle : layer.handles()) {
    if (handle.id.find("/failure") != std::string::npos)
      continue;
    handle.id += "/failure";
    handle.interactive = false;
    handle.label = "Failure: source / " + handle.label;
    markers.push_back(std::move(handle));
  }
  if (markers.empty() && inputs && input_cache) {
    const auto* sketch =
        inputs->find_sketch(SketchId(ui->sketch->currentData().toString().toStdString()));
    if (sketch) {
      auto frame = geometry::WorkplaneResolver{}.resolve_for_sketch(*inputs, *sketch, *input_cache);
      if (frame) {
        GuiViewportManipulatorHandle marker;
        marker.id = "sketch_driven/failure/source";
        marker.origin = frame.value().origin;
        marker.axis = frame.value().normal;
        marker.interactive = false;
        marker.label = "Failure: source Sketch";
        markers.push_back(marker);
      }
    }
  }
  if (markers.empty() && input_cache) {
    const auto* sketch = window.session().part_document()->find_sketch(
        SketchId(ui->sketch->currentData().toString().toStdString()));
    if (sketch) {
      auto frame = geometry::WorkplaneResolver{}.resolve_for_sketch(
          *window.session().part_document(), *sketch, *input_cache);
      if (frame) {
        GuiViewportManipulatorHandle marker;
        marker.id = "sketch_driven/failure/source";
        marker.origin = frame.value().origin;
        marker.axis = frame.value().normal;
        marker.interactive = false;
        marker.label = "Failure: source Sketch";
        markers.push_back(marker);
      }
    }
  }
  (void)layer.set_handles(std::move(markers));
  overlay_refresh();
}
Result<std::size_t> ShellSketchDrivenTools::preview() {
  auto& p = *impl_;
  if (!p.current())
    return Result<std::size_t>::failure(error("The model changed. Reopen this feature editor."));
  p.valid = false;
  p.ui->apply->setEnabled(false);
  auto candidate = p.candidate();
  if (!candidate) {
    p.fail(candidate.error());
    return Result<std::size_t>::failure(candidate.error());
  }
  const auto* record = candidate.value().find_sketch_driven_feature(p.feature);
  if (!record)
    return Result<std::size_t>::failure(error("Candidate feature is unavailable."));
  p.guides(candidate.value(), *record);
  auto cache = geometry::ShapeCache::create(ShapeCacheId("sketch_driven.complete_preview"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto computed = query_with_step_import_context(p.window.session(), [&](auto* context) {
    return geometry::GeometryRecomputeExecutor(context).execute_document(candidate.value(),
                                                                         cache.value());
  });
  if (!computed) {
    p.fail(computed.error());
    return Result<std::size_t>::failure(computed.error());
  }
  const auto* shape = cache.value().find_body_shape(record->target_body());
  if (!shape) {
    auto e = error("Complete history has no target Body result.");
    p.fail(e);
    return Result<std::size_t>::failure(e);
  }
  auto shown =
      p.window.viewport().set_feature_preview(*shape, "body/" + record->target_body().value());
  if (!shown) {
    p.fail(shown.error());
    return shown;
  }
  p.valid = true;
  p.ui->apply->setEnabled(true);
  (void)p.window.session().task().show_preview();
  p.ui->status->setText(
      QString("Valid complete history. Input prefix: %1. Apply publishes one undo step.")
          .arg(p.prefix));
  p.overlay_refresh();
  return Result<std::size_t>::success(1);
}
void ShellSketchDrivenTools::Impl::apply_grip(const GuiViewportManipulatorCandidate& candidate) {
  auto found = grip_fields.find(candidate.handle_id);
  if (found == grip_fields.end())
    return;
  loading = true;
  found->second->setText(QString::number(candidate.scalar_value, 'g', 17));
  loading = false;
  (void)owner.preview();
}
Result<std::size_t> ShellSketchDrivenTools::set_drag_text(const QString& text) {
  auto& p = *impl_;
  p.numeric_valid = p.current() && !p.layer.active_handle_id().empty() &&
                    p.layer.set_numeric_input(text.toStdString()) &&
                    p.layer.candidate().has_value();
  if (!p.numeric_valid) {
    p.valid = false;
    p.ui->apply->setEnabled(false);
    p.ui->status->setText("Enter a valid active grip value, with mm or deg.");
    return Result<std::size_t>::failure(error("Invalid grip value"));
  }
  p.apply_grip(*p.layer.candidate());
  return p.valid ? Result<std::size_t>::success(1)
                 : Result<std::size_t>::failure(error("Grip preview is invalid."));
}
bool ShellSketchDrivenTools::accept_selection(const GuiSelection& selection) {
  auto& p = *impl_;
  if (!p.current())
    return false;
  const auto select = [](QComboBox* field, const std::string& key) {
    int at = field->findData(QString::fromStdString(key));
    if (at < 0)
      return false;
    field->setCurrentIndex(at);
    return true;
  };
  if (selection.kind == GuiSelectionKind::Body && !p.editing &&
      selection.semantic_id.starts_with("body/"))
    return select(p.ui->body, selection.semantic_id.substr(5));
  if (selection.kind == GuiSelectionKind::SketchEntity) {
    for (const auto& sketch : p.window.session().part_document()->sketches()) {
      const auto prefix = "sketch/" + sketch.id().value() + "/";
      if (!selection.semantic_id.starts_with(prefix))
        continue;
      select(p.ui->sketch, sketch.id().value());
      const auto suffix = selection.semantic_id.substr(prefix.size());
      if (suffix.starts_with("entity/") && p.ui->mode->currentIndex() < 2) {
        p.ui->source_kind->setCurrentIndex(0);
        if (select(p.ui->curve, suffix.substr(7))) {
          p.add_curve();
          return true;
        }
      }
      if (suffix.starts_with("profile/")) {
        p.ui->source_kind->setCurrentIndex(1);
        return select(p.ui->profile, suffix.substr(8));
      }
    }
  }
  if (selection.kind == GuiSelectionKind::Face) {
    for (const auto& target : p.targets)
      if (target.selection.semantic_id == selection.semantic_id) {
        const auto key =
            interactive_face_identity(std::get<FaceReference>(target.selection.reference));
        if (p.ui->mode->currentIndex() < 2) {
          p.ui->extent->setCurrentIndex(1);
          return select(p.ui->limit_face, key);
        }
        if (select(p.ui->face, key)) {
          p.add_face();
          return true;
        }
        p.fail(error("The selected support belongs to a different target Body."));
        return true;
      }
  }
  return false;
}
bool ShellSketchDrivenTools::eventFilter(QObject* watched, QEvent* event) {
  auto& p = *impl_;
  if (!p.active || p.source_edit)
    return QObject::eventFilter(watched, event);
  if (event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    if (!p.layer.active_handle_id().empty()) {
      p.layer.cancel();
      if (p.drag_field) {
        p.loading = true;
        p.drag_field->setText(p.drag_start);
        p.loading = false;
      }
      p.ui->scroll->setEnabled(true);
      p.ui->grip_numeric->setEnabled(false);
      p.window.viewport().release_interaction_mouse();
      (void)preview();
    } else
      cancel();
    return true;
  }
  if (event->type() == QEvent::KeyPress && !p.layer.active_handle_id().empty() &&
      (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Enter)) {
    if (!p.numeric_valid)
      return true;
    auto final = p.layer.end_drag(p.grip_pointer);
    p.ui->scroll->setEnabled(true);
    p.ui->grip_numeric->setEnabled(false);
    p.window.viewport().release_interaction_mouse();
    if (final)
      p.apply_grip(final.value());
    p.drag_field = nullptr;
    return true;
  }
  if (watched != &p.window.viewport())
    return QObject::eventFilter(watched, event);
  if (event->type() == QEvent::Resize) {
    p.overlay_refresh();
    return false;
  }
  if (event->type() == QEvent::MouseButtonPress) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() == Qt::LeftButton && p.valid) {
      p.overlay_refresh();
      if (p.layer.begin_drag({mouse->position().x(), mouse->position().y()})) {
        auto field = p.grip_fields.find(std::string(p.layer.active_handle_id()));
        if (field == p.grip_fields.end()) {
          p.layer.cancel();
          return false;
        }
        p.drag_field = field->second;
        p.drag_start = p.drag_field->text();
        p.numeric_valid = true;
        p.grip_pointer = {mouse->position().x(), mouse->position().y()};
        p.ui->grip_numeric->setEnabled(true);
        p.ui->grip_numeric->setText(
            p.drag_start + (p.layer.candidate() && p.layer.candidate()->value_kind ==
                                                       GuiViewportManipulatorValueKind::Angle
                                ? " deg"
                                : " mm"));
        p.ui->grip_numeric->setFocus();
        p.ui->grip_numeric->selectAll();
        p.ui->scroll->setEnabled(false);
        p.window.viewport().grab_interaction_mouse();
        return true;
      }
    }
  }
  if (event->type() == QEvent::MouseMove && !p.layer.active_handle_id().empty()) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    p.grip_pointer = {mouse->position().x(), mouse->position().y()};
    if (p.layer.update_drag(p.grip_pointer) && p.layer.candidate())
      p.apply_grip(*p.layer.candidate());
    return true;
  }
  if (event->type() == QEvent::MouseButtonRelease && !p.layer.active_handle_id().empty()) {
    auto* mouse = static_cast<QMouseEvent*>(event);
    auto final = p.numeric_valid ? p.layer.end_drag({mouse->position().x(), mouse->position().y()})
                                 : std::optional<GuiViewportManipulatorCandidate>{};
    if (!p.numeric_valid)
      p.layer.cancel();
    p.ui->grip_numeric->setEnabled(false);
    p.ui->scroll->setEnabled(true);
    p.window.viewport().release_interaction_mouse();
    if (final)
      p.apply_grip(final.value());
    else if (p.drag_field) {
      p.loading = true;
      p.drag_field->setText(p.drag_start);
      p.loading = false;
      (void)preview();
    }
    p.drag_field = nullptr;
    return true;
  }
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
