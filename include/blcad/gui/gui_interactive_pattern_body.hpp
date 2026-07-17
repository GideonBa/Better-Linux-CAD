#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include "blcad/core/body_boolean_feature.hpp"
#include "blcad/core/body_transform.hpp"
#include "blcad/core/mirror_feature.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/part_pattern_feature.hpp"
#include "blcad/core/quantity.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

// Block 126 interactive Pattern / Mirror / Body Boolean / Body Transform.
//
// Headless GUI controllers over the frozen Block-63..69 contracts. Patterns and
// Mirror collect ordered Feature/Body sources; Body Boolean collects tool
// bodies; Body Transform appends one Translate/Rotate/UniformScale record to the
// persistent transform stack. Each previews a throwaway PartDocument clone and
// commits one transaction that seeds any driving parameters and adds the record.
inline constexpr std::string_view kPatternSpacingHandleId = "pattern.spacing";
inline constexpr std::string_view kPatternCountHandleId = "pattern.count";
inline constexpr std::string_view kPatternAngleHandleId = "pattern.angle";
inline constexpr std::string_view kTransformTranslatePrefix = "transform.translate";
inline constexpr std::string_view kTransformAngleHandleId = "transform.angle";
inline constexpr std::string_view kTransformScaleHandleId = "transform.scale";

enum class GuiPatternMirrorKind { LinearPattern, CircularPattern, Mirror };

