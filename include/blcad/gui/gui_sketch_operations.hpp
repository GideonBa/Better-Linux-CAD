#pragma once

#include "blcad/core/sketch_dimension_catalog_system.hpp"
#include "blcad/core/sketch_offset_project.hpp"
#include "blcad/core/sketch_transform_pattern.hpp"
#include "blcad/gui/gui_document_session.hpp"

namespace blcad::gui {

enum class GuiSketchOperationKind {
  Move,
  Rotate,
  Scale,
  Copy,
  Mirror,
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
  std::optional<ProjectedSketchPoint> projected_point;
  std::optional<ProjectedSketchLine> projected_line;
};

struct GuiSketchOperationPreview {
  Sketch candidate;
  std::vector<SketchEntityId> created_entities;
  std::vector<std::string> uncopied_references;
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
          return document.update_sketch(std::move(result.value().candidate));
        });
  }

private:
  [[nodiscard]] static Result<GuiSketchOperationPreview> fail(std::string id, std::string message) {
    return Result<GuiSketchOperationPreview>::failure(
        Error::validation(std::move(id), std::move(message)));
  }

  [[nodiscard]] static Result<GuiSketchOperationPreview>
  evaluate(const PartDocument& document, SketchId id, const GuiSketchOperationRequest& r,
           const SketchConstraintCatalog& constraints, const SketchDimensionCatalog& dimensions) {
    const auto* source = document.find_sketch(id);
    if (!source)
      return fail(id.value(), "operation target Sketch no longer exists");
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
          return S::offset_chain(*source, r.entities, r.value);
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
        {std::move(transformed.value().sketch), std::move(transformed.value().created_entities),
         std::move(report)});
  }
};

} // namespace blcad::gui
