#include "advanced_fillet_controls.hpp"
#include "blcad/geometry/fillet_guides.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QSignalBlocker>
namespace blcad::gui {
bool ShellFeatureTools::typed_finishing() const noexcept {
  return command_ == Command::Fillet && advanced_fillet_ &&
         (advanced_fillet_->editing || finishing_mode_->currentIndex() > 0);
}
Result<std::size_t> ShellFeatureTools::begin_fillet_edit(FeatureId id) {
  return begin_finishing(Command::Fillet, std::move(id));
}
void ShellFeatureTools::show_fillet_failure(const Error& error) {
  auto& controls = *advanced_fillet_;
  controls.failure = true;
  std::string location = controls.name;
  if (controls.current_mode() == 1)
    for (const auto& group : controls.groups) {
      if (group.id == error.object_id())
        location = group.name;
      for (std::size_t row = 0; row < group.stations.size(); ++row)
        if (group.stations[row].id == error.object_id())
          location = group.name + " / Station " + std::to_string(row + 1);
    }
  if (controls.current_mode() >= 2)
    for (const auto* role : {&controls.first, &controls.second, &controls.middle})
      if (role->id == error.object_id())
        location = role->name;
  if (controls.policy && controls.policy->id == error.object_id())
    location = "Setback";
  controls.capability->setToolTip(QString::fromStdString(error.object_id()));
  controls.capability->setText(QString::fromStdString(location + ": " + error.message()));
  bool focus_changed = false;
  for (std::size_t i = 0; i < controls.groups.size(); ++i) {
    const auto& group = controls.groups[i];
    auto station = std::find_if(group.stations.begin(), group.stations.end(),
                                [&](const auto& value) { return value.id == error.object_id(); });
    if (group.id != error.object_id() && station == group.stations.end())
      continue;
    if (controls.group->currentIndex() != int(i)) {
      QSignalBlocker block(controls.group);
      controls.group->setCurrentIndex(int(i));
      controls.configure();
      focus_changed = true;
    }
    if (station != group.stations.end())
      controls.stations->setCurrentCell(int(station - group.stations.begin()), 1);
    break;
  }
  if (controls.current_mode() >= 2) {
    const std::array<const FilletFaceSet*, 3> roles{&controls.first, &controls.second,
                                                    &controls.middle};
    for (int i = 0; i < controls.group->count(); ++i)
      if (roles[i]->id == error.object_id() && controls.group->currentIndex() != i) {
        QSignalBlocker block(controls.group);
        controls.group->setCurrentIndex(i);
        controls.configure();
        focus_changed = true;
        break;
      }
  }
  if (focus_changed)
    reload_finishing_targets(false);
  std::vector<GuiViewportManipulatorHandle> markers;
  if (controls.live && controls.input_cache && controls.input_document &&
      controls.live->record.advanced_intent() &&
      std::holds_alternative<VariableFilletIntent>(
          controls.live->record.advanced_intent()->definition)) {
    auto guides = query_with_step_import_context(session_, [&](auto* context) {
      return geometry::FilletGuideQuery::stations(*controls.input_document, controls.live->record,
                                                  *controls.input_cache, context,
                                                  controls.input_prefix);
    });
    if (guides) {
      const bool exact =
          std::any_of(guides.value().begin(), guides.value().end(),
                      [&](const auto& guide) { return guide.station == error.object_id(); });
      const bool exact_group =
          std::any_of(guides.value().begin(), guides.value().end(),
                      [&](const auto& guide) { return guide.group == error.object_id(); });
      for (const auto& guide : guides.value()) {
        if ((exact && guide.station != error.object_id()) ||
            (!exact && exact_group && guide.group != error.object_id()))
          continue;
        GuiViewportManipulatorHandle marker;
        marker.id = "fillet.failure/" + std::to_string(guide.group.size()) + ":" + guide.group +
                    "/" + guide.station;
        marker.origin = guide.anchor;
        marker.axis = guide.radius_axis;
        marker.interactive = false;
        marker.label = "Failure: " + location;
        markers.push_back(marker);
      }
    }
  }
  if (markers.empty()) {
    std::vector<std::string> keys;
    if (controls.current_mode() == 1) {
      for (const auto& group : controls.groups)
        for (const auto& edge : group.edges)
          keys.push_back(interactive_edge_identity(edge.edge));
    } else if (controls.current_mode() >= 2) {
      for (const auto& face : controls.first.faces)
        keys.push_back(interactive_face_identity(face));
      for (const auto& face : controls.second.faces)
        keys.push_back(interactive_face_identity(face));
      if (controls.current_mode() == 3)
        for (const auto& face : controls.middle.faces)
          keys.push_back(interactive_face_identity(face));
    } else
      keys = controls.selected_keys();
    for (const auto& target : finishing_targets_) {
      const auto key = std::visit(
          [](const auto& reference) {
            using T = std::decay_t<decltype(reference)>;
            if constexpr (std::is_same_v<T, EdgeReference>)
              return interactive_edge_identity(reference);
            else
              return interactive_face_identity(reference);
          },
          target.reference);
      if (std::find(keys.begin(), keys.end(), key) == keys.end())
        continue;
      GuiViewportManipulatorHandle marker;
      marker.id = "fillet.failure/input/" + key;
      marker.origin = target.anchor;
      marker.interactive = false;
      marker.label = "Failure: " + location;
      markers.push_back(marker);
    }
  }
  (void)layer_.set_handles(std::move(markers));
  controls.capability->setText(QString::fromStdString(location + ": " + error.message()));
}
Result<std::size_t> ShellFeatureTools::rebuild_advanced_fillet_controller() {
  auto& controls = *advanced_fillet_;
  if (!controls.current(session_))
    return Result<std::size_t>::failure(
        Error::validation("fillet", "The model changed. Reopen the Fillet editor."));
  controls.body = BodyId(finishing_body_->currentData().toString().toStdString());
  auto state = controls.build();
  if (!state)
    return Result<std::size_t>::failure(state.error());
  auto candidate = *session_.part_document();
  auto staged = state.value()->apply(candidate);
  if (!staged)
    return staged;
  if (!controls.editing && session_.history_prefix_count()) {
    auto scoped = session_.prepare_part_history_candidate(candidate);
    if (!scoped)
      return scoped;
  }
  const auto history = candidate.executable_history();
  if (!history)
    return Result<std::size_t>::failure(history.error());
  const auto found =
      std::find_if(history.value().begin(), history.value().end(),
                   [&](const auto& row) { return row.node_id == finishing_feature_.value(); });
  if (found == history.value().end())
    return Result<std::size_t>::failure(
        Error::dependency(finishing_feature_.value(), "Fillet input history is unavailable"));
  const auto prefix = std::size_t(found - history.value().begin());
  auto cache = geometry::ShapeCache::create(ShapeCacheId("fillet.edit.inputs"));
  if (!cache)
    return Result<std::size_t>::failure(cache.error());
  auto prepared = query_with_step_import_context(session_, [&](auto* context) {
    return geometry::GeometryRecomputeExecutor(context).execute_history_prefix(candidate, prefix,
                                                                               cache.value());
  });
  if (!prepared)
    return Result<std::size_t>::failure(prepared.error());
  controls.input_document = std::move(candidate);
  controls.input_cache = std::move(cache.value());
  controls.input_prefix = prefix;
  reload_finishing_targets(false);
  auto& draft = *state.value();
  if (draft.record.advanced_intent() &&
      std::holds_alternative<VariableFilletIntent>(draft.record.advanced_intent()->definition)) {
    auto guides = query_with_step_import_context(session_, [&](auto* context) {
      return geometry::FilletGuideQuery::stations(*controls.input_document, draft.record,
                                                  *controls.input_cache, context, prefix);
    });
    if (!guides)
      return Result<std::size_t>::failure(guides.error());
    const auto* active = controls.active_group();
    bool direction = false;
    for (const auto& guide : guides.value()) {
      if (active && guide.group != active->id)
        continue;
      GuiViewportManipulatorHandle handle;
      handle.id = "fillet.station/" + std::to_string(guide.group.size()) + ":" + guide.group + "/" +
                  guide.station;
      handle.origin = guide.anchor;
      handle.axis = guide.radius_axis;
      handle.reference_value = draft.value(*session_.part_document(), guide.radius);
      handle.minimum_value = 1e-7;
      draft.handles.push_back(handle);
      draft.handle_bindings.emplace(handle.id, guide.radius);
      if (!direction) {
        GuiViewportManipulatorHandle arrow;
        arrow.id = "fillet.direction/" + guide.group;
        arrow.origin = guide.basis_origin;
        arrow.axis = guide.basis_direction;
        arrow.interactive = false;
        arrow.label = "Law Direction";
        arrow.display_size_dip = 45;
        draft.handles.push_back(arrow);
        direction = true;
      }
    }
  } else if (draft.record.advanced_intent() &&
             std::holds_alternative<FullRoundIntent>(draft.record.advanced_intent()->definition)) {
    auto size = query_with_step_import_context(session_, [&](auto* context) {
      return geometry::FilletGuideQuery::full_round_half_width(
          *controls.input_document, draft.record, *controls.input_cache, context, prefix);
    });
    if (!size)
      return Result<std::size_t>::failure(size.error());
    const auto& intent = std::get<FullRoundIntent>(draft.record.advanced_intent()->definition);
    controls.derived->setText(QString(intent.continuity == FilletContinuity::Tangent
                                          ? "Derived Radius: %1 mm"
                                          : "Derived Half-width: %1 mm")
                                  .arg(AdvancedFilletControls::number(size.value())));
  } else {
    const auto parameters = draft.record.parameter_dependencies();
    if (!parameters.empty()) {
      GuiViewportManipulatorHandle handle;
      handle.id = std::string(kFilletRadiusHandleId);
      handle.reference_value = draft.value(*session_.part_document(), parameters.front());
      handle.minimum_value = 1e-7;
      const auto keys = controls.selected_keys();
      for (const auto& target : finishing_targets_) {
        const auto key = std::visit(
            [](const auto& r) {
              using T = std::decay_t<decltype(r)>;
              if constexpr (std::is_same_v<T, EdgeReference>)
                return interactive_edge_identity(r);
              else
                return interactive_face_identity(r);
            },
            target.reference);
        if (std::find(keys.begin(), keys.end(), key) != keys.end()) {
          handle.origin = target.anchor;
          break;
        }
      }
      draft.handles.push_back(handle);
      draft.handle_bindings.emplace(handle.id, parameters.front());
    }
  }
  auto begun = coordinator_.finishing().begin_draft(session_, state.value());
  if (begun)
    coordinator_.refresh_handles();
  return begun;
}
} // namespace blcad::gui
