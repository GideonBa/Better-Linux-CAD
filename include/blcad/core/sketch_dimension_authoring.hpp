#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_constraint_authoring.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

enum class SketchDimensionKind {
  HorizontalDistance,
  VerticalDistance,
  AlignedDistance,
  PointToPointDistance,
  Length,
  Radius,
  Diameter,
  Angle,
  ArcLength,
};

[[nodiscard]] constexpr std::string_view to_string(SketchDimensionKind kind) noexcept {
  switch (kind) {
  case SketchDimensionKind::HorizontalDistance: return "horizontal_distance";
  case SketchDimensionKind::VerticalDistance: return "vertical_distance";
  case SketchDimensionKind::AlignedDistance: return "aligned_distance";
  case SketchDimensionKind::PointToPointDistance: return "point_to_point_distance";
  case SketchDimensionKind::Length: return "length";
  case SketchDimensionKind::Radius: return "radius";
  case SketchDimensionKind::Diameter: return "diameter";
  case SketchDimensionKind::Angle: return "angle";
  case SketchDimensionKind::ArcLength: return "arc_length";
  }
  return "aligned_distance";
}

enum class SketchDimensionMode { Driving, Reference };

[[nodiscard]] constexpr std::string_view to_string(SketchDimensionMode mode) noexcept {
  switch (mode) {
  case SketchDimensionMode::Driving: return "driving";
  case SketchDimensionMode::Reference: return "reference";
  }
  return "driving";
}

class SketchDimensionIntent {
public:
  [[nodiscard]] static Result<SketchDimensionIntent>
  create(SketchDimensionId id, SketchDimensionKind kind, SketchDimensionMode mode,
         std::vector<SketchConstraintIntentTarget> targets,
         std::optional<ParameterId> parameter = std::nullopt) {
    const std::string object_id = id.empty() ? "sketch_dimension_intent" : id.value();
    if (id.empty())
      return Result<SketchDimensionIntent>::failure(
          Error::validation(object_id, "dimension id must not be empty"));
    if (targets.size() != expected_target_count(kind))
      return Result<SketchDimensionIntent>::failure(Error::validation(
          object_id, "dimension has the wrong number of semantic targets"));
    if (!target_signature_valid(kind, targets))
      return Result<SketchDimensionIntent>::failure(Error::validation(
          object_id, "dimension target kinds are incompatible with the selected family"));
    for (std::size_t index = 0U; index < targets.size(); ++index)
      for (std::size_t previous = 0U; previous < index; ++previous)
        if (targets[index] == targets[previous])
          return Result<SketchDimensionIntent>::failure(
              Error::validation(object_id, "dimension targets must be distinct"));
    if (mode == SketchDimensionMode::Driving && (!parameter || parameter->empty()))
      return Result<SketchDimensionIntent>::failure(Error::validation(
          object_id, "driving dimension requires a parameter binding"));
    if (mode == SketchDimensionMode::Reference && parameter.has_value())
      return Result<SketchDimensionIntent>::failure(Error::validation(
          object_id, "reference dimension stores a derived measurement, not a parameter binding"));
    return Result<SketchDimensionIntent>::success(SketchDimensionIntent(
        std::move(id), kind, mode, std::move(targets), std::move(parameter)));
  }

  [[nodiscard]] const SketchDimensionId& id() const noexcept { return id_; }
  [[nodiscard]] SketchDimensionKind kind() const noexcept { return kind_; }
  [[nodiscard]] SketchDimensionMode mode() const noexcept { return mode_; }
  [[nodiscard]] const std::vector<SketchConstraintIntentTarget>& targets() const noexcept {
    return targets_;
  }
  [[nodiscard]] const std::optional<ParameterId>& parameter() const noexcept { return parameter_; }
  [[nodiscard]] bool driving() const noexcept { return mode_ == SketchDimensionMode::Driving; }

  [[nodiscard]] Result<SketchDimensionIntent> with_mode(
      SketchDimensionMode mode, std::optional<ParameterId> parameter = std::nullopt) const {
    return create(id_, kind_, mode, targets_, std::move(parameter));
  }

