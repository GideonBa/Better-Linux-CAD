#pragma once

#include "blcad/core/sketch_dimension_catalog_system.hpp"
#include "blcad/core/sketch_offset_project.hpp"
#include "blcad/core/sketch_transform_pattern.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include <limits>

namespace blcad::gui {

enum class GuiSketchOperationKind {
  Move,
  Rotate,
  Scale,
  Copy,
  Mirror,
  Symmetric,
  ProjectGeometry,
  RectangularPattern,
  CircularPattern,
  Offset,
  ProjectPoint,
  ProjectLine,
  BreakLinkLine
};

struct GuiSketchOperationRequest {
  GuiSketchOperationKind kind{GuiSketchOperationKind::Move};
  std::vector<SketchEntityId> entities;
  Point2 first{};    // translation, center, mirror start, or first pattern step
  Point2 second{};   // mirror end or second pattern step
  double value{0.0}; // angle in radians, scale factor, or signed offset in mm
  std::size_t first_count{1U};
  std::size_t second_count{1U};
  std::optional<SketchEntityId> axis;
  std::optional<SketchId> source_sketch;
  std::optional<SketchEntityId> source_entity;
  // Plane-projected snapshots, derived by the shell from the current source.
  std::vector<LineSegment> projected_curves;
  std::vector<SplineSegment> projected_splines;
  std::optional<ProjectedSketchPoint> projected_point;
  std::optional<ProjectedSketchLine> projected_line;
};

struct GuiSketchOperationPreview {
  Sketch candidate;
  std::vector<SketchEntityId> created_entities;
  std::vector<std::string> uncopied_references;
  std::optional<SketchConstraintCatalog> constraints_after;
};

// One authority shared by numeric previews and Apply. No preview mutates the
// session. Apply derives a fresh candidate inside the document transaction.
class GuiSketchOperationController {
public:
  [[nodiscard]] static std::string label(GuiSketchOperationKind kind) {
    switch (kind) {
    case GuiSketchOperationKind::Move:
      return "Move sketch geometry";
    case GuiSketchOperationKind::Rotate:
      return "Rotate sketch geometry";
    case GuiSketchOperationKind::Scale:
      return "Scale sketch geometry";
    case GuiSketchOperationKind::Copy:
      return "Copy sketch geometry";
    case GuiSketchOperationKind::Symmetric:
      return "Constrain sketch symmetry";
    case GuiSketchOperationKind::ProjectGeometry:
      return "Project sketch geometry";
    case GuiSketchOperationKind::Mirror:
      return "Mirror sketch geometry";
    case GuiSketchOperationKind::RectangularPattern:
      return "Create rectangular sketch pattern";
    case GuiSketchOperationKind::CircularPattern:
      return "Create circular sketch pattern";
    case GuiSketchOperationKind::Offset:
      return "Offset sketch geometry";
    case GuiSketchOperationKind::ProjectPoint:
      return "Project point into sketch";
    case GuiSketchOperationKind::ProjectLine:
      return "Project line into sketch";
    case GuiSketchOperationKind::BreakLinkLine:
      return "Detach projected sketch line";
    }
    return "Edit sketch geometry";
  }

  [[nodiscard]] static Result<GuiSketchOperationPreview>
  preview(const GuiDocumentSession& session, SketchId id,
          const GuiSketchOperationRequest& request) {
    if (!session.part_document())
      return fail(id.value(), "no active Part document");
    auto constraints = session.sketch_constraint_catalog(id);
    if (!constraints)
      return Result<GuiSketchOperationPreview>::failure(constraints.error());
    auto dimensions = session.sketch_dimension_catalog(id);
    if (!dimensions)
      return Result<GuiSketchOperationPreview>::failure(dimensions.error());
    return evaluate(*session.part_document(), id, request, constraints.value(), dimensions.value());
  }

