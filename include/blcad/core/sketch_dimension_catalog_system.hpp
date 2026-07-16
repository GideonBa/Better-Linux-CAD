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
    if (document.id() != dimensions.document() ||
        solve_topology.sketch() != dimensions.sketch() ||
        dimension_value_topology.sketch() != dimensions.sketch())
      return Result<SketchConstraintSystem>::failure(Error::validation(
          dimensions.sketch().value(), "dimension catalog solve scope does not match Part topology"));
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
    if (document.id() != constraints.document() || document.id() != dimensions.document() ||
        constraints.sketch() != dimensions.sketch())
      return Result<SketchDimensionAuthoringPreview>::failure(Error::validation(
          dimensions.sketch().value(), "dimension preview catalogs do not share one Part/Sketch scope"));
    const Sketch* sketch = document.find_sketch(dimensions.sketch());
    if (sketch == nullptr)
      return Result<SketchDimensionAuthoringPreview>::failure(Error::validation(
          dimensions.sketch().value(), "dimension target Sketch does not exist"));
    if (dimensions.find(candidate.id()) != nullptr)
      return Result<SketchDimensionAuthoringPreview>::failure(Error::validation(
          candidate.id().value(), "dimension candidate id already exists"));

    auto topology = SketchTopology::migrate_legacy(*sketch);
    if (topology.has_error())
      return Result<SketchDimensionAuthoringPreview>::failure(topology.error());
    auto after_result = SketchDimensionCatalog::create(
        dimensions.document(), dimensions.sketch(), dimensions.dimensions());
    if (after_result.has_error())
      return Result<SketchDimensionAuthoringPreview>::failure(after_result.error());
    SketchDimensionCatalog after = std::move(after_result.value());
    auto added = after.add(candidate);
    if (added.has_error())
      return Result<SketchDimensionAuthoringPreview>::failure(added.error());

    SketchTopology calibration = topology.value();
    std::optional<SketchSolveResult> final_solve;
    bool measurements_valid = false;
    constexpr std::size_t kMaximumCalibrationPasses = 10U;
    for (std::size_t pass = 0U; pass < kMaximumCalibrationPasses; ++pass) {
      auto system = SketchDimensionCatalogSystemBuilder::build(
          document, topology.value(), calibration, constraints, after);
      if (system.has_error())
        return Result<SketchDimensionAuthoringPreview>::failure(system.error());
      auto solved = SketchConstraintSolver{}.solve(topology.value(), system.value());
      if (solved.has_error())
        return Result<SketchDimensionAuthoringPreview>::failure(solved.error());
      final_solve = std::move(solved.value());
      const bool converged = final_solve->status == SketchSolveStatus::UnderConstrained ||
                             final_solve->status == SketchSolveStatus::FullyConstrained;
      if (!converged) break;
      auto valid = SketchDimensionCatalogSystemBuilder::validate_measurements(
          document, final_solve->topology, after);
      if (!valid.has_error()) {
        measurements_valid = true;
        break;
      }
      calibration = final_solve->topology;
    }
    if (!final_solve)
      return Result<SketchDimensionAuthoringPreview>::failure(Error::internal(
          candidate.id().value(), "dimension preview produced no solver result"));

    const bool solver_accepted =
        final_solve->status == SketchSolveStatus::UnderConstrained ||
        final_solve->status == SketchSolveStatus::FullyConstrained;
    const bool accepted = solver_accepted && measurements_valid;
    std::optional<Sketch> solved_sketch;
    std::optional<SketchDimensionMeasurement> measurement;
    SketchDimensionCatalog published_catalog = dimensions;
    if (accepted) {
      auto materialized = SketchTopologyLegacyMaterializer{}.materialize(
          *sketch, final_solve->topology);
      if (materialized.has_error())
        return Result<SketchDimensionAuthoringPreview>::failure(materialized.error());
      solved_sketch = std::move(materialized.value());
      auto measured = SketchDimensionMeasurementEvaluator::measure(
          final_solve->topology, candidate);
      if (measured.has_error())
        return Result<SketchDimensionAuthoringPreview>::failure(measured.error());
      measurement = std::move(measured.value());
      published_catalog = std::move(after);
    }

    auto conflicts = strip_prefixes(final_solve->conflicting_constraint_ids);
    auto redundant = strip_prefixes(final_solve->redundant_constraint_ids);
    if (solver_accepted && !measurements_valid)
      conflicts.push_back(candidate.id().value());
    return Result<SketchDimensionAuthoringPreview>::success(
        {candidate, std::move(published_catalog), std::move(*final_solve),
         std::move(solved_sketch), std::move(measurement), accepted,
         std::move(conflicts), std::move(redundant)});
  }

private:
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
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }
};

} // namespace blcad