  [[nodiscard]] Result<SketchDimensionIntent> with_parameter(ParameterId parameter) const {
    return create(id_, kind_, SketchDimensionMode::Driving, targets_, std::move(parameter));
  }

  friend bool operator==(const SketchDimensionIntent&, const SketchDimensionIntent&) = default;

private:
  SketchDimensionIntent(SketchDimensionId id, SketchDimensionKind kind,
                        SketchDimensionMode mode,
                        std::vector<SketchConstraintIntentTarget> targets,
                        std::optional<ParameterId> parameter)
      : id_(std::move(id)), kind_(kind), mode_(mode), targets_(std::move(targets)),
        parameter_(std::move(parameter)) {}

  [[nodiscard]] static std::size_t expected_target_count(SketchDimensionKind kind) noexcept {
    switch (kind) {
    case SketchDimensionKind::HorizontalDistance:
    case SketchDimensionKind::VerticalDistance:
    case SketchDimensionKind::AlignedDistance:
    case SketchDimensionKind::PointToPointDistance:
    case SketchDimensionKind::Angle: return 2U;
    case SketchDimensionKind::Length:
    case SketchDimensionKind::Radius:
    case SketchDimensionKind::Diameter:
    case SketchDimensionKind::ArcLength: return 1U;
    }
    return 0U;
  }

  [[nodiscard]] static bool target_signature_valid(
      SketchDimensionKind kind,
      const std::vector<SketchConstraintIntentTarget>& targets) noexcept {
    const auto point = [&targets](std::size_t index) {
      return targets[index].kind() == SketchConstraintIntentTargetKind::Point;
    };
    const auto entity = [&targets](std::size_t index) {
      return targets[index].kind() == SketchConstraintIntentTargetKind::Entity;
    };
    switch (kind) {
    case SketchDimensionKind::HorizontalDistance:
    case SketchDimensionKind::VerticalDistance:
    case SketchDimensionKind::AlignedDistance:
    case SketchDimensionKind::PointToPointDistance: return point(0U) && point(1U);
    case SketchDimensionKind::Length:
    case SketchDimensionKind::Radius:
    case SketchDimensionKind::Diameter:
    case SketchDimensionKind::ArcLength: return entity(0U);
    case SketchDimensionKind::Angle: return entity(0U) && entity(1U);
    }
    return false;
  }

  SketchDimensionId id_;
  SketchDimensionKind kind_;
  SketchDimensionMode mode_;
  std::vector<SketchConstraintIntentTarget> targets_;
  std::optional<ParameterId> parameter_;
};