  [[nodiscard]] static Result<std::size_t> commit(GuiDocumentSession& session, SketchId id,
                                                  GuiSketchOperationRequest request) {
    return session.commit_part_sketch_intent_transaction(
        label(request.kind),
        [id, request = std::move(request)](
            PartDocument& document, std::vector<SketchConstraintCatalog>& constraints,
            std::vector<SketchDimensionCatalog>& dimensions) -> Result<std::size_t> {
          auto cc = SketchConstraintCatalog::create(document.id(), id, {});
          auto dc = SketchDimensionCatalog::create(document.id(), id, {});
          if (!cc)
            return Result<std::size_t>::failure(cc.error());
          if (!dc)
            return Result<std::size_t>::failure(dc.error());
          for (const auto& c : constraints)
            if (c.sketch() == id)
              cc = Result<SketchConstraintCatalog>::success(c);
          for (const auto& d : dimensions)
            if (d.sketch() == id)
              dc = Result<SketchDimensionCatalog>::success(d);
          auto result = evaluate(document, id, request, cc.value(), dc.value());
          if (!result)
            return Result<std::size_t>::failure(result.error());
          if (result.value().constraints_after) {
            auto found = std::find_if(constraints.begin(), constraints.end(),
                                      [&](const auto& c) { return c.sketch() == id; });
            if (found == constraints.end())
              constraints.push_back(*result.value().constraints_after);
            else
              *found = *result.value().constraints_after;
          }
          return document.update_sketch(std::move(result.value().candidate));
        });
  }

private:
  [[nodiscard]] static Result<GuiSketchOperationPreview> fail(std::string id, std::string message) {
    return Result<GuiSketchOperationPreview>::failure(
        Error::validation(std::move(id), std::move(message)));
  }

