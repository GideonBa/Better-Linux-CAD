#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_profile_materialization.hpp"
#include "blcad/gui/gui_modeling_workspace.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"

#include "blcad/core/feature.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_feature_input_reference.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/revolve_feature.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {

// Block 124 interactive Extrude / path Extrude / Revolve authoring.
//
// These are headless GUI controllers. They own one disposable candidate feature
// parameter set, consume Block-123 manipulator candidates, validate against a
// throwaway PartDocument clone (Geometry recompute plan), and commit exactly one
// document transaction on Apply. Qt (Block-122 workspace + Block-123 manipulator
// shell) stays a candidate/preview client; no new Core intent is introduced.
//
// Stable manipulator handle ids the binder and tests drive:
inline constexpr std::string_view kExtrudeExtentHandleId = "extrude.extent";
inline constexpr std::string_view kExtrudeTaperHandleId = "extrude.taper";
inline constexpr std::string_view kExtrudeThinHandleId = "extrude.thin";
inline constexpr std::string_view kRevolveAngleHandleId = "revolve.angle";

// Model-space frame a later command supplies from the resolved profile plane /
// revolve axis. Defaults describe a Z-normal profile so headless tests can drive
// the handles without a viewport.
struct GuiFeatureManipulatorFrame {
  Point3 origin{0.0, 0.0, 0.0};
  Vector3 axis{0.0, 0.0, 1.0};
  Vector3 plane_normal{0.0, 0.0, 1.0};
  Vector3 reference_direction{1.0, 0.0, 0.0};
};

class GuiInteractiveExtrudeController {
public:
  // Begin authoring an additive extrude of one materialized profile region. The
  // driving distance parameter must already exist as a Length; its current value
  // seeds the extent-arrow drag so the preview never jumps to the handle tip.
  [[nodiscard]] Result<std::size_t>
  begin(const GuiDocumentSession& session, SketchId sketch, ProfileId profile, FeatureId feature,
        std::string name, ParameterId distance_parameter, BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr)
      return fail("interactive extrude requires an active Part document");
    auto pending_profile = prepare_interactive_profile(*part, sketch, profile);
    if (pending_profile.has_error())
      return Result<std::size_t>::failure(pending_profile.error());
    const Parameter* driver = part->find_parameter(distance_parameter);
    if (driver == nullptr || driver->type() != ParameterType::Length)
      return fail("interactive extrude requires an existing Length distance parameter");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive extrude requires an existing result Body");

    pending_distance_parameter_.reset();
    pending_result_body_.reset();
    pending_profile_materialization_ = std::move(pending_profile.value());
    initialize(std::move(sketch), std::move(profile), std::move(feature), std::move(name),
               std::move(distance_parameter), std::move(result_body),
               driver->value().millimeters());
    return Result<std::size_t>::success(1);
  }

  // Shell-reset Block 135 auto-provisioning boundary. A freshly created Part
  // has no Body or feature-distance parameter yet. Keep those objects in the
  // disposable candidate and add them in the same Apply transaction as the
  // feature, so Cancel is mutation-free and one Undo removes the complete
  // operation. The established begin(...) path for existing inputs is
  // unchanged.
  [[nodiscard]] Result<std::size_t>
  begin_with_new_inputs(const GuiDocumentSession& session, SketchId sketch, ProfileId profile,
                        FeatureId feature, std::string name, Parameter distance_parameter,
                        Body result_body) {
    const auto* part = session.part_document();
    if (part == nullptr)
      return fail("interactive extrude requires an active Part document");
    auto pending_profile = prepare_interactive_profile(*part, sketch, profile);
    if (pending_profile.has_error())
      return Result<std::size_t>::failure(pending_profile.error());
    if (distance_parameter.type() != ParameterType::Length)
      return fail("interactive extrude requires a Length distance parameter");
    if (part->find_parameter(distance_parameter.id()) != nullptr)
      return fail("auto-provisioned extrude distance parameter must be new");
    if (part->find_body(result_body.id()) != nullptr)
      return fail("auto-provisioned extrude result Body must be new");

    PartDocument candidate = *part;
    if (pending_profile.value().has_value()) {
      auto updated = candidate.update_sketch(pending_profile.value()->completed_sketch);
      if (updated.has_error())
        return Result<std::size_t>::failure(updated.error());
    }
    auto added_parameter = candidate.add_parameter(distance_parameter);
    if (added_parameter.has_error())
      return Result<std::size_t>::failure(added_parameter.error());
    auto added_body = candidate.add_body(result_body);
    if (added_body.has_error())
      return Result<std::size_t>::failure(added_body.error());

    const ParameterId parameter_id = distance_parameter.id();
    const BodyId body_id = result_body.id();
    const double distance = distance_parameter.value().millimeters();
    pending_distance_parameter_ = std::move(distance_parameter);
    pending_result_body_ = std::move(result_body);
    pending_profile_materialization_ = std::move(pending_profile.value());
    initialize(std::move(sketch), std::move(profile), std::move(feature), std::move(name),
               parameter_id, body_id, distance);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] const FeatureId& feature_id() const noexcept { return feature_; }
  [[nodiscard]] const ParameterId& distance_parameter_id() const noexcept {
    return distance_parameter_;
  }
  [[nodiscard]] const BodyId& result_body_id() const noexcept { return result_body_; }