class SketchDimensionCatalog {
public:
  [[nodiscard]] static Result<SketchDimensionCatalog>
  create(DocumentId document, SketchId sketch,
         std::vector<SketchDimensionIntent> dimensions = {}) {
    const std::string object_id = sketch.empty() ? "sketch_dimension_catalog" : sketch.value();
    if (document.empty() || sketch.empty())
      return Result<SketchDimensionCatalog>::failure(Error::validation(
          object_id, "dimension catalog requires document and Sketch ids"));
    std::sort(dimensions.begin(), dimensions.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
    for (std::size_t index = 1U; index < dimensions.size(); ++index)
      if (dimensions[index - 1U].id() == dimensions[index].id())
        return Result<SketchDimensionCatalog>::failure(Error::validation(
            dimensions[index].id().value(), "dimension catalog ids must be unique"));
    return Result<SketchDimensionCatalog>::success(SketchDimensionCatalog(
        std::move(document), std::move(sketch), std::move(dimensions)));
  }

  [[nodiscard]] const DocumentId& document() const noexcept { return document_; }
  [[nodiscard]] const SketchId& sketch() const noexcept { return sketch_; }
  [[nodiscard]] const std::vector<SketchDimensionIntent>& dimensions() const noexcept {
    return dimensions_;
  }
  [[nodiscard]] const SketchDimensionIntent* find(SketchDimensionId id) const noexcept {
    const auto found = std::lower_bound(
        dimensions_.begin(), dimensions_.end(), id.value(),
        [](const auto& dimension, std::string_view value) {
          return dimension.id().value() < value;
        });
    return found != dimensions_.end() && found->id() == id ? &*found : nullptr;
  }

  [[nodiscard]] Result<std::size_t> add(SketchDimensionIntent dimension) {
    if (find(dimension.id()) != nullptr)
      return Result<std::size_t>::failure(
          Error::validation(dimension.id().value(), "dimension id already exists"));
    dimensions_.push_back(std::move(dimension));
    sort_dimensions();
    return Result<std::size_t>::success(dimensions_.size());
  }

  [[nodiscard]] Result<std::size_t> replace(SketchDimensionIntent dimension) {
    const auto found = std::find_if(dimensions_.begin(), dimensions_.end(),
                                    [&dimension](const auto& value) {
                                      return value.id() == dimension.id();
                                    });
    if (found == dimensions_.end())
      return Result<std::size_t>::failure(
          Error::validation(dimension.id().value(), "dimension does not exist"));
    *found = std::move(dimension);
    sort_dimensions();
    return Result<std::size_t>::success(dimensions_.size());
  }

  [[nodiscard]] Result<std::size_t> remove(SketchDimensionId id) {
    const auto found = std::find_if(dimensions_.begin(), dimensions_.end(),
                                    [&id](const auto& value) { return value.id() == id; });
    if (found == dimensions_.end())
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "dimension does not exist"));
    dimensions_.erase(found);
    return Result<std::size_t>::success(dimensions_.size());
  }

  friend bool operator==(const SketchDimensionCatalog&, const SketchDimensionCatalog&) = default;

private:
  SketchDimensionCatalog(DocumentId document, SketchId sketch,
                         std::vector<SketchDimensionIntent> dimensions)
      : document_(std::move(document)), sketch_(std::move(sketch)),
        dimensions_(std::move(dimensions)) {}

  void sort_dimensions() {
    std::sort(dimensions_.begin(), dimensions_.end(), [](const auto& left, const auto& right) {
      return left.id().value() < right.id().value();
    });
  }

  DocumentId document_;
  SketchId sketch_;
  std::vector<SketchDimensionIntent> dimensions_;
};

struct SketchDimensionMeasurement {
  SketchDimensionId dimension;
  SketchDimensionKind kind;
  Quantity value;

  friend bool operator==(const SketchDimensionMeasurement&,
                         const SketchDimensionMeasurement&) = default;
};

