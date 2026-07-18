#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include "blcad/core/loft_feature.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/path_curve.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/sweep_feature.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

// Block 127 interactive PathCurve, Sweep, and Loft authoring.
//
// Headless GUI controllers over the frozen Block-70..79 contracts. PathCurve
// collects an ordered, duplicate-free chain of connected segments; Sweep picks a
// profile and trajectory with an optional twist parameter or guide (never both);
// Loft collects ordered sections with drag-reorder and a C0/G1/G2 continuity.
// Each previews a throwaway PartDocument clone and commits one transaction.
inline constexpr std::string_view kSweepTwistHandleId = "sweep.twist";

enum class GuiPathSweepMode { PathCurve, Sweep };

class GuiInteractivePathSweepController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_path_curve(const GuiDocumentSession& session, PathCurveId path_id, std::string name,
                   PathClosure closure = PathClosure::Open,
                   PathOrientationRule orientation = PathOrientationRule::MinimumTwist) {
    if (session.part_document() == nullptr)
      return fail("interactive path curve requires an active Part document");
    reset();
    mode_ = GuiPathSweepMode::PathCurve;
    path_id_ = std::move(path_id);
    name_ = std::move(name);
    closure_ = closure;
    orientation_ = orientation;
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_sweep(const GuiDocumentSession& session, FeatureId feature, std::string name,
              SweepFeatureKind kind, SweepProfileReference profile, PathCurveId path,
              BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive sweep requires an active Part document");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive sweep requires an existing result Body");
    if (part->find_path_curve(path) == nullptr)
      return fail("interactive sweep requires an existing trajectory PathCurve");
    reset();
    mode_ = GuiPathSweepMode::Sweep;
    feature_ = std::move(feature);
    name_ = std::move(name);
    sweep_kind_ = kind;
    profile_ = std::move(profile);
    sweep_path_ = std::move(path);
    result_body_ = std::move(result_body);
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] GuiPathSweepMode mode() const noexcept { return mode_; }
  [[nodiscard]] std::size_t segment_count() const noexcept { return segments_.size(); }
  [[nodiscard]] double twist() const noexcept { return twist_deg_; }
  [[nodiscard]] bool has_twist() const noexcept { return twist_parameter_.has_value(); }
  [[nodiscard]] bool has_guide() const noexcept { return guide_path_.has_value(); }

  // PathCurve: append one connected segment, rejecting a duplicate stable key.
  [[nodiscard]] Result<std::size_t> add_segment(PathSegmentReference segment) {
    if (!active_ || mode_ != GuiPathSweepMode::PathCurve)
      return fail("no path curve command is active");
    const std::string key = segment.stable_key();
    for (const auto& existing : segments_)
      if (existing.stable_key() == key)
        return fail("segment is already in the path");
    segments_.push_back(std::move(segment));
    return Result<std::size_t>::success(segments_.size());
  }
  void set_closure(PathClosure closure) noexcept { closure_ = closure; }
  void set_orientation(PathOrientationRule orientation) noexcept { orientation_ = orientation; }
  void set_continuity_hint(std::optional<PathContinuityHint> hint) noexcept {
    continuity_hint_ = hint;
  }
  void set_up_vector(std::optional<Vector3> up) noexcept { up_vector_ = up; }

  // Sweep: optional twist about the path and an optional guide path. The Core
  // rejects both together; the controller surfaces that at preview.
  void set_sweep_kind(SweepFeatureKind kind) noexcept { sweep_kind_ = kind; }
  void set_twist(ParameterId parameter, double degrees) {
    twist_parameter_ = std::move(parameter);
    twist_deg_ = degrees;
  }
  void clear_twist() noexcept { twist_parameter_.reset(); }
  void set_guide(PathCurveId guide) { guide_path_ = std::move(guide); }
  void clear_guide() noexcept { guide_path_.reset(); }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (!active_ || mode_ != GuiPathSweepMode::Sweep || !twist_parameter_.has_value())
      return handles;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kSweepTwistHandleId);
    handle.kind = GuiViewportManipulatorKind::Angular;
    handle.origin = frame_.origin;
    handle.plane_normal = frame_.plane_normal;
    handle.reference_direction = frame_.reference_direction;
    handle.reference_value = twist_deg_;
    handles.push_back(std::move(handle));
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (candidate.handle_id == kSweepTwistHandleId && twist_parameter_.has_value() &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Angle) {
      twist_deg_ = candidate.scalar_value;
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no path/sweep command is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    PartDocument candidate = *part;
    if (auto seeded = seed_parameters(candidate); seeded.has_error())
      return Result<GuiPartFeaturePreview>::failure(seeded.error());
    if (auto added = add_record(candidate); added.has_error())
      return Result<GuiPartFeaturePreview>::failure(added.error());
    auto plan = candidate.create_recompute_plan();
    if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
    return Result<GuiPartFeaturePreview>::success({feature_, summary(), plan.value().step_count()});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_part_transaction(
        std::string(mode_ == GuiPathSweepMode::PathCurve ? "Create Path Curve" : "Apply Sweep"),
        [this](PartDocument& part) -> Result<std::size_t> {
          if (auto seeded = seed_parameters(part); seeded.has_error()) return seeded;
          return add_record(part);
        });
    if (committed.has_error()) return committed;
    active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  void reset() {
    segments_.clear();
    profile_.reset();
    twist_parameter_.reset();
    guide_path_.reset();
    continuity_hint_.reset();
    up_vector_.reset();
    twist_deg_ = 0.0;
    closure_ = PathClosure::Open;
    orientation_ = PathOrientationRule::MinimumTwist;
    sweep_kind_ = SweepFeatureKind::Sweep;
  }

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    if (mode_ != GuiPathSweepMode::Sweep || !twist_parameter_.has_value())
      return Result<std::size_t>::success(0);
    auto quantity = Quantity::angle_deg(twist_deg_, twist_parameter_->value());
    if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
    auto changed = part.set_parameter_value(*twist_parameter_, quantity.value());
    return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                               : Result<std::size_t>::success(changed.value().size());
  }

  [[nodiscard]] Result<std::size_t> add_record(PartDocument& part) const {
    if (mode_ == GuiPathSweepMode::PathCurve) {
      if (segments_.empty())
        return fail("path curve requires at least one segment");
      auto path = PathCurve::create(path_id_, name_, segments_, closure_, orientation_, up_vector_,
                                    continuity_hint_);
      if (path.has_error()) return Result<std::size_t>::failure(path.error());
      return part.add_path_curve(std::move(path.value()));
    }
    if (!profile_.has_value())
      return fail("sweep requires a profile");
    auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                    result_body_);
    if (context.has_error()) return Result<std::size_t>::failure(context.error());
    Result<SweepFeature> feature = build_sweep(context.value());
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    return part.add_sweep_feature(std::move(feature.value()));
  }

  [[nodiscard]] Result<SweepFeature> build_sweep(const FeatureBodyResultContext& context) const {
    switch (sweep_kind_) {
    case SweepFeatureKind::Sweep:
      return SweepFeature::create_sweep(feature_, name_, *profile_, sweep_path_, context,
                                        std::nullopt, std::nullopt, twist_parameter_, guide_path_);
    case SweepFeatureKind::SweepCut:
      return SweepFeature::create_sweep_cut(feature_, name_, *profile_, sweep_path_, context,
                                            std::nullopt, std::nullopt, twist_parameter_,
                                            guide_path_);
    case SweepFeatureKind::SweepSurface:
      return SweepFeature::create_sweep_surface(feature_, name_, *profile_, sweep_path_, context,
                                                std::nullopt, std::nullopt, twist_parameter_,
                                                guide_path_);
    }
    return Result<SweepFeature>::failure(
        Error::validation(feature_.value(), "unsupported sweep kind"));
  }

  [[nodiscard]] std::string summary() const {
    if (mode_ == GuiPathSweepMode::PathCurve)
      return "path curve, " + std::to_string(segments_.size()) + " segments, " +
             std::string(to_string(closure_));
    return std::string("sweep ") + std::string(to_string(sweep_kind_)) +
           (twist_parameter_.has_value() ? ", twist" : "") +
           (guide_path_.has_value() ? ", guide" : "");
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_path_sweep", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_path_sweep", std::move(message)));
  }

  bool active_{false};
  GuiPathSweepMode mode_{GuiPathSweepMode::PathCurve};
  PathCurveId path_id_{""};
  FeatureId feature_{""};
  std::string name_;
  // Path state.
  std::vector<PathSegmentReference> segments_;
  PathClosure closure_{PathClosure::Open};
  PathOrientationRule orientation_{PathOrientationRule::MinimumTwist};
  std::optional<PathContinuityHint> continuity_hint_;
  std::optional<Vector3> up_vector_;
  // Sweep state.
  SweepFeatureKind sweep_kind_{SweepFeatureKind::Sweep};
  std::optional<SweepProfileReference> profile_;
  PathCurveId sweep_path_{""};
  BodyId result_body_{""};
  std::optional<ParameterId> twist_parameter_;
  double twist_deg_{0.0};
  std::optional<PathCurveId> guide_path_;
  GuiFeatureManipulatorFrame frame_;
};