class GuiInteractivePatternMirrorController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_linear_pattern(const GuiDocumentSession& session, FeatureId feature, std::string name,
                       AxisReference direction, ParameterId count_parameter,
                       LinearPatternExtentMode extent_mode, ParameterId extent_parameter,
                       BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive pattern requires an active Part document");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive pattern requires an existing result Body");
    const Parameter* count = part->find_parameter(count_parameter);
    if (count == nullptr || count->type() != ParameterType::Count)
      return fail("linear pattern requires an existing Count parameter");
    const Parameter* extent = part->find_parameter(extent_parameter);
    if (extent == nullptr || extent->type() != ParameterType::Length)
      return fail("linear pattern requires an existing Length spacing/extent parameter");
    reset(std::move(feature), std::move(name), std::move(result_body));
    kind_ = GuiPatternMirrorKind::LinearPattern;
    direction_ = std::move(direction);
    count_parameter_ = std::move(count_parameter);
    extent_parameter_ = std::move(extent_parameter);
    extent_mode_ = extent_mode;
    count_ = static_cast<int>(count->value().count_value());
    spacing_ = extent->value().millimeters();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_circular_pattern(const GuiDocumentSession& session, FeatureId feature, std::string name,
                         AxisReference axis, ParameterId count_parameter, BodyId result_body,
                         bool equal_spacing = true) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive pattern requires an active Part document");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive pattern requires an existing result Body");
    const Parameter* count = part->find_parameter(count_parameter);
    if (count == nullptr || count->type() != ParameterType::Count)
      return fail("circular pattern requires an existing Count parameter");
    reset(std::move(feature), std::move(name), std::move(result_body));
    kind_ = GuiPatternMirrorKind::CircularPattern;
    direction_ = std::move(axis);
    count_parameter_ = std::move(count_parameter);
    equal_spacing_ = equal_spacing;
    count_ = static_cast<int>(count->value().count_value());
    angle_ = 360.0;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_mirror(const GuiDocumentSession& session, FeatureId feature, std::string name,
               PlaneReference mirror_plane, BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive mirror requires an active Part document");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive mirror requires an existing result Body");
    reset(std::move(feature), std::move(name), std::move(result_body));
    kind_ = GuiPatternMirrorKind::Mirror;
    mirror_plane_ = std::move(mirror_plane);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] GuiPatternMirrorKind kind() const noexcept { return kind_; }
  [[nodiscard]] std::size_t source_count() const noexcept { return sources_.size(); }
  [[nodiscard]] int count() const noexcept { return count_; }
  [[nodiscard]] double spacing() const noexcept { return spacing_; }
  [[nodiscard]] double angle() const noexcept { return angle_; }

  void set_count(int value) noexcept { count_ = std::max(1, value); }
  void set_spacing(double millimeters) noexcept { spacing_ = millimeters; }
  void set_angle(double degrees) noexcept { angle_ = degrees; }
  void set_extent_mode(LinearPatternExtentMode mode) noexcept { extent_mode_ = mode; }
  void set_direction_sign(PatternDirectionSign sign) noexcept { direction_sign_ = sign; }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] Result<std::size_t> add_source_body(BodyId body) {
    return add_source(PatternSource{std::move(body)});
  }
  [[nodiscard]] Result<std::size_t> add_source_feature(FeatureId feature) {
    return add_source(PatternSource{std::move(feature)});
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (!active_) return handles;
    if (kind_ == GuiPatternMirrorKind::LinearPattern) {
      handles.push_back(linear_handle(kPatternSpacingHandleId, spacing_, true));
      handles.push_back(count_handle());
    } else if (kind_ == GuiPatternMirrorKind::CircularPattern) {
      handles.push_back(angular_handle(kPatternAngleHandleId, angle_));
      handles.push_back(count_handle());
    }
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (candidate.handle_id == kPatternSpacingHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance) {
      spacing_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kPatternAngleHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Angle) {
      angle_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kPatternCountHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Count) {
      count_ = std::max(1, candidate.count_value);
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no pattern/mirror command is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    if (sources_.empty()) return preview_fail("pattern/mirror requires at least one source");
    PartDocument candidate = *part;
    if (auto seeded = seed_parameters(candidate); seeded.has_error())
      return Result<GuiPartFeaturePreview>::failure(seeded.error());
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
        std::string(apply_label()),
        [this](PartDocument& part) -> Result<std::size_t> {
          if (auto seeded = seed_parameters(part); seeded.has_error()) return seeded;
          return add_feature(part);
        });
    if (committed.has_error()) return committed;
    active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  void reset(FeatureId feature, std::string name, BodyId result_body) {
    feature_ = std::move(feature);
    name_ = std::move(name);
    result_body_ = std::move(result_body);
    sources_.clear();
    count_ = 1;
    spacing_ = 0.0;
    angle_ = 360.0;
    extent_mode_ = LinearPatternExtentMode::Spacing;
    direction_sign_ = PatternDirectionSign::Positive;
    equal_spacing_ = true;
    active_ = true;
  }

  [[nodiscard]] Result<std::size_t> add_source(PatternSource source) {
    if (!active_)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_pattern", "no pattern/mirror command is active"));
    if (std::find(sources_.begin(), sources_.end(), source) != sources_.end())
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_pattern", "source is already selected"));
    sources_.push_back(std::move(source));
    return Result<std::size_t>::success(sources_.size());
  }

  [[nodiscard]] GuiViewportManipulatorHandle linear_handle(std::string_view id, double reference,
                                                           bool non_negative) const {
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(id);
    handle.kind = GuiViewportManipulatorKind::Linear;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    handle.reference_value = reference;
    if (non_negative) handle.minimum_value = 0.0;
    return handle;
  }
  [[nodiscard]] GuiViewportManipulatorHandle angular_handle(std::string_view id,
                                                            double reference) const {
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(id);
    handle.kind = GuiViewportManipulatorKind::Angular;
    handle.origin = frame_.origin;
    handle.plane_normal = frame_.plane_normal;
    handle.reference_direction = frame_.reference_direction;
    handle.reference_value = reference;
    return handle;
  }
  [[nodiscard]] GuiViewportManipulatorHandle count_handle() const {
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kPatternCountHandleId);
    handle.kind = GuiViewportManipulatorKind::PatternCount;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    handle.reference_value = static_cast<double>(count_);
    handle.minimum_value = 1.0;
    return handle;
  }

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    if (kind_ == GuiPatternMirrorKind::Mirror) return Result<std::size_t>::success(0);
    auto count = Quantity::count(static_cast<double>(count_), count_parameter_.value());
    if (count.has_error()) return Result<std::size_t>::failure(count.error());
    if (auto changed = part.set_parameter_value(count_parameter_, count.value()); changed.has_error())
      return Result<std::size_t>::failure(changed.error());
    if (kind_ == GuiPatternMirrorKind::LinearPattern) {
      auto spacing = Quantity::length_mm(spacing_, extent_parameter_.value());
      if (spacing.has_error()) return Result<std::size_t>::failure(spacing.error());
      if (auto changed = part.set_parameter_value(extent_parameter_, spacing.value());
          changed.has_error())
        return Result<std::size_t>::failure(changed.error());
    }
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<FeatureBodyResultContext> new_body_context() const {
    return FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                            result_body_);
  }

  [[nodiscard]] Result<std::size_t> add_feature(PartDocument& part) const {
    auto context = new_body_context();
    if (context.has_error()) return Result<std::size_t>::failure(context.error());
    if (kind_ == GuiPatternMirrorKind::Mirror) {
      std::vector<MirrorSourceReference> sources;
      for (const auto& source : sources_) {
        auto reference = mirror_source(source);
        if (reference.has_error()) return Result<std::size_t>::failure(reference.error());
        sources.push_back(std::move(reference.value()));
      }
      auto feature =
          MirrorFeature::create(feature_, name_, std::move(sources), *mirror_plane_, context.value());
      if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
      return part.add_mirror_feature(std::move(feature.value()));
    }

    std::vector<PatternSourceReference> sources;
    for (const auto& source : sources_) {
      auto reference = pattern_source(source);
      if (reference.has_error()) return Result<std::size_t>::failure(reference.error());
      sources.push_back(std::move(reference.value()));
    }
    if (kind_ == GuiPatternMirrorKind::LinearPattern) {
      auto feature = LinearPatternFeature::create(feature_, name_, std::move(sources), *direction_,
                                                  count_parameter_, extent_mode_, extent_parameter_,
                                                  direction_sign_, context.value());
      if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
      return part.add_linear_pattern_feature(std::move(feature.value()));
    }
    auto feature = CircularPatternFeature::create(feature_, name_, std::move(sources), *direction_,
                                                  count_parameter_, angle_, equal_spacing_,
                                                  context.value());
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    return part.add_circular_pattern_feature(std::move(feature.value()));
  }

  [[nodiscard]] static Result<PatternSourceReference> pattern_source(const PatternSource& source) {
    return std::holds_alternative<BodyId>(source)
               ? PatternSourceReference::body(std::get<BodyId>(source))
               : PatternSourceReference::feature(std::get<FeatureId>(source));
  }
  [[nodiscard]] static Result<MirrorSourceReference> mirror_source(const PatternSource& source) {
    return std::holds_alternative<BodyId>(source)
               ? MirrorSourceReference::body(std::get<BodyId>(source))
               : MirrorSourceReference::feature(std::get<FeatureId>(source));
  }

  [[nodiscard]] std::string_view apply_label() const noexcept {
    switch (kind_) {
    case GuiPatternMirrorKind::LinearPattern: return "Apply Linear Pattern";
    case GuiPatternMirrorKind::CircularPattern: return "Apply Circular Pattern";
    case GuiPatternMirrorKind::Mirror: return "Apply Mirror";
    }
    return "Apply Pattern";
  }
  [[nodiscard]] std::string summary() const {
    switch (kind_) {
    case GuiPatternMirrorKind::LinearPattern:
      return "linear pattern, " + std::to_string(count_) + " x, " +
             std::string(to_string(extent_mode_));
    case GuiPatternMirrorKind::CircularPattern:
      return "circular pattern, " + std::to_string(count_) + " x";
    case GuiPatternMirrorKind::Mirror:
      return "mirror, " + std::to_string(sources_.size()) + " sources";
    }
    return "pattern";
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_pattern", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_pattern", std::move(message)));
  }

  bool active_{false};
  GuiPatternMirrorKind kind_{GuiPatternMirrorKind::LinearPattern};
  FeatureId feature_{""};
  std::string name_;
  BodyId result_body_{""};
  std::vector<PatternSource> sources_;
  std::optional<AxisReference> direction_;
  std::optional<PlaneReference> mirror_plane_;
  ParameterId count_parameter_{""};
  ParameterId extent_parameter_{""};
  LinearPatternExtentMode extent_mode_{LinearPatternExtentMode::Spacing};
  PatternDirectionSign direction_sign_{PatternDirectionSign::Positive};
  bool equal_spacing_{true};
  int count_{1};
  double spacing_{0.0};
  double angle_{360.0};
  GuiFeatureManipulatorFrame frame_;
};