private:
  void initialize(SketchId sketch, ProfileId profile, FeatureId feature, std::string name,
                  ParameterId distance_parameter, BodyId result_body, double distance) {
    sketch_ = std::move(sketch);
    profile_ = std::move(profile);
    feature_ = std::move(feature);
    name_ = std::move(name);
    distance_parameter_ = std::move(distance_parameter);
    result_body_ = std::move(result_body);
    distance_ = distance;
    extent_mode_ = ExtrudeExtentMode::Distance;
    operation_ = FeatureBodyOperationMode::NewBody;
    direction_ = ExtrudeDirection::SketchNormal;
    taper_deg_.reset();
    thin_.reset();
    path_curve_.reset();
    first_face_.reset();
    second_face_.reset();
    second_distance_parameter_.reset();
    pending_second_distance_parameter_.reset();
    active_ = true;
  }

public:
  [[nodiscard]] bool active() const noexcept { return active_; }

  void set_operation(FeatureBodyOperationMode operation, std::optional<BodyId> target = std::nullopt) {
    operation_ = operation;
    if (operation == FeatureBodyOperationMode::NewBody && pending_result_body_.has_value())
      result_body_ = pending_result_body_->id();
    else if (operation != FeatureBodyOperationMode::NewBody && target.has_value())
      result_body_ = std::move(*target);
  }
  void set_extent_mode(ExtrudeExtentMode mode) noexcept { extent_mode_ = mode; }
  void set_distance(double millimeters) noexcept { distance_ = millimeters; }
  void set_direction(ExtrudeDirection direction) noexcept { direction_ = direction; }
  void set_taper(std::optional<double> degrees) noexcept { taper_deg_ = degrees; }
  void set_two_sided_parameter(ParameterId second) {
    second_distance_parameter_ = std::move(second);
    pending_second_distance_parameter_.reset();
  }
  // A new second distance belongs to the same disposable candidate as the
  // primary distance and Body. Switching modes must not publish unused inputs.
  void set_two_sided_new_parameter(Parameter second) {
    second_distance_parameter_ = second.id();
    pending_second_distance_parameter_ = std::move(second);
  }
  void set_limit_faces(std::optional<FaceReference> first,
                       std::optional<FaceReference> second = std::nullopt) {
    first_face_ = std::move(first);
    second_face_ = std::move(second);
  }
  // Enable thin-wall mode driven by an existing Length thickness parameter.
  void set_thin(ExtrudeThinMode mode, ParameterId thickness_parameter, double thickness_mm,
                std::optional<ParameterId> second_thickness = std::nullopt) {
    thin_ = ThinState{mode, std::move(thickness_parameter), thickness_mm, std::move(second_thickness)};
  }
  void clear_thin() noexcept { thin_.reset(); }
  // Path Extrude: follow a connected PathCurve instead of a scalar extent.
  void set_path_curve(PathCurveId path_curve) {
    path_curve_ = std::move(path_curve);
    direction_ = ExtrudeDirection::Path;
  }

  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] double distance() const noexcept { return distance_; }
  [[nodiscard]] const std::optional<double>& taper() const noexcept { return taper_deg_; }
  [[nodiscard]] ExtrudeExtentMode extent_mode() const noexcept { return extent_mode_; }
  [[nodiscard]] FeatureBodyOperationMode operation() const noexcept { return operation_; }

  // Handle for the extent arrow; empty unless the current mode drags a distance.
  [[nodiscard]] std::optional<GuiViewportManipulatorHandle> extent_handle() const {
    if (!drives_distance()) return std::nullopt;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kExtrudeExtentHandleId);
    handle.kind = GuiViewportManipulatorKind::Linear;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    handle.reference_value = distance_;
    // A zero-length prism is not a valid preview. Keep drag candidates inside
    // the same strictly-positive domain as Length parameters.
    handle.minimum_value = std::numeric_limits<double>::epsilon();
    return handle;
  }
  [[nodiscard]] std::optional<GuiViewportManipulatorHandle> taper_handle() const {
    if (!taper_deg_.has_value()) return std::nullopt;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kExtrudeTaperHandleId);
    handle.kind = GuiViewportManipulatorKind::Angular;
    handle.origin = frame_.origin;
    handle.plane_normal = frame_.plane_normal;
    handle.reference_direction = frame_.reference_direction;
    handle.reference_value = *taper_deg_;
    handle.minimum_value = std::nextafter(-90.0, 0.0);
    handle.maximum_value = std::nextafter(90.0, 0.0);
    return handle;
  }
  [[nodiscard]] std::optional<GuiViewportManipulatorHandle> thin_handle() const {
    if (!thin_.has_value()) return std::nullopt;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kExtrudeThinHandleId);
    handle.kind = GuiViewportManipulatorKind::Linear;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    handle.reference_value = thin_->thickness_mm;
    handle.minimum_value = std::numeric_limits<double>::epsilon();
    return handle;
  }

  // Every interactive controller exposes the same `handles()` surface so the
  // coordinator can publish them without knowing which family is active.
  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (auto handle = extent_handle()) handles.push_back(std::move(*handle));
    if (auto handle = taper_handle()) handles.push_back(std::move(*handle));
    if (auto handle = thin_handle()) handles.push_back(std::move(*handle));
    return handles;
  }

  // Fold a Block-123 candidate into the matching parameter. Returns true when a
  // field changed so the caller re-previews.
  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (candidate.handle_id == kExtrudeExtentHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance &&
        std::isfinite(candidate.scalar_value) && candidate.scalar_value > 0.0) {
      distance_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kExtrudeTaperHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Angle &&
        std::isfinite(candidate.scalar_value) && candidate.scalar_value > -90.0 &&
        candidate.scalar_value < 90.0) {
      taper_deg_ = candidate.scalar_value;
      return true;
    }
    if (candidate.handle_id == kExtrudeThinHandleId && thin_.has_value() &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance &&
        std::isfinite(candidate.scalar_value) && candidate.scalar_value > 0.0) {
      thin_->thickness_mm = candidate.scalar_value;
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no interactive extrude is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    auto feature = build_feature();
    if (feature.has_error()) return Result<GuiPartFeaturePreview>::failure(feature.error());
    PartDocument candidate = *part;
    if (auto provisioned = provision_pending_inputs(candidate); provisioned.has_error())
      return Result<GuiPartFeaturePreview>::failure(provisioned.error());
    if (auto seeded = seed_parameters(candidate); seeded.has_error())
      return Result<GuiPartFeaturePreview>::failure(seeded.error());
    auto added = candidate.add_feature(feature.value());
    if (added.has_error()) return Result<GuiPartFeaturePreview>::failure(added.error());
    auto plan = candidate.create_recompute_plan();
    if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
    return Result<GuiPartFeaturePreview>::success(
        {feature_, summary(), plan.value().step_count()});
  }

  // Build the real disposable OCCT result used by the shell's visual preview.
  // This follows the same candidate path as Apply without mutating the session.
  [[nodiscard]] Result<geometry::GeometryShape>
  preview_shape(const GuiDocumentSession& session) const {
    if (!active_)
      return shape_preview_fail("no interactive extrude is active");
    const auto* part = session.part_document();
    if (part == nullptr)
      return shape_preview_fail("preview requires an active Part document");
    auto feature = build_feature();
    if (feature.has_error())
      return Result<geometry::GeometryShape>::failure(feature.error());
    PartDocument candidate = *part;
    if (auto provisioned = provision_pending_inputs(candidate); provisioned.has_error())
      return Result<geometry::GeometryShape>::failure(provisioned.error());
    if (auto seeded = seed_parameters(candidate); seeded.has_error())
      return Result<geometry::GeometryShape>::failure(seeded.error());
    if (auto added = candidate.add_feature(std::move(feature.value())); added.has_error())
      return Result<geometry::GeometryShape>::failure(added.error());
    auto cache = geometry::ShapeCache::create(
        ShapeCacheId(feature_.value() + ".interactive_preview"));
    if (cache.has_error())
      return Result<geometry::GeometryShape>::failure(cache.error());
    auto recomputed =
        geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
    if (recomputed.has_error())
      return Result<geometry::GeometryShape>::failure(recomputed.error());
    const auto* shape = cache.value().find_body_shape(result_body_);
    if (shape == nullptr || shape->empty())
      return shape_preview_fail("extrude preview produced no visible Body shape");
    return Result<geometry::GeometryShape>::success(*shape);
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto feature = build_feature();
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    const std::string label =
        direction_ == ExtrudeDirection::Path ? "Apply Path Extrude" : "Apply Extrude";
    Result<std::size_t> committed = [&]() {
      if (pending_profile_materialization_.has_value()) {
        return session.commit_part_sketch_intent_transaction(
            label,
            [this, feature = std::move(feature.value())](
                PartDocument& part,
                std::vector<SketchConstraintCatalog>& constraint_catalogs,
                std::vector<SketchDimensionCatalog>& dimension_catalogs) mutable
                -> Result<std::size_t> {
              auto profile = publish_interactive_profile(
                  part, *pending_profile_materialization_, constraint_catalogs,
                  dimension_catalogs);
              if (profile.has_error()) return profile;
              auto objects = provision_pending_objects(part);
              if (objects.has_error()) return objects;
              if (auto seeded = seed_parameters(part); seeded.has_error()) return seeded;
              auto added = part.add_feature(std::move(feature));
              if (added.has_error()) return added;
              return Result<std::size_t>::success(profile.value() + objects.value() +
                                                  added.value());
            });
      }
      return session.commit_part_transaction(
          label,
          [this, feature = std::move(feature.value())](PartDocument& part) mutable
              -> Result<std::size_t> {
            auto objects = provision_pending_objects(part);
            if (objects.has_error()) return objects;
            if (auto seeded = seed_parameters(part); seeded.has_error()) return seeded;
            return part.add_feature(std::move(feature));
          });
    }();
    if (committed.has_error()) return committed;
    active_ = false;
    pending_distance_parameter_.reset();
    pending_second_distance_parameter_.reset();
    pending_result_body_.reset();
    pending_profile_materialization_.reset();
    return committed;
  }

  void cancel() noexcept {
    active_ = false;
    pending_distance_parameter_.reset();
    pending_second_distance_parameter_.reset();
    pending_result_body_.reset();
    pending_profile_materialization_.reset();
  }