class GuiInteractiveLoftController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_loft(const GuiDocumentSession& session, FeatureId feature, std::string name,
             LoftFeatureKind kind, BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive loft requires an active Part document");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive loft requires an existing result Body");
    feature_ = std::move(feature);
    name_ = std::move(name);
    kind_ = kind;
    result_body_ = std::move(result_body);
    sections_.clear();
    path_curve_.reset();
    guide_curves_.clear();
    continuity_ = LoftContinuity::C0;
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] LoftFeatureKind kind() const noexcept { return kind_; }
  [[nodiscard]] LoftContinuity continuity() const noexcept { return continuity_; }
  [[nodiscard]] std::size_t section_count() const noexcept { return sections_.size(); }

  [[nodiscard]] Result<std::size_t>
  add_closed_section(SketchId sketch, ProfileId profile, bool flip_normal = false) {
    if (!active_) return fail("no loft command is active");
    auto reference = ProfileRegionReference::create(std::move(sketch), std::move(profile),
                                                    PartFeatureInputRole::LoftSection);
    if (reference.has_error()) return Result<std::size_t>::failure(reference.error());
    auto section = ProfileSectionReference::create_closed_region(reference.value(), std::nullopt,
                                                                 flip_normal);
    if (section.has_error()) return Result<std::size_t>::failure(section.error());
    return add_section(std::move(section.value()));
  }
  [[nodiscard]] Result<std::size_t> add_open_section(PathCurveId path_curve, bool flip_normal = false) {
    if (!active_) return fail("no loft command is active");
    auto section = ProfileSectionReference::create_open_path(std::move(path_curve), flip_normal);
    if (section.has_error()) return Result<std::size_t>::failure(section.error());
    return add_section(std::move(section.value()));
  }

  // Drag-reorder in the task list: move the section at `from` to index `to`.
  [[nodiscard]] Result<std::size_t> reorder_section(std::size_t from, std::size_t to) {
    if (!active_) return fail("no loft command is active");
    if (from >= sections_.size() || to >= sections_.size())
      return fail("loft section reorder index is out of range");
    if (from == to) return Result<std::size_t>::success(sections_.size());
    auto moved = std::move(sections_[from]);
    sections_.erase(sections_.begin() + static_cast<std::ptrdiff_t>(from));
    sections_.insert(sections_.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    return Result<std::size_t>::success(sections_.size());
  }

  void set_continuity(LoftContinuity continuity) noexcept { continuity_ = continuity; }
  void set_path_curve(std::optional<PathCurveId> path) { path_curve_ = std::move(path); }
  [[nodiscard]] Result<std::size_t> add_guide_curve(PathCurveId guide) {
    if (!active_) return fail("no loft command is active");
    guide_curves_.push_back(std::move(guide));
    return Result<std::size_t>::success(guide_curves_.size());
  }

  // Loft has no scalar handle; sections are task-list chips.
  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const { return {}; }
  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate&) { return false; }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no loft command is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    if (sections_.size() < 2U) return preview_fail("loft requires at least two ordered sections");
    PartDocument candidate = *part;
    if (auto added = add_feature(candidate); added.has_error())
      return Result<GuiPartFeaturePreview>::failure(added.error());
    auto plan = candidate.create_recompute_plan();
    if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
    return Result<GuiPartFeaturePreview>::success({feature_, summary(), plan.value().step_count()});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_part_transaction(
        "Apply Loft", [this](PartDocument& part) { return add_feature(part); });
    if (committed.has_error()) return committed;
    active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  [[nodiscard]] Result<std::size_t> add_section(ProfileSectionReference section) {
    const std::string id = section.source_node_id();
    for (const auto& existing : sections_)
      if (existing.source_node_id() == id)
        return fail("section is already in the loft");
    sections_.push_back(std::move(section));
    return Result<std::size_t>::success(sections_.size());
  }

  [[nodiscard]] Result<std::size_t> add_feature(PartDocument& part) const {
    auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                    result_body_);
    if (context.has_error()) return Result<std::size_t>::failure(context.error());
    Result<LoftFeature> feature = build_loft(context.value());
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    return part.add_loft_feature(std::move(feature.value()));
  }

  [[nodiscard]] Result<LoftFeature> build_loft(const FeatureBodyResultContext& context) const {
    switch (kind_) {
    case LoftFeatureKind::Loft:
      return LoftFeature::create_loft(feature_, name_, sections_, context, path_curve_,
                                      guide_curves_, continuity_);
    case LoftFeatureKind::LoftCut:
      return LoftFeature::create_loft_cut(feature_, name_, sections_, context, path_curve_,
                                          guide_curves_, continuity_);
    case LoftFeatureKind::LoftSurface:
      return LoftFeature::create_loft_surface(feature_, name_, sections_, context, path_curve_,
                                              guide_curves_, continuity_);
    }
    return Result<LoftFeature>::failure(
        Error::validation(feature_.value(), "unsupported loft kind"));
  }

  [[nodiscard]] std::string summary() const {
    return std::string("loft ") + std::string(to_string(kind_)) + ", " +
           std::to_string(sections_.size()) + " sections, " + std::string(to_string(continuity_));
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_loft", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_loft", std::move(message)));
  }

  bool active_{false};
  FeatureId feature_{""};
  std::string name_;
  LoftFeatureKind kind_{LoftFeatureKind::Loft};
  BodyId result_body_{""};
  std::vector<ProfileSectionReference> sections_;
  std::optional<PathCurveId> path_curve_;
  std::vector<PathCurveId> guide_curves_;
  LoftContinuity continuity_{LoftContinuity::C0};
};

} // namespace blcad::gui
