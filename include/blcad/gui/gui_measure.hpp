#pragma once

#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/geometry_measure.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {

enum class GuiMeasureKind { Distance, Angle, RadiusDiameter, BodySummary };

struct GuiMeasureResult {
  GuiMeasureKind kind{GuiMeasureKind::Distance};
  std::string status_text;
  std::string primary_unit;
  double primary_value{0.0};
  std::optional<double> secondary_value;
  std::vector<Point3> overlay_points;
  std::size_t solid_count{0U};
  double volume_mm3{0.0};
};

// Block-131 transient Measure command. It owns selections and display products
// only; every query consumes derived Geometry and never opens a transaction.
class GuiMeasureController {
public:
  [[nodiscard]] Result<std::size_t> begin(const GuiDocumentSession& session, GuiMeasureKind kind) {
    if (!session.has_document())
      return fail("Measure requires an active document");
    kind_ = kind;
    targets_.clear();
    body_.reset();
    active_ = true;
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] bool active() const noexcept {
    return active_;
  }
  [[nodiscard]] GuiMeasureKind kind() const noexcept {
    return kind_;
  }
  [[nodiscard]] std::size_t target_count() const noexcept {
    return targets_.size();
  }

  [[nodiscard]] Result<std::size_t> add_target(geometry::GeometryMeasureTarget target) {
    if (!active_ || kind_ == GuiMeasureKind::BodySummary)
      return fail("the active Measure command does not accept a geometry target");
    const std::size_t required = kind_ == GuiMeasureKind::RadiusDiameter ? 1U : 2U;
    if (targets_.size() >= required)
      return fail("Measure already has all required targets");
    targets_.push_back(std::move(target));
    return Result<std::size_t>::success(targets_.size());
  }

  [[nodiscard]] Result<std::size_t> select_body(BodyId body) {
    if (!active_ || kind_ != GuiMeasureKind::BodySummary)
      return fail("the active Measure command does not accept a Body");
    body_ = std::move(body);
    return Result<std::size_t>::success(1);
  }

  [[nodiscard]] Result<GuiMeasureResult> preview(const GuiDocumentSession& session) const {
    if (!active_)
      return preview_fail("no Measure command is active");
    if (!session.derived_results_fresh())
      return preview_fail("Measure requires fresh derived Geometry");
    if (kind_ == GuiMeasureKind::BodySummary)
      return preview_body(session);
    const std::size_t required = kind_ == GuiMeasureKind::RadiusDiameter ? 1U : 2U;
    if (targets_.size() != required)
      return preview_fail("Measure target selection is incomplete");

    const geometry::GeometryMeasureQuery query;
    if (kind_ == GuiMeasureKind::Distance) {
      auto value = query.distance(targets_[0], targets_[1]);
      if (value.has_error())
        return Result<GuiMeasureResult>::failure(value.error());
      return Result<GuiMeasureResult>::success({kind_,
                                                "Distance",
                                                "mm",
                                                value.value().distance_mm,
                                                std::nullopt,
                                                {value.value().witness_a, value.value().witness_b},
                                                0U,
                                                0.0});
    }
    if (kind_ == GuiMeasureKind::Angle) {
      auto value = query.angle(targets_[0], targets_[1]);
      if (value.has_error())
        return Result<GuiMeasureResult>::failure(value.error());
      return Result<GuiMeasureResult>::success(
          {kind_, "Angle", "deg", value.value().angle_deg, std::nullopt, {}, 0U, 0.0});
    }
    auto value = query.radius_diameter(targets_[0]);
    if (value.has_error())
      return Result<GuiMeasureResult>::failure(value.error());
    return Result<GuiMeasureResult>::success({kind_,
                                              "Radius / Diameter",
                                              "mm",
                                              value.value().radius_mm,
                                              value.value().diameter_mm,
                                              {},
                                              0U,
                                              0.0});
  }

  void cancel() noexcept {
    active_ = false;
    targets_.clear();
    body_.reset();
  }

private:
  [[nodiscard]] Result<GuiMeasureResult> preview_body(const GuiDocumentSession& session) const {
    if (!body_)
      return preview_fail("Body summary requires one Body selection");
    const auto* document = session.part_document();
    const auto* cache = session.part_shape_cache();
    if (document == nullptr || cache == nullptr)
      return preview_fail("Body summary requires a recomputed Part document");
    auto inspection = geometry::BodyResultInspector{}.inspect(*document, *cache);
    if (inspection.has_error())
      return Result<GuiMeasureResult>::failure(inspection.error());
    const auto found = std::find_if(
        inspection.value().bodies.begin(), inspection.value().bodies.end(),
        [this](const geometry::BodyResultInspection& value) { return value.body_id == *body_; });
    if (found == inspection.value().bodies.end())
      return preview_fail("selected Body does not exist");
    if (!found->shape_summary)
      return preview_fail("selected Body has no derived shape summary");
    return Result<GuiMeasureResult>::success({kind_,
                                              "Body volume / solids",
                                              "mm3",
                                              found->shape_summary->volume_mm3,
                                              std::nullopt,
                                              {},
                                              found->shape_summary->solid_count,
                                              found->shape_summary->volume_mm3});
  }

  [[nodiscard]] static Result<std::size_t> fail(std::string message) {
    return Result<std::size_t>::failure(Error::validation("gui.measure", std::move(message)));
  }
  [[nodiscard]] static Result<GuiMeasureResult> preview_fail(std::string message) {
    return Result<GuiMeasureResult>::failure(Error::validation("gui.measure", std::move(message)));
  }

  bool active_{false};
  GuiMeasureKind kind_{GuiMeasureKind::Distance};
  std::vector<geometry::GeometryMeasureTarget> targets_;
  std::optional<BodyId> body_;
};

} // namespace blcad::gui
