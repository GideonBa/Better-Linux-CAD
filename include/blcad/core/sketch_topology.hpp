#pragma once

#include "blcad/core/sketch.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace blcad {

enum class SketchTopologyEntityKind {
  Line,
  Arc,
  Spline,
  RectangleProfile,
  CircleProfile,
  ClosedProfile,
  ArcClosedProfile,
  CompositeClosedProfile,
  CircularHolePattern,
  ProjectedPoint,
  ProjectedLine,
  ReferenceGeneratedLine,
};

[[nodiscard]] constexpr std::string_view to_string(SketchTopologyEntityKind kind) noexcept {
  switch (kind) {
  case SketchTopologyEntityKind::Line: return "line";
  case SketchTopologyEntityKind::Arc: return "arc";
  case SketchTopologyEntityKind::Spline: return "spline";
  case SketchTopologyEntityKind::RectangleProfile: return "rectangle_profile";
  case SketchTopologyEntityKind::CircleProfile: return "circle_profile";
  case SketchTopologyEntityKind::ClosedProfile: return "closed_profile";
  case SketchTopologyEntityKind::ArcClosedProfile: return "arc_closed_profile";
  case SketchTopologyEntityKind::CompositeClosedProfile: return "composite_closed_profile";
  case SketchTopologyEntityKind::CircularHolePattern: return "circular_hole_pattern";
  case SketchTopologyEntityKind::ProjectedPoint: return "projected_point";
  case SketchTopologyEntityKind::ProjectedLine: return "projected_line";
  case SketchTopologyEntityKind::ReferenceGeneratedLine: return "reference_generated_line";
  }
  return "line";
}

[[nodiscard]] inline std::optional<SketchTopologyEntityKind>
sketch_topology_entity_kind_from_string(std::string_view value) noexcept {
  if (value == "line") return SketchTopologyEntityKind::Line;
  if (value == "arc") return SketchTopologyEntityKind::Arc;
  if (value == "spline") return SketchTopologyEntityKind::Spline;
  if (value == "rectangle_profile") return SketchTopologyEntityKind::RectangleProfile;
  if (value == "circle_profile") return SketchTopologyEntityKind::CircleProfile;
  if (value == "closed_profile") return SketchTopologyEntityKind::ClosedProfile;
  if (value == "arc_closed_profile") return SketchTopologyEntityKind::ArcClosedProfile;
  if (value == "composite_closed_profile") return SketchTopologyEntityKind::CompositeClosedProfile;
  if (value == "circular_hole_pattern") return SketchTopologyEntityKind::CircularHolePattern;
  if (value == "projected_point") return SketchTopologyEntityKind::ProjectedPoint;
  if (value == "projected_line") return SketchTopologyEntityKind::ProjectedLine;
  if (value == "reference_generated_line") return SketchTopologyEntityKind::ReferenceGeneratedLine;
  return std::nullopt;
}

struct SketchTopologyFlags {
  bool construction{false};
  bool reference{false};

  friend bool operator==(const SketchTopologyFlags&, const SketchTopologyFlags&) = default;
};

class SketchTopologyPoint {
public:
  [[nodiscard]] static Result<SketchTopologyPoint>
  create(SketchPointId id, Point2 position, SketchTopologyFlags flags = {}) {
    const std::string object_id = id.empty() ? "sketch_point" : id.value();
    if (id.empty())
      return Result<SketchTopologyPoint>::failure(
          Error::validation(object_id, "sketch point id must not be empty"));
    if (!std::isfinite(position.x) || !std::isfinite(position.y))
      return Result<SketchTopologyPoint>::failure(
          Error::validation(object_id, "sketch point coordinates must be finite"));
    if (flags.reference && flags.construction)
      return Result<SketchTopologyPoint>::failure(Error::validation(
          object_id, "reference sketch points cannot also be construction points"));
    return Result<SketchTopologyPoint>::success(
        SketchTopologyPoint(std::move(id), position, flags));
  }

