#include "advanced_surface_controls.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QLocale>
#include <QSignalBlocker>
#include <type_traits>
namespace blcad::gui {
Result<std::size_t> ShellFeatureTools::begin_surface_edit(FeatureId id) {
  const auto* part = session_.part_document();
  const auto* saved = part ? part->find_surface_feature(id) : nullptr;
  if (!saved)
    return Result<std::size_t>::failure(
        Error::validation("surface", "Select an existing Surface feature"));
  const auto record = *saved;
  const auto type = surface_feature_kind(record);
  const auto command = type == SurfaceFeatureKind::OffsetSurface ? Command::OffsetSurface
                       : type == SurfaceFeatureKind::Thicken     ? Command::Thicken
                       : type == SurfaceFeatureKind::TrimSurface ? Command::TrimSurface
                                                                 : Command::ExtendSurface;
  auto begun = begin_surface_task(command, id);
  if (!begun)
    return begun;
  auto& ui = *advanced_surface_;
  surface_feature_ = id;
  surface_result_ = surface_feature_result_body(record);
  surface_name_ = surface_feature_name(record);
  const auto number = [&](const ParameterId& parameter) {
    const auto* p = part->find_parameter(parameter);
    return p ? p->value().millimeters() : 0;
  };
  const auto choose_target = [&](const SurfaceReference& reference) {
    if (reference.source_kind() == SurfaceReferenceSourceKind::SelectedFaces) {
      const auto& selection = std::get<SurfaceFaceSelection>(reference.source());
      if (selection.faces.size() > 1) {
        const QSignalBlocker block(ui.face_set);
        ui.face_set->setChecked(true);
        ui.face_owner = selection.body;
        ui.face_refs = selection.faces;
        for (const auto& face : selection.faces) {
          auto single = SurfaceReference::create_faces(selection.body, {face});
          std::string label = "Saved face (repair or replace)";
          for (const auto& input : surface_inputs_)
            if (single && input.surface &&
                input.surface->stable_key() == single.value().stable_key())
              label = input.label;
          ui.selected_faces->addItem(QString::fromStdString(label));
        }
      }
    }
    for (int row = 1; row < surface_target_->count(); ++row) {
      const auto& input = surface_inputs_[surface_target_->itemData(row).toInt()];
      if (input.surface && input.surface->stable_key() == reference.stable_key()) {
        surface_target_->setCurrentIndex(row);
        return;
      }
    }
    // Preserve authored semantic sets and unavailable references for repair.
    const int index = surface_inputs_.size();
    surface_inputs_.push_back({"Saved source (select to replace)", "", {}, reference, {}});
    surface_target_->addItem("Saved source (select to replace)", index);
    surface_target_->setCurrentIndex(surface_target_->count() - 1);
  };
  const auto trim_key = [](const TrimmingReference& reference) {
    return std::visit(
        [](const auto& source) {
          using T = std::decay_t<decltype(source)>;
          if constexpr (std::is_same_v<T, ProfileRegionReference>)
            return source.sketch().value() + "/" + source.profile().value();
          else
            return source.stable_key();
        },
        reference.source());
  };
  const auto choose_source = [&](const auto& reference) {
    for (int row = 1; row < surface_source_->count(); ++row) {
      const auto& input = surface_inputs_[surface_source_->itemData(row).toInt()];
      bool match = false;
      if constexpr (std::is_same_v<std::decay_t<decltype(reference)>, TrimmingReference>)
        match = input.trimming && trim_key(*input.trimming) == trim_key(reference);
      else
        match = input.boundary && input.boundary->stable_key() == reference.stable_key();
      if (match) {
        surface_source_->setCurrentIndex(row);
        return;
      }
    }
    const int index = surface_inputs_.size();
    if constexpr (std::is_same_v<std::decay_t<decltype(reference)>, TrimmingReference>)
      surface_inputs_.push_back({"Saved contour (select to replace)", "", {}, {}, reference});
    else
      surface_inputs_.push_back({"Saved boundary (select to replace)", "", reference, {}, {}});
    surface_source_->addItem(QString::fromStdString(surface_inputs_.back().label), index);
    surface_source_->setCurrentIndex(surface_source_->count() - 1);
  };
  {
    const QSignalBlocker a(ui.side), b(ui.trim_side), c(ui.projection), d(ui.extension),
        e(ui.approximation), f(ui.operation), g(ui.body), h(ui.tolerance), j(ui.deviation),
        k(surface_target_), l(surface_source_);
    std::visit(
        [&](const auto& feature) {
          using T = std::decay_t<decltype(feature)>;
          if constexpr (std::is_same_v<T, SurfaceOffsetFeature>) {
            choose_target(feature.target());
            const auto& parameters = feature.parameters();
            surface_parameter_ = parameters.distance;
            surface_value_ = number(parameters.distance);
            ui.side->setCurrentIndex(int(parameters.side));
            ui.approximation->setCurrentIndex(int(parameters.approximation));
            ui.tolerance->setText(
                QString::number(number(parameters.tolerance), 'g', QLocale::FloatingPointShortest));
            if (parameters.maximum_deviation)
              ui.deviation->setText(QString::number(number(*parameters.maximum_deviation), 'g',
                                                    QLocale::FloatingPointShortest));
            const auto& context = feature.body_result_context();
            ui.operation->setCurrentIndex(int(context.operation_mode()));
            if (context.target_body())
              ui.body->setCurrentIndex(
                  ui.body->findData(QString::fromStdString(context.target_body()->value())));
          } else if constexpr (std::is_same_v<T, TrimSurfaceFeature>) {
            choose_target(feature.target());
            choose_source(feature.trimming());
            if (feature.options()) {
              const auto& options = *feature.options();
              ui.trim_side->setCurrentIndex(int(options.side));
              ui.projection->setCurrentIndex(int(options.projection));
              ui.tolerance->setText(
                  QString::number(number(options.tolerance), 'g', QLocale::FloatingPointShortest));
              if (options.direction) {
                const std::array<double, 3> xyz{options.direction->x, options.direction->y,
                                                options.direction->z};
                for (int i = 0; i < 3; ++i) {
                  const QSignalBlocker block(ui.direction[i]);
                  ui.direction[i]->setText(
                      QString::number(xyz[i], 'g', QLocale::FloatingPointShortest));
                }
              }
            }
          } else if constexpr (std::is_same_v<T, ExtendSurfaceFeature>) {
            choose_target(feature.target());
            choose_source(feature.boundary());
            surface_parameter_ = feature.distance_parameter();
            surface_value_ = number(surface_parameter_);
            if (feature.options()) {
              const auto& options = *feature.options();
              ui.extension->setCurrentIndex(int(options.mode));
              ui.tolerance->setText(
                  QString::number(number(options.tolerance), 'g', QLocale::FloatingPointShortest));
              if (options.maximum_deviation)
                ui.deviation->setText(QString::number(number(*options.maximum_deviation), 'g',
                                                      QLocale::FloatingPointShortest));
            }
          }
        },
        record);
  }
  ui.configure(type);
  show_task_panel(QString("Edit %1").arg(QString::fromStdString(surface_name_)), "mm",
                  surface_value_);
  update_surface_pick_scope();
  refresh_surface_preview();
  window_.refresh();
  return begun;
}
} // namespace blcad::gui
