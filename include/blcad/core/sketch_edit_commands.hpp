#pragma once

#include "blcad/core/sketch_topology.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace blcad {

enum class SketchEditCommandKind { Add, Move, Replace, Remove };

[[nodiscard]] constexpr std::string_view to_string(SketchEditCommandKind kind) noexcept {
  switch (kind) {
  case SketchEditCommandKind::Add: return "add";
  case SketchEditCommandKind::Move: return "move";
  case SketchEditCommandKind::Replace: return "replace";
  case SketchEditCommandKind::Remove: return "remove";
  }
  return "add";
}

class SketchEditCommand {
public:
  struct AddPoint {
    SketchTopologyPoint point;
  };
  struct AddEntity {
    SketchTopologyEntity entity;
  };
  struct MovePoint {
    SketchPointId point;
    Point2 position;
  };
  struct ReplaceEntity {
    SketchTopologyEntity entity;
  };
  struct RemovePoint {
    SketchPointId point;
  };
  struct RemoveEntity {
    std::string entity;
  };

  using Payload = std::variant<AddPoint, AddEntity, MovePoint, ReplaceEntity, RemovePoint,
                               RemoveEntity>;

  [[nodiscard]] static SketchEditCommand add_point(SketchTopologyPoint point) {
    return SketchEditCommand(SketchEditCommandKind::Add, AddPoint{std::move(point)});
  }
  [[nodiscard]] static SketchEditCommand add_entity(SketchTopologyEntity entity) {
    return SketchEditCommand(SketchEditCommandKind::Add, AddEntity{std::move(entity)});
  }
  [[nodiscard]] static Result<SketchEditCommand> move_point(SketchPointId point, Point2 position) {
    if (point.empty())
      return Result<SketchEditCommand>::failure(
          Error::validation("sketch_edit_command", "move point id must not be empty"));
    if (!std::isfinite(position.x) || !std::isfinite(position.y))
      return Result<SketchEditCommand>::failure(
          Error::validation(point.value(), "move point coordinates must be finite"));
    return Result<SketchEditCommand>::success(
        SketchEditCommand(SketchEditCommandKind::Move, MovePoint{std::move(point), position}));
  }
  [[nodiscard]] static SketchEditCommand replace_entity(SketchTopologyEntity entity) {
    return SketchEditCommand(SketchEditCommandKind::Replace,
                             ReplaceEntity{std::move(entity)});
  }
  [[nodiscard]] static Result<SketchEditCommand> remove_point(SketchPointId point) {
    if (point.empty())
      return Result<SketchEditCommand>::failure(
          Error::validation("sketch_edit_command", "remove point id must not be empty"));
    return Result<SketchEditCommand>::success(
        SketchEditCommand(SketchEditCommandKind::Remove, RemovePoint{std::move(point)}));
  }
  [[nodiscard]] static Result<SketchEditCommand> remove_entity(std::string entity) {
    if (entity.empty())
      return Result<SketchEditCommand>::failure(
          Error::validation("sketch_edit_command", "remove entity id must not be empty"));
    return Result<SketchEditCommand>::success(
        SketchEditCommand(SketchEditCommandKind::Remove, RemoveEntity{std::move(entity)}));
  }

  [[nodiscard]] SketchEditCommandKind kind() const noexcept { return kind_; }
  [[nodiscard]] const Payload& payload() const noexcept { return payload_; }

private:
  template <typename T>
  SketchEditCommand(SketchEditCommandKind kind, T payload)
      : kind_(kind), payload_(std::move(payload)) {}

  SketchEditCommandKind kind_;
  Payload payload_;
};

class SketchEditTransaction {
public:
  [[nodiscard]] const SketchTopology& before() const noexcept { return before_; }
  [[nodiscard]] const SketchTopology& after() const noexcept { return after_; }
  [[nodiscard]] SketchEditCommandKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::string& object_id() const noexcept { return object_id_; }
  [[nodiscard]] SketchTopology undo() const { return before_; }
  [[nodiscard]] SketchTopology redo() const { return after_; }

private:
  friend class SketchEditCommandExecutor;
  SketchEditTransaction(SketchTopology before, SketchTopology after, SketchEditCommandKind kind,
                        std::string object_id)
      : before_(std::move(before)), after_(std::move(after)), kind_(kind),
        object_id_(std::move(object_id)) {}

  SketchTopology before_;
  SketchTopology after_;
  SketchEditCommandKind kind_;
  std::string object_id_;
};