  [[nodiscard]] const SketchPointId& id() const noexcept { return id_; }
  [[nodiscard]] Point2 position() const noexcept { return position_; }
  [[nodiscard]] SketchTopologyFlags flags() const noexcept { return flags_; }
  [[nodiscard]] bool construction() const noexcept { return flags_.construction; }
  [[nodiscard]] bool reference() const noexcept { return flags_.reference; }

  [[nodiscard]] Result<SketchTopologyPoint> with_position(Point2 position) const {
    return create(id_, position, flags_);
  }

  friend bool operator==(const SketchTopologyPoint&, const SketchTopologyPoint&) = default;

private:
  SketchTopologyPoint(SketchPointId id, Point2 position, SketchTopologyFlags flags)
      : id_(std::move(id)), position_(position), flags_(flags) {}

  SketchPointId id_;
  Point2 position_;
  SketchTopologyFlags flags_;
};

class SketchTopologyEntity {
public:
  [[nodiscard]] static Result<SketchTopologyEntity>
  create(std::string id, SketchTopologyEntityKind kind, std::vector<SketchPointId> points,
         std::vector<std::string> entity_dependencies = {}, SketchTopologyFlags flags = {}) {
    const std::string object_id = id.empty() ? "sketch_topology_entity" : id;
    if (id.empty())
      return Result<SketchTopologyEntity>::failure(
          Error::validation(object_id, "sketch topology entity id must not be empty"));
    if (flags.reference && flags.construction)
      return Result<SketchTopologyEntity>::failure(Error::validation(
          object_id, "reference sketch entities cannot also be construction entities"));
    for (const auto& point : points)
      if (point.empty())
        return Result<SketchTopologyEntity>::failure(
            Error::validation(object_id, "sketch topology point references must not be empty"));
    for (std::size_t index = 0; index < entity_dependencies.size(); ++index) {
      if (entity_dependencies[index].empty())
        return Result<SketchTopologyEntity>::failure(
            Error::validation(object_id, "sketch topology dependencies must not be empty"));
      if (entity_dependencies[index] == id)
        return Result<SketchTopologyEntity>::failure(
            Error::validation(object_id, "sketch topology entity cannot depend on itself"));
      for (std::size_t previous = 0; previous < index; ++previous)
        if (entity_dependencies[previous] == entity_dependencies[index])
          return Result<SketchTopologyEntity>::failure(Error::validation(
              object_id, "ordered sketch topology dependencies must be unique"));
    }

    const std::size_t expected_points = [&] {
      switch (kind) {
      case SketchTopologyEntityKind::Line: return std::size_t{2};
      case SketchTopologyEntityKind::Arc: return std::size_t{3};
      case SketchTopologyEntityKind::Spline: return std::size_t{4};
      case SketchTopologyEntityKind::RectangleProfile:
      case SketchTopologyEntityKind::CircleProfile:
      case SketchTopologyEntityKind::CircularHolePattern: return std::size_t{1};
      case SketchTopologyEntityKind::ClosedProfile:
      case SketchTopologyEntityKind::ArcClosedProfile:
      case SketchTopologyEntityKind::CompositeClosedProfile:
      case SketchTopologyEntityKind::ProjectedPoint:
      case SketchTopologyEntityKind::ProjectedLine:
      case SketchTopologyEntityKind::ReferenceGeneratedLine: return std::size_t{0};
      }
      return std::size_t{0};
    }();
    if (points.size() != expected_points)
      return Result<SketchTopologyEntity>::failure(Error::validation(
          object_id, "sketch topology entity has the wrong number of point references"));
    if ((kind == SketchTopologyEntityKind::Line && points[0] == points[1]) ||
        (kind == SketchTopologyEntityKind::Arc &&
         (points[0] == points[1] || points[1] == points[2] || points[0] == points[2])))
      return Result<SketchTopologyEntity>::failure(Error::validation(
          object_id, "sketch topology entity requires distinct defining point identities"));

    return Result<SketchTopologyEntity>::success(SketchTopologyEntity(
        std::move(id), kind, std::move(points), std::move(entity_dependencies), flags));
  }

