#include "advanced_loft_controls.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QLocale>
#include <QSignalBlocker>
#include <algorithm>
namespace blcad::gui {
Result<std::size_t> ShellFeatureTools::begin_spatial_edit(FeatureId id) {
  const auto* part = session_.part_document();
  const auto* sweep = part ? part->find_sweep_feature(id) : nullptr;
  const auto* loft = part ? part->find_loft_feature(id) : nullptr;
  if (!sweep && !loft)
    return Result<std::size_t>::failure(
        Error::validation("spatial.edit", "Choose an editable Sweep or Loft"));
  const auto context = sweep ? sweep->body_result_context() : loft->body_result_context();
  auto begun = begin_spatial(sweep ? Command::Sweep : Command::Loft);
  if (!begun)
    return begun;
  spatial_editing_ = id;
  spatial_feature_ = id;
  spatial_result_ = context.effective_produced_body();
  spatial_name_ = sweep ? sweep->name() : loft->name();
  auto history = part->executable_history();
  if (!history) {
    cancel();
    return Result<std::size_t>::failure(history.error());
  }
  const auto entry = std::find_if(history.value().begin(), history.value().end(),
                                  [&](const auto& row) { return row.node_id == id.value(); });
  const auto prefix = std::size_t(entry - history.value().begin());
  auto cache = geometry::ShapeCache::create(ShapeCacheId("spatial.edit.inputs"));
  if (!cache) {
    cancel();
    return Result<std::size_t>::failure(cache.error());
  }
  auto computed = query_with_step_import_context(session_, [&](auto* context) {
    return geometry::GeometryRecomputeExecutor{context}.execute_history_prefix(*part, prefix,
                                                                               cache.value());
  });
  if (!computed) {
    cancel();
    return Result<std::size_t>::failure(computed.error());
  }
  spatial_input_cache_ = std::move(cache.value());
  spatial_input_prefix_ = prefix;
  const auto number = [](double value) {
    return QString::number(value, 'g', QLocale::FloatingPointShortest);
  };
  const auto choose_profile = [&](const std::variant<ProfileRegionReference, PathCurveId>& source) {
    for (std::size_t i = 0; i < spatial_inputs_.size(); ++i) {
      const auto& candidate = spatial_inputs_[i];
      if (const auto* p = std::get_if<ProfileRegionReference>(&source)) {
        if (candidate.profile && candidate.profile->first == p->sketch() &&
            candidate.profile->second == p->profile())
          return int(i);
      } else if (candidate.path && candidate.path == std::get<PathCurveId>(source))
        return int(i);
    }
    return -1;
  };
  {
    const QSignalBlocker mode(spatial_mode_), path(spatial_path_), guide(spatial_guide_),
        source(spatial_source_), target(spatial_target_), region(sweep_region_policy_),
        twist(sweep_twist_), order(spatial_order_), guides(spatial_guides_),
        alignment(spatial_auto_align_), continuity(spatial_continuity_),
        orientation(spatial_orientation_);
    spatial_mode_->setCurrentIndex(sweep ? int(sweep->kind()) : int(loft->kind()));
    if (context.target_body()) {
      const auto value = QString::fromStdString(context.target_body()->value());
      if (spatial_target_->findData(value) < 0)
        spatial_target_->addItem("Saved Target", value);
      spatial_target_->setCurrentIndex(spatial_target_->findData(value));
    }
    spatial_edit_context_->setText(
        QString("Editing %1; Body operation: %2. Authored result ownership is retained.")
            .arg(QString::fromStdString(spatial_name_),
                 QString::fromUtf8(to_string(context.operation_mode()).data(),
                                   int(to_string(context.operation_mode()).size()))));
    if (sweep) {
      const auto& profile = sweep->profile();
      const int selected = profile.kind() == SweepProfileKind::ClosedRegion
                               ? choose_profile(std::get<ProfileRegionReference>(profile.source()))
                               : choose_profile(std::get<PathCurveId>(profile.source()));
      spatial_source_->setCurrentIndex(selected);
      spatial_path_->setCurrentIndex(
          spatial_path_->findData(QString::fromStdString(sweep->path().value())));
      spatial_guide_->setCurrentIndex(
          sweep->guide_path()
              ? spatial_guide_->findData(QString::fromStdString(sweep->guide_path()->value()))
              : 0);
      sweep_region_policy_->setCurrentIndex(
          profile.region_policy() == SweepRegionPolicy::MaterialRegion ? 1 : 0);
      sweep_twist_->setChecked(sweep->twist_parameter().has_value());
      if (sweep->twist_parameter()) {
        spatial_twist_parameter_ = *sweep->twist_parameter();
        const auto* parameter = part->find_parameter(spatial_twist_parameter_);
        spatial_twist_ = parameter ? parameter->value().degrees() : 0;
      }
      spatial_orientation_->setCurrentIndex(
          sweep->orientation_override() ? int(*sweep->orientation_override()) : 3);
      if (sweep->fixed_up_vector_override()) {
        const auto v = *sweep->fixed_up_vector_override();
        const std::array<double, 3> xyz{v.x, v.y, v.z};
        for (int i = 0; i < 3; ++i) {
          const QSignalBlocker block(spatial_up_[i]);
          spatial_up_[i]->setText(number(xyz[i]));
        }
      }
    } else {
      spatial_saved_sections_ = loft->sections();
      spatial_auto_align_->setChecked(false);
      spatial_continuity_->setCurrentIndex(int(loft->continuity()));
      spatial_path_->setCurrentIndex(
          loft->path_curve()
              ? spatial_path_->findData(QString::fromStdString(loft->path_curve()->value()))
              : 0);
      for (const auto& section : loft->sections()) {
        int choice = choose_profile(section.source());
        if (choice < 0) {
          choice = int(spatial_inputs_.size());
          SpatialInput input{"Saved Section (repair or replace)", "", {}, {}, {}};
          if (section.kind() == LoftSectionKind::ClosedRegion) {
            const auto& p = std::get<ProfileRegionReference>(section.source());
            input.profile = std::pair{p.sketch(), p.profile()};
          } else
            input.path = std::get<PathCurveId>(section.source());
          spatial_inputs_.push_back(std::move(input));
          spatial_source_->addItem("Saved Section (repair or replace)", choice);
        }
        auto* item = new QListWidgetItem(QString::fromStdString(spatial_inputs_[choice].label),
                                         spatial_order_);
        item->setData(Qt::UserRole, choice);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(section.flip_normal() ? Qt::Checked : Qt::Unchecked);
      }
      for (const auto& rail : loft->guide_curves()) {
        const auto* p = part->find_path_curve(rail);
        auto* item = new QListWidgetItem(
            p ? QString::fromStdString(p->name()) : "Saved Guide (repair)", spatial_guides_);
        item->setData(Qt::UserRole, QString::fromStdString(rail.value()));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool reversed = false;
        if (loft->constraints())
          for (const auto& binding : loft->constraints()->guides)
            if (binding.guide == rail)
              reversed = binding.reversed;
        item->setCheckState(reversed ? Qt::Checked : Qt::Unchecked);
      }
    }
  }
  if (loft) {
    {
      const QSignalBlocker block(advanced_loft_->include_hidden);
      advanced_loft_->include_hidden->setChecked(true);
    }
    auto faces = refresh_loft_supports();
    if (!faces) {
      cancel();
      return faces;
    }
    advanced_loft_->sections = loft->sections();
    advanced_loft_->guides = loft->guide_curves();
    advanced_loft_->reversals.clear();
    for (int i = 0; i < spatial_guides_->count(); ++i)
      advanced_loft_->reversals.push_back(spatial_guides_->item(i)->checkState() == Qt::Checked);
    advanced_loft_->load(*part, loft->constraints());
  }
  coordinator_.cancel();
  show_task_panel("Edit " + QString::fromStdString(spatial_name_), "deg", spatial_twist_);
  window_.refresh_feature_selection_scene();
  update_spatial_pick_scope();
  refresh_spatial_preview();
  return begun;
}
} // namespace blcad::gui