class SketchEditCommandExecutor {
public:
  [[nodiscard]] Result<SketchEditTransaction>
  apply(const SketchTopology& topology, const SketchEditCommand& command) const {
    std::vector<SketchTopologyPoint> points = topology.points();
    std::vector<SketchTopologyEntity> entities = topology.entities();
    std::vector<SketchTopologyDependency> dependencies = topology.dependencies();
    std::string object_id;

    const auto point_index = [&points](const SketchPointId& id) {
      return std::find_if(points.begin(), points.end(),
                          [&](const auto& point) { return point.id() == id; });
    };
    const auto entity_index = [&entities](std::string_view id) {
      return std::find_if(entities.begin(), entities.end(),
                          [&](const auto& entity) { return entity.id() == id; });
    };
    constexpr double tolerance = 1.0e-9;
    const auto same_position = [](Point2 left, Point2 right) {
      return std::abs(left.x - right.x) <= tolerance &&
             std::abs(left.y - right.y) <= tolerance;
    };

    const auto validate_entity_points = [&](const SketchTopologyEntity& entity)
        -> Result<std::size_t> {
      for (const auto& point : entity.points())
        if (point_index(point) == points.end())
          return Result<std::size_t>::failure(Error::validation(
              entity.id(), "replacement entity references an unknown Sketch point"));
      for (const auto& dependency : entity.entity_dependencies())
        if (entity_index(dependency) == entities.end() && dependency != entity.id())
          return Result<std::size_t>::failure(Error::validation(
              entity.id(), "replacement entity references an unknown Sketch entity"));
      return Result<std::size_t>::success(entity.points().size());
    };

    auto applied = std::visit(
        [&](const auto& payload) -> Result<std::size_t> {
          using T = std::decay_t<decltype(payload)>;
          if constexpr (std::is_same_v<T, SketchEditCommand::AddPoint>) {
            object_id = payload.point.id().value();
            if (point_index(payload.point.id()) != points.end())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "Sketch point id already exists"));
            const auto coincident = std::find_if(points.begin(), points.end(), [&](const auto& point) {
              return point.flags() == payload.point.flags() &&
                     same_position(point.position(), payload.point.position());
            });
            if (coincident != points.end())
              return Result<std::size_t>::failure(Error::validation(
                  object_id, "coincident coordinates already have canonical point identity " +
                                 coincident->id().value()));
            points.push_back(payload.point);
            return Result<std::size_t>::success(points.size() - 1U);
          } else if constexpr (std::is_same_v<T, SketchEditCommand::AddEntity>) {
            object_id = payload.entity.id();
            if (entity_index(payload.entity.id()) != entities.end())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "Sketch topology entity id already exists"));
            auto valid = validate_entity_points(payload.entity);
            if (valid.has_error()) return valid;
            entities.push_back(payload.entity);
            for (const auto& dependency : payload.entity.entity_dependencies())
              dependencies.push_back({payload.entity.id(), dependency, "topology"});
            return Result<std::size_t>::success(entities.size() - 1U);
          } else if constexpr (std::is_same_v<T, SketchEditCommand::MovePoint>) {
            object_id = payload.point.value();
            const auto point = point_index(payload.point);
            if (point == points.end())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "Sketch point does not exist"));
            if (point->reference())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "reference Sketch points are read-only"));
            const auto collision = std::find_if(points.begin(), points.end(), [&](const auto& other) {
              return other.id() != payload.point && other.flags() == point->flags() &&
                     same_position(other.position(), payload.position);
            });
            if (collision != points.end())
              return Result<std::size_t>::failure(Error::validation(
                  object_id, "move would create duplicate point identity at " +
                                 collision->id().value()));
            auto replacement = point->with_position(payload.position);
            if (replacement.has_error())
              return Result<std::size_t>::failure(replacement.error());
            *point = std::move(replacement.value());
            return Result<std::size_t>::success(
                static_cast<std::size_t>(std::distance(points.begin(), point)));
          } else if constexpr (std::is_same_v<T, SketchEditCommand::ReplaceEntity>) {
            object_id = payload.entity.id();
            const auto entity = entity_index(payload.entity.id());
            if (entity == entities.end())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "Sketch topology entity does not exist"));
            if (entity->reference())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "reference Sketch entities are read-only"));
            if (payload.entity.reference())
              return Result<std::size_t>::failure(Error::validation(
                  object_id, "replace cannot convert authored geometry into reference geometry"));
            auto valid = validate_entity_points(payload.entity);
            if (valid.has_error()) return valid;
            dependencies.erase(
                std::remove_if(dependencies.begin(), dependencies.end(), [&](const auto& dependency) {
                  return dependency.consumer_id == object_id && dependency.role == "topology";
                }),
                dependencies.end());
            *entity = payload.entity;
            for (const auto& dependency : payload.entity.entity_dependencies())
              dependencies.push_back({payload.entity.id(), dependency, "topology"});
            return Result<std::size_t>::success(
                static_cast<std::size_t>(std::distance(entities.begin(), entity)));
          } else if constexpr (std::is_same_v<T, SketchEditCommand::RemovePoint>) {
            object_id = payload.point.value();
            const auto point = point_index(payload.point);
            if (point == points.end())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "Sketch point does not exist"));
            if (point->reference())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "reference Sketch points are read-only"));
            for (const auto& entity : entities)
              if (std::find(entity.points().begin(), entity.points().end(), payload.point) !=
                  entity.points().end())
                return Result<std::size_t>::failure(Error::dependency(
                    object_id, "Sketch point is still referenced by " + entity.id()));
            const auto index = static_cast<std::size_t>(std::distance(points.begin(), point));
            points.erase(point);
            return Result<std::size_t>::success(index);
          } else {
            object_id = payload.entity;
            const auto entity = entity_index(payload.entity);
            if (entity == entities.end())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "Sketch topology entity does not exist"));
            if (entity->reference())
              return Result<std::size_t>::failure(
                  Error::validation(object_id, "reference Sketch entities are read-only"));
            for (const auto& dependency : dependencies)
              if (dependency.source_entity_id == payload.entity &&
                  dependency.consumer_id != payload.entity)
                return Result<std::size_t>::failure(Error::dependency(
                    object_id, "Sketch entity is still referenced by " + dependency.consumer_id));
            const auto index = static_cast<std::size_t>(std::distance(entities.begin(), entity));
            entities.erase(entity);
            dependencies.erase(
                std::remove_if(dependencies.begin(), dependencies.end(), [&](const auto& dependency) {
                  return dependency.consumer_id == object_id ||
                         dependency.source_entity_id == object_id;
                }),
                dependencies.end());
            return Result<std::size_t>::success(index);
          }
        },
        command.payload());

    if (applied.has_error()) return Result<SketchEditTransaction>::failure(applied.error());
    auto after = SketchTopology::create(topology.sketch(), std::move(points), std::move(entities),
                                        std::move(dependencies));
    if (after.has_error()) return Result<SketchEditTransaction>::failure(after.error());
    return Result<SketchEditTransaction>::success(SketchEditTransaction(
        topology, std::move(after.value()), command.kind(), std::move(object_id)));
  }
};