  [[nodiscard]] const std::string& id() const noexcept { return id_; }
  [[nodiscard]] SketchTopologyEntityKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::vector<SketchPointId>& points() const noexcept { return points_; }
  [[nodiscard]] const std::vector<std::string>& entity_dependencies() const noexcept {
    return entity_dependencies_;
  }
  [[nodiscard]] SketchTopologyFlags flags() const noexcept { return flags_; }
  [[nodiscard]] bool construction() const noexcept { return flags_.construction; }
  [[nodiscard]] bool reference() const noexcept { return flags_.reference; }

  friend bool operator==(const SketchTopologyEntity&, const SketchTopologyEntity&) = default;

private:
  SketchTopologyEntity(std::string id, SketchTopologyEntityKind kind,
                       std::vector<SketchPointId> points,
                       std::vector<std::string> entity_dependencies,
                       SketchTopologyFlags flags)
      : id_(std::move(id)), kind_(kind), points_(std::move(points)),
        entity_dependencies_(std::move(entity_dependencies)), flags_(flags) {}

  std::string id_;
  SketchTopologyEntityKind kind_;
  std::vector<SketchPointId> points_;
  std::vector<std::string> entity_dependencies_;
  SketchTopologyFlags flags_;
};

struct SketchTopologyDependency {
  std::string consumer_id;
  std::string source_entity_id;
  std::string role;

  friend bool operator==(const SketchTopologyDependency&, const SketchTopologyDependency&) = default;
};

struct SketchTopologyIdentityChange {
  std::string previous_identity;
  std::string canonical_identity;
  std::string reason;

  friend bool operator==(const SketchTopologyIdentityChange&,
                         const SketchTopologyIdentityChange&) = default;
};

class SketchTopologyMigrationReport {
public:
  [[nodiscard]] bool migrated() const noexcept { return migrated_; }
  [[nodiscard]] const std::vector<SketchTopologyIdentityChange>& identity_changes() const noexcept {
    return identity_changes_;
  }

  friend bool operator==(const SketchTopologyMigrationReport&,
                         const SketchTopologyMigrationReport&) = default;

private:
  friend class SketchTopology;
  bool migrated_{false};
  std::vector<SketchTopologyIdentityChange> identity_changes_;
};

class SketchTopology {
public:
  [[nodiscard]] static Result<SketchTopology>
  create(SketchId sketch, std::vector<SketchTopologyPoint> points,
         std::vector<SketchTopologyEntity> entities,
         std::vector<SketchTopologyDependency> dependencies = {}) {
    const std::string object_id = sketch.empty() ? "sketch_topology" : sketch.value();
    if (sketch.empty())
      return Result<SketchTopology>::failure(
          Error::validation(object_id, "sketch topology requires a Sketch id"));

    std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
    for (std::size_t index = 1; index < points.size(); ++index)
      if (points[index - 1].id() == points[index].id())
        return Result<SketchTopology>::failure(Error::validation(
            points[index].id().value(), "sketch point ids must be unique"));

    std::sort(entities.begin(), entities.end(), [](const auto& left, const auto& right) {
      return left.id() < right.id();
    });
    for (std::size_t index = 1; index < entities.size(); ++index)
      if (entities[index - 1].id() == entities[index].id())
        return Result<SketchTopology>::failure(Error::validation(
            entities[index].id(), "sketch topology entity ids must be unique"));

    const auto find_point = [&points](const SketchPointId& id) {
      return std::find_if(points.begin(), points.end(),
                          [&](const auto& point) { return point.id() == id; });
    };
    for (const auto& entity : entities)
      for (const auto& point : entity.points())
        if (find_point(point) == points.end())
          return Result<SketchTopology>::failure(Error::validation(
              entity.id(), "sketch topology entity references an unknown point"));

    const auto find_entity = [&entities](std::string_view id) {
      return std::find_if(entities.begin(), entities.end(),
                          [&](const auto& entity) { return entity.id() == id; });
    };
    for (const auto& entity : entities)
      for (const auto& dependency : entity.entity_dependencies())
        if (find_entity(dependency) == entities.end())
          return Result<SketchTopology>::failure(Error::validation(
              entity.id(), "sketch topology entity references an unknown entity"));

    std::sort(dependencies.begin(), dependencies.end(), [](const auto& left, const auto& right) {
      if (left.consumer_id != right.consumer_id) return left.consumer_id < right.consumer_id;
      if (left.source_entity_id != right.source_entity_id)
        return left.source_entity_id < right.source_entity_id;
      return left.role < right.role;
    });
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
    for (const auto& dependency : dependencies) {
      if (dependency.consumer_id.empty() || dependency.source_entity_id.empty() ||
          dependency.role.empty())
        return Result<SketchTopology>::failure(Error::validation(
            object_id, "sketch topology dependency fields must not be empty"));
      if (find_entity(dependency.source_entity_id) == entities.end())
        return Result<SketchTopology>::failure(Error::validation(
            dependency.consumer_id, "sketch topology dependency references an unknown entity"));
    }

    return Result<SketchTopology>::success(SketchTopology(
        std::move(sketch), std::move(points), std::move(entities), std::move(dependencies)));
  }