  [[nodiscard]] static Result<GuiSketchOperationPreview>
  symmetry(const PartDocument& document, const Sketch& source, const GuiSketchOperationRequest& r,
           const SketchConstraintCatalog& constraints, const SketchDimensionCatalog& dimensions) {
    if (!r.axis || r.entities.empty())
      return fail(source.id().value(), "Symmetrieachse und Elemente auswählen");
    const auto* axis = source.find_line_segment(*r.axis);
    if (!axis)
      return fail(r.axis->value(), "Symmetrieachse muss eine Skizzenlinie sein");
    auto topology = SketchTopology::migrate_legacy(source);
    if (!topology)
      return Result<GuiSketchOperationPreview>::failure(topology.error());
    std::vector<const SketchTopologyEntity*> entities;
    for (const auto& id : r.entities) {
      const auto* e = topology.value().find_entity("entity/" + id.value());
      if (!e || id == *r.axis ||
          (e->kind() != SketchTopologyEntityKind::Line &&
           e->kind() != SketchTopologyEntityKind::Arc &&
           e->kind() != SketchTopologyEntityKind::Spline))
        return fail(id.value(), "Bearbeitbare Linien, Bögen oder Splines auswählen");
      if (std::find(entities.begin(), entities.end(), e) == entities.end())
        entities.push_back(e);
    }
    if (entities.size() != 1U && entities.size() % 2U != 0U)
      return fail(source.id().value(),
                  "Eine einzelne Linie oder eine gerade Anzahl von Kurven auswählen");
    auto authored = constraints.constraints();
    std::vector<std::pair<SketchPointId, SketchPointId>> pairs;
    const auto append = [&](SketchPointId a, SketchPointId b) {
      if (b.value() < a.value())
        std::swap(a, b);
      if (std::find(pairs.begin(), pairs.end(), std::pair{a, b}) == pairs.end())
        pairs.emplace_back(a, b);
    };
    if (entities.size() == 1U) {
      if (entities.front()->kind() != SketchTopologyEntityKind::Line)
        return fail(source.id().value(), "Einzelsymmetrie benötigt eine Linie");
      append(entities.front()->points().front(), entities.front()->points().back());
    } else {
      const Point2 d{axis->end().x - axis->start().x, axis->end().y - axis->start().y};
      const double n = d.x * d.x + d.y * d.y;
      const auto reflected = [&](Point2 p) {
        const double t = ((p.x - axis->start().x) * d.x + (p.y - axis->start().y) * d.y) / n;
        return Point2{2 * (axis->start().x + t * d.x) - p.x, 2 * (axis->start().y + t * d.y) - p.y};
      };
      const auto position = [&](SketchPointId p) {
        return topology.value().find_point(p)->position();
      };
      const auto cost = [&](const auto* a, const auto* b, bool reverse) {
        double sum = 0;
        for (std::size_t k = 0; k < a->points().size(); ++k) {
          const auto p = reflected(position(a->points()[k]));
          const auto q = position(b->points()[reverse ? b->points().size() - 1 - k : k]);
          sum += (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
        }
        return sum;
      };
      // Pair each curve with its nearest reflected counterpart, independent of click order.
      while (!entities.empty()) {
        const auto* a = entities.front();
        std::size_t best = 0;
        double best_cost = std::numeric_limits<double>::infinity();
        bool reverse = false;
        for (std::size_t j = 1; j < entities.size(); ++j) {
          if (entities[j]->kind() != a->kind() || entities[j]->weights() != a->weights())
            continue;
          for (bool rev : {false, true}) {
            const auto value = cost(a, entities[j], rev);
            if (value < best_cost) {
              best_cost = value;
              best = j;
              reverse = rev;
            }
          }
        }
        if (best == 0)
          return fail(a->id(), "Passende Kurvenpaare gleichen Typs auswählen");
        const auto* b = entities[best];
        for (std::size_t k = 0; k < a->points().size(); ++k)
          append(a->points()[k], b->points()[reverse ? b->points().size() - 1 - k : k]);
        entities.erase(entities.begin() + best);
        entities.erase(entities.begin());
      }
    }
    for (const auto& [a, b] : pairs) {
      const auto axis_target =
          SketchConstraintIntentTarget::entity("entity/" + r.axis->value()).value();
      std::vector<SketchConstraintIntentTarget> targets{
          SketchConstraintIntentTarget::point(a).value()};
      auto kind = SketchSolverConstraintKind::PointOnObject;
      if (a != b) {
        kind = SketchSolverConstraintKind::Symmetric;
        targets.push_back(SketchConstraintIntentTarget::point(b).value());
      }
      targets.push_back(axis_target);
      if (std::any_of(authored.begin(), authored.end(),
                      [&](const auto& c) { return c.kind() == kind && c.targets() == targets; }))
        continue;
      std::size_t number = 1;
      while (std::any_of(authored.begin(), authored.end(), [&](const auto& c) {
        return c.id().value() == "symmetry." + std::to_string(number);
      }))
        ++number;
      auto intent = SketchConstraintIntent::create(
          SketchConstraintId("symmetry." + std::to_string(number)), kind, std::move(targets));
      if (!intent)
        return Result<GuiSketchOperationPreview>::failure(intent.error());
      authored.push_back(std::move(intent.value()));
    }
    auto after = SketchConstraintCatalog::create(document.id(), source.id(), std::move(authored));
    if (!after)
      return Result<GuiSketchOperationPreview>::failure(after.error());
    auto system = SketchDimensionCatalogSystemBuilder::build(document, topology.value(),
                                                             after.value(), dimensions);
    if (!system)
      return Result<GuiSketchOperationPreview>::failure(system.error());
    auto equations = system.value().constraints();
    // The chosen axis stays where the user placed it during this solve.
    auto fixed = SketchSolverConstraint::create(
        "symmetry.axis", SketchSolverConstraintKind::Fixed,
        {SketchSolverTarget::entity("entity/" + r.axis->value()).value()});
    if (!fixed)
      return Result<GuiSketchOperationPreview>::failure(fixed.error());
    equations.push_back(std::move(fixed.value()));
    auto combined = SketchConstraintSystem::create(source.id(), std::move(equations));
    if (!combined)
      return Result<GuiSketchOperationPreview>::failure(combined.error());
    auto solve = SketchConstraintSolver{}.solve(topology.value(), combined.value());
    if (!solve)
      return Result<GuiSketchOperationPreview>::failure(solve.error());
    if (solve.value().status == SketchSolveStatus::InvalidReference ||
        solve.value().status == SketchSolveStatus::Conflicting ||
        solve.value().status == SketchSolveStatus::NonConvergent ||
        !std::isfinite(solve.value().residuals.final_rms) ||
        solve.value().residuals.final_rms > 1e-8)
      return fail(source.id().value(),
                  "Symmetrie widerspricht vorhandenen Abhängigkeiten oder Bemaßungen");
    auto materialized =
        SketchTopologyLegacyMaterializer{}.materialize(source, solve.value().topology);
    if (!materialized)
      return Result<GuiSketchOperationPreview>::failure(materialized.error());
    return Result<GuiSketchOperationPreview>::success(
        {std::move(materialized.value()), {}, {}, after.value()});
  }

  [[nodiscard]] static Result<GuiSketchOperationPreview>
  evaluate(const PartDocument& document, SketchId id, const GuiSketchOperationRequest& r,
           const SketchConstraintCatalog& constraints, const SketchDimensionCatalog& dimensions) {
    const auto* source = document.find_sketch(id);
    if (!source)
      return fail(id.value(), "operation target Sketch no longer exists");
    if (r.kind == GuiSketchOperationKind::Symmetric)
      return symmetry(document, *source, r, constraints, dimensions);
    if (r.kind == GuiSketchOperationKind::ProjectGeometry) {
      if (r.projected_curves.empty() && r.projected_splines.empty())
        return fail(id.value(), "Keine projizierbare Skizzengeometrie ausgewählt");
      Sketch candidate = *source;
      std::vector<SketchEntityId> created;
      for (const auto& curve : r.projected_curves) {
        auto added = candidate.add_entity(curve);
        if (!added)
          return Result<GuiSketchOperationPreview>::failure(added.error());
        created.push_back(curve.id());
      }
      for (const auto& curve : r.projected_splines) {
        auto added = candidate.add_entity(curve);
        if (!added)
          return Result<GuiSketchOperationPreview>::failure(added.error());
        created.push_back(curve.id());
      }
      return Result<GuiSketchOperationPreview>::success(
          {std::move(candidate), std::move(created), {}, {}});
    }
    auto transformed = [&]() -> Result<SketchTransformPatternResult> {
      using S = SketchTransformPatternService;
      switch (r.kind) {
      case GuiSketchOperationKind::Move:
        return S::move(*source, r.entities, r.first);
      case GuiSketchOperationKind::Rotate:
        return S::rotate(*source, r.entities, r.first, r.value);
      case GuiSketchOperationKind::Scale:
        return S::scale(*source, r.entities, r.first, r.value);
      case GuiSketchOperationKind::Copy:
        return S::copy(*source, r.entities, r.first);
      case GuiSketchOperationKind::Mirror:
        return S::mirror(*source, r.entities, r.first, r.second);
      case GuiSketchOperationKind::RectangularPattern:
        return S::rectangular_pattern(*source, r.entities, r.first, r.first_count, r.second,
                                      r.second_count, SketchPatternMode::Exploded);
      case GuiSketchOperationKind::CircularPattern:
        return S::circular_pattern(*source, r.entities, r.first, r.first_count, r.value,
                                   SketchPatternMode::Exploded);
      default:
        break;
      }
      auto offset = [&]() -> Result<SketchOffsetProjectResult> {
        using S = SketchOffsetProjectService;
        if (r.kind == GuiSketchOperationKind::Offset)
          return S::offset_selection(*source, r.entities, r.value);
        if (r.kind == GuiSketchOperationKind::ProjectPoint && r.projected_point)
          return S::project_point(*source, *r.projected_point);
        if (r.kind == GuiSketchOperationKind::ProjectLine && r.projected_line)
          return S::project_line(*source, *r.projected_line);
        if (r.kind == GuiSketchOperationKind::BreakLinkLine && r.entities.size() == 1U)
          return S::break_link_line(*source, r.entities.front(), r.first, r.second);
        return Result<SketchOffsetProjectResult>::failure(
            Error::validation(id.value(), "incomplete sketch operation"));
      }();
      if (!offset)
        return Result<SketchTransformPatternResult>::failure(offset.error());
      return Result<SketchTransformPatternResult>::success(
          {std::move(offset.value().sketch),
           std::move(offset.value().created_entities),
           std::nullopt,
           {}});
    }();
    if (!transformed)
      return Result<GuiSketchOperationPreview>::failure(transformed.error());

    const bool replaces = r.kind == GuiSketchOperationKind::Move ||
                          r.kind == GuiSketchOperationKind::Rotate ||
                          r.kind == GuiSketchOperationKind::Scale;
    auto before = SketchTopology::migrate_legacy(*source);
    if (!before)
      return Result<GuiSketchOperationPreview>::failure(before.error());
    if (replaces) {
      auto after = SketchTopology::migrate_legacy(transformed.value().sketch);
      if (!after)
        return Result<GuiSketchOperationPreview>::failure(after.error());
      auto system = SketchDimensionCatalogSystemBuilder::build(document, after.value(), constraints,
                                                               dimensions);
      if (!system)
        return Result<GuiSketchOperationPreview>::failure(system.error());
      // Fixed constraints bind to the pre-command coordinates. Evaluating only
      // the candidate would incorrectly redefine their fixed position.
      for (const auto& constraint : system.value().constraints()) {
        if (constraint.kind() != SketchSolverConstraintKind::Fixed)
          continue;
        for (const auto& target : constraint.targets()) {
          std::vector<SketchPointId> points;
          if (target.kind() == SketchSolverTargetKind::Point)
            points.emplace_back(target.id());
          else if (const auto* entity = before.value().find_entity(target.id()))
            points = entity->points();
          for (const auto& point : points) {
            const auto* a = before.value().find_point(point);
            const auto* b = after.value().find_point(point);
            if (!a || !b ||
                std::hypot(a->position().x - b->position().x, a->position().y - b->position().y) >
                    1.0e-7)
              return fail(constraint.id(), "transformation would move fixed geometry");
          }
        }
      }
      // Validate the requested transform itself. Do not silently let the solver
      // undo a rotation/scale or move unrelated geometry to make it acceptable.
      SketchSolverOptions options;
      options.maximum_iterations = 1U;
      auto solve = SketchConstraintSolver{}.solve(after.value(), system.value(), options);
      if (!solve)
        return Result<GuiSketchOperationPreview>::failure(solve.error());
      if (solve.value().status == SketchSolveStatus::InvalidReference ||
          !std::isfinite(solve.value().residuals.initial_rms) ||
          solve.value().residuals.initial_rms > 1.0e-8)
        return fail(id.value(), "transformation conflicts with existing constraints or dimensions");
      auto measured = SketchDimensionCatalogSystemBuilder::validate_measurements(
          document, after.value(), dimensions);
      if (!measured)
        return Result<GuiSketchOperationPreview>::failure(measured.error());
    }
    const bool duplicates = r.kind == GuiSketchOperationKind::Copy ||
                            r.kind == GuiSketchOperationKind::Mirror ||
                            r.kind == GuiSketchOperationKind::RectangularPattern ||
                            r.kind == GuiSketchOperationKind::CircularPattern;
    auto& report = transformed.value().uncopied_references;
    if (duplicates) {
      const auto touches = [&](const auto& target) {
        for (const auto& selected : r.entities) {
          const auto* entity = before.value().find_entity("entity/" + selected.value());
          if (!entity)
            continue;
          if (target.kind() == SketchConstraintIntentTargetKind::Entity &&
              target.id() == entity->id())
            return true;
          for (const auto& point : entity->points())
            if (target.id() == point.value())
              return true;
        }
        return false;
      };
      for (const auto& c : constraints.constraints())
        if (std::any_of(c.targets().begin(), c.targets().end(), touches))
          report.push_back(c.id().value());
      for (const auto& d : dimensions.dimensions())
        if (std::any_of(d.targets().begin(), d.targets().end(), touches))
          report.push_back(d.id().value());
      std::sort(report.begin(), report.end());
      report.erase(std::unique(report.begin(), report.end()), report.end());
    }
    return Result<GuiSketchOperationPreview>::success(
        {std::move(transformed.value().sketch),
         std::move(transformed.value().created_entities),
         std::move(report),
         {}});
  }
};

} // namespace blcad::gui