class SketchTopologyUndoStack {
public:
  explicit SketchTopologyUndoStack(SketchTopology topology) : current_(std::move(topology)) {}

  [[nodiscard]] const SketchTopology& current() const noexcept { return current_; }
  [[nodiscard]] std::size_t undo_count() const noexcept { return undo_.size(); }
  [[nodiscard]] std::size_t redo_count() const noexcept { return redo_.size(); }

  [[nodiscard]] Result<std::size_t> apply(const SketchEditCommand& command) {
    auto transaction = SketchEditCommandExecutor{}.apply(current_, command);
    if (transaction.has_error()) return Result<std::size_t>::failure(transaction.error());
    current_ = transaction.value().after();
    undo_.push_back(std::move(transaction.value()));
    redo_.clear();
    return Result<std::size_t>::success(undo_.size());
  }

  [[nodiscard]] Result<std::size_t> undo() {
    if (undo_.empty())
      return Result<std::size_t>::failure(
          Error::validation(current_.sketch().value(), "Sketch topology undo stack is empty"));
    SketchEditTransaction transaction = undo_.back();
    undo_.pop_back();
    current_ = transaction.undo();
    redo_.push_back(std::move(transaction));
    return Result<std::size_t>::success(undo_.size());
  }

  [[nodiscard]] Result<std::size_t> redo() {
    if (redo_.empty())
      return Result<std::size_t>::failure(
          Error::validation(current_.sketch().value(), "Sketch topology redo stack is empty"));
    SketchEditTransaction transaction = redo_.back();
    redo_.pop_back();
    current_ = transaction.redo();
    undo_.push_back(std::move(transaction));
    return Result<std::size_t>::success(redo_.size());
  }

private:
  SketchTopology current_;
  std::vector<SketchEditTransaction> undo_;
  std::vector<SketchEditTransaction> redo_;
};

} // namespace blcad