  [[nodiscard]] static Result<SketchTopology>
  migrate_legacy(const Sketch& sketch, SketchTopologyMigrationReport* report = nullptr) {
    struct Candidate {
      std::string id;
      SketchTopologyEntityKind kind;
      std::vector<std::pair<std::string, Point2>> point_usages;
      std::vector<std::string> dependencies;
      SketchTopologyFlags flags;
    };
    struct PointUsage {
      std::string proposed_id;
      Point2 position;
      SketchTopologyFlags flags;
    };

    std::vector<Candidate> candidates;
    std::vector<std::pair<std::string, std::string>> shared_usage_pairs;
    const auto entity_id = [](const SketchEntityId& id) { return "entity/" + id.value(); };
    const auto profile_id = [](const ProfileId& id) { return "profile/" + id.value(); };
    const auto usage_id = [](std::string_view owner, std::string_view role) {
      return std::string(owner) + "/" + std::string(role);
    };

    for (const auto& line : sketch.line_segments())
      candidates.push_back({entity_id(line.id()), SketchTopologyEntityKind::Line,
                            {{"start", line.start()}, {"end", line.end()}}, {}, {}});
    for (const auto& arc : sketch.arc_segments())
      candidates.push_back({entity_id(arc.id()), SketchTopologyEntityKind::Arc,
                            {{"start", arc.start()}, {"mid", arc.mid()}, {"end", arc.end()}}, {}, {}});
    for (const auto& spline : sketch.spline_segments())
      candidates.push_back({entity_id(spline.id()), SketchTopologyEntityKind::Spline,
                            {{"start", spline.start()}, {"control1", spline.control1()},
                             {"control2", spline.control2()}, {"end", spline.end()}}, {}, {}});
    for (const auto& profile : sketch.rectangle_profiles())
      candidates.push_back({profile_id(profile.id()), SketchTopologyEntityKind::RectangleProfile,
                            {{"center", profile.center()}}, {}, {}});
    for (const auto& profile : sketch.circle_profiles())
      candidates.push_back({profile_id(profile.id()), SketchTopologyEntityKind::CircleProfile,
                            {{"center", profile.center()}}, {}, {}});
    for (const auto& profile : sketch.closed_profiles()) {
      std::vector<std::string> ordered_dependencies;
      for (const auto& line : profile.line_segments()) ordered_dependencies.push_back(entity_id(line));
      candidates.push_back({profile_id(profile.id()), SketchTopologyEntityKind::ClosedProfile,
                            {}, std::move(ordered_dependencies), {}});
    }
    for (const auto& profile : sketch.arc_closed_profiles()) {
      std::vector<std::string> ordered_dependencies;
      for (const auto& curve : profile.curve_segments()) ordered_dependencies.push_back(entity_id(curve));
      candidates.push_back({profile_id(profile.id()), SketchTopologyEntityKind::ArcClosedProfile,
                            {}, std::move(ordered_dependencies), {}});
    }
    for (const auto& profile : sketch.composite_closed_profiles()) {
      std::vector<std::string> ordered_dependencies;
      for (const auto& curve : profile.outer_contour()) ordered_dependencies.push_back(entity_id(curve));
      for (const auto& contour : profile.inner_contours())
        for (const auto& curve : contour) ordered_dependencies.push_back(entity_id(curve));
      candidates.push_back({profile_id(profile.id()), SketchTopologyEntityKind::CompositeClosedProfile,
                            {}, std::move(ordered_dependencies), {}});
    }
    for (const auto& pattern : sketch.circular_hole_patterns())
      candidates.push_back({profile_id(pattern.id()), SketchTopologyEntityKind::CircularHolePattern,
                            {{"center", pattern.center()}}, {}, {}});
    for (const auto& point : sketch.projected_points())
      candidates.push_back({entity_id(point.id()), SketchTopologyEntityKind::ProjectedPoint,
                            {}, {}, {.construction = false, .reference = true}});
    for (const auto& line : sketch.projected_lines())
      candidates.push_back({entity_id(line.id()), SketchTopologyEntityKind::ProjectedLine,
                            {}, {}, {.construction = false, .reference = true}});
    for (const auto& line : sketch.reference_generated_lines())
      candidates.push_back({entity_id(line.id()), SketchTopologyEntityKind::ReferenceGeneratedLine,
                            {}, {}, {.construction = false, .reference = true}});

    const auto endpoint_usages = [&](const SketchEntityId& id)
        -> std::optional<std::pair<std::string, std::string>> {
      const std::string owner = entity_id(id);
      if (sketch.find_line_segment(id) != nullptr || sketch.find_arc_segment(id) != nullptr ||
          sketch.find_spline_segment(id) != nullptr)
        return std::pair{usage_id(owner, "start"), usage_id(owner, "end")};
      return std::nullopt;
    };
    const auto connect_contour = [&](const std::vector<SketchEntityId>& contour) {
      if (contour.empty()) return;
      for (std::size_t index = 0; index < contour.size(); ++index) {
        const auto current = endpoint_usages(contour[index]);
        const auto next = endpoint_usages(contour[(index + 1U) % contour.size()]);
        if (current && next) shared_usage_pairs.emplace_back(current->second, next->first);
      }
    };
    for (const auto& profile : sketch.closed_profiles()) connect_contour(profile.line_segments());
    for (const auto& profile : sketch.arc_closed_profiles()) connect_contour(profile.curve_segments());
    for (const auto& profile : sketch.composite_closed_profiles()) {
      connect_contour(profile.outer_contour());
      for (const auto& contour : profile.inner_contours()) connect_contour(contour);
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
      return left.id < right.id;
    });
    std::sort(shared_usage_pairs.begin(), shared_usage_pairs.end());
    shared_usage_pairs.erase(std::unique(shared_usage_pairs.begin(), shared_usage_pairs.end()),
                             shared_usage_pairs.end());

