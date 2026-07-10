#include "blcad/core/sketch.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {
namespace {

[[nodiscard]] Error validation_error(std::string object_id, std::string message) {
  return Error::validation(std::move(object_id), std::move(message));
}

[[nodiscard]] Result<std::size_t> validate_contour(const std::string& object_id,
                                                   const std::vector<SketchEntityId>& contour,
                                                   std::string_view contour_kind) {
  if (contour.size() < 3U) {
    return Result<std::size_t>::failure(validation_error(
        object_id, std::string(contour_kind) + " contour requires at least three line segments"));
  }
  for (const auto& line : contour) {
    if (line.empty()) {
      return Result<std::size_t>::failure(validation_error(
          object_id, std::string(contour_kind) + " contour line segment ids must not be empty"));
    }
  }
  for (std::size_t i = 0; i < contour.size(); ++i) {
    for (std::size_t j = i + 1U; j < contour.size(); ++j) {
      if (contour[i] == contour[j]) {
        return Result<std::size_t>::failure(validation_error(
            object_id, std::string(contour_kind) + " contour line segment ids must be unique"));
      }
    }
  }
  return Result<std::size_t>::success(contour.size());
}

[[nodiscard]] bool id_in_contour(const SketchEntityId& id,
                                 const std::vector<SketchEntityId>& contour) noexcept {
  for (const auto& line : contour) {
    if (line == id)
      return true;
  }
  return false;
}

} // namespace

Result<CompositeClosedProfile>
CompositeClosedProfile::create(ProfileId id, std::vector<SketchEntityId> outer_contour,
                               std::vector<std::vector<SketchEntityId>> inner_contours) {
  const auto object_id = id.empty() ? std::string("composite_closed_profile") : id.value();
  if (id.empty()) {
    return Result<CompositeClosedProfile>::failure(
        validation_error(object_id, "composite closed profile id must not be empty"));
  }
  if (inner_contours.empty()) {
    return Result<CompositeClosedProfile>::failure(validation_error(
        object_id, "composite closed profile requires at least one inner contour"));
  }

  auto valid_outer = validate_contour(object_id, outer_contour, "outer");
  if (valid_outer.has_error())
    return Result<CompositeClosedProfile>::failure(valid_outer.error());

  for (const auto& inner : inner_contours) {
    auto valid_inner = validate_contour(object_id, inner, "inner");
    if (valid_inner.has_error())
      return Result<CompositeClosedProfile>::failure(valid_inner.error());
    for (const auto& inner_line : inner) {
      if (id_in_contour(inner_line, outer_contour)) {
        return Result<CompositeClosedProfile>::failure(validation_error(
            object_id, "composite closed profile contours must not share line segment ids"));
      }
    }
  }

  for (std::size_t i = 0; i < inner_contours.size(); ++i) {
    for (std::size_t j = i + 1U; j < inner_contours.size(); ++j) {
      for (const auto& line : inner_contours[i]) {
        if (id_in_contour(line, inner_contours[j])) {
          return Result<CompositeClosedProfile>::failure(validation_error(
              object_id,
              "composite closed profile inner contours must not share line segment ids"));
        }
      }
    }
  }

  return Result<CompositeClosedProfile>::success(
      CompositeClosedProfile(std::move(id), std::move(outer_contour), std::move(inner_contours)));
}

const ProfileId& CompositeClosedProfile::id() const noexcept {
  return id_;
}
const std::vector<SketchEntityId>& CompositeClosedProfile::outer_contour() const noexcept {
  return outer_contour_;
}
const std::vector<std::vector<SketchEntityId>>&
CompositeClosedProfile::inner_contours() const noexcept {
  return inner_contours_;
}

CompositeClosedProfile::CompositeClosedProfile(
    ProfileId id, std::vector<SketchEntityId> outer_contour,
    std::vector<std::vector<SketchEntityId>> inner_contours)
    : id_(std::move(id)), outer_contour_(std::move(outer_contour)),
      inner_contours_(std::move(inner_contours)) {}

const std::vector<CompositeClosedProfile>& Sketch::composite_closed_profiles() const noexcept {
  return composite_closed_profiles_;
}

const CompositeClosedProfile* Sketch::find_composite_closed_profile(ProfileId id) const noexcept {
  for (const auto& profile : composite_closed_profiles_) {
    if (profile.id() == id)
      return &profile;
  }
  return nullptr;
}

Result<std::size_t> Sketch::add_profile(CompositeClosedProfile profile) {
  if (has_profile_id(profile.id())) {
    return Result<std::size_t>::failure(
        Error::validation(profile.id().value(), "profile id must be unique within sketch"));
  }

  auto valid = validate_composite_closed_profile(profile);
  if (valid.has_error())
    return Result<std::size_t>::failure(valid.error());

  composite_closed_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(composite_closed_profiles_.size() - 1U);
}

Result<std::size_t>
Sketch::validate_composite_closed_profile(const CompositeClosedProfile& profile) const {
  auto outer =
      ClosedProfile::create(ProfileId(profile.id().value() + ".outer"), profile.outer_contour());
  if (outer.has_error())
    return Result<std::size_t>::failure(outer.error());
  auto outer_vertices = validate_closed_profile(outer.value());
  if (outer_vertices.has_error())
    return Result<std::size_t>::failure(outer_vertices.error());

  for (std::size_t index = 0; index < profile.inner_contours().size(); ++index) {
    auto inner =
        ClosedProfile::create(ProfileId(profile.id().value() + ".inner." + std::to_string(index)),
                              profile.inner_contours()[index]);
    if (inner.has_error())
      return Result<std::size_t>::failure(inner.error());
    auto inner_vertices = validate_closed_profile(inner.value());
    if (inner_vertices.has_error())
      return Result<std::size_t>::failure(inner_vertices.error());
  }

  return Result<std::size_t>::success(profile.inner_contours().size());
}

} // namespace blcad
