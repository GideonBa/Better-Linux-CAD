#pragma once

#include "blcad/core/sketch.hpp"

#include <algorithm>

namespace blcad {

// Profile records remain persistent when their boundary becomes auxiliary.
// Geometry consumers count eligible profiles, independently of authored records.
template <typename Profiles>
[[nodiscard]] std::size_t count_sketch_geometry_profiles(const Sketch& sketch,
                                                         const Profiles& profiles) {
  return static_cast<std::size_t>(
      std::count_if(profiles.begin(), profiles.end(), [&](const auto& profile) {
        return !sketch.profile_has_construction(profile.id());
      }));
}

[[nodiscard]] inline std::size_t sketch_geometry_profile_count(const Sketch& sketch) {
  return count_sketch_geometry_profiles(sketch, sketch.rectangle_profiles()) +
         count_sketch_geometry_profiles(sketch, sketch.circle_profiles()) +
         count_sketch_geometry_profiles(sketch, sketch.closed_profiles()) +
         count_sketch_geometry_profiles(sketch, sketch.arc_closed_profiles()) +
         count_sketch_geometry_profiles(sketch, sketch.composite_closed_profiles()) +
         count_sketch_geometry_profiles(sketch, sketch.circular_hole_patterns());
}

template <typename Profile>
[[nodiscard]] const Profile* sole_sketch_geometry_profile(const Sketch& sketch,
                                                          const std::vector<Profile>& profiles) {
  if (sketch_geometry_profile_count(sketch) != 1U)
    return nullptr;
  const auto found = std::find_if(profiles.begin(), profiles.end(), [&](const auto& profile) {
    return !sketch.profile_has_construction(profile.id());
  });
  return found == profiles.end() ? nullptr : &*found;
}

} // namespace blcad
