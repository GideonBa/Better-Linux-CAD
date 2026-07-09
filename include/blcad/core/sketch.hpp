#pragma once

#include "blcad/core/id.hpp"
#include "blcad/core/result.hpp"
#include "blcad/core/spatial.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace blcad {

class LineSegment {
public:
  [[nodiscard]] static Result<LineSegment> create(SketchEntityId id, Point2 start, Point2 end);

  [[nodiscard]] const SketchEntityId& id() const noexcept;
  [[nodiscard]] Point2 start() const noexcept;
  [[nodiscard]] Point2 end() const noexcept;

private:
  LineSegment(SketchEntityId id, Point2 start, Point2 end);

  SketchEntityId id_;
  Point2 start_;
  Point2 end_;
};

class RectangleProfile {
public:
  [[nodiscard]] static Result<RectangleProfile> create(ProfileId id, ParameterId width_parameter,
                                                       ParameterId height_parameter,
                                                       Point2 center = {});

  [[nodiscard]] const ProfileId& id() const noexcept;
  [[nodiscard]] Point2 center() const noexcept;
  [[nodiscard]] const ParameterId& width_parameter() const noexcept;
  [[nodiscard]] const ParameterId& height_parameter() const noexcept;

private:
  RectangleProfile(ProfileId id, ParameterId width_parameter, ParameterId height_parameter,
                   Point2 center);

  ProfileId id_;
  ParameterId width_parameter_;
  ParameterId height_parameter_;
  Point2 center_;
};

class CircleProfile {
public:
  [[nodiscard]] static Result<CircleProfile> create(ProfileId id, ParameterId diameter_parameter,
                                                    Point2 center = {});

  [[nodiscard]] const ProfileId& id() const noexcept;
  [[nodiscard]] Point2 center() const noexcept;
  [[nodiscard]] const ParameterId& diameter_parameter() const noexcept;

private:
  CircleProfile(ProfileId id, ParameterId diameter_parameter, Point2 center);

  ProfileId id_;
  ParameterId diameter_parameter_;
  Point2 center_;
};

class ClosedProfile {
public:
  [[nodiscard]] static Result<ClosedProfile> create(ProfileId id,
                                                    std::vector<SketchEntityId> line_segments);

  [[nodiscard]] const ProfileId& id() const noexcept;
  [[nodiscard]] const std::vector<SketchEntityId>& line_segments() const noexcept;

private:
  ClosedProfile(ProfileId id, std::vector<SketchEntityId> line_segments);

  ProfileId id_;
  std::vector<SketchEntityId> line_segments_;
};

class Sketch {
public:
  [[nodiscard]] static Result<Sketch> create(SketchId id, std::string name, DatumPlaneId workplane);

  [[nodiscard]] Result<std::size_t> add_entity(LineSegment line_segment);
  [[nodiscard]] Result<std::size_t> add_profile(RectangleProfile profile);
  [[nodiscard]] Result<std::size_t> add_profile(CircleProfile profile);
  [[nodiscard]] Result<std::size_t> add_profile(ClosedProfile profile);

  [[nodiscard]] const SketchId& id() const noexcept;
  [[nodiscard]] const std::string& name() const noexcept;
  [[nodiscard]] const DatumPlaneId& workplane() const noexcept;
  [[nodiscard]] const std::vector<LineSegment>& line_segments() const noexcept;
  [[nodiscard]] const std::vector<RectangleProfile>& rectangle_profiles() const noexcept;
  [[nodiscard]] const std::vector<CircleProfile>& circle_profiles() const noexcept;
  [[nodiscard]] const std::vector<ClosedProfile>& closed_profiles() const noexcept;
  [[nodiscard]] std::size_t profile_count() const noexcept;

  [[nodiscard]] const LineSegment* find_line_segment(SketchEntityId id) const noexcept;
  [[nodiscard]] const RectangleProfile* find_rectangle_profile(ProfileId id) const noexcept;
  [[nodiscard]] const CircleProfile* find_circle_profile(ProfileId id) const noexcept;
  [[nodiscard]] const ClosedProfile* find_closed_profile(ProfileId id) const noexcept;
  [[nodiscard]] Result<std::vector<Point2>> closed_profile_vertices(
      const ClosedProfile& profile) const;

private:
  Sketch(SketchId id, std::string name, DatumPlaneId workplane);

  [[nodiscard]] bool has_entity_id(const SketchEntityId& id) const noexcept;
  [[nodiscard]] bool has_profile_id(const ProfileId& id) const noexcept;
  [[nodiscard]] Result<std::vector<Point2>> validate_closed_profile(
      const ClosedProfile& profile) const;

  SketchId id_;
  std::string name_;
  DatumPlaneId workplane_;
  std::vector<LineSegment> line_segments_;
  std::vector<RectangleProfile> rectangle_profiles_;
  std::vector<CircleProfile> circle_profiles_;
  std::vector<ClosedProfile> closed_profiles_;
};

} // namespace blcad
