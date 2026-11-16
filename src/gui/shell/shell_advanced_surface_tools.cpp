#include "advanced_surface_controls.hpp"
#include "blcad/geometry/surface_boundary_query.hpp"
#include "blcad/geometry/surface_offset_adapter.hpp"
#include "blcad/geometry/surface_selection_geometry.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_reference_repair_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QListWidget>
#include <QPushButton>
#include <QUuid>
#include <algorithm>
#include <cmath>
namespace blcad::gui {
namespace {
Result<std::size_t> fail(std::string text) {
  return Result<std::size_t>::failure(Error::validation("surface", std::move(text)));
}
SurfaceFeatureKind kind(ShellFeatureTools::Command command) {
  using C = ShellFeatureTools::Command;
  if (command == C::BoundarySurface)
    return SurfaceFeatureKind::BoundarySurface;
  if (command == C::FillSurface)
    return SurfaceFeatureKind::FillSurface;
  if (command == C::SurfaceStitch)
    return SurfaceFeatureKind::SurfaceStitch;
  if (command == C::ClosedShellToSolid)
    return SurfaceFeatureKind::ClosedShellToSolid;
  return command == C::OffsetSurface ? SurfaceFeatureKind::OffsetSurface
         : command == C::Thicken     ? SurfaceFeatureKind::Thicken
         : command == C::TrimSurface ? SurfaceFeatureKind::TrimSurface
                                     : SurfaceFeatureKind::ExtendSurface;
}

} // namespace
bool ShellFeatureTools::advanced_surface() const noexcept {
  return command_ == Command::OffsetSurface || command_ == Command::Thicken ||
         command_ == Command::TrimSurface || command_ == Command::ExtendSurface;
}
void ShellFeatureTools::build_advanced_surface_controls(QWidget* parent) {
  advanced_surface_ = std::make_unique<AdvancedSurfaceControls>(parent);
  auto& ui = *advanced_surface_;
  build_surface_input_controls();
  surface_form_->addRow(ui.panel);
  for (auto* input : {ui.side, ui.trim_side, ui.projection, ui.extension, ui.approximation,
                      ui.operation, ui.body})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (surface()) {
        advanced_surface_->configure(kind(command_));
        refresh_surface_preview();
      }
    });
  for (auto* input : {ui.tolerance, ui.deviation})
    connect(input, &QLineEdit::textChanged, this, [this] {
      if (advanced_surface())
        refresh_surface_preview();
    });
  for (auto* input : ui.direction)
    connect(input, &QLineEdit::textChanged, this, [this] {
      if (advanced_surface())
        refresh_surface_preview();
    });
  for (auto* input : {ui.boundaries, ui.override_formulas})
    connect(input, &QCheckBox::toggled, this, [this] {
      if (surface())
        refresh_surface_preview();
    });
  auto* repair = new QPushButton("Repair References", ui.panel);
  repair->setObjectName("blcad.surface.repair");
  ui.form->addRow(repair);
  connect(repair, &QPushButton::clicked, this, [this] {
    cancel();
    auto begun = window_.reference_repair_tools().begin();
    if (!begun)
      report(begun.error());
  });
}
Result<std::size_t> ShellFeatureTools::rebuild_advanced_surface_controller() {
  auto& ui = *advanced_surface_;
  ui.configure(kind(command_));
  auto& controller = coordinator_.surface();
  const auto* part = session_.part_document();
  if (!part)
    return fail("Open an editable Part");
  const auto selected = [&](QComboBox* box) -> const SurfaceInput* {
    const int i = box->currentData().toInt();
    return i >= 0 && std::size_t(i) < surface_inputs_.size() ? &surface_inputs_[i] : nullptr;
  };
  const auto* source = selected(surface_source_);
  auto target = selected_surface_target();
  if (!target)
    return Result<std::size_t>::failure(target.error());
  bool numeric = false;
  const double tolerance = ui.tolerance->text().toDouble(&numeric);
  if (!numeric || !std::isfinite(tolerance) || tolerance < 1e-9 || tolerance > 1)
    return fail("Enter a tolerance from 1e-9 to 1 mm");
  const bool offset = command_ == Command::OffsetSurface || command_ == Command::Thicken;
  const bool approximation = offset && ui.approximation->currentIndex() == 1;
  const bool tangent = command_ == Command::ExtendSurface && ui.extension->currentIndex() == 1;
  double deviation = 0;
  if (approximation || tangent) {
    deviation = ui.deviation->text().toDouble(&numeric);
    if (!numeric || !std::isfinite(deviation) || deviation <= 0)
      return fail("Enter a positive maximum deviation in mm");
  }
  std::vector<Parameter> pending;
  std::vector<std::pair<ParameterId, double>> changes;
  std::vector<std::pair<ParameterId, double>> bindings;
  const auto length = [&](ParameterId original, double value, int slot) -> Result<ParameterId> {
    const auto* previous = part->find_parameter(original);
    ParameterId id = original;
    const bool changed = previous && previous->value().millimeters() != value;
    if (changed && previous->is_expression() && !ui.override_formulas->isChecked())
      return Result<ParameterId>::failure(
          Error::validation(original.value(), "Formula-bound value: enable the explicit "
                                              "independent-value override before changing it"));
    if (changed && ui.override_formulas->isChecked()) {
      if (ui.override_ids[slot].empty())
        ui.override_ids[slot] = ParameterId(
            "surface.override." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
      id = ui.override_ids[slot];
      previous = nullptr;
    }
    const auto shared = std::find_if(bindings.begin(), bindings.end(),
                                     [&](const auto& binding) { return binding.first == id; });
    if (shared != bindings.end() && shared->second != value)
      return Result<ParameterId>::failure(
          Error::validation(id.value(), "Shared parameter fields need equal values; enable "
                                        "independent changed values to split their bindings"));
    bindings.emplace_back(id, value);
    if (!previous) {
      auto quantity = Quantity::length_mm(value, id.value());
      if (!quantity)
        return Result<ParameterId>::failure(quantity.error());
      auto parameter = Parameter::create_length(
          id,
          surface_name_ +
              (slot == 0   ? (command_ == Command::Thicken ? " Thickness" : " Distance")
               : slot == 1 ? " Tolerance"
                           : " Deviation") +
              (id == original ? "" : " Independent"),
          quantity.value());
      if (!parameter)
        return Result<ParameterId>::failure(parameter.error());
      pending.push_back(parameter.value());
    } else if (previous->value().millimeters() != value)
      changes.push_back({id, value});
    return Result<ParameterId>::success(id);
  };
  ParameterId distance_id = surface_parameter_,
              tolerance_id(surface_parameter_.value() + ".tolerance"),
              deviation_id(surface_parameter_.value() + ".deviation");
  const SurfaceFeature* original = ui.editing ? part->find_surface_feature(*ui.editing) : nullptr;
  if (original)
    std::visit(
        [&](const auto& record) {
          using T = std::decay_t<decltype(record)>;
          if constexpr (std::is_same_v<T, SurfaceOffsetFeature>) {
            distance_id = record.parameters().distance;
            tolerance_id = record.parameters().tolerance;
            if (record.parameters().maximum_deviation)
              deviation_id = *record.parameters().maximum_deviation;
          }
          if constexpr (std::is_same_v<T, TrimSurfaceFeature> ||
                        std::is_same_v<T, ExtendSurfaceFeature>) {
            if (record.options()) {
              tolerance_id = record.options()->tolerance;
              if constexpr (std::is_same_v<T, ExtendSurfaceFeature>)
                if (record.options()->maximum_deviation)
                  deviation_id = *record.options()->maximum_deviation;
            }
          }
          if constexpr (std::is_same_v<T, ExtendSurfaceFeature>)
            distance_id = record.distance_parameter();
        },
        *original);
  auto tol = length(tolerance_id, tolerance, 1);
  if (!tol)
    return Result<std::size_t>::failure(tol.error());
  std::optional<ParameterId> dev;
  if (approximation || tangent) {
    auto value = length(deviation_id, deviation, 2);
    if (!value)
      return Result<std::size_t>::failure(value.error());
    dev = value.value();
  }
  std::optional<Body> body;
  FeatureBodyOperationMode mode = FeatureBodyOperationMode::NewBody;
  if (command_ == Command::Thicken)
    mode = static_cast<FeatureBodyOperationMode>(ui.operation->currentIndex());
  if (!ui.editing && mode == FeatureBodyOperationMode::NewBody) {
    auto created = Body::create(surface_result_, surface_name_,
                                command_ == Command::Thicken ? BodyKind::Solid : BodyKind::Surface);
    if (!created)
      return Result<std::size_t>::failure(created.error());
    body = created.value();
  }
  if (ui.editing) {
    auto begun = controller.begin_edit(session_, *ui.editing);
    if (!begun)
      return begun;
  }
  Result<std::size_t> begun = Result<std::size_t>::success(1);
  if (offset) {
    auto distance = length(distance_id, surface_value_, 0);
    if (!distance)
      return Result<std::size_t>::failure(distance.error());
    const BodyId target_body(ui.body->currentData().toString().toStdString());
    auto context = FeatureBodyResultContext::create(
        mode, mode == FeatureBodyOperationMode::NewBody ? std::nullopt : std::optional(target_body),
        mode == FeatureBodyOperationMode::NewBody ? std::optional(surface_result_) : std::nullopt);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    SurfaceOffsetParameters parameters{
        distance.value(), tol.value(), static_cast<SurfaceOffsetSide>(ui.side->currentIndex()),
        approximation ? SurfaceOffsetApproximation::VerifiedApproximation
                      : SurfaceOffsetApproximation::ExactAnalytic,
        dev};
    if (ui.editing)
      controller.set_offset_intent(parameters, context.value());
    else
      begun = controller.begin_offset(session_, surface_feature_, surface_name_, kind(command_),
                                      target.value(), parameters, context.value(), body, pending);
  } else if (command_ == Command::TrimSurface) {
    if (!source || !source->trimming)
      return fail("Select a closed trimming contour");
    if (!ui.editing)
      begun = controller.begin_trim(session_, surface_feature_, surface_name_, target.value(),
                                    *source->trimming, surface_result_, body);
    controller.set_trimming(*source->trimming);
    std::optional<Vector3> direction;
    if (ui.projection->currentIndex() == 1) {
      std::array<double, 3> xyz;
      for (int i = 0; i < 3; ++i) {
        bool valid = false;
        xyz[i] = ui.direction[i]->text().toDouble(&valid);
        if (!valid || !std::isfinite(xyz[i]))
          return fail("Enter three finite projection-vector components");
      }
      direction = Vector3{xyz[0], xyz[1], xyz[2]};
    }
    controller.set_trim_options(SurfaceTrimOptions{
        static_cast<SurfaceTrimSide>(ui.trim_side->currentIndex()),
        static_cast<SurfaceTrimProjection>(ui.projection->currentIndex()), direction, tol.value()});
  } else {
    if (!source || !source->boundary)
      return fail("Select one complete free native boundary");
    auto distance = length(distance_id, surface_value_, 0);
    if (!distance)
      return Result<std::size_t>::failure(distance.error());
    if (!ui.editing) {
      std::vector<Parameter> driving;
      for (const auto& value : pending)
        if (value.id() == distance.value())
          driving.push_back(value);
      begun = controller.begin_extend(session_, surface_feature_, surface_name_, target.value(),
                                      *source->boundary, distance.value(), surface_result_, body,
                                      driving);
    }
    controller.set_distance_binding(distance.value());
    controller.set_boundary(*source->boundary);
    controller.set_extend_options(SurfaceExtendOptions{
        static_cast<SurfaceExtendMode>(ui.extension->currentIndex()), tol.value(), dev});
  }
  if (!begun)
    return begun;
  controller.set_target(target.value());
  controller.set_pending_parameters(pending);
  controller.set_numeric_parameters(changes);
  if (source && source->pending_path)
    controller.set_pending_paths({*source->pending_path});
  if (command_ != Command::TrimSurface)
    controller.set_distance(surface_value_);
  // Direction and preview resolve the same native source. Pending edge paths
  // exist only in this disposable candidate and are committed with the feature.
  auto candidate = *part;
  for (const auto& [id, value] : changes) {
    auto quantity = Quantity::length_mm(value, id.value());
    if (!quantity)
      return Result<std::size_t>::failure(quantity.error());
    auto updated = candidate.set_parameter_value(id, quantity.value());
    if (!updated)
      return Result<std::size_t>::failure(updated.error());
  }
  for (const auto& path : source && source->pending_path
                              ? std::vector<PathCurve>{*source->pending_path}
                              : std::vector<PathCurve>{}) {
    auto added = candidate.add_path_curve(path);
    if (!added)
      return added;
  }
  auto frame = query_with_step_import_context(
      session_, [&](auto* context) -> Result<GuiFeatureManipulatorFrame> {
        GuiFeatureManipulatorFrame result;
        const auto* cache = ui.input_cache ? &*ui.input_cache : session_.part_shape_cache();
        auto pending_cache = geometry::ShapeCache::create(ShapeCacheId("surface.pending.input"));
        if ((source && source->pending_path) || !changes.empty()) {
          if (!pending_cache)
            return Result<GuiFeatureManipulatorFrame>::failure(pending_cache.error());
          auto computed = ui.input_prefix
                              ? geometry::GeometryRecomputeExecutor{context}.execute_history_prefix(
                                    candidate, *ui.input_prefix, pending_cache.value())
                              : geometry::GeometryRecomputeExecutor{context}.execute_document(
                                    candidate, pending_cache.value());
          if (!computed)
            return Result<GuiFeatureManipulatorFrame>::failure(computed.error());
          cache = &pending_cache.value();
        }
        if (command_ == Command::ExtendSurface) {
          auto value = geometry::GeometryRecomputeExecutor{context}.surface_extension_frame(
              candidate, target.value(), *source->boundary, *cache);
          if (!value)
            return Result<GuiFeatureManipulatorFrame>::failure(value.error());
          result.origin = value.value().origin;
          result.axis = value.value().direction;
        } else if (offset) {
          const auto consumer =
              target.value().source_kind() == SurfaceReferenceSourceKind::SemanticFace
                  ? FeatureId("surface.direction.preview")
                  : FeatureId{};
          auto shape = geometry::SurfaceSelectionGeometry::resolve(
              candidate, *cache, target.value(), consumer, tolerance, context);
          if (!shape)
            return Result<GuiFeatureManipulatorFrame>::failure(shape.error());
          auto value = geometry::SurfaceSelectionGeometry::normal_frame(shape.value());
          if (!value)
            return Result<GuiFeatureManipulatorFrame>::failure(value.error());
          result.origin = value.value().origin;
          result.axis = value.value().normal;
        }
        return Result<GuiFeatureManipulatorFrame>::success(result);
      });
  if (!frame)
    return Result<std::size_t>::failure(frame.error());
  controller.set_manipulator_frame(frame.value());
  if (layer_.active_handle_id().empty())
    coordinator_.refresh_handles();
  return begun;
}
void ShellFeatureTools::refresh_surface_diagnostics(const geometry::GeometryShape* result) {
  if (!surface() || !advanced_surface_)
    return;
  auto& ui = *advanced_surface_;
  QStringList messages;
  if (result)
    if (const auto report = geometry::SurfaceOffsetAdapter::verification(*result)) {
      const char* criterion =
          report->criterion == geometry::SurfaceDistanceCriterion::OffsetDistance
              ? "Normal offset distance"
          : report->criterion == geometry::SurfaceDistanceCriterion::WallThickness
              ? "Normal wall thickness"
              : "Normal skin separation";
      messages << (QString(criterion) +
                   QString(" %1…%2 mm; maximum error %3 mm ≤ %4 mm (%5 native stations).%6"))
                      .arg(report->minimum_mm, 0, 'g', 7)
                      .arg(report->maximum_mm, 0, 'g', 7)
                      .arg(report->maximum_error_mm, 0, 'g', 7)
                      .arg(report->permitted_error_mm, 0, 'g', 7)
                      .arg(report->stations)
                      .arg(report->before_body_operation
                               ? " Tool verification before Body operation."
                               : "");
    }
  std::vector<geometry::GeometryShape> sources;
  double tolerance =
      command_ == Command::SurfaceStitch ? surface_value_ : ui.tolerance->text().toDouble();
  auto context = make_step_import_context(session_);
  if (context) {
    const auto append = [&](const SurfaceReference& reference) {
      const auto consumer = reference.source_kind() == SurfaceReferenceSourceKind::SemanticFace
                                ? FeatureId("surface.diagnostics.preview")
                                : FeatureId{};
      auto shape = geometry::SurfaceSelectionGeometry::resolve(
          *session_.part_document(),
          ui.input_cache ? *ui.input_cache : *session_.part_shape_cache(), reference, consumer,
          tolerance, context.value().get());
      if (shape)
        sources.push_back(shape.value());
    };
    if (command_ == Command::SurfaceStitch)
      for (int row = 0; row < surface_order_->count(); ++row) {
        const auto& input = surface_inputs_[surface_order_->item(row)->data(Qt::UserRole).toInt()];
        if (input.surface)
          append(*input.surface);
      }
    else if (auto target = selected_surface_target(); target)
      append(target.value());
  }
  std::vector<geometry::GeometryShape> edges;
  bool invalid = result == nullptr;
  if (!sources.empty() && std::isfinite(tolerance) && tolerance >= 1e-9 && tolerance <= 1) {
    const auto report =
        geometry::SurfaceBoundaryQuery::inspect(sources, tolerance, std::max(.1, 10 * tolerance));
    if (report) {
      double maximum = 0;
      bool bad = false;
      for (const auto& gap : report.value().gaps) {
        maximum = std::max(maximum, gap.maximum_mm);
        bad |= !gap.within_tolerance;
      }
      invalid |= bad || report.value().non_manifold_edges > 0;
      messages << QString("Source boundaries: %1 free edges, %2 non-manifold.")
                      .arg(report.value().free_edges.size())
                      .arg(report.value().non_manifold_edges);
      if (report.value().gaps.empty())
        messages << "No complete seam pairs within the search radius.";
      else
        messages << QString("Nearby whole-edge gap maximum %1 mm, limit %2 mm.")
                        .arg(maximum, 0, 'g', 7)
                        .arg(tolerance, 0, 'g', 7);
      if (ui.boundaries->isChecked())
        for (const auto& edge : report.value().free_edges)
          edges.push_back(edge.shape);
    } else
      messages << QString::fromStdString(report.error().message());
  }
  (void)viewport_.set_geometry_diagnostics(std::move(edges), invalid);
  ui.feedback->setText(messages.join('\n'));
}

} // namespace blcad::gui
