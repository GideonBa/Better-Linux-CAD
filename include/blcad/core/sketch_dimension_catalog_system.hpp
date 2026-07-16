#pragma once

#include "blcad/core/sketch_constraint_catalog_system.hpp"
#include "blcad/core/sketch_dimension_authoring.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad {

struct SketchDimensionCatalogSolve {
  SketchSolveResult solve;
  std::optional<Sketch> solved_sketch;
  bool accepted{false};
  std::vector<std::string> conflict_ids;
  std::vector<std::string> redundant_ids;
};

class SketchDimensionCatalogSystemBuilder {
public:
  [[nodiscard]] static Result<SketchConstraintSystem>
  build(const PartDocument& document, const SketchTopology& topology,
        const SketchConstraintCatalog& constraints,
        const SketchDimensionCatalog& dimensions) {
    return build(document, topology, topology, constraints, dimensions);
  }

  [[nodiscard]] static Result<SketchConstraintSystem>
  build(const PartDocument& document, const SketchTopology& solve_topology,
        const SketchTopology& dimension_value_topology,
        const SketchConstraintCatalog& constraints,
        const SketchDimensionCatalog& dimensions) {
    if (document.id() != constraints.document() ||
        document.id() != dimensions.document() ||
        solve_topology.sketch() != constraints.sketch() ||
        solve_topology.sketch() != dimensions.sketch() ||
        dimension_value_topology.sketch() != dimensions.sketch())
      return Result<SketchConstraintSystem>::failure(Error::validation(
          dimensions.sketch().value(),
          "dimension catalog solve scope does not match Part topology or constraints"));
    auto base = SketchConstraintCatalogSystemBuilder::build(document, solve_topology, constraints);
    if (base.has_error()) return Result<SketchConstraintSystem>::failure(base.error());
    std::vector<SketchSolverConstraint> authored = base.value().constraints();
    for (const auto& dimension : dimensions.dimensions()) {
      auto converted = SketchDimensionSolverAdapter::to_solver_constraint(
          document, dimension_value_topology, dimension);
      if (converted.has_error())
        return Result<SketchConstraintSystem>::failure(converted.error());
      if (converted.value()) authored.push_back(std::move(*converted.value()));
    }
    return SketchConstraintSystem::create(dimensions.sketch(), std::move(authored));
  }

  [[nodiscard]] static Result<std::size_t>
  validate_measurements(const PartDocument& document, const SketchTopology& topology,
                        const SketchDimensionCatalog& dimensions,
                        double relative_tolerance = 1.0e-7) {
    for (const auto& dimension : dimensions.dimensions()) {
      if (!dimension.driving()) continue;
      auto measured = SketchDimensionMeasurementEvaluator::measure(topology, dimension);
      if (measured.has_error()) return Result<std::size_t>::failure(measured.error());
      auto expected = SketchDimensionSolverAdapter::parameter_value(document, dimension);
      if (expected.has_error()) return Result<std::size_t>::failure(expected.error());
      const double actual = dimension.kind() == SketchDimensionKind::Angle
                                ? measured.value().value.degrees()
                                : measured.value().value.millimeters();
      const double scale = std::max({1.0, std::abs(actual), std::abs(expected.value())});
      if (!std::isfinite(actual) ||
          std::abs(actual - expected.value()) > relative_tolerance * scale)
        return Result<std::size_t>::failure(Error::validation(
            dimension.id().value(), "solved Sketch does not satisfy its driving dimension value"));
    }
    return Result<std::size_t>::success(dimensions.dimensions().size());
  }