private:
  struct ThinState {
    ExtrudeThinMode mode{ExtrudeThinMode::OneSided};
    ParameterId parameter;
    double thickness_mm{0.0};
    std::optional<ParameterId> second_parameter;
  };

  [[nodiscard]] bool drives_distance() const noexcept {
    return extent_mode_ == ExtrudeExtentMode::Distance ||
           extent_mode_ == ExtrudeExtentMode::Symmetric ||
           extent_mode_ == ExtrudeExtentMode::TwoSided;
  }

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    if (drives_distance()) {
      auto quantity = Quantity::length_mm(distance_, distance_parameter_.value());
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      auto changed = part.set_parameter_value(distance_parameter_, quantity.value());
      if (changed.has_error()) return Result<std::size_t>::failure(changed.error());
    }
    if (thin_.has_value()) {
      auto quantity = Quantity::length_mm(thin_->thickness_mm, thin_->parameter.value());
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      auto changed = part.set_parameter_value(thin_->parameter, quantity.value());
      if (changed.has_error()) return Result<std::size_t>::failure(changed.error());
    }
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t> provision_pending_profile(PartDocument& part) const {
    if (!pending_profile_materialization_.has_value())
      return Result<std::size_t>::success(0U);
    return part.update_sketch(pending_profile_materialization_->completed_sketch);
  }

  [[nodiscard]] Result<std::size_t> provision_pending_objects(PartDocument& part) const {
    std::size_t changed = 0U;
    if (drives_distance() && pending_distance_parameter_.has_value()) {
      auto added = part.add_parameter(*pending_distance_parameter_);
      if (added.has_error()) return added;
      changed += added.value();
    }
    if (extent_mode_ == ExtrudeExtentMode::TwoSided && pending_second_distance_parameter_) {
      auto added = part.add_parameter(*pending_second_distance_parameter_);
      if (added.has_error())
        return added;
      changed += added.value();
    }
    if (operation_ == FeatureBodyOperationMode::NewBody && pending_result_body_.has_value()) {
      auto added = part.add_body(*pending_result_body_);
      if (added.has_error()) return added;
      changed += added.value();
    }
    return Result<std::size_t>::success(changed);
  }

  [[nodiscard]] Result<std::size_t> provision_pending_inputs(PartDocument& part) const {
    auto profile = provision_pending_profile(part);
    if (profile.has_error()) return profile;
    auto objects = provision_pending_objects(part);
    if (objects.has_error()) return objects;
    return Result<std::size_t>::success(profile.value() + objects.value());
  }

  [[nodiscard]] Result<ExtrudeExtentIntent> build_extent() const {
    switch (extent_mode_) {
    case ExtrudeExtentMode::Distance:
      return ExtrudeExtentIntent::distance(distance_parameter_);
    case ExtrudeExtentMode::Symmetric:
      return ExtrudeExtentIntent::symmetric(distance_parameter_);
    case ExtrudeExtentMode::TwoSided:
      if (!second_distance_parameter_.has_value())
        return Result<ExtrudeExtentIntent>::failure(
            Error::validation(feature_.value(), "two-sided extrude requires a second distance parameter"));
      return ExtrudeExtentIntent::two_sided(distance_parameter_, *second_distance_parameter_);
    case ExtrudeExtentMode::ThroughAll:
      return Result<ExtrudeExtentIntent>::success(ExtrudeExtentIntent::through_all());
    case ExtrudeExtentMode::ToNext:
      return Result<ExtrudeExtentIntent>::success(ExtrudeExtentIntent::to_next());
    case ExtrudeExtentMode::ToFace:
      if (!first_face_.has_value())
        return Result<ExtrudeExtentIntent>::failure(
            Error::validation(feature_.value(), "to-face extrude requires a picked limit face"));
      return ExtrudeExtentIntent::to_face(*first_face_);
    case ExtrudeExtentMode::Between:
      if (!first_face_.has_value() || !second_face_.has_value())
        return Result<ExtrudeExtentIntent>::failure(
            Error::validation(feature_.value(), "between extrude requires two picked limit faces"));
      return ExtrudeExtentIntent::between(*first_face_, *second_face_);
    }
    return Result<ExtrudeExtentIntent>::failure(
        Error::validation(feature_.value(), "unsupported extrude extent mode"));
  }

  [[nodiscard]] Result<FeatureBodyResultContext> build_context() const {
    const bool new_body = operation_ == FeatureBodyOperationMode::NewBody;
    return FeatureBodyResultContext::create(
        operation_, new_body ? std::optional<BodyId>{} : std::optional<BodyId>{result_body_},
        new_body ? std::optional<BodyId>{result_body_} : std::optional<BodyId>{});
  }

  [[nodiscard]] Result<Feature> build_feature() const {
    auto context = build_context();
    if (context.has_error()) return Result<Feature>::failure(context.error());

    if (path_curve_.has_value()) {
      auto feature =
          Feature::create_additive_path_extrude(feature_, name_, sketch_, *path_curve_);
      if (feature.has_error()) return feature;
      auto profiled = feature.value().with_profile(profile_);
      if (profiled.has_error()) return profiled;
      return profiled.value().with_body_result_context(context.value());
    }

    auto extent = build_extent();
    if (extent.has_error()) return Result<Feature>::failure(extent.error());
    std::optional<ExtrudeThinIntent> thin;
    if (thin_.has_value()) {
      auto thin_intent =
          ExtrudeThinIntent::create(thin_->mode, thin_->parameter, thin_->second_parameter);
      if (thin_intent.has_error()) return Result<Feature>::failure(thin_intent.error());
      thin = std::move(thin_intent.value());
    }
    auto intent = ExtrudeFeatureIntent::create(std::move(extent.value()), taper_deg_, std::move(thin));
    if (intent.has_error()) return Result<Feature>::failure(intent.error());
    auto feature =
        Feature::create_additive_extrude(feature_, name_, sketch_, std::move(intent.value()), direction_);
    if (feature.has_error()) return feature;
    auto profiled = feature.value().with_profile(profile_);
    if (profiled.has_error()) return profiled;
    return profiled.value().with_body_result_context(context.value());
  }

  [[nodiscard]] std::string summary() const {
    if (path_curve_.has_value())
      return "additive path extrude, " + std::string(to_string(operation_));
    return "additive extrude, " + std::string(to_string(extent_mode_)) + ", " +
           std::string(to_string(operation_));
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.interactive_extrude", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_extrude", std::move(message)));
  }
  [[nodiscard]] static Result<geometry::GeometryShape>
  shape_preview_fail(std::string message) {
    return Result<geometry::GeometryShape>::failure(
        Error::validation("gui.interactive_extrude", std::move(message)));
  }

  bool active_{false};
  SketchId sketch_{""};
  ProfileId profile_{""};
  FeatureId feature_{""};
  std::string name_;
  ParameterId distance_parameter_{""};
  std::optional<ParameterId> second_distance_parameter_;
  BodyId result_body_{""};
  double distance_{0.0};
  ExtrudeExtentMode extent_mode_{ExtrudeExtentMode::Distance};
  FeatureBodyOperationMode operation_{FeatureBodyOperationMode::NewBody};
  ExtrudeDirection direction_{ExtrudeDirection::SketchNormal};
  std::optional<double> taper_deg_;
  std::optional<ThinState> thin_;
  std::optional<PathCurveId> path_curve_;
  std::optional<FaceReference> first_face_;
  std::optional<FaceReference> second_face_;
  std::optional<Parameter> pending_distance_parameter_;
  std::optional<Parameter> pending_second_distance_parameter_;
  std::optional<Body> pending_result_body_;
  std::optional<GuiPendingProfileMaterialization> pending_profile_materialization_;
  GuiFeatureManipulatorFrame frame_;
};

