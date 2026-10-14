#pragma once

#include "blcad/core/sketch_dimension_catalog_system.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace blcad::gui {

// Finish the corner authoring candidate inside the same document transaction as
// the curve rewrite. Qt never publishes intermediate geometry or orphan sizes.
[[nodiscard]] inline Result<std::size_t>
author_sketch_corner(PartDocument& document, const Sketch& before,
                     const SketchEntityId& first, const SketchEntityId& second, bool fillet,
                     std::vector<SketchConstraintCatalog>& constraints,
                     std::vector<SketchDimensionCatalog>& dimensions) {
  const SketchId sketch = before.id();
  const auto fail = [&](std::string message) {
    return Result<std::size_t>::failure(Error::validation(sketch.value(), std::move(message)));
  };
  const Sketch* source = document.find_sketch(sketch);
  if (!source) return fail("corner Sketch does not exist");
  std::optional<SketchEntityId> connector;
  if (fillet) {
    for (const auto& arc : source->arc_segments())
      if (!before.find_arc_segment(arc.id())) connector = arc.id();
  } else {
    for (const auto& line : source->line_segments())
      if (!before.find_line_segment(line.id())) connector = line.id();
  }
  if (!connector) return fail("corner connector was not created");
  auto topology = SketchTopology::migrate_legacy(*source);
  if (!topology) return Result<std::size_t>::failure(topology.error());
  auto constraint_entry = std::find_if(constraints.begin(), constraints.end(),
      [&](const auto& catalog) { return catalog.sketch() == sketch; });
  auto dimension_entry = std::find_if(dimensions.begin(), dimensions.end(),
      [&](const auto& catalog) { return catalog.sketch() == sketch; });
  auto catalog = constraint_entry == constraints.end()
      ? SketchConstraintCatalog::create(document.id(), sketch).value() : *constraint_entry;
  auto dimension_catalog = dimension_entry == dimensions.end()
      ? SketchDimensionCatalog::create(document.id(), sketch).value() : *dimension_entry;

  std::vector<SketchConstraintId> broken;
  for (const auto& intent : catalog.constraints()) {
    if (intent.kind() != SketchSolverConstraintKind::Coincident || intent.targets().size() != 2U)
      continue;
    const auto* a = topology.value().find_point(SketchPointId(intent.targets()[0].id()));
    const auto* b = topology.value().find_point(SketchPointId(intent.targets()[1].id()));
    if (!a || !b || std::hypot(a->position().x - b->position().x,
                              a->position().y - b->position().y) > 1.0e-5)
      broken.push_back(intent.id());
  }
  for (const auto& id : broken) {
    auto removed = catalog.remove(id);
    if (!removed) return removed;
  }
  const auto author = [&](SketchSolverConstraintKind kind,
                           std::vector<SketchConstraintIntentTarget> targets) -> Result<std::size_t> {
    if (kind == SketchSolverConstraintKind::Coincident && targets[0] == targets[1])
      return Result<std::size_t>::success(0U); // profile topology already shares the point
    std::size_t number = 1U;
    while (catalog.find(SketchConstraintId("constraint.corner." + std::to_string(number)))) ++number;
    auto intent = SketchConstraintIntent::create(
        SketchConstraintId("constraint.corner." + std::to_string(number)), kind,
        std::move(targets), SketchConstraintIntentSource::Automatic);
    if (!intent) return Result<std::size_t>::failure(intent.error());
    auto candidate = catalog;
    auto added = candidate.add(intent.value());
    if (!added) return added;
    auto solved = SketchDimensionCatalogSystemBuilder::solve(document, candidate, dimension_catalog);
    if (!solved) return Result<std::size_t>::failure(solved.error());
    if (!solved.value().accepted || !solved.value().solved_sketch) {
      if (solved.value().solve.status == SketchSolveStatus::Redundant)
        return Result<std::size_t>::success(0U);
      return fail("corner constraints conflict with the existing Sketch intent");
    }
    auto updated = document.update_sketch(*solved.value().solved_sketch);
    if (!updated) return updated;
    catalog = std::move(candidate);
    return Result<std::size_t>::success(1U);
  };
  const auto* inserted = topology.value().find_entity("entity/" + connector->value());
  if (!inserted) return fail("corner connector has no topology");
  for (const auto& id : {first, second}) {
    const auto* entity = topology.value().find_entity("entity/" + id.value());
    if (!entity) return fail("corner source has no topology");
    bool joined = false;
    for (const auto& a : {entity->points().front(), entity->points().back()})
      for (const auto& b : {inserted->points().front(), inserted->points().back()}) {
        const auto* pa = topology.value().find_point(a);
        const auto* pb = topology.value().find_point(b);
        if (!pa || !pb || std::hypot(pa->position().x - pb->position().x,
                                    pa->position().y - pb->position().y) > 1.0e-6) continue;
        auto result = author(SketchSolverConstraintKind::Coincident,
            {SketchConstraintIntentTarget::point(a).value(),
             SketchConstraintIntentTarget::point(b).value()});
        if (!result) return result;
        joined = true;
      }
    if (!joined) return fail("corner connector is not attached to both source curves");
    if (fillet) {
      auto result = author(SketchSolverConstraintKind::Tangent,
          {SketchConstraintIntentTarget::entity(entity->id()).value(),
           SketchConstraintIntentTarget::entity(inserted->id()).value()});
      if (!result) return result;
    }
  }

  const auto kind = fillet ? SketchDimensionKind::Radius : SketchDimensionKind::Length;
  std::size_t number = 1U;
  while (dimension_catalog.find(SketchDimensionId("dimension.corner." + std::to_string(number)))) ++number;
  const SketchDimensionId dimension_id("dimension.corner." + std::to_string(number));
  const auto target = SketchConstraintIntentTarget::entity(inserted->id()).value();
  auto measured_intent = SketchDimensionIntent::create(dimension_id, kind,
      SketchDimensionMode::Reference, {target});
  if (!measured_intent) return Result<std::size_t>::failure(measured_intent.error());
  auto current_topology = SketchTopology::migrate_legacy(*document.find_sketch(sketch));
  if (!current_topology) return Result<std::size_t>::failure(current_topology.error());
  auto measured = SketchDimensionMeasurementEvaluator::measure(current_topology.value(), measured_intent.value());
  if (!measured) return Result<std::size_t>::failure(measured.error());
  number = 1U;
  while (document.find_parameter(ParameterId("d" + std::to_string(number)))) ++number;
  const ParameterId parameter_id("d" + std::to_string(number));
  auto parameter = Parameter::create_length(parameter_id, parameter_id.value(), measured.value().value);
  if (!parameter) return Result<std::size_t>::failure(parameter.error());
  auto added_parameter = document.add_parameter(parameter.value());
  if (!added_parameter) return added_parameter;
  auto dimension = measured_intent.value().with_parameter(parameter_id);
  if (!dimension) return Result<std::size_t>::failure(dimension.error());
  auto preview = SketchDimensionAuthoringService{}.preview(document, catalog, dimension_catalog, dimension.value());
  if (!preview) return Result<std::size_t>::failure(preview.error());
  if (!preview.value().accepted || !preview.value().solved_sketch)
    return fail("corner size conflicts with existing dimensions");
  auto updated = document.update_sketch(*preview.value().solved_sketch);
  if (!updated) return updated;
  if (constraint_entry == constraints.end()) constraints.push_back(std::move(catalog));
  else *constraint_entry = std::move(catalog);
  if (dimension_entry == dimensions.end()) dimensions.push_back(std::move(preview.value().catalog_after));
  else *dimension_entry = std::move(preview.value().catalog_after);
  return Result<std::size_t>::success(1U);
}

} // namespace blcad::gui