class SketchDimensionMeasurementEvaluator {
public:
  [[nodiscard]] static Result<SketchDimensionMeasurement>
  measure(const SketchTopology& topology, const SketchDimensionIntent& dimension) {
    constexpr double kPi = 3.141592653589793238462643383279502884;
    const auto point = [&topology, &dimension](std::size_t index) -> Result<Point2> {
      const auto* found = topology.find_point(SketchPointId(dimension.targets()[index].id()));
      if (found == nullptr)
        return Result<Point2>::failure(Error::validation(
            dimension.id().value(), "dimension references an unknown Sketch point"));
      return Result<Point2>::success(found->position());
    };
    const auto entity = [&topology, &dimension](std::size_t index)
        -> Result<const SketchTopologyEntity*> {
      const auto* found = topology.find_entity(dimension.targets()[index].id());
      if (found == nullptr)
        return Result<const SketchTopologyEntity*>::failure(Error::validation(
            dimension.id().value(), "dimension references an unknown Sketch entity"));
      return Result<const SketchTopologyEntity*>::success(found);
    };
    const auto length_quantity = [&dimension](double value) {
      auto quantity = Quantity::length_mm(value, dimension.id().value());
      if (quantity.has_error())
        return Result<SketchDimensionMeasurement>::failure(quantity.error());
      return Result<SketchDimensionMeasurement>::success(
          {dimension.id(), dimension.kind(), quantity.value()});
    };
    const auto angle_quantity = [&dimension](double value) {
      auto quantity = Quantity::angle_deg(value, dimension.id().value());
      if (quantity.has_error())
        return Result<SketchDimensionMeasurement>::failure(quantity.error());
      return Result<SketchDimensionMeasurement>::success(
          {dimension.id(), dimension.kind(), quantity.value()});
    };

    if (dimension.kind() == SketchDimensionKind::HorizontalDistance ||
        dimension.kind() == SketchDimensionKind::VerticalDistance ||
        dimension.kind() == SketchDimensionKind::AlignedDistance ||
        dimension.kind() == SketchDimensionKind::PointToPointDistance) {
      auto first = point(0U);
      auto second = point(1U);
      if (first.has_error()) return Result<SketchDimensionMeasurement>::failure(first.error());
      if (second.has_error()) return Result<SketchDimensionMeasurement>::failure(second.error());
      const double dx = second.value().x - first.value().x;
      const double dy = second.value().y - first.value().y;
      if (dimension.kind() == SketchDimensionKind::HorizontalDistance)
        return length_quantity(std::abs(dx));
      if (dimension.kind() == SketchDimensionKind::VerticalDistance)
        return length_quantity(std::abs(dy));
      return length_quantity(std::hypot(dx, dy));
    }

    if (dimension.kind() == SketchDimensionKind::Length) {
      auto selected = entity(0U);
      if (selected.has_error())
        return Result<SketchDimensionMeasurement>::failure(selected.error());
      if (selected.value()->kind() != SketchTopologyEntityKind::Line ||
          selected.value()->points().size() != 2U)
        return Result<SketchDimensionMeasurement>::failure(Error::validation(
            dimension.id().value(), "length dimension requires one line entity"));
      const auto* start = topology.find_point(selected.value()->points()[0]);
      const auto* end = topology.find_point(selected.value()->points()[1]);
      return length_quantity(std::hypot(end->position().x - start->position().x,
                                        end->position().y - start->position().y));
    }

    if (dimension.kind() == SketchDimensionKind::Angle) {
      auto first = entity(0U);
      auto second = entity(1U);
      if (first.has_error()) return Result<SketchDimensionMeasurement>::failure(first.error());
      if (second.has_error()) return Result<SketchDimensionMeasurement>::failure(second.error());
      if (first.value()->kind() != SketchTopologyEntityKind::Line ||
          second.value()->kind() != SketchTopologyEntityKind::Line)
        return Result<SketchDimensionMeasurement>::failure(Error::validation(
            dimension.id().value(), "angle dimension requires two line entities"));
      const auto vector = [&topology](const SketchTopologyEntity& line) {
        const Point2 start = topology.find_point(line.points()[0])->position();
        const Point2 end = topology.find_point(line.points()[1])->position();
        return Point2{end.x - start.x, end.y - start.y};
      };
      const Point2 first_vector = vector(*first.value());
      const Point2 second_vector = vector(*second.value());
      double angle = std::abs(std::atan2(first_vector.x * second_vector.y -
                                             first_vector.y * second_vector.x,
                                         first_vector.x * second_vector.x +
                                             first_vector.y * second_vector.y));
      if (angle > kPi) angle = 2.0 * kPi - angle;
      return angle_quantity(angle * 180.0 / kPi);
    }

    auto selected = entity(0U);
    if (selected.has_error())
      return Result<SketchDimensionMeasurement>::failure(selected.error());
    auto arc = arc_geometry(topology, *selected.value(), dimension.id().value());
    if (arc.has_error())
      return Result<SketchDimensionMeasurement>::failure(arc.error());
    if (dimension.kind() == SketchDimensionKind::Radius)
      return length_quantity(arc.value().radius);
    if (dimension.kind() == SketchDimensionKind::Diameter)
      return length_quantity(2.0 * arc.value().radius);
    return length_quantity(arc.value().radius * arc.value().sweep_radians);
  }

  struct ArcMeasurementGeometry {
    Point2 center;
    double radius{0.0};
    double sweep_radians{0.0};
  };