    std::vector<PointUsage> usages;
    std::unordered_map<std::string, std::size_t> usage_indices;
    for (const auto& candidate : candidates)
      for (const auto& [role, position] : candidate.point_usages) {
        const std::string proposed = usage_id(candidate.id, role);
        usage_indices.emplace(proposed, usages.size());
        usages.push_back({proposed, position, candidate.flags});
      }

    std::vector<std::size_t> parent(usages.size());
    for (std::size_t index = 0; index < parent.size(); ++index) parent[index] = index;
    const auto root = [&parent](std::size_t index) {
      while (parent[index] != index) {
        parent[index] = parent[parent[index]];
        index = parent[index];
      }
      return index;
    };
    const auto unite = [&parent, &root](std::size_t first, std::size_t second) {
      first = root(first);
      second = root(second);
      if (first == second) return;
      if (first < second)
        parent[second] = first;
      else
        parent[first] = second;
    };

    constexpr double connectivity_tolerance = 1.0e-9;
    const auto same_position = [](Point2 left, Point2 right) {
      return std::abs(left.x - right.x) <= connectivity_tolerance &&
             std::abs(left.y - right.y) <= connectivity_tolerance;
    };
    for (const auto& [first_id, second_id] : shared_usage_pairs) {
      const auto first = usage_indices.find(first_id);
      const auto second = usage_indices.find(second_id);
      if (first == usage_indices.end() || second == usage_indices.end()) continue;
      if (usages[first->second].flags != usages[second->second].flags ||
          !same_position(usages[first->second].position, usages[second->second].position))
        return Result<SketchTopology>::failure(Error::validation(
            sketch.id().value(),
            "legacy profile connectivity does not resolve to coincident compatible endpoint usages"));
      unite(first->second, second->second);
    }