class GuiInteractiveRevolveController {
public:
  [[nodiscard]] Result<std::size_t>
  begin(const GuiDocumentSession& session, SketchId sketch, ProfileId profile, FeatureId feature,
        std::string name, AxisReference axis, BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr)
      return fail("interactive revolve requires an active Part document");
    auto pending_profile = prepare_interactive_profile(*part, sketch, profile);
    if (pending_profile.has_error())
      return Result<std::size_t>::failure(pending_profile.error());
    if (part->find_body(result_body) == nullptr)
      return fail("interactive revolve requires an existing result Body");

    pending_result_body_.reset();
    pending_profile_materialization_ = std::move(pending_profile.value());
    initialize(std::move(sketch), std::move(profile), std::move(feature), std::move(name),
               std::move(axis), std::move(result_body));
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_with_new_body(const GuiDocumentSession& session, SketchId sketch, ProfileId profile,
                      FeatureId feature, std::string name, AxisReference axis,
                      Body result_body) {
    const auto* part = session.part_document();
    if (part == nullptr)
      return fail("interactive revolve requires an active Part document");
    auto pending_profile = prepare_interactive_profile(*part, sketch, profile);
    if (pending_profile.has_error())
      return Result<std::size_t>::failure(pending_profile.error());
    if (part->find_body(result_body.id()) != nullptr)
      return fail("auto-provisioned revolve result Body must be new");
    PartDocument candidate = *part;
    if (pending_profile.value().has_value()) {
      auto updated = candidate.update_sketch(pending_profile.value()->completed_sketch);
      if (updated.has_error())
        return Result<std::size_t>::failure(updated.error());
    }
    auto added = candidate.add_body(result_body);
    if (added.has_error()) return Result<std::size_t>::failure(added.error());

    const BodyId body_id = result_body.id();
    pending_result_body_ = std::move(result_body);
    pending_profile_materialization_ = std::move(pending_profile.value());
    initialize(std::move(sketch), std::move(profile), std::move(feature), std::move(name),
               std::move(axis), body_id);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] const FeatureId& feature_id() const noexcept { return feature_; }
  [[nodiscard]] const BodyId& result_body_id() const noexcept { return result_body_; }

private:
  void initialize(SketchId sketch, ProfileId profile, FeatureId feature, std::string name,
                  AxisReference axis, BodyId result_body) {
    sketch_ = std::move(sketch);
    profile_ = std::move(profile);
    feature_ = std::move(feature);
    name_ = std::move(name);
    axis_ = std::move(axis);
    result_body_ = std::move(result_body);
    kind_ = RevolveFeatureKind::Revolve;
    extent_mode_ = RevolveExtentMode::Full;
    angle_deg_ = 360.0;
    side_ = RevolveSide::Positive;
    operation_ = FeatureBodyOperationMode::NewBody;
    active_ = true;
  }

public:
  [[nodiscard]] bool active() const noexcept { return active_; }