  [[nodiscard]] static Result<ArcMeasurementGeometry>
  arc_geometry(const SketchTopology& topology, const SketchTopologyEntity& entity,
               std::string_view object_id) {
    constexpr double kPi = 3.141592653589793238462643383279502884;
    if (entity.kind() != SketchTopologyEntityKind::Arc || entity.points().size() != 3U)
      return Result<ArcMeasurementGeometry>::failure(
          Error::validation(std::string(object_id), "arc dimension requires one arc entity"));
    const Point2 start = topology.find_point(entity.points()[0])->position();
    const Point2 mid = topology.find_point(entity.points()[1])->position();
    const Point2 end = topology.find_point(entity.points()[2])->position();
    const double denominator =
        2.0 * (start.x * (mid.y - end.y) + mid.x * (end.y - start.y) +
               end.x * (start.y - mid.y));
    if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-12)
      return Result<ArcMeasurementGeometry>::failure(
          Error::validation(std::string(object_id), "arc dimension references a degenerate arc"));
    const double start_norm = start.x * start.x + start.y * start.y;
    const double mid_norm = mid.x * mid.x + mid.y * mid.y;
    const double end_norm = end.x * end.x + end.y * end.y;
    const Point2 center{
        (start_norm * (mid.y - end.y) + mid_norm * (end.y - start.y) +
         end_norm * (start.y - mid.y)) /
            denominator,
        (start_norm * (end.x - mid.x) + mid_norm * (start.x - end.x) +
         end_norm * (mid.x - start.x)) /
            denominator};
    const double radius = std::hypot(start.x - center.x, start.y - center.y);
    const auto normalized = [kPi](double value) {
      double result = std::fmod(value, 2.0 * kPi);
      if (result < 0.0) result += 2.0 * kPi;
      return result;
    };
    const double start_angle = std::atan2(start.y - center.y, start.x - center.x);
    const double mid_angle = std::atan2(mid.y - center.y, mid.x - center.x);
    const double end_angle = std::atan2(end.y - center.y, end.x - center.x);
    const double ccw_end = normalized(end_angle - start_angle);
    const double ccw_mid = normalized(mid_angle - start_angle);
    const double sweep = ccw_mid <= ccw_end + 1.0e-10 ? ccw_end : 2.0 * kPi - ccw_end;
    if (!std::isfinite(radius) || radius <= 0.0 || !std::isfinite(sweep) || sweep <= 0.0)
      return Result<ArcMeasurementGeometry>::failure(
          Error::validation(std::string(object_id), "arc dimension has invalid radius or sweep"));
    return Result<ArcMeasurementGeometry>::success({center, radius, sweep});
  }
};