    std::vector<std::string> canonical_ids(usages.size());
    for (std::size_t index = 0; index < usages.size(); ++index) {
      const std::size_t representative = root(index);
      if (canonical_ids[representative].empty() ||
          usages[index].proposed_id < canonical_ids[representative])
        canonical_ids[representative] = usages[index].proposed_id;
    }

    SketchTopologyMigrationReport migration;
    migration.migrated_ = true;
    std::vector<SketchTopologyPoint> points;
    std::unordered_map<std::string, SketchPointId> point_by_usage;
    std::unordered_map<std::string, bool> point_created;
    for (std::size_t index = 0; index < usages.size(); ++index) {
      const std::string& canonical = canonical_ids[root(index)];
      point_by_usage.emplace(usages[index].proposed_id, SketchPointId(canonical));
      if (!point_created[canonical]) {
        auto point = SketchTopologyPoint::create(SketchPointId(canonical), usages[index].position,
                                                 usages[index].flags);
        if (point.has_error()) return Result<SketchTopology>::failure(point.error());
        points.push_back(std::move(point.value()));
        point_created[canonical] = true;
      }
      if (usages[index].proposed_id != canonical)
        migration.identity_changes_.push_back(
            {usages[index].proposed_id, canonical,
             "legacy profile connectivity now shares one canonical Sketch point"});
    }

    std::vector<SketchTopologyEntity> entities;
    for (const auto& candidate : candidates) {
      std::vector<SketchPointId> point_ids;
      for (const auto& [role, position] : candidate.point_usages) {
        (void)position;
        const auto found = point_by_usage.find(usage_id(candidate.id, role));
        if (found == point_by_usage.end())
          return Result<SketchTopology>::failure(Error::internal(
              candidate.id, "legacy Sketch migration lost a point-usage mapping"));
        point_ids.push_back(found->second);
      }
      auto entity = SketchTopologyEntity::create(candidate.id, candidate.kind, std::move(point_ids),
                                                 candidate.dependencies, candidate.flags);
      if (entity.has_error()) return Result<SketchTopology>::failure(entity.error());
      entities.push_back(std::move(entity.value()));
    }