  void set_kind(RevolveFeatureKind kind) noexcept { kind_ = kind; }
  void set_operation(FeatureBodyOperationMode operation,
                     std::optional<BodyId> target = std::nullopt) {
    operation_ = operation;
    if (operation == FeatureBodyOperationMode::NewBody && pending_result_body_.has_value())
      result_body_ = pending_result_body_->id();
    else if (operation != FeatureBodyOperationMode::NewBody && target.has_value())
      result_body_ = std::move(*target);
  }
  void set_full() noexcept { extent_mode_ = RevolveExtentMode::Full; }
  void set_angle(double degrees, RevolveSide side = RevolveSide::Positive) noexcept {
    extent_mode_ = RevolveExtentMode::Angle;
    angle_deg_ = degrees;
    side_ = side;
  }
  void set_symmetric(double total_degrees) noexcept {
    extent_mode_ = RevolveExtentMode::Symmetric;
    angle_deg_ = total_degrees;
  }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] RevolveExtentMode extent_mode() const noexcept { return extent_mode_; }
  [[nodiscard]] double angle() const noexcept { return angle_deg_; }
  [[nodiscard]] FeatureBodyOperationMode operation() const noexcept { return operation_; }

  [[nodiscard]] std::optional<GuiViewportManipulatorHandle> angle_handle() const {
    if (extent_mode_ == RevolveExtentMode::Full) return std::nullopt;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kRevolveAngleHandleId);
    handle.kind = GuiViewportManipulatorKind::Angular;
    handle.origin = frame_.origin;
    handle.plane_normal = frame_.plane_normal;
    handle.reference_direction = frame_.reference_direction;
    handle.reference_value = angle_deg_;
    handle.minimum_value = std::numeric_limits<double>::epsilon();
    handle.maximum_value = std::nextafter(360.0, 0.0);
    return handle;
  }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (auto handle = angle_handle()) handles.push_back(std::move(*handle));
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (candidate.handle_id == kRevolveAngleHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Angle &&
        extent_mode_ != RevolveExtentMode::Full && std::isfinite(candidate.scalar_value) &&
        candidate.scalar_value > 0.0 && candidate.scalar_value < 360.0) {
      angle_deg_ = candidate.scalar_value;
      return true;
    }
    return false;
  }

  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no interactive revolve is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    auto feature = build_feature();
    if (feature.has_error()) return Result<GuiPartFeaturePreview>::failure(feature.error());
    PartDocument candidate = *part;
    if (auto profile = provision_pending_profile(candidate); profile.has_error())
      return Result<GuiPartFeaturePreview>::failure(profile.error());
    if (operation_ == FeatureBodyOperationMode::NewBody && pending_result_body_.has_value()) {
      auto body = candidate.add_body(*pending_result_body_);
      if (body.has_error()) return Result<GuiPartFeaturePreview>::failure(body.error());
    }
    auto added = candidate.add_revolve_feature(feature.value());
    if (added.has_error()) return Result<GuiPartFeaturePreview>::failure(added.error());
    auto plan = candidate.create_recompute_plan();
    if (plan.has_error()) return Result<GuiPartFeaturePreview>::failure(plan.error());
    return Result<GuiPartFeaturePreview>::success(
        {feature_, summary(), plan.value().step_count()});
  }

  [[nodiscard]] Result<geometry::GeometryShape>
  preview_shape(const GuiDocumentSession& session) const {
    if (!active_)
      return shape_preview_fail("no interactive revolve is active");
    const auto* part = session.part_document();
    if (part == nullptr)
      return shape_preview_fail("preview requires an active Part document");
    auto feature = build_feature();
    if (feature.has_error())
      return Result<geometry::GeometryShape>::failure(feature.error());
    PartDocument candidate = *part;
    if (auto profile = provision_pending_profile(candidate); profile.has_error())
      return Result<geometry::GeometryShape>::failure(profile.error());
    if (operation_ == FeatureBodyOperationMode::NewBody && pending_result_body_.has_value()) {
      auto body = candidate.add_body(*pending_result_body_);
      if (body.has_error())
        return Result<geometry::GeometryShape>::failure(body.error());
    }
    if (auto added = candidate.add_revolve_feature(std::move(feature.value()));
        added.has_error())
      return Result<geometry::GeometryShape>::failure(added.error());
    auto cache = geometry::ShapeCache::create(
        ShapeCacheId(feature_.value() + ".interactive_preview"));
    if (cache.has_error())
      return Result<geometry::GeometryShape>::failure(cache.error());
    auto recomputed =
        geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
    if (recomputed.has_error())
      return Result<geometry::GeometryShape>::failure(recomputed.error());
    const auto* shape = cache.value().find_body_shape(result_body_);
    if (shape == nullptr || shape->empty())
      return shape_preview_fail("revolve preview produced no visible Body shape");
    return Result<geometry::GeometryShape>::success(*shape);
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto feature = build_feature();
    if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
    const std::string label =
        kind_ == RevolveFeatureKind::RevolveCut ? "Apply Revolve Cut" : "Apply Revolve";
    Result<std::size_t> committed = [&]() {
      if (pending_profile_materialization_.has_value()) {
        return session.commit_part_sketch_intent_transaction(
            label,
            [this, feature = std::move(feature.value())](
                PartDocument& part,
                std::vector<SketchConstraintCatalog>& constraint_catalogs,
                std::vector<SketchDimensionCatalog>& dimension_catalogs) mutable
                -> Result<std::size_t> {
              auto profile = publish_interactive_profile(
                  part, *pending_profile_materialization_, constraint_catalogs,
                  dimension_catalogs);
              if (profile.has_error()) return profile;
              auto body = provision_pending_body(part);
              if (body.has_error()) return body;
              auto added = part.add_revolve_feature(std::move(feature));
              if (added.has_error()) return added;
              return Result<std::size_t>::success(profile.value() + body.value() +
                                                  added.value());
            });
      }
      return session.commit_part_transaction(
          label, [this, feature = std::move(feature.value())](PartDocument& part) mutable {
            auto body = provision_pending_body(part);
            if (body.has_error()) return body;
            return part.add_revolve_feature(std::move(feature));
          });
    }();
    if (committed.has_error()) return committed;
    active_ = false;
    pending_result_body_.reset();
    pending_profile_materialization_.reset();
    return committed;
  }

  void cancel() noexcept {
    active_ = false;
    pending_result_body_.reset();
    pending_profile_materialization_.reset();
  }