  [[nodiscard]] static Result<SketchDimensionCatalogSolve>
  solve(const PartDocument& document, const SketchConstraintCatalog& constraints,
        const SketchDimensionCatalog& dimensions) {
    if (document.id() != constraints.document() ||
        document.id() != dimensions.document() ||
        constraints.sketch() != dimensions.sketch())
      return Result<SketchDimensionCatalogSolve>::failure(Error::validation(
          dimensions.sketch().value(),
          "dimension solve catalogs do not share one Part/Sketch scope"));
    const Sketch* sketch = document.find_sketch(dimensions.sketch());
    if (sketch == nullptr)
      return Result<SketchDimensionCatalogSolve>::failure(Error::validation(
          dimensions.sketch().value(), "dimension solve target Sketch does not exist"));
    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<SketchDimensionCatalogSolve>::failure(topology.error());

    SketchTopology calibration = topology.value();
    std::optional<SketchSolveResult> final_solve;
    bool measurements_valid = false;
    constexpr std::size_t kMaximumCalibrationPasses = 10U;
    for (std::size_t pass = 0U; pass < kMaximumCalibrationPasses; ++pass) {
      auto system = build(document, topology.value(), calibration, constraints, dimensions);
      if (system.has_error())
        return Result<SketchDimensionCatalogSolve>::failure(system.error());
      auto solved = SketchConstraintSolver{}.solve(topology.value(), system.value());
      if (solved.has_error())
        return Result<SketchDimensionCatalogSolve>::failure(solved.error());
      final_solve = std::move(solved.value());
      const bool converged = accepted_status(final_solve->status);
      if (!converged) break;
      auto valid = validate_measurements(document, final_solve->topology, dimensions);
      if (!valid.has_error()) {
        measurements_valid = true;
        break;
      }
      calibration = final_solve->topology;
    }
    if (!final_solve)
      return Result<SketchDimensionCatalogSolve>::failure(Error::internal(
          dimensions.sketch().value(), "dimension solve produced no solver result"));

    const bool accepted = accepted_status(final_solve->status) && measurements_valid;
    std::optional<Sketch> solved_sketch;
    if (accepted) {
      auto materialized = SketchTopologyLegacyMaterializer{}.materialize(
          *sketch, final_solve->topology);
      if (materialized.has_error())
        return Result<SketchDimensionCatalogSolve>::failure(materialized.error());
      auto represented = SketchTopology::migrate_legacy(materialized.value());
      if (represented.has_error())
        return Result<SketchDimensionCatalogSolve>::failure(represented.error());
      if (represented.value() != final_solve->topology)
        return Result<SketchDimensionCatalogSolve>::failure(Error::validation(
            dimensions.sketch().value(),
            "solved dimensions cannot be represented by historical Sketch JSON without identity loss"));
      solved_sketch = std::move(materialized.value());
    }

    auto conflicts = strip_prefixes(final_solve->conflicting_constraint_ids);
    auto redundant = strip_prefixes(final_solve->redundant_constraint_ids);
    if (accepted_status(final_solve->status) && !measurements_valid) {
      for (const auto& dimension : dimensions.dimensions())
        if (dimension.driving()) conflicts.push_back(dimension.id().value());
      canonicalize(conflicts);
    }
    return Result<SketchDimensionCatalogSolve>::success(
        {std::move(*final_solve), std::move(solved_sketch), accepted,
         std::move(conflicts), std::move(redundant)});
  }

private:
  [[nodiscard]] static bool accepted_status(SketchSolveStatus status) noexcept {
    return status == SketchSolveStatus::UnderConstrained ||
           status == SketchSolveStatus::FullyConstrained;
  }

  static void canonicalize(std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
  }

  [[nodiscard]] static std::vector<std::string>
  strip_prefixes(const std::vector<std::string>& ids) {
    std::vector<std::string> result;
    result.reserve(ids.size());
    for (const auto& id : ids) {
      constexpr std::string_view intent_prefix = "intent/";
      constexpr std::string_view dimension_prefix = "dimension/";
      if (std::string_view(id).starts_with(intent_prefix))
        result.push_back(id.substr(intent_prefix.size()));
      else if (std::string_view(id).starts_with(dimension_prefix)) {
        std::string value = id.substr(dimension_prefix.size());
        const auto suffix = value.find('/');
        result.push_back(suffix == std::string::npos ? std::move(value)
                                                      : value.substr(0U, suffix));
      } else
        result.push_back(id);
    }
    canonicalize(result);
    return result;
  }
};

struct SketchDimensionAuthoringPreview {
  SketchDimensionIntent candidate;
  SketchDimensionCatalog catalog_after;
  SketchSolveResult solve;
  std::optional<Sketch> solved_sketch;
  std::optional<SketchDimensionMeasurement> measurement;
  bool accepted{false};
  std::vector<std::string> conflict_ids;
  std::vector<std::string> redundant_ids;
};

class SketchDimensionAuthoringService {
public:
  [[nodiscard]] Result<SketchDimensionAuthoringPreview>
  preview(const PartDocument& document, const SketchConstraintCatalog& constraints,
          const SketchDimensionCatalog& dimensions,
          const SketchDimensionIntent& candidate) const {
    if (dimensions.find(candidate.id()) != nullptr)
      return Result<SketchDimensionAuthoringPreview>::failure(Error::validation(
          candidate.id().value(), "dimension candidate id already exists"));
    SketchDimensionCatalog after = dimensions;
    auto added = after.add(candidate);
    if (added.has_error())
      return Result<SketchDimensionAuthoringPreview>::failure(added.error());

    auto solved = SketchDimensionCatalogSystemBuilder::solve(document, constraints, after);
    if (solved.has_error())
      return Result<SketchDimensionAuthoringPreview>::failure(solved.error());
    std::optional<SketchDimensionMeasurement> measurement;
    SketchDimensionCatalog published_catalog = dimensions;
    if (solved.value().accepted) {
      auto measured = SketchDimensionMeasurementEvaluator::measure(
          solved.value().solve.topology, candidate);
      if (measured.has_error())
        return Result<SketchDimensionAuthoringPreview>::failure(measured.error());
      measurement = std::move(measured.value());
      published_catalog = std::move(after);
    }
    return Result<SketchDimensionAuthoringPreview>::success(
        {candidate, std::move(published_catalog), std::move(solved.value().solve),
         std::move(solved.value().solved_sketch), std::move(measurement),
         solved.value().accepted, std::move(solved.value().conflict_ids),
         std::move(solved.value().redundant_ids)});
  }
};

} // namespace blcad