    std::vector<SketchTopologyDependency> dependencies;
    const auto add_target_dependency = [&](std::string consumer, const SketchReferenceTarget& target,
                                           std::string role) {
      dependencies.push_back(
          {std::move(consumer), entity_id(target.entity()), std::move(role)});
    };
    for (const auto& constraint : sketch.constraints()) {
      const std::string consumer = "constraint/" + constraint.id().value();
      add_target_dependency(consumer, constraint.constrained_target(), "constrained");
      add_target_dependency(consumer, constraint.reference_target(), "reference");
    }
    for (const auto& constraint : sketch.geometric_constraints()) {
      const std::string consumer = "geometric-constraint/" + constraint.id().value();
      add_target_dependency(consumer, constraint.first_target(), "first");
      if (constraint.second_target())
        add_target_dependency(consumer, *constraint.second_target(), "second");
    }
    for (const auto& dimension : sketch.driving_dimensions()) {
      const std::string consumer = "dimension/" + dimension.id().value();
      add_target_dependency(consumer, dimension.first_target(), "first");
      add_target_dependency(consumer, dimension.second_target(), "second");
    }
    for (const auto& operation : sketch.trim_extend_operations())
      dependencies.push_back({"trim-extend/" + operation.id().value(),
                              entity_id(operation.target_entity()), "target"});
    for (const auto& continuity : sketch.tangent_continuities()) {
      const std::string consumer = "tangent/" + continuity.id().value();
      dependencies.push_back({consumer, entity_id(continuity.first_entity()), "first"});
      dependencies.push_back({consumer, entity_id(continuity.second_entity()), "second"});
    }
    for (const auto& candidate : candidates)
      for (const auto& dependency : candidate.dependencies)
        dependencies.push_back({candidate.id, dependency, "topology"});

    auto topology = create(sketch.id(), std::move(points), std::move(entities),
                           std::move(dependencies));
    if (topology.has_error()) return topology;
    std::sort(migration.identity_changes_.begin(), migration.identity_changes_.end(),
              [](const auto& left, const auto& right) {
                if (left.previous_identity != right.previous_identity)
                  return left.previous_identity < right.previous_identity;
                return left.canonical_identity < right.canonical_identity;
              });
    if (report != nullptr) *report = std::move(migration);
    return topology;
  }

  [[nodiscard]] const SketchId& sketch() const noexcept { return sketch_; }
  [[nodiscard]] const std::vector<SketchTopologyPoint>& points() const noexcept { return points_; }
  [[nodiscard]] const std::vector<SketchTopologyEntity>& entities() const noexcept {
    return entities_;
  }
  [[nodiscard]] const std::vector<SketchTopologyDependency>& dependencies() const noexcept {
    return dependencies_;
  }

  [[nodiscard]] const SketchTopologyPoint* find_point(SketchPointId id) const noexcept {
    const auto found = std::lower_bound(points_.begin(), points_.end(), id.value(),
                                        [](const auto& point, std::string_view value) {
                                          return point.id().value() < value;
                                        });
    return found != points_.end() && found->id() == id ? &*found : nullptr;
  }

  [[nodiscard]] const SketchTopologyEntity* find_entity(std::string_view id) const noexcept {
    const auto found = std::lower_bound(entities_.begin(), entities_.end(), id,
                                        [](const auto& entity, std::string_view value) {
                                          return entity.id() < value;
                                        });
    return found != entities_.end() && found->id() == id ? &*found : nullptr;
  }

  [[nodiscard]] std::vector<SketchTopologyDependency>
  dependents_of(std::string_view entity_id) const {
    std::vector<SketchTopologyDependency> result;
    for (const auto& dependency : dependencies_)
      if (dependency.source_entity_id == entity_id) result.push_back(dependency);
    return result;
  }

  friend bool operator==(const SketchTopology&, const SketchTopology&) = default;

private:
  SketchTopology(SketchId sketch, std::vector<SketchTopologyPoint> points,
                 std::vector<SketchTopologyEntity> entities,
                 std::vector<SketchTopologyDependency> dependencies)
      : sketch_(std::move(sketch)), points_(std::move(points)), entities_(std::move(entities)),
        dependencies_(std::move(dependencies)) {}

  SketchId sketch_;
  std::vector<SketchTopologyPoint> points_;
  std::vector<SketchTopologyEntity> entities_;
  std::vector<SketchTopologyDependency> dependencies_;
};

} // namespace blcad
