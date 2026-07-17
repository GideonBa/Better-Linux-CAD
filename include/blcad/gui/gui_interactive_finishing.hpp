#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include "blcad/core/draft_feature.hpp"
#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/shell_feature.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace blcad::gui {

// Stable per-edge / per-face identity, matching the Core edge/face-treatment
// dedup key so pick-time rejection agrees with feature validation.
[[nodiscard]] inline std::string interactive_edge_identity(const EdgeReference& edge) {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticEdgeReference>)
          return source.node_id();
        else
          return source.source_feature().value() + ":" + source.source_profile().value() + ":" +
                 std::string(to_string(source.edge()));
      },
      edge.source());
}
[[nodiscard]] inline std::string interactive_face_identity(const FaceReference& face) {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return source.source_feature().value() + ":" + std::string(to_string(source.face()));
        else
          return source.source_feature().value() + ":" + source.source_profile().value() + ":" +
                 std::string(to_string(source.face()));
      },
      face.source());
}

// Block 125 interactive Fillet / Chamfer / Shell / Draft authoring.
//
// Headless GUI controllers over the frozen Block-56..61 finishing contracts.
// Each collects an ordered, duplicate-free chain of semantic edges (finishing)
// or faces (shell/draft), drives Block-123 handles into existing driving
// parameters, previews a throwaway PartDocument clone through the recompute
// plan, and commits one transaction that seeds the parameters and adds the
// feature. Edges/faces without stable semantic identity are unpickable at the
// selection layer and never reach these controllers as references.
inline constexpr std::string_view kFilletRadiusHandleId = "fillet.radius";
inline constexpr std::string_view kChamferDistanceHandleId = "chamfer.distance";
inline constexpr std::string_view kChamferDistance2HandleId = "chamfer.distance2";
inline constexpr std::string_view kChamferAngleHandleId = "chamfer.angle";
inline constexpr std::string_view kShellThicknessHandleId = "shell.thickness";
inline constexpr std::string_view kDraftAngleHandleId = "draft.angle";

enum class GuiFinishingKind { Fillet, Chamfer };