class SketchDimensionSolverAdapter {
public:
  [[nodiscard]] static Result<std::optional<SketchSolverConstraint>>
  to_solver_constraint(const PartDocument& document, const SketchTopology& topology,
                       const SketchDimensionIntent& dimension) {
    if (!dimension.driving())
      return Result<std::optional<SketchSolverConstraint>>::success(std::nullopt);
    auto value = parameter_value(document, dimension);
    if (value.has_error())
      return Result<std::optional<SketchSolverConstraint>>::failure(value.error());

    std::vector<SketchSolverTarget> targets;
    SketchSolverConstraintKind kind = SketchSolverConstraintKind::AlignedDistance;
    if (dimension.kind() == SketchDimensionKind::HorizontalDistance)
      kind = SketchSolverConstraintKind::HorizontalDistance;
    else if (dimension.kind() == SketchDimensionKind::VerticalDistance)
      kind = SketchSolverConstraintKind::VerticalDistance;
    else if (dimension.kind() == SketchDimensionKind::Radius ||
             dimension.kind() == SketchDimensionKind::ArcLength)
      kind = SketchSolverConstraintKind::Radial;
    else if (dimension.kind() == SketchDimensionKind::Diameter)
      kind = SketchSolverConstraintKind::Diameter;
    else if (dimension.kind() == SketchDimensionKind::Angle)
      kind = SketchSolverConstraintKind::Angular;

    if (dimension.kind() == SketchDimensionKind::Length) {
      const auto* entity = topology.find_entity(dimension.targets()[0].id());
      if (entity == nullptr || entity->kind() != SketchTopologyEntityKind::Line ||
          entity->points().size() != 2U)
        return Result<std::optional<SketchSolverConstraint>>::failure(Error::validation(
            dimension.id().value(), "driving length dimension requires one line entity"));
      auto first = SketchSolverTarget::point(entity->points()[0]);
      auto second = SketchSolverTarget::point(entity->points()[1]);
      if (first.has_error())
        return Result<std::optional<SketchSolverConstraint>>::failure(first.error());
      if (second.has_error())
        return Result<std::optional<SketchSolverConstraint>>::failure(second.error());
      targets = {std::move(first.value()), std::move(second.value())};
    } else {
      for (const auto& target : dimension.targets()) {
        if (target.kind() == SketchConstraintIntentTargetKind::Point) {
          if (topology.find_point(SketchPointId(target.id())) == nullptr)
            return Result<std::optional<SketchSolverConstraint>>::failure(Error::validation(
                dimension.id().value(), "dimension references an unknown Sketch point"));
          auto converted = SketchSolverTarget::point(SketchPointId(target.id()));
          if (converted.has_error())
            return Result<std::optional<SketchSolverConstraint>>::failure(converted.error());
          targets.push_back(std::move(converted.value()));
        } else {
          if (topology.find_entity(target.id()) == nullptr)
            return Result<std::optional<SketchSolverConstraint>>::failure(Error::validation(
                dimension.id().value(), "dimension references an unknown Sketch entity"));
          auto converted = SketchSolverTarget::entity(target.id());
          if (converted.has_error())
            return Result<std::optional<SketchSolverConstraint>>::failure(converted.error());
          targets.push_back(std::move(converted.value()));
        }
      }
    }

    double solver_value = value.value();
    std::string solver_id = "dimension/" + dimension.id().value();
    if (dimension.kind() == SketchDimensionKind::ArcLength) {
      const auto* entity = topology.find_entity(dimension.targets()[0].id());
      if (entity == nullptr)
        return Result<std::optional<SketchSolverConstraint>>::failure(Error::validation(
            dimension.id().value(), "arc-length dimension entity does not exist"));
      auto arc = SketchDimensionMeasurementEvaluator::arc_geometry(
          topology, *entity, dimension.id().value());
      if (arc.has_error())
        return Result<std::optional<SketchSolverConstraint>>::failure(arc.error());
      solver_value = value.value() / arc.value().sweep_radians;
      solver_id += "/equivalent_radius";
    }

    auto converted = SketchSolverConstraint::create(
        std::move(solver_id), kind, std::move(targets), solver_value);
    if (converted.has_error())
      return Result<std::optional<SketchSolverConstraint>>::failure(converted.error());
    return Result<std::optional<SketchSolverConstraint>>::success(
        std::optional<SketchSolverConstraint>(std::move(converted.value())));
  }

  [[nodiscard]] static Result<double>
  parameter_value(const PartDocument& document, const SketchDimensionIntent& dimension) {
    if (!dimension.parameter())
      return Result<double>::failure(Error::validation(
          dimension.id().value(), "driving dimension has no parameter binding"));
    const Parameter* parameter = document.find_parameter(*dimension.parameter());
    if (parameter == nullptr)
      return Result<double>::failure(Error::validation(
          dimension.id().value(), "dimension parameter does not exist"));
    const ParameterType expected = dimension.kind() == SketchDimensionKind::Angle
                                       ? ParameterType::Angle
                                       : ParameterType::Length;
    if (parameter->type() != expected)
      return Result<double>::failure(Error::validation(
          dimension.id().value(), "dimension parameter type does not match its family"));
    const double value = expected == ParameterType::Angle
                             ? parameter->value().degrees()
                             : parameter->value().millimeters();
    if (!std::isfinite(value) || value <= 0.0)
      return Result<double>::failure(Error::validation(
          dimension.id().value(), "driving dimension value must be finite and positive"));
    return Result<double>::success(value);
  }
};

