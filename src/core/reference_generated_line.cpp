#include "blcad/core/sketch.hpp"

#include <string>
#include <utility>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

} // namespace

Result<ReferenceGeneratedLine> ReferenceGeneratedLine::create_from_projected_point_constraints(
    SketchEntityId id, SketchConstraintId start_constraint, SketchConstraintId end_constraint) {
  const auto object_id = id.empty() ? std::string("reference_generated_line") : id.value();
  if (id.empty()) {
    return Result<ReferenceGeneratedLine>::failure(
        validation_error(object_id, "reference-generated line id must not be empty"));
  }
  if (start_constraint.empty() || end_constraint.empty()) {
    return Result<ReferenceGeneratedLine>::failure(validation_error(
        object_id, "reference-generated line endpoint constraint ids must not be empty"));
  }
  if (start_constraint == end_constraint) {
    return Result<ReferenceGeneratedLine>::failure(validation_error(
        object_id, "reference-generated line endpoint constraints must be distinct"));
  }
  return Result<ReferenceGeneratedLine>::success(ReferenceGeneratedLine(
      std::move(id), std::move(start_constraint), std::move(end_constraint), std::nullopt));
}

Result<ReferenceGeneratedLine> ReferenceGeneratedLine::create_with_projected_line_direction(
    SketchEntityId id, SketchConstraintId start_constraint, SketchConstraintId end_constraint,
    SketchConstraintId direction_constraint) {
  const auto object_id = id.empty() ? std::string("reference_generated_line") : id.value();
  if (direction_constraint.empty()) {
    return Result<ReferenceGeneratedLine>::failure(validation_error(
        object_id, "reference-generated line direction constraint id must not be empty"));
  }
  auto line = create_from_projected_point_constraints(std::move(id), std::move(start_constraint),
                                                      std::move(end_constraint));
  if (line.has_error()) {
    return Result<ReferenceGeneratedLine>::failure(line.error());
  }
  line.value().direction_constraint_ = std::move(direction_constraint);
  return line;
}

const SketchEntityId& ReferenceGeneratedLine::id() const noexcept {
  return id_;
}
const SketchConstraintId& ReferenceGeneratedLine::start_constraint() const noexcept {
  return start_constraint_;
}
const SketchConstraintId& ReferenceGeneratedLine::end_constraint() const noexcept {
  return end_constraint_;
}
const std::optional<SketchConstraintId>&
ReferenceGeneratedLine::direction_constraint() const noexcept {
  return direction_constraint_;
}

ReferenceGeneratedLine::ReferenceGeneratedLine(
    SketchEntityId id, SketchConstraintId start_constraint, SketchConstraintId end_constraint,
    std::optional<SketchConstraintId> direction_constraint)
    : id_(std::move(id)), start_constraint_(std::move(start_constraint)),
      end_constraint_(std::move(end_constraint)),
      direction_constraint_(std::move(direction_constraint)) {}

const std::vector<ReferenceGeneratedLine>& Sketch::reference_generated_lines() const noexcept {
  return reference_generated_lines_;
}

const ReferenceGeneratedLine*
Sketch::find_reference_generated_line(SketchEntityId id) const noexcept {
  for (const auto& line : reference_generated_lines_) {
    if (line.id() == id) {
      return &line;
    }
  }
  return nullptr;
}

Result<std::size_t> Sketch::add_reference(ReferenceGeneratedLine line_reference) {
  if (find_line_segment(line_reference.id()) != nullptr ||
      find_projected_point(line_reference.id()) != nullptr ||
      find_projected_line(line_reference.id()) != nullptr ||
      find_reference_generated_line(line_reference.id()) != nullptr) {
    return Result<std::size_t>::failure(Error::validation(
        line_reference.id().value(), "reference-generated line id must be unique within sketch"));
  }

  reference_generated_lines_.push_back(std::move(line_reference));
  return Result<std::size_t>::success(reference_generated_lines_.size() - 1U);
}

} // namespace blcad
