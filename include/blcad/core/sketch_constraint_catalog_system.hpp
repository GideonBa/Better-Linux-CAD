#pragma once

#include "blcad/core/sketch_constraint_authoring.hpp"

#include <string>
#include <utility>
#include <vector>

namespace blcad {

class SketchConstraintCatalogSystemBuilder {
public:
  [[nodiscard]] static Result<SketchConstraintSystem>
  build(const PartDocument& document, const SketchTopology& topology,
        const SketchConstraintCatalog& catalog) {
    if (document.id() != catalog.document() || topology.sketch() != catalog.sketch())
      return Result<SketchConstraintSystem>::failure(Error::validation(
          catalog.sketch().value(), "constraint catalog solve scope does not match Part topology"));
    const Sketch* sketch = document.find_sketch(catalog.sketch());
    if (sketch == nullptr)
      return Result<SketchConstraintSystem>::failure(Error::validation(
          catalog.sketch().value(), "constraint catalog solve Sketch does not exist"));
    auto legacy = SketchConstraintSystemBuilder::from_legacy(topology, *sketch, document);
    if (legacy.has_error())
      return Result<SketchConstraintSystem>::failure(legacy.error());
    std::vector<SketchSolverConstraint> constraints = legacy.value().constraints();
    for (const auto& intent : catalog.constraints()) {
      auto converted = solver_constraint(intent, topology, document, *sketch);
      if (converted.has_error())
        return Result<SketchConstraintSystem>::failure(converted.error());
      constraints.push_back(std::move(converted.value()));
    }
    return SketchConstraintSystem::create(catalog.sketch(), std::move(constraints));
  }

private:
  [[nodiscard]] static Result<SketchSolverConstraint>
  solver_constraint(const SketchConstraintIntent& intent, const SketchTopology& topology,
                    const PartDocument& document, const Sketch& sketch) {
    std::vector<SketchSolverTarget> targets;
    targets.reserve(intent.targets().size());
    for (const auto& target : intent.targets()) {
      if (target.kind() == SketchConstraintIntentTargetKind::Point) {
        if (topology.find_point(SketchPointId(target.id())) == nullptr)
          return Result<SketchSolverConstraint>::failure(Error::validation(
              intent.id().value(), "constraint intent references an unknown Sketch point"));
        auto converted = SketchSolverTarget::point(SketchPointId(target.id()));
        if (converted.has_error())
          return Result<SketchSolverConstraint>::failure(converted.error());
        targets.push_back(std::move(converted.value()));
      } else {
        if (topology.find_entity(target.id()) == nullptr)
          return Result<SketchSolverConstraint>::failure(Error::validation(
              intent.id().value(), "constraint intent references an unknown Sketch entity"));
        auto converted = SketchSolverTarget::entity(target.id());
        if (converted.has_error())
          return Result<SketchSolverConstraint>::failure(converted.error());
        targets.push_back(std::move(converted.value()));
      }
    }
    return SketchSolverConstraint::create("intent/" + intent.id().value(), intent.kind(),
                                          std::move(targets),
                                          tangent_circle_radius_for_intent(intent, topology, document, sketch));
  }
};

} // namespace blcad
