#pragma once

#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_part_foundation_workbench.hpp"
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
// One headless controller over the frozen Block-88..92 SurfaceFeature contracts:
// Boundary/Fill collect boundary-curve chains; Trim picks a target surface and a
// trimming contour; Extend drags a boundary distance; Stitch collects surfaces
// with a typed tolerance; ClosedShellToSolid converts a closed shell. Because
// stitch free-edge/gap and closed-shell closedness/manifold diagnostics are
// geometric, preview runs the real recompute on a throwaway clone so the
// authoritative diagnostic is surfaced before commit. Apply commits one
// transaction that seeds any driving parameter and adds the record.
inline constexpr std::string_view kSurfaceExtendHandleId = "surface.extend";

class GuiInteractiveSurfaceController {
public:
  [[nodiscard]] Result<std::size_t>
  begin_boundary(const GuiDocumentSession& session, FeatureId feature, std::string name,
                 BodyId result_body) {
    return begin_curve_surface(session, SurfaceFeatureKind::BoundarySurface, std::move(feature),
                               std::move(name), std::move(result_body));
  }
  [[nodiscard]] Result<std::size_t>
  begin_fill(const GuiDocumentSession& session, FeatureId feature, std::string name,
             BodyId result_body) {
    return begin_curve_surface(session, SurfaceFeatureKind::FillSurface, std::move(feature),
                               std::move(name), std::move(result_body));
  }

  [[nodiscard]] Result<std::size_t>
  begin_trim(const GuiDocumentSession& session, FeatureId feature, std::string name,
             SurfaceReference target, TrimmingReference trimming, BodyId result_body) {
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body);
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::TrimSurface;
    target_ = std::move(target);
    trimming_ = std::move(trimming);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_extend(const GuiDocumentSession& session, FeatureId feature, std::string name,
               SurfaceReference target, BoundaryCurveReference boundary,
               ParameterId distance_parameter, BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive extend requires an active Part document");
    const Parameter* distance = part->find_parameter(distance_parameter);
    if (distance == nullptr || distance->type() != ParameterType::Length)
      return fail("surface extend requires an existing Length distance parameter");
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body);
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::ExtendSurface;
    target_ = std::move(target);
    extend_boundary_ = std::move(boundary);
    distance_parameter_ = std::move(distance_parameter);
    distance_ = distance->value().millimeters();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_stitch(const GuiDocumentSession& session, FeatureId feature, std::string name,
               ParameterId tolerance_parameter, BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive stitch requires an active Part document");
    const Parameter* tolerance = part->find_parameter(tolerance_parameter);
    if (tolerance == nullptr || tolerance->type() != ParameterType::Length)
      return fail("surface stitch requires an existing Length tolerance parameter");
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body);
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::SurfaceStitch;
    tolerance_parameter_ = std::move(tolerance_parameter);
    tolerance_ = tolerance->value().millimeters();
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<std::size_t>
  begin_closed_shell_to_solid(const GuiDocumentSession& session, FeatureId feature, std::string name,
                              SurfaceReference shell, BodyId result_body) {
    if (auto ok = begin_common(session, std::move(feature), std::move(name), result_body);
        ok.has_error())
      return ok;
    kind_ = SurfaceFeatureKind::ClosedShellToSolid;
    target_ = std::move(shell);
    return Result<std::size_t>::success(1);
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

  void set_distance(double millimeters) noexcept { distance_ = millimeters; }
  void set_tolerance(double millimeters) noexcept { tolerance_ = millimeters; }
  void set_manipulator_frame(GuiFeatureManipulatorFrame frame) noexcept { frame_ = frame; }

  [[nodiscard]] std::vector<GuiViewportManipulatorHandle> handles() const {
    std::vector<GuiViewportManipulatorHandle> handles;
    if (!active_ || kind_ != SurfaceFeatureKind::ExtendSurface) return handles;
    GuiViewportManipulatorHandle handle;
    handle.id = std::string(kSurfaceExtendHandleId);
    handle.kind = GuiViewportManipulatorKind::Linear;
    handle.origin = frame_.origin;
    handle.axis = frame_.axis;
    handle.reference_value = distance_;
    handle.minimum_value = 0.0;
    handles.push_back(std::move(handle));
    return handles;
  }

  [[nodiscard]] bool apply_manipulator(const GuiViewportManipulatorCandidate& candidate) {
    if (candidate.handle_id == kSurfaceExtendHandleId &&
        candidate.value_kind == GuiViewportManipulatorValueKind::Distance) {
      distance_ = candidate.scalar_value;
      return true;
    }
    return false;
  }

  // Preview runs the real recompute on a clone so geometric diagnostics
  // (free edges, gaps, non-manifold, non-closed shell) surface before commit.
  [[nodiscard]] Result<GuiPartFeaturePreview> preview(const GuiDocumentSession& session) const {
    if (!active_) return preview_fail("no surface command is active");
    const auto* part = session.part_document();
    if (part == nullptr) return preview_fail("preview requires an active Part document");
    if (auto ready = ready_to_build(); ready.has_error())
      return Result<GuiPartFeaturePreview>::failure(ready.error());
    PartDocument candidate = *part;
    if (auto seeded = seed_parameters(candidate); seeded.has_error())
      return Result<GuiPartFeaturePreview>::failure(seeded.error());
    auto feature = build_feature();
    if (feature.has_error()) return Result<GuiPartFeaturePreview>::failure(feature.error());
    if (auto added = candidate.add_surface_feature(std::move(feature.value())); added.has_error())
      return Result<GuiPartFeaturePreview>::failure(added.error());
    auto cache = geometry::ShapeCache::create(ShapeCacheId(feature_.value() + ".surface_preview"));
    if (cache.has_error()) return Result<GuiPartFeaturePreview>::failure(cache.error());
    auto executed =
        geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache.value());
    if (executed.has_error()) return Result<GuiPartFeaturePreview>::failure(executed.error());
    return Result<GuiPartFeaturePreview>::success(
        {feature_, summary(), cache.value().body_shape_count()});
  }

