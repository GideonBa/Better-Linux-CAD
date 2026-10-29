#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"

#include <string>
#include <utility>

namespace blcad {

class PartDocument;

// A line is owned by one Sketch. Coordinates and display names are never identity.
class SketchLineReference {
public:
  [[nodiscard]] static Result<SketchLineReference> create(SketchId sketch, SketchEntityId entity) {
    if (sketch.empty() || entity.empty())
      return Result<SketchLineReference>::failure(
          Error::validation("sketch_line_reference",
                            "Sketch-line axis requires nonempty Sketch and entity identities"));
    return Result<SketchLineReference>::success(
        SketchLineReference(std::move(sketch), std::move(entity)));
  }
  [[nodiscard]] const SketchId& sketch() const noexcept {
    return sketch_;
  }
  [[nodiscard]] const SketchEntityId& entity() const noexcept {
    return entity_;
  }
  [[nodiscard]] std::string identity() const {
    return "sketch-line/" + std::to_string(sketch_.value().size()) + ":" + sketch_.value() + "/" +
           std::to_string(entity_.value().size()) + ":" + entity_.value();
  }
  friend bool operator==(const SketchLineReference&, const SketchLineReference&) = default;

private:
  SketchLineReference(SketchId sketch, SketchEntityId entity)
      : sketch_(std::move(sketch)), entity_(std::move(entity)) {}
  SketchId sketch_;
  SketchEntityId entity_;
};

[[nodiscard]] Result<std::size_t>
validate_sketch_line_reference(const PartDocument& document, const SketchLineReference& reference);

} // namespace blcad
