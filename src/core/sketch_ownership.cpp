#include "blcad/core/sketch_ownership.hpp"

#include <utility>

namespace blcad {

std::string_view to_string(SketchAssociation association) noexcept {
  switch (association) {
  case SketchAssociation::DrivesBody:
    return "drives_body";
  case SketchAssociation::ConsumedByBody:
    return "consumed_by_body";
  case SketchAssociation::ReferenceOnly:
    return "reference_only";
  }
  return "unknown";
}

Result<SketchOwnership> SketchOwnership::create(SketchId sketch, BodyId owning_body,
                                                SketchAssociation association) {
  if (sketch.empty())
    return Result<SketchOwnership>::failure(
        Error::validation("sketch_ownership", "owned sketch id must not be empty"));
  if (owning_body.empty())
    return Result<SketchOwnership>::failure(
        Error::validation(sketch.value(), "owning body id must not be empty"));
  switch (association) {
  case SketchAssociation::DrivesBody:
  case SketchAssociation::ConsumedByBody:
  case SketchAssociation::ReferenceOnly:
    break;
  default:
    return Result<SketchOwnership>::failure(
        Error::validation(sketch.value(), "unsupported sketch association"));
  }
  return Result<SketchOwnership>::success(
      SketchOwnership(std::move(sketch), std::move(owning_body), association));
}

const SketchId& SketchOwnership::sketch() const noexcept { return sketch_; }
const BodyId& SketchOwnership::owning_body() const noexcept { return owning_body_; }
SketchAssociation SketchOwnership::association() const noexcept { return association_; }

SketchOwnership::SketchOwnership(SketchId sketch, BodyId owning_body,
                                 SketchAssociation association)
    : sketch_(std::move(sketch)), owning_body_(std::move(owning_body)),
      association_(association) {}

} // namespace blcad