  [[nodiscard]] Result<std::size_t> apply(GuiDocumentSession& session) {
    auto checked = preview(session);
    if (checked.has_error()) return Result<std::size_t>::failure(checked.error());
    auto committed = session.commit_part_transaction(
        std::string(apply_label()), [this](PartDocument& part) -> Result<std::size_t> {
          if (auto seeded = seed_parameters(part); seeded.has_error()) return seeded;
          auto feature = build_feature();
          if (feature.has_error()) return Result<std::size_t>::failure(feature.error());
          return part.add_surface_feature(std::move(feature.value()));
        });
    if (committed.has_error()) return committed;
    active_ = false;
    return committed;
  }

  void cancel() noexcept { active_ = false; }

private:
  [[nodiscard]] Result<std::size_t>
  begin_common(const GuiDocumentSession& session, FeatureId feature, std::string name,
               BodyId result_body) {
    const auto* part = session.part_document();
    if (part == nullptr) return fail("interactive surface requires an active Part document");
    if (part->find_body(result_body) == nullptr)
      return fail("interactive surface requires an existing result Body");
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
  [[nodiscard]] Result<std::size_t>
  begin_curve_surface(const GuiDocumentSession& session, SurfaceFeatureKind kind, FeatureId feature,
                      std::string name, BodyId result_body) {
    if (auto ok = begin_common(session, std::move(feature), std::move(name), std::move(result_body));
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
    case SurfaceFeatureKind::ClosedShellToSolid:
      return target_.has_value() ? Result<std::size_t>::success(1)
                                 : fail("closed-shell-to-solid requires a shell");
    }
    return fail("unsupported surface feature kind");
  }

  [[nodiscard]] Result<std::size_t> seed_parameters(PartDocument& part) const {
    if (kind_ == SurfaceFeatureKind::ExtendSurface) {
      auto quantity = Quantity::length_mm(distance_, distance_parameter_.value());
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      auto changed = part.set_parameter_value(distance_parameter_, quantity.value());
      return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                                 : Result<std::size_t>::success(changed.value().size());
    }
    if (kind_ == SurfaceFeatureKind::SurfaceStitch) {
      auto quantity = Quantity::length_mm(tolerance_, tolerance_parameter_.value());
      if (quantity.has_error()) return Result<std::size_t>::failure(quantity.error());
      auto changed = part.set_parameter_value(tolerance_parameter_, quantity.value());
      return changed.has_error() ? Result<std::size_t>::failure(changed.error())
                                 : Result<std::size_t>::success(changed.value().size());
    }
    return Result<std::size_t>::success(0);
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
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    case SurfaceFeatureKind::ExtendSurface: {
      auto feature = ExtendSurfaceFeature::create(feature_, name_, *target_, *extend_boundary_,
                                                  distance_parameter_, result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
      return Result<SurfaceFeature>::success(std::move(feature.value()));
    }
    case SurfaceFeatureKind::SurfaceStitch: {
      auto feature = SurfaceStitchFeature::create(feature_, name_, surfaces_, tolerance_parameter_,
                                                  result_body_);
      if (feature.has_error()) return Result<SurfaceFeature>::failure(feature.error());
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
  [[nodiscard]] static Result<GuiPartFeaturePreview> preview_fail(std::string message) {
    return Result<GuiPartFeaturePreview>::failure(
        Error::validation("gui.interactive_surface", std::move(message)));
  }

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
