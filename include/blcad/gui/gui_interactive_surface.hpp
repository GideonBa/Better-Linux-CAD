#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_pending_feature_inputs.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include "blcad/core/parameter.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/surface_feature.hpp"

#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

// Block 128 interactive Surface authoring and closed-shell-to-solid conversion.
//
// One shared creation/edit controller over legacy and typed advanced Surface intent:
// Boundary/Fill collect boundary-curve chains; Trim picks a target surface and a
// trimming contour; Extend drags a boundary distance; Stitch collects surfaces
// with a typed tolerance; ClosedShellToSolid converts a closed shell. Because
// stitch free-edge/gap and closed-shell closedness/manifold diagnostics are
// geometric, preview runs the real recompute on a throwaway clone so the
// authoritative diagnostic is surfaced before commit. Apply commits one
// transaction that seeds any driving parameter and adds the record.
inline constexpr std::string_view kSurfaceToleranceValueId = "surface.stitch_tolerance";
inline constexpr std::string_view kSurfaceExtendHandleId = "surface.extend";
inline constexpr std::string_view kSurfaceOffsetHandleId = "surface.offset";

class GuiInteractiveSurfaceController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_boundary(const GuiDocumentSession& session, FeatureId feature, std::string name,
                 BodyId result_body, std::optional<Body> pending_body = std::nullopt) {
    return begin_curve_surface(session, SurfaceFeatureKind::BoundarySurface, std::move(feature),
                               std::move(name), std::move(result_body), std::move(pending_body));
  }
  [[nodiscard]] Result<std::size_t> begin_fill(const GuiDocumentSession& session, FeatureId feature,
                                               std::string name, BodyId result_body,
                                               std::optional<Body> pending_body = std::nullopt) {
    return begin_curve_surface(session, SurfaceFeatureKind::FillSurface, std::move(feature),
                               std::move(name), std::move(result_body), std::move(pending_body));
  }

  [[nodiscard]] Result<std::size_t> begin_trim(const GuiDocumentSession& session, FeatureId feature,
                                               std::string name, SurfaceReference target,
                                               TrimmingReference trimming, BodyId result_body,
                                               std::optional<Body> pending_body = std::nullopt) {
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body,
                               std::move(pending_body));
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::TrimSurface;
    target_ = std::move(target);
    trimming_ = std::move(trimming);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> begin_extend(const GuiDocumentSession& session,
                                                 FeatureId feature, std::string name,
                                                 SurfaceReference target,
                                                 BoundaryCurveReference boundary,
                                                 ParameterId distance_parameter, BodyId result_body,
                                                 std::optional<Body> pending_body = std::nullopt,
                                                 std::vector<Parameter> pending_parameters = {}) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive extend requires an active Part document");
    if (auto valid = detail::validate_new_feature_parameters(
            session, pending_parameters, {{distance_parameter, ParameterType::Length}});
        !valid)
      return valid;
    const Parameter* distance = part->find_parameter(distance_parameter);
    for (const auto& parameter : pending_parameters)
      if (parameter.id() == distance_parameter)
        distance = &parameter;
    if (distance == nullptr || distance->type() != ParameterType::Length)
      return fail("surface extend requires an existing Length distance parameter");
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body,
                               std::move(pending_body));
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::ExtendSurface;
    target_ = std::move(target);
    extend_boundary_ = std::move(boundary);
    distance_parameter_ = std::move(distance_parameter);
    distance_ = distance->value().millimeters();
    pending_parameters_ = std::move(pending_parameters);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_offset(const GuiDocumentSession& session, FeatureId feature, std::string name,
               SurfaceFeatureKind kind, SurfaceReference source, SurfaceOffsetParameters parameters,
               FeatureBodyResultContext context, std::optional<Body> pending_body = {},
               std::vector<Parameter> pending_parameters = {}) {
    if (kind != SurfaceFeatureKind::OffsetSurface && kind != SurfaceFeatureKind::Thicken)
      return fail("Choose Offset Surface or Thicken");
    std::vector<std::pair<ParameterId, ParameterType>> expected = {
        {parameters.distance, ParameterType::Length},
        {parameters.tolerance, ParameterType::Length}};
    if (parameters.maximum_deviation)
      expected.push_back({*parameters.maximum_deviation, ParameterType::Length});
    if (auto valid = detail::validate_new_feature_parameters(session, pending_parameters, expected);
        !valid)
      return valid;
    const Parameter* distance = session.part_document()->find_parameter(parameters.distance);
    for (const auto& value : pending_parameters)
      if (value.id() == parameters.distance)
        distance = &value;
    if (!distance || distance->type() != ParameterType::Length)
      return fail("Select a Length offset/thickness");
    if (auto begun = begin_common(session, std::move(feature), std::move(name),
                                  context.effective_produced_body(), std::move(pending_body));
        !begun)
      return begun;
    kind_ = kind;
    target_ = std::move(source);
    offset_parameters_ = std::move(parameters);
    offset_context_ = std::move(context);
    pending_parameters_ = std::move(pending_parameters);
    distance_parameter_ = offset_parameters_->distance;
    distance_ = distance->value().millimeters();
    return Result<std::size_t>::success(1);
  }
  // The same candidate authority serves creation and editing. Retain every
  // authored field and binding until the user explicitly changes that input.
  [[nodiscard]] Result<std::size_t> begin_edit(const GuiDocumentSession& session, FeatureId id) {
    const auto* part = session.part_document();
    const auto* record = part ? part->find_surface_feature(id) : nullptr;
    if (!record)
      return fail("Select an existing Surface feature");
    if (auto begun = begin_common(session, id, surface_feature_name(*record),
                                  surface_feature_result_body(*record), {});
        !begun)
      return begun;
    kind_ = surface_feature_kind(*record);
    editing_ = true;
    std::visit(
        [&](const auto& f) {
          using T = std::decay_t<decltype(f)>;
          if constexpr (std::is_same_v<T, BoundarySurfaceFeature> ||
                        std::is_same_v<T, FillSurfaceFeature>)
            boundaries_ = f.boundaries();
          else if constexpr (std::is_same_v<T, TrimSurfaceFeature>) {
            target_ = f.target();
            trimming_ = f.trimming();
            trim_options_ = f.options();
          } else if constexpr (std::is_same_v<T, ExtendSurfaceFeature>) {
            target_ = f.target();
            extend_boundary_ = f.boundary();
            distance_parameter_ = f.distance_parameter();
            extend_options_ = f.options();
          } else if constexpr (std::is_same_v<T, SurfaceStitchFeature>) {
            surfaces_ = f.surfaces();
            tolerance_parameter_ = f.tolerance_parameter();
          } else if constexpr (std::is_same_v<T, ClosedShellToSolidFeature>)
            target_ = f.shell();
          else {
            target_ = f.target();
            offset_parameters_ = f.parameters();
            offset_context_ = f.body_result_context();
            distance_parameter_ = f.parameters().distance;
          }
        },
        *record);
    if (const auto* value = part->find_parameter(distance_parameter_))
      distance_ = value->value().millimeters();
    if (const auto* value = part->find_parameter(tolerance_parameter_))
      tolerance_ = value->value().millimeters();
    return Result<std::size_t>::success(1);
  }
  void set_trim_options(std::optional<SurfaceTrimOptions> value) {
    trim_options_ = std::move(value);
  }
  void set_extend_options(std::optional<SurfaceExtendOptions> value) {
    extend_options_ = std::move(value);
  }
  void set_offset_side(SurfaceOffsetSide side) {
    if (offset_parameters_)
      offset_parameters_->side = side;
  }
  void set_offset_intent(SurfaceOffsetParameters parameters, FeatureBodyResultContext context) {
    offset_parameters_ = std::move(parameters);
    offset_context_ = std::move(context);
    distance_parameter_ = offset_parameters_->distance;
    result_body_ = offset_context_->effective_produced_body();
  }
  void set_distance_binding(ParameterId id) {
    distance_parameter_ = std::move(id);
  }
  void set_numeric_parameters(std::vector<std::pair<ParameterId, double>> values) {
    numeric_parameters_ = std::move(values);
  }
  void set_pending_parameters(std::vector<Parameter> values) {
    pending_parameters_ = std::move(values);
  }
  void set_pending_paths(std::vector<PathCurve> paths) {
    pending_paths_ = std::move(paths);
  }
  void set_target(SurfaceReference value) {
    target_ = std::move(value);
  }
  [[nodiscard]] bool editing() const noexcept {
    return editing_;
  }

  [[nodiscard]] Result<std::size_t> begin_stitch(const GuiDocumentSession& session,
                                                 FeatureId feature, std::string name,
                                                 ParameterId tolerance_parameter,
                                                 BodyId result_body,
                                                 std::optional<Body> pending_body = std::nullopt,
                                                 std::vector<Parameter> pending_parameters = {}) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive stitch requires an active Part document");
    if (auto valid = detail::validate_new_feature_parameters(
            session, pending_parameters, {{tolerance_parameter, ParameterType::Length}});
        !valid)
      return valid;
    const Parameter* tolerance = part->find_parameter(tolerance_parameter);
    for (const auto& parameter : pending_parameters)
      if (parameter.id() == tolerance_parameter)
        tolerance = &parameter;
    if (tolerance == nullptr || tolerance->type() != ParameterType::Length)
      return fail("surface stitch requires an existing Length tolerance parameter");
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body,
                               std::move(pending_body));
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::SurfaceStitch;
    tolerance_parameter_ = std::move(tolerance_parameter);
    tolerance_ = tolerance->value().millimeters();
    pending_parameters_ = std::move(pending_parameters);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_closed_shell_to_solid(const GuiDocumentSession& session, FeatureId feature,
                              std::string name, SurfaceReference shell, BodyId result_body,
                              std::optional<Body> pending_body = std::nullopt) {
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body,
                               std::move(pending_body));
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::ClosedShellToSolid;
    target_ = std::move(shell);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<geometry::GeometryShape>
  preview_shape(const GuiDocumentSession& session) const {
    auto candidate = candidate_document(session);
    if (!candidate)
      return Result<geometry::GeometryShape>::failure(candidate.error());
    return detail::preview_feature_body(session, candidate.value(), feature_, result_body_);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] SurfaceFeatureKind kind() const noexcept { return kind_; }
  [[nodiscard]] std::size_t boundary_count() const noexcept { return boundaries_.size(); }
  [[nodiscard]] std::size_t surface_count() const noexcept { return surfaces_.size(); }
  [[nodiscard]] double distance() const noexcept { return distance_; }
  [[nodiscard]] double tolerance() const noexcept { return tolerance_; }

  [[nodiscard]] Result<std::size_t> add_boundary_curve(BoundaryCurveReference boundary) {
    if (!active_ || (kind_ != SurfaceFeatureKind::BoundarySurface &&
                     kind_ != SurfaceFeatureKind::FillSurface))
      return fail("no boundary/fill surface command is active");
    const std::string key = boundary.stable_key();
    for (const auto& existing : boundaries_)
      if (existing.stable_key() == key) return fail("boundary curve is already selected");
    boundaries_.push_back(std::move(boundary));
    return Result<std::size_t>::success(boundaries_.size());
  }
  [[nodiscard]] Result<std::size_t> add_surface(SurfaceReference surface) {
    if (!active_ || kind_ != SurfaceFeatureKind::SurfaceStitch)
      return fail("no stitch command is active");
    const std::string key = surface.stable_key();
    for (const auto& existing : surfaces_)
      if (existing.stable_key() == key) return fail("surface is already selected");
    surfaces_.push_back(std::move(surface));
    return Result<std::size_t>::success(surfaces_.size());
  }

  void set_distance(double millimeters) noexcept {
    distance_ = millimeters;
    distance_changed_ = true;
  }
  void set_tolerance(double millimeters) noexcept {
    tolerance_ = millimeters;
    tolerance_changed_ = true;
  }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    const bool offset =
        kind_ == SurfaceFeatureKind::OffsetSurface || kind_ == SurfaceFeatureKind::Thicken;
    if (!active_ || (kind_ != SurfaceFeatureKind::ExtendSurface && !offset))
      return handles;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(offset ? kSurfaceOffsetHandleId : kSurfaceExtendHandleId);
    handle.kind = GuiViewportManipulatorKind::Linear;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    if (offset && offset_parameters_->side == SurfaceOffsetSide::OppositeNormal)
      handle.axis = {-frame_.axis.x, -frame_.axis.y, -frame_.axis.z};
    if (offset && offset_parameters_->side == SurfaceOffsetSide::Symmetric)
      handle.model_units_per_value = .5;
    handle.reference_value = distance_;
    handle.minimum_value = offset ? 1e-6 : 0.0;
    if (offset)
      handle.maximum_value = 1e6;
    handles.push_back(std::move(handle));
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (!active_)
      return false;
    if (kind_ == SurfaceFeatureKind::SurfaceStitch &&
        candidate.handle_id == kSurfaceToleranceValueId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance) {
      set_tolerance(candidate.scalar_value);
      return true;
    }
    if (((kind_ == SurfaceFeatureKind::ExtendSurface &&
          candidate.handle_id == kSurfaceExtendHandleId) ||
         ((kind_ == SurfaceFeatureKind::OffsetSurface || kind_ == SurfaceFeatureKind::Thicken) &&
          candidate.handle_id == kSurfaceOffsetHandleId)) &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance) {
      set_distance(candidate.scalar_value);
      return true;
    }
    return false;
  }

  // Preview runs the real recompute on a clone so geometric diagnostics
  // (free edges, gaps, non-manifold, non-closed shell) surface before commit.
  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    auto candidate = candidate_document(session);
    if (!candidate)
      return Result<GuiPartFeaturePreview>::failure(candidate.error());
    auto cache = detail::recompute_preview_geometry(session, candidate.value(), {feature_.value()});
    if (cache.has_error())
      return Result<GuiPartFeaturePreview>::failure(cache.error());
    return Result<GuiPartFeaturePreview>::success(
        {feature_, summary(), cache.value().body_shape_count()});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_part_transaction(
        std::string(apply_label()), [this](PartDocument& part) -> Result<std::size_t> {
          if (auto provisioned = provision_inputs(part); !provisioned)
            return provisioned;
          if (auto seeded = seed_parameters(part); seeded.has_error())
            return seeded;
          auto feature = build_feature();
          if (feature.has_error())
            return Result<std::size_t>::failure(feature.error());
          return editing_ ? part.update_surface_feature(std::move(feature.value()))
                          : part.add_surface_feature(std::move(feature.value()));
        });
    if (committed.has_error()) return committed;
    active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  [[nodiscard]] Result<std::size_t> provision_inputs(PartDocument& part) const {
    if (pending_body_) {
      auto added = part.add_body(*pending_body_);
      if (!added)
        return added;
    }
    if (auto parameters = detail::provision_new_feature_parameters(part, pending_parameters_);
        !parameters)
      return parameters;
    for (const auto& path : pending_paths_) {
      if (auto added = part.add_path_curve(path); !added)
        return added;
    }
    return Result<std::size_t>::success(pending_parameters_.size() + pending_paths_.size());
  }
  [[nodiscard]] Result<PartDocument> candidate_document(const GuiDocumentSession& session) const {
    if (!active_ || !session.part_document())
      return Result<PartDocument>::failure(
          Error::validation("surface", "no active surface command"));
    auto ready = ready_to_build();
    if (!ready)
      return Result<PartDocument>::failure(ready.error());
    PartDocument candidate = *session.part_document();
    auto provisioned = provision_inputs(candidate);
    if (!provisioned)
      return Result<PartDocument>::failure(provisioned.error());
    auto seeded = seed_parameters(candidate);
    if (!seeded)
      return Result<PartDocument>::failure(seeded.error());
    auto feature = build_feature();
    if (!feature)
      return Result<PartDocument>::failure(feature.error());
    auto added = editing_ ? candidate.update_surface_feature(std::move(feature.value()))
                          : candidate.add_surface_feature(std::move(feature.value()));
    if (!added)
      return Result<PartDocument>::failure(added.error());
    return Result<PartDocument>::success(std::move(candidate));
  }
  [[nodiscard]] Result<std::size_t> begin_common(const GuiDocumentSession& session,
                                                 FeatureId feature, std::string name,
                                                 BodyId result_body,
                                                 std::optional<Body> pending_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive surface requires an active Part document");
    if (auto valid = detail::validate_new_feature_body(session, result_body, pending_body); !valid)
      return valid;
    pending_body_ = std::move(pending_body);
    pending_parameters_.clear();
    pending_paths_.clear();
    numeric_parameters_.clear();
    editing_ = false;
    distance_changed_ = false;
    tolerance_changed_ = false;
    trim_options_.reset();
    extend_options_.reset();
    offset_parameters_.reset();
    offset_context_.reset();
    distance_parameter_ = ParameterId("");
    tolerance_parameter_ = ParameterId("");
    feature_ = std::move(feature);
    name_ = std::move(name);
    result_body_ = std::move(result_body);
    boundaries_.clear();
    surfaces_.clear();
    target_.reset();
    trimming_.reset();
    extend_boundary_.reset();
    active_ = true;
    return Result<std::size_t>::success(1);
  }
  [[nodiscard]] Result<std::size_t> begin_curve_surface(const GuiDocumentSession& session,
                                                        SurfaceFeatureKind kind, FeatureId feature,
                                                        std::string name, BodyId result_body,
                                                        std::optional<Body> pending_body) {
    if (auto ok = begin_common(session, std::move(feature), std::move(name), std::move(result_body),
                               std::move(pending_body));
        ok.has_error())
      return ok;
    kind_ = kind;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> ready_to_build() const {
    switch (kind_) {
    case SurfaceFeatureKind::BoundarySurface:
    case SurfaceFeatureKind::FillSurface:
      return boundaries_.empty() ? fail("surface requires at least one boundary curve")
                                 : Result<std::size_t>::success(1);
    case SurfaceFeatureKind::SurfaceStitch:
      return surfaces_.size() < 2U ? fail("stitch requires at least two surfaces")
                                   : Result<std::size_t>::success(1);
    case SurfaceFeatureKind::TrimSurface:
      return (target_.has_value() && trimming_.has_value())
                 ? Result<std::size_t>::success(1)
                 : fail("trim requires a target surface and a trimming contour");
    case SurfaceFeatureKind::ExtendSurface:
      return (target_.has_value() && extend_boundary_.has_value())
                 ? Result<std::size_t>::success(1)
                 : fail("extend requires a target surface and a boundary");
    case SurfaceFeatureKind::OffsetSurface:
    case SurfaceFeatureKind::Thicken:
      return target_ && offset_parameters_ && offset_context_
                 ? Result<std::size_t>::success(1)
                 : fail("Select owned Surface faces and an output mode");
    case SurfaceFeatureKind::ClosedShellToSolid:
      return target_.has_value() ? Result<std::size_t>::success(1)
                                 : fail("closed-shell-to-solid requires a shell");
    }
    return fail("unsupported surface feature kind");
  }

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    auto values = numeric_parameters_;
    if (distance_changed_ &&
        (kind_ == SurfaceFeatureKind::ExtendSurface || kind_ == SurfaceFeatureKind::OffsetSurface ||
         kind_ == SurfaceFeatureKind::Thicken))
      values.emplace_back(distance_parameter_, distance_);
    if (tolerance_changed_ && kind_ == SurfaceFeatureKind::SurfaceStitch)
      values.emplace_back(tolerance_parameter_, tolerance_);
    std::size_t count = 0;
    for (const auto& [id, value] : values) {
      const auto* previous = part.find_parameter(id);
      if (previous && previous->value().millimeters() == value)
        continue;
      auto quantity = Quantity::length_mm(value, id.value());
      if (!quantity)
        return Result<std::size_t>::failure(quantity.error());
      auto updated = part.set_parameter_value(id, quantity.value());
      if (!updated)
        return Result<std::size_t>::failure(updated.error());
      count += updated.value().size();
    }
    return Result<std::size_t>::success(count);
  }

  [[nodiscard]] Result<SurfaceFeature> build_feature() const {
    switch (kind_) {
    case SurfaceFeatureKind::BoundarySurface: {
      auto feature = BoundarySurfaceFeature::create(feature_, name_, boundaries_, result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    case SurfaceFeatureKind::FillSurface: {
      auto feature = FillSurfaceFeature::create(feature_, name_, boundaries_, result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    case SurfaceFeatureKind::TrimSurface: {
      auto feature =
          TrimSurfaceFeature::create(feature_, name_, *target_, *trimming_, result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      auto configured = feature.value().with_options(trim_options_);
      if (!configured)
        return Result<SurfaceFeature>::failure(configured.error());
      return Result<SurfaceFeature>::success(std::move(configured.value()));
    }
    case SurfaceFeatureKind::ExtendSurface: {
      auto feature = ExtendSurfaceFeature::create(feature_, name_, *target_, *extend_boundary_,
                                                  distance_parameter_, result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      auto configured = feature.value().with_options(extend_options_);
      if (!configured)
        return Result<SurfaceFeature>::failure(configured.error());
      return Result<SurfaceFeature>::success(std::move(configured.value()));
    }
    case SurfaceFeatureKind::SurfaceStitch: {
      auto feature = SurfaceStitchFeature::create(feature_, name_, surfaces_, tolerance_parameter_,
                                                  result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    case SurfaceFeatureKind::OffsetSurface:
    case SurfaceFeatureKind::Thicken: {
      auto feature = kind_ == SurfaceFeatureKind::Thicken
                         ? SurfaceOffsetFeature::create_thicken(
                               feature_, name_, *target_, *offset_parameters_, *offset_context_)
                         : SurfaceOffsetFeature::create_offset(
                               feature_, name_, *target_, *offset_parameters_, *offset_context_);
      if (!feature)
        return Result<SurfaceFeature>::failure(feature.error());
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    case SurfaceFeatureKind::ClosedShellToSolid: {
      auto feature = ClosedShellToSolidFeature::create(feature_, name_, *target_, result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    }
    return Result<SurfaceFeature>::failure(
        Error::validation(feature_.value(), "unsupported surface feature kind"));
  }

  [[nodiscard]] std::string_view apply_label() const noexcept {
    if (editing_)
      return "Edit Surface";
    return kind_ == SurfaceFeatureKind::ClosedShellToSolid ? "Convert Shell to Solid"
                                                           : "Apply Surface";
  }
  [[nodiscard]] std::string summary() const {
    return std::string("surface ") + std::string(to_string(kind_)) +
           (kind_ == SurfaceFeatureKind::ClosedShellToSolid ? " -> solid" : " -> surface");
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_surface", std::move(message)));
  }

  bool editing_ = false, distance_changed_ = false, tolerance_changed_ = false;
  std::optional<SurfaceTrimOptions> trim_options_;
  std::optional<SurfaceExtendOptions> extend_options_;
  std::optional<SurfaceOffsetParameters> offset_parameters_;
  std::optional<FeatureBodyResultContext> offset_context_;
  std::vector<PathCurve> pending_paths_;
  std::vector<std::pair<ParameterId, double>> numeric_parameters_;
  std::optional<Body> pending_body_;
  std::vector<Parameter> pending_parameters_;
  bool active_{false};
  SurfaceFeatureKind kind_{SurfaceFeatureKind::BoundarySurface};
  FeatureId feature_{""};
  std::string name_;
  BodyId result_body_{""};
  std::vector<BoundaryCurveReference> boundaries_;
  std::vector<SurfaceReference> surfaces_;
  std::optional<SurfaceReference> target_;
  std::optional<TrimmingReference> trimming_;
  std::optional<BoundaryCurveReference> extend_boundary_;
  ParameterId distance_parameter_{""};
  double distance_{0.0};
  ParameterId tolerance_parameter_{""};
  double tolerance_{0.0};
  GuiFeatureManipulatorFrame frame_;
};

} // namespace blcad::gui