class SketchDimensionEditService {
public:
  [[nodiscard]] static Result<std::size_t>
  set_literal(PartDocument& document, const SketchDimensionCatalog& catalog,
              SketchDimensionId dimension_id, Quantity value) {
    const SketchDimensionIntent* dimension = catalog.find(dimension_id);
    if (dimension == nullptr || !dimension->driving() || !dimension->parameter())
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "literal edit requires a parameter-backed driving dimension"));
    const Parameter* parameter = document.find_parameter(*dimension->parameter());
    if (parameter == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "dimension parameter does not exist"));
    if (parameter->is_expression())
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "expression-backed dimension requires a formula edit"));
    const ParameterType expected = dimension->kind() == SketchDimensionKind::Angle
                                       ? ParameterType::Angle
                                       : ParameterType::Length;
    if (parameter->type() != expected ||
        (expected == ParameterType::Angle && value.kind() != QuantityKind::AngleDeg) ||
        (expected == ParameterType::Length && value.kind() != QuantityKind::LengthMm))
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "literal edit quantity type does not match the dimension"));
    auto affected = document.set_parameter_value(*dimension->parameter(), value);
    if (affected.has_error()) return Result<std::size_t>::failure(affected.error());
    return Result<std::size_t>::success(affected.value().size());
  }

  [[nodiscard]] static Result<std::size_t>
  set_formula(PartDocument& document, SketchDimensionCatalog& catalog,
              SketchDimensionId dimension_id, std::string formula,
              std::optional<ParameterId> replacement_parameter = std::nullopt,
              std::string replacement_name = {}) {
    const SketchDimensionIntent* stored = catalog.find(dimension_id);
    if (stored == nullptr || !stored->driving() || !stored->parameter())
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "formula edit requires a parameter-backed driving dimension"));
    const SketchDimensionIntent dimension = *stored;
    const Parameter* parameter = document.find_parameter(*dimension.parameter());
    if (parameter == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "dimension parameter does not exist"));
    if (parameter->is_expression()) {
      auto affected = document.set_parameter_formula(parameter->id(), std::move(formula));
      if (affected.has_error()) return Result<std::size_t>::failure(affected.error());
      return Result<std::size_t>::success(affected.value().size());
    }
    if (!replacement_parameter || replacement_parameter->empty() ||
        *replacement_parameter == parameter->id())
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(),
          "converting a direct dimension to an expression requires a new ParameterId"));
    if (replacement_name.empty()) replacement_name = replacement_parameter->value();
    const ParameterType type = dimension.kind() == SketchDimensionKind::Angle
                                   ? ParameterType::Angle
                                   : ParameterType::Length;
    auto added = document.add_expression_parameter(*replacement_parameter, replacement_name, type,
                                                   std::move(formula));
    if (added.has_error()) return added;
    auto rebound = dimension.with_parameter(*replacement_parameter);
    if (rebound.has_error()) return Result<std::size_t>::failure(rebound.error());
    auto replaced = catalog.replace(std::move(rebound.value()));
    if (replaced.has_error()) return replaced;
    return Result<std::size_t>::success(added.value() + replaced.value());
  }

  [[nodiscard]] static Result<std::size_t>
  rebind_parameter(const PartDocument& document, SketchDimensionCatalog& catalog,
                   SketchDimensionId dimension_id, ParameterId parameter_id) {
    const SketchDimensionIntent* stored = catalog.find(dimension_id);
    if (stored == nullptr || !stored->driving())
      return Result<std::size_t>::failure(Error::validation(
          dimension_id.value(), "parameter rebinding requires a driving dimension"));
    const Parameter* parameter = document.find_parameter(parameter_id);
    if (parameter == nullptr)
      return Result<std::size_t>::failure(Error::validation(
          parameter_id.value(), "replacement dimension parameter does not exist"));
    const ParameterType expected = stored->kind() == SketchDimensionKind::Angle
                                       ? ParameterType::Angle
                                       : ParameterType::Length;
    if (parameter->type() != expected)
      return Result<std::size_t>::failure(Error::validation(
          parameter_id.value(), "replacement parameter type does not match the dimension"));
    auto rebound = stored->with_parameter(std::move(parameter_id));
    if (rebound.has_error()) return Result<std::size_t>::failure(rebound.error());
    return catalog.replace(std::move(rebound.value()));
  }
};

} // namespace blcad
