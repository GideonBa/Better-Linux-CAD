#include "blcad/core/sketch.hpp"

#include <utility>

namespace blcad {

Result<RectangleProfile> RectangleProfile::create(ProfileId id, ParameterId width_parameter,
                                                  ParameterId height_parameter, Point2 center) {
  const auto object_id = id.empty() ? std::string("rectangle_profile") : id.value();

  if (id.empty()) {
    return Result<RectangleProfile>::failure(
        Error::validation(object_id, "rectangle profile id must not be empty"));
  }

  if (width_parameter.empty()) {
    return Result<RectangleProfile>::failure(
        Error::validation(object_id, "rectangle width parameter id must not be empty"));
  }

  if (height_parameter.empty()) {
    return Result<RectangleProfile>::failure(
        Error::validation(object_id, "rectangle height parameter id must not be empty"));
  }

  return Result<RectangleProfile>::success(RectangleProfile(
      std::move(id), std::move(width_parameter), std::move(height_parameter), center));
}

const ProfileId& RectangleProfile::id() const noexcept {
  return id_;
}

Point2 RectangleProfile::center() const noexcept {
  return center_;
}

const ParameterId& RectangleProfile::width_parameter() const noexcept {
  return width_parameter_;
}

const ParameterId& RectangleProfile::height_parameter() const noexcept {
  return height_parameter_;
}

RectangleProfile::RectangleProfile(ProfileId id, ParameterId width_parameter,
                                   ParameterId height_parameter, Point2 center)
    : id_(std::move(id)), width_parameter_(std::move(width_parameter)),
      height_parameter_(std::move(height_parameter)), center_(center) {}

Result<CircleProfile> CircleProfile::create(ProfileId id, ParameterId diameter_parameter,
                                            Point2 center) {
  const auto object_id = id.empty() ? std::string("circle_profile") : id.value();

  if (id.empty()) {
    return Result<CircleProfile>::failure(
        Error::validation(object_id, "circle profile id must not be empty"));
  }

  if (diameter_parameter.empty()) {
    return Result<CircleProfile>::failure(
        Error::validation(object_id, "circle diameter parameter id must not be empty"));
  }

  return Result<CircleProfile>::success(
      CircleProfile(std::move(id), std::move(diameter_parameter), center));
}

const ProfileId& CircleProfile::id() const noexcept {
  return id_;
}

Point2 CircleProfile::center() const noexcept {
  return center_;
}

const ParameterId& CircleProfile::diameter_parameter() const noexcept {
  return diameter_parameter_;
}

CircleProfile::CircleProfile(ProfileId id, ParameterId diameter_parameter, Point2 center)
    : id_(std::move(id)), diameter_parameter_(std::move(diameter_parameter)), center_(center) {}

Result<Sketch> Sketch::create(SketchId id, std::string name, DatumPlaneId workplane) {
  const auto object_id = id.empty() ? std::string("sketch") : id.value();

  if (id.empty()) {
    return Result<Sketch>::failure(Error::validation(object_id, "sketch id must not be empty"));
  }

  if (name.empty()) {
    return Result<Sketch>::failure(Error::validation(object_id, "sketch name must not be empty"));
  }

  if (workplane.empty()) {
    return Result<Sketch>::failure(
        Error::validation(object_id, "sketch workplane id must not be empty"));
  }

  return Result<Sketch>::success(Sketch(std::move(id), std::move(name), std::move(workplane)));
}

Result<std::size_t> Sketch::add_profile(RectangleProfile profile) {
  if (has_profile_id(profile.id())) {
    return Result<std::size_t>::failure(
        Error::validation(profile.id().value(), "profile id must be unique within sketch"));
  }

  rectangle_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(profile_count() - 1);
}

Result<std::size_t> Sketch::add_profile(CircleProfile profile) {
  if (has_profile_id(profile.id())) {
    return Result<std::size_t>::failure(
        Error::validation(profile.id().value(), "profile id must be unique within sketch"));
  }

  circle_profiles_.push_back(std::move(profile));
  return Result<std::size_t>::success(profile_count() - 1);
}

const SketchId& Sketch::id() const noexcept {
  return id_;
}

const std::string& Sketch::name() const noexcept {
  return name_;
}

const DatumPlaneId& Sketch::workplane() const noexcept {
  return workplane_;
}

const std::vector<RectangleProfile>& Sketch::rectangle_profiles() const noexcept {
  return rectangle_profiles_;
}

const std::vector<CircleProfile>& Sketch::circle_profiles() const noexcept {
  return circle_profiles_;
}

std::size_t Sketch::profile_count() const noexcept {
  return rectangle_profiles_.size() + circle_profiles_.size();
}

const RectangleProfile* Sketch::find_rectangle_profile(ProfileId id) const noexcept {
  for (const auto& profile : rectangle_profiles_) {
    if (profile.id() == id) {
      return &profile;
    }
  }

  return nullptr;
}

const CircleProfile* Sketch::find_circle_profile(ProfileId id) const noexcept {
  for (const auto& profile : circle_profiles_) {
    if (profile.id() == id) {
      return &profile;
    }
  }

  return nullptr;
}

Sketch::Sketch(SketchId id, std::string name, DatumPlaneId workplane)
    : id_(std::move(id)), name_(std::move(name)), workplane_(std::move(workplane)) {}

bool Sketch::has_profile_id(const ProfileId& id) const noexcept {
  return find_rectangle_profile(id) != nullptr || find_circle_profile(id) != nullptr;
}

} // namespace blcad
