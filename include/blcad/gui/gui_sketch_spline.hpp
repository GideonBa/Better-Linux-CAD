#pragma once

#include "blcad/core/sketch_spline_edit.hpp"
#include "blcad/geometry/sketch_spline_geometry.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace blcad::gui {

struct GuiSketchSplinePreview {
  Sketch candidate;
  SketchSplineEditPreview authoring;
  geometry::SketchSplineGeometryResult geometry;
};

// Headless GUI-layer controller for Block 113. Widgets can bind its semantic
// handles to the viewport, while preview and commit remain independent of Qt.
class GuiSketchSplineController {
public:
  [[nodiscard]] static Result<GuiSketchSplineController>
  create(const PartDocument& document, SketchId sketch,
         std::vector<SketchEntityId> ordered_entities) {
    const Sketch* source = document.find_sketch(sketch);
    if (source == nullptr)
      return Result<GuiSketchSplineController>::failure(
          Error::validation(sketch.value(), "spline editor requires an existing Sketch"));
    auto model = SketchSplineEditModel::from_segments(*source, std::move(ordered_entities));
    if (model.has_error())
      return Result<GuiSketchSplineController>::failure(model.error());
    return Result<GuiSketchSplineController>::success(
        GuiSketchSplineController(*source, std::move(model.value())));
  }

  [[nodiscard]] const Sketch& source_sketch() const noexcept { return source_; }
  [[nodiscard]] const SketchSplineEditModel& model() const noexcept { return model_; }

  [[nodiscard]] Result<std::size_t> convert_to_fit_points() {
    return model_.convert_to_fit_points();
  }
  [[nodiscard]] Result<std::size_t> convert_to_control_points() {
    return model_.convert_to_control_points();
  }
  [[nodiscard]] Result<std::size_t> move_control_point(std::size_t segment,
                                                       std::size_t point,
                                                       Point2 position) {
    return model_.move_control_point(segment, point, position);
  }
  [[nodiscard]] Result<std::size_t> move_fit_point(std::size_t index, Point2 position) {
    return model_.move_fit_point(index, position);
  }
  [[nodiscard]] Result<std::size_t> insert_fit_point(std::size_t index, Point2 position) {
    return model_.insert_fit_point(index, position);
  }
  [[nodiscard]] Result<std::size_t> remove_fit_point(std::size_t index) {
    return model_.remove_fit_point(index);
  }
  [[nodiscard]] Result<std::size_t> align_tangent(std::size_t junction) {
    return model_.align_tangent(junction);
  }

  [[nodiscard]] Result<GuiSketchSplinePreview> preview() const {
    auto candidate = model_.build_sketch(source_);
    if (candidate.has_error())
      return Result<GuiSketchSplinePreview>::failure(candidate.error());
    auto evaluated = geometry::SketchSplineGeometryEvaluator{}.evaluate(model_.segments());
    if (evaluated.has_error())
      return Result<GuiSketchSplinePreview>::failure(evaluated.error());
    return Result<GuiSketchSplinePreview>::success(
        {std::move(candidate.value()), model_.preview(), std::move(evaluated.value())});
  }

  [[nodiscard]] Result<std::size_t> commit(GuiDocumentSession& session) const {
    if (session.part_document() == nullptr)
      return Result<std::size_t>::failure(
          Error::validation(source_.id().value(), "spline edit requires an active Part document"));
    const SketchId sketch_id = source_.id();
    const SketchSplineEditModel model = model_;
    const std::vector<SplineSegment> source_segments = source_segments_;
    return session.commit_part_transaction(
        "Edit sketch spline",
        [sketch_id, model, source_segments](PartDocument& document) -> Result<std::size_t> {
          const Sketch* current = document.find_sketch(sketch_id);
          if (current == nullptr)
            return Result<std::size_t>::failure(
                Error::validation(sketch_id.value(), "spline edit target no longer exists"));
          for (const auto& snapshot : source_segments) {
            const SplineSegment* current_segment = current->find_spline_segment(snapshot.id());
            if (current_segment == nullptr || !same_segment(*current_segment, snapshot))
              return Result<std::size_t>::failure(Error::dependency(
                  snapshot.id().value(), "spline edit source changed after the editor was opened"));
          }
          auto candidate = model.build_sketch(*current);
          if (candidate.has_error()) return Result<std::size_t>::failure(candidate.error());
          return document.update_sketch(std::move(candidate.value()));
        });
  }

private:
  GuiSketchSplineController(Sketch source, SketchSplineEditModel model)
      : source_(std::move(source)), model_(std::move(model)), source_segments_(model_.segments()) {}

  [[nodiscard]] static bool same_point(Point2 first, Point2 second) noexcept {
    return first == second;
  }
  [[nodiscard]] static bool same_segment(const SplineSegment& first,
                                         const SplineSegment& second) noexcept {
    return first.id() == second.id() && same_point(first.start(), second.start()) &&
           same_point(first.control1(), second.control1()) &&
           same_point(first.control2(), second.control2()) && same_point(first.end(), second.end());
  }

  Sketch source_;
  SketchSplineEditModel model_;
  std::vector<SplineSegment> source_segments_;
};

} // namespace blcad::gui