enum class GuiBodyOperationKind { Boolean, Transform };

class GuiInteractiveBodyOperationController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_boolean(const GuiDocumentSession& session, FeatureId feature, BodyBooleanOperation operation,
                BodyId target_body, BodyBooleanResultMode result_mode,
                std::optional<BodyId> produced_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive boolean requires an active Part document");
    if (part->find_body(target_body) == nullptr)
      return fail("interactive boolean requires an existing target Body");
    if (result_mode == BodyBooleanResultMode::NewBody &&
        (!produced_body.has_value() || part->find_body(*produced_body) == nullptr))
      return fail("boolean NewBody requires an existing produced Body");
    kind_ = GuiBodyOperationKind::Boolean;
    feature_ = std::move(feature);
    operation_ = operation;
    target_body_ = std::move(target_body);
    tool_bodies_.clear();
    result_mode_ = result_mode;
    produced_body_ = std::move(produced_body);
    keep_tool_bodies_ = false;
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> add_tool_body(const GuiDocumentSession& session, BodyId body) {
    if (!active_ || kind_ != GuiBodyOperationKind::Boolean)
      return fail("no boolean command is active");
    const auto* part = session.part_document();
    if (part == nullptr || part->find_body(body) == nullptr)
      return fail("boolean tool Body must exist");
    if (body == target_body_ ||
        std::find(tool_bodies_.begin(), tool_bodies_.end(), body) != tool_bodies_.end())
      return fail("boolean tool Body is already a role in this operation");
    tool_bodies_.push_back(std::move(body));
    return Result<std::size_t>::success(tool_bodies_.size());
  }

  void set_operation(BodyBooleanOperation operation) noexcept { operation_ = operation; }
  void set_keep_tool_bodies(bool keep) noexcept { keep_tool_bodies_ = keep; }

  [[nodiscard]] Result<std::size_t>
  begin_translate(const GuiDocumentSession& session, BodyTransformId id, BodyId body) {
    if (auto ok = begin_transform(session, std::move(id), std::move(body)); ok.has_error())
      return ok;
    transform_kind_ = BodyTransformKind::Translate;
    translation_ = {0.0, 0.0, 0.0};
    return Result<std::size_t>::success(1);
  }
  [[nodiscard]] Result<std::size_t>
  begin_rotate(const GuiDocumentSession& session, BodyTransformId id, BodyId body,
               BodyTransformRotationAxis rotation_axis) {
    if (auto ok = begin_transform(session, std::move(id), std::move(body)); ok.has_error())
      return ok;
    transform_kind_ = BodyTransformKind::Rotate;
    rotation_axis_ = std::move(rotation_axis);
    angle_ = 0.0;
    return Result<std::size_t>::success(1);
  }
  [[nodiscard]] Result<std::size_t>
  begin_uniform_scale(const GuiDocumentSession& session, BodyTransformId id, BodyId body,
                      Point3 center = {}) {
    if (auto ok = begin_transform(session, std::move(id), std::move(body)); ok.has_error())
      return ok;
    transform_kind_ = BodyTransformKind::UniformScale;
    scale_center_ = center;
    scale_factor_ = 1.0;
    return Result<std::size_t>::success(1);
  }

  void set_translation(Vector3 translation_mm) noexcept { translation_ = translation_mm; }
  void set_angle(double degrees) noexcept { angle_ = degrees; }
  void set_scale_factor(double factor) noexcept { scale_factor_ = factor; }
  void set_apply_to_owned_sketches(bool value) noexcept { apply_to_owned_sketches_ = value; }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] GuiBodyOperationKind kind() const noexcept { return kind_; }
  [[nodiscard]] std::size_t tool_count() const noexcept { return tool_bodies_.size(); }
  [[nodiscard]] BodyBooleanOperation operation() const noexcept { return operation_; }
  [[nodiscard]] BodyTransformKind transform_kind() const noexcept { return transform_kind_; }
  [[nodiscard]] Vector3 translation() const noexcept { return translation_; }
  [[nodiscard]] double angle() const noexcept { return angle_; }
  [[nodiscard]] double scale_factor() const noexcept { return scale_factor_; }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (!active_ || kind_ != GuiBodyOperationKind::Transform) return handles;
    if (transform_kind_ == BodyTransformKind::Translate) {
      handles = GuiViewportManipulatorLayer::translation_triad(std::string(kTransformTranslatePrefix),
                                                               frame_.origin);
    } else if (transform_kind_ == BodyTransformKind::Rotate) {
      GuiViewportManipulatorHandle handle;
      handle.id = std::string(kTransformAngleHandleId);
      handle.kind = GuiViewportManipulatorKind::Angular;
      handle.origin = frame_.origin;
      handle.plane_normal = frame_.plane_normal;
      handle.reference_direction = frame_.reference_direction;
      handle.reference_value = angle_;
      handles.push_back(std::move(handle));
    } else {
      GuiViewportManipulatorHandle handle;
      handle.id = std::string(kTransformScaleHandleId);
      handle.kind = GuiViewportManipulatorKind::Linear;
      handle.origin = frame_.origin;
      handle.axis = frame_.axis;
      handle.reference_value = scale_factor_;
      handle.minimum_value = 0.0;
      handles.push_back(std::move(handle));
    }
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (kind_ != GuiBodyOperationKind::Transform) return false;
    if (candidate.value_kind == GuiViewportManipulatorValueKind::Translation &&
        transform_kind_ == BodyTransformKind::Translate) {
      const std::string prefix(kTransformTranslatePrefix);
      if (candidate.handle_id == prefix + ".x") { translation_.x = candidate.scalar_value; return true; }
      if (candidate.handle_id == prefix + ".y") { translation_.y = candidate.scalar_value; return true; }
      if (candidate.handle_id == prefix + ".z") { translation_.z = candidate.scalar_value; return true; }
      return false;
    }
    if (candidate.handle_id == kTransformAngleHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Angle) {
      angle_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kTransformScaleHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance) {
      scale_factor_ = candidate.scalar_value;
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no body operation is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    PartDocument candidate = *part;
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
        std::string(kind_ == GuiBodyOperationKind::Boolean ? "Apply Boolean" : "Apply Move Body"),
        [this](PartDocument& part) { return add_record(part); });
    if (committed.has_error()) return committed;
    active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  [[nodiscard]] Result<std::size_t> begin_transform(const GuiDocumentSession& session,
                                                    BodyTransformId id, BodyId body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive transform requires an active Part document");
    if (part->find_body(body) == nullptr)
      return fail("interactive transform requires an existing Body");
    kind_ = GuiBodyOperationKind::Transform;
    feature_ = FeatureId(id.value());
    transform_id_ = std::move(id);
    transform_body_ = std::move(body);
    apply_to_owned_sketches_ = false;
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> add_record(PartDocument& part) const {
    if (kind_ == GuiBodyOperationKind::Boolean) {
      auto feature =
          BodyBooleanFeature::create(feature_, operation_, target_body_, tool_bodies_, result_mode_,
                                     produced_body_, keep_tool_bodies_);
      if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
      return part.add_body_boolean_feature(std::move(feature.value()));
    }
    Result<BodyTransform> transform = build_transform();
    if (transform.has_error()) return Result<std::size_t>::failure(transform.error());
    return part.add_body_transform(std::move(transform.value()));
  }

  [[nodiscard]] Result<BodyTransform> build_transform() const {
    switch (transform_kind_) {
    case BodyTransformKind::Translate:
      return BodyTransform::create_translate(transform_id_, transform_body_,
                                             BodyTransformCoordinateSpace::World, std::nullopt,
                                             translation_, apply_to_owned_sketches_, false);
    case BodyTransformKind::Rotate:
      if (!rotation_axis_.has_value())
        return Result<BodyTransform>::failure(
            Error::validation(transform_id_.value(), "rotate requires a rotation axis"));
      return BodyTransform::create_rotate(transform_id_, transform_body_,
                                          BodyTransformCoordinateSpace::World, std::nullopt,
                                          *rotation_axis_, angle_, apply_to_owned_sketches_, false);
    case BodyTransformKind::UniformScale:
      return BodyTransform::create_uniform_scale(transform_id_, transform_body_,
                                                 BodyTransformCoordinateSpace::World, std::nullopt,
                                                 scale_factor_, scale_center_,
                                                 apply_to_owned_sketches_, false);
    }
    return Result<BodyTransform>::failure(
        Error::validation(transform_id_.value(), "unsupported transform kind"));
  }

  [[nodiscard]] std::string summary() const {
    if (kind_ == GuiBodyOperationKind::Boolean)
      return std::string("boolean ") + std::string(to_string(operation_)) + ", " +
             std::to_string(tool_bodies_.size()) + " tools";
    return std::string("transform ") + std::string(to_string(transform_kind_));
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_body_operation", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_body_operation", std::move(message)));
  }

  bool active_{false};
  GuiBodyOperationKind kind_{GuiBodyOperationKind::Boolean};
  FeatureId feature_{""};
  // Boolean state.
  BodyBooleanOperation operation_{BodyBooleanOperation::Add};
  BodyId target_body_{""};
  std::vector<BodyId> tool_bodies_;
  BodyBooleanResultMode result_mode_{BodyBooleanResultMode::ModifyTarget};
  std::optional<BodyId> produced_body_;
  bool keep_tool_bodies_{false};
  // Transform state.
  BodyTransformId transform_id_{""};
  BodyId transform_body_{""};
  BodyTransformKind transform_kind_{BodyTransformKind::Translate};
  Vector3 translation_{0.0, 0.0, 0.0};
  std::optional<BodyTransformRotationAxis> rotation_axis_;
  double angle_{0.0};
  double scale_factor_{1.0};
  Point3 scale_center_{0.0, 0.0, 0.0};
  bool apply_to_owned_sketches_{false};
  GuiFeatureManipulatorFrame frame_;
};

} // namespace blcad::gui