private:
  [[nodiscard]] Result<std::size_t> provision_pending_profile(PartDocument& part) const {
    if (!pending_profile_materialization_.has_value())
      return Result<std::size_t>::success(0U);
    return part.update_sketch(pending_profile_materialization_->completed_sketch);
  }

  [[nodiscard]] Result<std::size_t> provision_pending_body(PartDocument& part) const {
    if (operation_ != FeatureBodyOperationMode::NewBody || !pending_result_body_.has_value())
      return Result<std::size_t>::success(0U);
    return part.add_body(*pending_result_body_);
  }

  [[nodiscard]] Result<RevolveAngleExtent> build_extent() const {
    switch (extent_mode_) {
    case RevolveExtentMode::Full:
      return Result<RevolveAngleExtent>::success(RevolveAngleExtent::full());
    case RevolveExtentMode::Angle:
      return RevolveAngleExtent::angle(angle_deg_, side_);
    case RevolveExtentMode::Symmetric:
      return RevolveAngleExtent::symmetric(angle_deg_);
    }
    return Result<RevolveAngleExtent>::failure(
        Error::validation(feature_.value(), "unsupported revolve extent mode"));
  }

  [[nodiscard]] Result<RevolveFeature> build_feature() const {
    if (!axis_.has_value())
      return Result<RevolveFeature>::failure(
          Error::validation(feature_.value(), "interactive revolve requires an axis reference"));
    auto profile = ProfileRegionReference::create(sketch_, profile_,
                                                  PartFeatureInputRole::RevolveProfile);
    if (profile.has_error()) return Result<RevolveFeature>::failure(profile.error());
    auto extent = build_extent();
    if (extent.has_error()) return Result<RevolveFeature>::failure(extent.error());
    const bool new_body = operation_ == FeatureBodyOperationMode::NewBody;
    auto context = FeatureBodyResultContext::create(
        operation_, new_body ? std::optional<BodyId>{} : std::optional<BodyId>{result_body_},
        new_body ? std::optional<BodyId>{result_body_} : std::optional<BodyId>{});
    if (context.has_error()) return Result<RevolveFeature>::failure(context.error());
    return kind_ == RevolveFeatureKind::RevolveCut
               ? RevolveFeature::create_revolve_cut(feature_, name_, profile.value(), *axis_,
                                                    std::move(extent.value()), context.value())
               : RevolveFeature::create_revolve(feature_, name_, profile.value(), *axis_,
                                                std::move(extent.value()), context.value());
  }

  [[nodiscard]] std::string summary() const {
    return std::string(to_string(kind_)) + ", " + std::string(to_string(extent_mode_)) + ", " +
           std::string(to_string(operation_));
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.interactive_revolve", std::move(message)));
  }
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_revolve", std::move(message)));
  }
  [[nodiscard]] static Result<geometry::GeometryShape>
  shape_preview_fail(std::string message) {
    return Result<geometry::GeometryShape>::failure(
        Error::validation("gui.interactive_revolve", std::move(message)));
  }

  bool active_{false};
  SketchId sketch_{""};
  ProfileId profile_{""};
  FeatureId feature_{""};
  std::string name_;
  std::optional<AxisReference> axis_;
  BodyId result_body_{""};
  RevolveFeatureKind kind_{RevolveFeatureKind::Revolve};
  RevolveExtentMode extent_mode_{RevolveExtentMode::Full};
  double angle_deg_{360.0};
  RevolveSide side_{RevolveSide::Positive};
  FeatureBodyOperationMode operation_{FeatureBodyOperationMode::NewBody};
  std::optional<Body> pending_result_body_;
  std::optional<GuiPendingProfileMaterialization> pending_profile_materialization_;
  GuiFeatureManipulatorFrame frame_;
};

} // namespace blcad::gui