class GuiInteractiveFinishingController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_fillet(const GuiDocumentSession& session, BodyId target_body, FeatureId feature,
               std::string name, ParameterId radius_parameter) {
    auto seeded = begin_common(session, target_body, radius_parameter, ParameterType::Length);
    if (seeded.has_error()) return Result<std::size_t>::failure(seeded.error());
    reset(std::move(target_body), std::move(feature), std::move(name));
    kind_ = GuiFinishingKind::Fillet;
    first_parameter_ = std::move(radius_parameter);
    first_value_ = seeded.value_as_double();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_chamfer_equal(const GuiDocumentSession& session, BodyId target_body, FeatureId feature,
                      std::string name, ParameterId distance_parameter) {
    auto seeded = begin_common(session, target_body, distance_parameter, ParameterType::Length);
    if (seeded.has_error()) return Result<std::size_t>::failure(seeded.error());
    reset(std::move(target_body), std::move(feature), std::move(name));
    kind_ = GuiFinishingKind::Chamfer;
    chamfer_mode_ = ChamferMode::EqualDistance;
    first_parameter_ = std::move(distance_parameter);
    first_value_ = seeded.value_as_double();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_chamfer_two_distance(const GuiDocumentSession& session, BodyId target_body,
                             FeatureId feature, std::string name, ParameterId first_distance,
                             ParameterId second_distance) {
    auto first = begin_common(session, target_body, first_distance, ParameterType::Length);
    if (first.has_error()) return Result<std::size_t>::failure(first.error());
    auto second = require_parameter(session, second_distance, ParameterType::Length);
    if (second.has_error()) return Result<std::size_t>::failure(second.error());
    reset(std::move(target_body), std::move(feature), std::move(name));
    kind_ = GuiFinishingKind::Chamfer;
    chamfer_mode_ = ChamferMode::TwoDistance;
    first_parameter_ = std::move(first_distance);
    first_value_ = first.value_as_double();
    second_parameter_ = std::move(second_distance);
    second_value_ = second.value_as_double();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_chamfer_distance_angle(const GuiDocumentSession& session, BodyId target_body,
                               FeatureId feature, std::string name, ParameterId distance,
                               ParameterId angle) {
    auto first = begin_common(session, target_body, distance, ParameterType::Length);
    if (first.has_error()) return Result<std::size_t>::failure(first.error());
    auto second = require_parameter(session, angle, ParameterType::Angle);
    if (second.has_error()) return Result<std::size_t>::failure(second.error());
    reset(std::move(target_body), std::move(feature), std::move(name));
    kind_ = GuiFinishingKind::Chamfer;
    chamfer_mode_ = ChamferMode::DistanceAngle;
    first_parameter_ = std::move(distance);
    first_value_ = first.value_as_double();
    second_parameter_ = std::move(angle);
    second_value_ = second.value_as_double();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] GuiFinishingKind kind() const noexcept { return kind_; }
  [[nodiscard]] ChamferMode chamfer_mode() const noexcept { return chamfer_mode_; }
  [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }
  [[nodiscard]] double first_value() const noexcept { return first_value_; }
  [[nodiscard]] double second_value() const noexcept { return second_value_; }

  // Typed numeric-HUD input for the primary (radius/first distance) and, for
  // two-parameter chamfers, the secondary (second distance / angle) value.
  void set_first_value(double value) noexcept { first_value_ = value; }
  void set_second_value(double value) noexcept { second_value_ = value; }

  // Append one edge to the ordered chain, rejecting a duplicate stable id.
  [[nodiscard]] Result<std::size_t> add_edge(EdgeReference edge) {
    if (!active_)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_finishing", "no finishing command is active"));
    const std::string id = interactive_edge_identity(edge);
    for (const auto& existing : edges_)
      if (interactive_edge_identity(existing) == id)
        return Result<std::size_t>::failure(
            Error::validation(id, "edge is already in the finishing chain"));
    edges_.push_back(std::move(edge));
    return Result<std::size_t>::success(edges_.size());
  }

  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (!active_) return handles;
    if (kind_ == GuiFinishingKind::Fillet) {
      handles.push_back(linear_handle(kFilletRadiusHandleId, first_value_));
    } else {
      handles.push_back(linear_handle(kChamferDistanceHandleId, first_value_));
      if (chamfer_mode_ == ChamferMode::TwoDistance)
        handles.push_back(linear_handle(kChamferDistance2HandleId, second_value_));
      else if (chamfer_mode_ == ChamferMode::DistanceAngle)
        handles.push_back(angular_handle(kChamferAngleHandleId, second_value_));
    }
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    const bool distance = candidate.value_kind == GuiViewportManipulatorValueKind::Distance;
    const bool angle = candidate.value_kind == GuiViewportManipulatorValueKind::Angle;
    if (candidate.handle_id == kFilletRadiusHandleId && distance) {
      first_value_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kChamferDistanceHandleId && distance) {
      first_value_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kChamferDistance2HandleId && distance &&
        chamfer_mode_ == ChamferMode::TwoDistance) {
      second_value_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kChamferAngleHandleId && angle &&
        chamfer_mode_ == ChamferMode::DistanceAngle) {
      second_value_ = candidate.scalar_value;
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_)
      return preview_fail("no finishing command is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    if (edges_.empty()) return preview_fail("finishing requires at least one picked edge");
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
        std::string(kind_ == GuiFinishingKind::Fillet ? "Apply Fillet" : "Apply Chamfer"),
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
  struct SeededParameter {
    double value{0.0};
    [[nodiscard]] double value_as_double() const noexcept { return value; }
    [[nodiscard]] bool has_error() const noexcept { return errored_; }
    [[nodiscard]] const Error& error() const noexcept { return error_; }
    bool errored_{false};
    Error error_{Error::validation("", "")};
  };

  [[nodiscard]] SeededParameter require_parameter(const GuiDocumentSession& session,
                                                  const ParameterId& id, ParameterType type) const {
    const auto* part = session.part_document();
    if (part == nullptr)
      return {0.0, true, Error::validation("gui.interactive_finishing", "no active Part document")};
    const Parameter* parameter = part->find_parameter(id);
    if (parameter == nullptr || parameter->type() != type)
      return {0.0, true, Error::validation(id.value(), "finishing requires the named driving parameter")};
    const double value = type == ParameterType::Angle ? parameter->value().degrees()
                                                       : parameter->value().millimeters();
    return {value, false, Error::validation("", "")};
  }

  [[nodiscard]] SeededParameter begin_common(const GuiDocumentSession& session,
                                             const BodyId& target_body, const ParameterId& parameter,
                                             ParameterType type) const {
    const auto* part = session.part_document();
    if (part == nullptr)
      return {0.0, true, Error::validation("gui.interactive_finishing", "no active Part document")};
    if (part->find_body(target_body) == nullptr)
      return {0.0, true,
              Error::validation(target_body.value(), "finishing requires an existing target Body")};
    return require_parameter(session, parameter, type);
  }

  void reset(BodyId target_body, FeatureId feature, std::string name) {
    target_body_ = std::move(target_body);
    feature_ = std::move(feature);
    name_ = std::move(name);
    edges_.clear();
    second_parameter_.reset();
    second_value_ = 0.0;
    active_ = true;
  }

  [[nodiscard]] GuiViewportManipulatorHandle linear_handle(std::string_view id,
                                                           double reference) const {
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(id);
    handle.kind = GuiViewportManipulatorKind::Linear;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    handle.reference_value = reference;
    handle.minimum_value = 0.0;
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

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    if (auto seeded = seed_length(part, first_parameter_, first_value_); seeded.has_error())
      return seeded;
    if (kind_ == GuiFinishingKind::Chamfer && second_parameter_.has_value()) {
      if (chamfer_mode_ == ChamferMode::DistanceAngle)
        return seed_angle(part, *second_parameter_, second_value_);
      return seed_length(part, *second_parameter_, second_value_);
    }
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] static Result<std::size_t> seed_length(PartDocument& part, const ParameterId& id,
                                                       double value) {
    auto quantity = Quantity::length_mm(value, id.value());
    if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
    auto changed = part.set_parameter_value(id, quantity.value());
    return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                               : Result<std::size_t>::success(changed.value().size());
  }
  [[nodiscard]] static Result<std::size_t> seed_angle(PartDocument& part, const ParameterId& id,
                                                      double value) {
    auto quantity = Quantity::angle_deg(value, id.value());
    if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
    auto changed = part.set_parameter_value(id, quantity.value());
    return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                               : Result<std::size_t>::success(changed.value().size());
  }

  [[nodiscard]] Result<std::size_t> add_feature(PartDocument& part) const {
    if (kind_ == GuiFinishingKind::Fillet) {
      auto feature = FilletFeature::create(feature_, name_, target_body_, edges_, first_parameter_);
      if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
      return part.add_fillet_feature(std::move(feature.value()));
    }
    Result<ChamferFeature> feature = Result<ChamferFeature>::failure(
        Error::validation(feature_.value(), "unsupported chamfer mode"));
    switch (chamfer_mode_) {
    case ChamferMode::EqualDistance:
      feature = ChamferFeature::create_equal_distance(feature_, name_, target_body_, edges_,
                                                      first_parameter_);
      break;
    case ChamferMode::TwoDistance:
      feature = ChamferFeature::create_two_distance(feature_, name_, target_body_, edges_,
                                                    first_parameter_, *second_parameter_);
      break;
    case ChamferMode::DistanceAngle:
      feature = ChamferFeature::create_distance_angle(feature_, name_, target_body_, edges_,
                                                      first_parameter_, *second_parameter_);
      break;
    }
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    return part.add_chamfer_feature(std::move(feature.value()));
  }

  [[nodiscard]] std::string summary() const {
    if (kind_ == GuiFinishingKind::Fillet)
      return "fillet, " + std::to_string(edges_.size()) + " edges";
    return std::string("chamfer, ") + std::string(to_string(chamfer_mode_)) + ", " +
           std::to_string(edges_.size()) + " edges";
  }

  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_finishing", std::move(message)));
  }

  bool active_{false};
  GuiFinishingKind kind_{GuiFinishingKind::Fillet};
  ChamferMode chamfer_mode_{ChamferMode::EqualDistance};
  BodyId target_body_{""};
  FeatureId feature_{""};
  std::string name_;
  std::vector<EdgeReference> edges_;
  ParameterId first_parameter_{""};
  double first_value_{0.0};
  std::optional<ParameterId> second_parameter_;
  double second_value_{0.0};
  GuiFeatureManipulatorFrame frame_;
};

enum class GuiShellDraftKind { Shell, Draft };

class GuiInteractiveShellDraftController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_shell(const GuiDocumentSession& session, BodyId target_body, FeatureId feature,
              std::string name, ParameterId thickness_parameter,
              ShellDirection direction = ShellDirection::Inward) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive shell requires an active Part document");
    if (part->find_body(target_body) == nullptr)
      return fail("interactive shell requires an existing target Body");
    const Parameter* thickness = part->find_parameter(thickness_parameter);
    if (thickness == nullptr || thickness->type() != ParameterType::Length)
      return fail("interactive shell requires an existing Length thickness parameter");
    reset(std::move(target_body), std::move(feature), std::move(name));
    kind_ = GuiShellDraftKind::Shell;
    thickness_parameter_ = std::move(thickness_parameter);
    thickness_ = thickness->value().millimeters();
    direction_ = direction;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_draft(const GuiDocumentSession& session, BodyId target_body, FeatureId feature,
              std::string name, AxisReference pull_direction, PlaneReference neutral_plane,
              ParameterId angle_parameter) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive draft requires an active Part document");
    if (part->find_body(target_body) == nullptr)
      return fail("interactive draft requires an existing target Body");
    const Parameter* angle = part->find_parameter(angle_parameter);
    if (angle == nullptr || angle->type() != ParameterType::Angle)
      return fail("interactive draft requires an existing Angle parameter");
    reset(std::move(target_body), std::move(feature), std::move(name));
    kind_ = GuiShellDraftKind::Draft;
    pull_direction_ = std::move(pull_direction);
    neutral_plane_ = std::move(neutral_plane);
    angle_parameter_ = std::move(angle_parameter);
    angle_ = angle->value().degrees();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] GuiShellDraftKind kind() const noexcept { return kind_; }
  [[nodiscard]] std::size_t face_count() const noexcept { return faces_.size(); }
  [[nodiscard]] double thickness() const noexcept { return thickness_; }
  [[nodiscard]] double angle() const noexcept { return angle_; }
  [[nodiscard]] ShellDirection direction() const noexcept { return direction_; }

  void set_direction(ShellDirection direction) noexcept { direction_ = direction; }
  void set_thickness(double millimeters) noexcept { thickness_ = millimeters; }
  void set_angle(double degrees) noexcept { angle_ = degrees; }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] Result<std::size_t> add_face(FaceReference face) {
    if (!active_)
      return Result<std::size_t>::failure(
          Error::validation("gui.interactive_shell_draft", "no shell/draft command is active"));
    const std::string id = interactive_face_identity(face);
    for (const auto& existing : faces_)
      if (interactive_face_identity(existing) == id)
        return Result<std::size_t>::failure(
            Error::validation(id, "face is already selected"));
    faces_.push_back(std::move(face));
    return Result<std::size_t>::success(faces_.size());
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (!active_) return handles;
    if (kind_ == GuiShellDraftKind::Shell) {
      GuiViewportManipulatorHandle handle;
      handle.id = std::string(kShellThicknessHandleId);
      handle.kind = GuiViewportManipulatorKind::Linear;
      handle.origin = frame_.origin;
      handle.axis = frame_.axis;
      handle.reference_value = thickness_;
      handle.minimum_value = 0.0;
      handles.push_back(std::move(handle));
    } else {
      GuiViewportManipulatorHandle handle;
      handle.id = std::string(kDraftAngleHandleId);
      handle.kind = GuiViewportManipulatorKind::Angular;
      handle.origin = frame_.origin;
      handle.plane_normal = frame_.plane_normal;
      handle.reference_direction = frame_.reference_direction;
      handle.reference_value = angle_;
      handles.push_back(std::move(handle));
    }
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (candidate.handle_id == kShellThicknessHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance) {
      thickness_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kDraftAngleHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Angle) {
      angle_ = candidate.scalar_value;
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no shell/draft command is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    if (faces_.empty()) return preview_fail("shell/draft requires at least one picked face");
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
        std::string(kind_ == GuiShellDraftKind::Shell ? "Apply Shell" : "Apply Draft"),
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
  void reset(BodyId target_body, FeatureId feature, std::string name) {
    target_body_ = std::move(target_body);
    feature_ = std::move(feature);
    name_ = std::move(name);
    faces_.clear();
    active_ = true;
  }

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    if (kind_ == GuiShellDraftKind::Shell) {
      auto quantity = Quantity::length_mm(thickness_, thickness_parameter_.value());
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      auto changed = part.set_parameter_value(thickness_parameter_, quantity.value());
      return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                                 : Result<std::size_t>::success(changed.value().size());
    }
    auto quantity = Quantity::angle_deg(angle_, angle_parameter_.value());
    if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
    auto changed = part.set_parameter_value(angle_parameter_, quantity.value());
    return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                               : Result<std::size_t>::success(changed.value().size());
  }

  [[nodiscard]] Result<std::size_t> add_feature(PartDocument& part) const {
    if (kind_ == GuiShellDraftKind::Shell) {
      auto feature = ShellFeature::create(feature_, name_, target_body_, faces_,
                                          thickness_parameter_, direction_);
      if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
      return part.add_shell_feature(std::move(feature.value()));
    }
    if (!pull_direction_.has_value() || !neutral_plane_.has_value())
      return Result<std::size_t>::failure(
          Error::validation(feature_.value(), "draft requires a pull direction and neutral plane"));
    auto feature = DraftFeature::create(feature_, name_, target_body_, faces_, *pull_direction_,
                                        *neutral_plane_, angle_parameter_);
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    return part.add_draft_feature(std::move(feature.value()));
  }

  [[nodiscard]] std::string summary() const {
    if (kind_ == GuiShellDraftKind::Shell)
      return std::string("shell, ") + std::string(to_string(direction_)) + ", " +
             std::to_string(faces_.size()) + " faces";
    return "draft, " + std::to_string(faces_.size()) + " faces";
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(
        Error::validation("gui.interactive_shell_draft", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_shell_draft", std::move(message)));
  }

  bool active_{false};
  GuiShellDraftKind kind_{GuiShellDraftKind::Shell};
  BodyId target_body_{""};
  FeatureId feature_{""};
  std::string name_;
  std::vector<FaceReference> faces_;
  ParameterId thickness_parameter_{""};
  double thickness_{0.0};
  ShellDirection direction_{ShellDirection::Inward};
  std::optional<AxisReference> pull_direction_;
  std::optional<PlaneReference> neutral_plane_;
  ParameterId angle_parameter_{""};
  double angle_{0.0};
  GuiFeatureManipulatorFrame frame_;
};

} // namespace blcad::gui
