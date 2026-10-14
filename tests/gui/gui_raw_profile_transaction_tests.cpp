#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <array>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

std::array<SketchEntityId, 4U>
add_line_loop(Sketch& sketch, std::string prefix, Point2 first, Point2 second) {
  std::array<SketchEntityId, 4U> ids{
      SketchEntityId(prefix + ".a"), SketchEntityId(prefix + ".b"),
      SketchEntityId(prefix + ".c"), SketchEntityId(prefix + ".d")};
  REQUIRE(sketch.add_entity(
      LineSegment::create(ids[0], first, {second.x, first.y}).value()));
  REQUIRE(sketch.add_entity(
      LineSegment::create(ids[1], {second.x, first.y}, second).value()));
  REQUIRE(sketch.add_entity(
      LineSegment::create(ids[2], second, {first.x, second.y}).value()));
  REQUIRE(sketch.add_entity(
      LineSegment::create(ids[3], {first.x, second.y}, first).value()));
  return ids;
}

void seed_raw_intent_catalogs(GuiDocumentSession& session, const SketchId& sketch,
                              std::string_view line_prefix) {
  const std::string owner = "entity/" + std::string(line_prefix);
  auto first = SketchConstraintIntentTarget::point(SketchPointId(owner + ".a/end"));
  auto duplicate =
      SketchConstraintIntentTarget::point(SketchPointId(owner + ".b/start"));
  auto upper = SketchConstraintIntentTarget::point(SketchPointId(owner + ".b/end"));
  REQUIRE(first);
  REQUIRE(duplicate);
  REQUIRE(upper);

  auto coincidence = SketchConstraintIntent::create(
      SketchConstraintId("constraint.raw.corner"),
      SketchSolverConstraintKind::Coincident, {first.value(), duplicate.value()});
  REQUIRE(coincidence);
  auto height = SketchDimensionIntent::create(
      SketchDimensionId("dimension.raw.height"),
      SketchDimensionKind::VerticalDistance, SketchDimensionMode::Reference,
      {duplicate.value(), upper.value()});
  REQUIRE(height);

  const DocumentId document = session.part_document()->id();
  auto constraints = SketchConstraintCatalog::create(
      document, sketch, {std::move(coincidence.value())});
  auto dimensions = SketchDimensionCatalog::create(
      document, sketch, {std::move(height.value())});
  REQUIRE(constraints);
  REQUIRE(dimensions);
  REQUIRE(session.commit_part_sketch_intent_transaction(
      "Seed raw Sketch intents",
      [constraints = std::move(constraints.value()),
       dimensions = std::move(dimensions.value())](
          PartDocument&, std::vector<SketchConstraintCatalog>& constraint_catalogs,
          std::vector<SketchDimensionCatalog>& dimension_catalogs) mutable {
        constraint_catalogs.push_back(std::move(constraints));
        dimension_catalogs.push_back(std::move(dimensions));
        return Result<std::size_t>::success(2U);
      }));
}

QTreeWidgetItem* find_tree_item(QTreeWidgetItem* item, const QString& semantic_id) {
  if (item->data(0, Qt::UserRole).toString() == semantic_id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_tree_item(item->child(index), semantic_id))
      return found;
  return nullptr;
}

QTreeWidgetItem* find_tree_item(QTreeWidget& tree, const QString& semantic_id) {
  for (int index = 0; index < tree.topLevelItemCount(); ++index)
    if (auto* found = find_tree_item(tree.topLevelItem(index), semantic_id))
      return found;
  return nullptr;
}

void add_pattern_parameters(PartDocument& part) {
  auto radius = Parameter::create_length(
      ParameterId("pattern.radius"), "Radius",
      Quantity::length_mm(20.0, "pattern.radius").value());
  auto count = Parameter::create_count(
      ParameterId("pattern.count"), "Count",
      Quantity::count(6.0, "pattern.count").value());
  auto diameter = Parameter::create_length(
      ParameterId("pattern.diameter"), "Diameter",
      Quantity::length_mm(4.0, "pattern.diameter").value());
  REQUIRE(radius);
  REQUIRE(count);
  REQUIRE(diameter);
  REQUIRE(part.add_parameter(std::move(radius.value())));
  REQUIRE(part.add_parameter(std::move(count.value())));
  REQUIRE(part.add_parameter(std::move(diameter.value())));
}

} // namespace

TEST_CASE("Raw profile Revolve remaps Sketch intents and undoes as one transaction",
          "[gui][raw-profile-transaction][extrude-revolve][undo]") {
  GuiDocumentSession session;
  GuiSketchWorkbench sketches;
  const SketchId sketch_id("sketch.raw_revolve");
  const ProfileId profile_id("generated.region.sketch.raw_revolve.0");
  const FeatureId feature_id("feature.raw_revolve");
  const BodyId body_id("body.raw_revolve");
  REQUIRE(session.create_part(DocumentId("part.raw_revolve"), "Raw Revolve"));
  REQUIRE(sketches.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));
  REQUIRE(sketches.create_datum_axis(
      session, DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {},
                                          {1.0, 0.0, 0.0})
                   .value()));
  auto sketch = Sketch::create(sketch_id, "Raw Revolve", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line_loop(sketch.value(), "raw", {5.0, 2.0}, {15.0, 8.0});
  REQUIRE(sketches.create_sketch(session, std::move(sketch.value())));
  seed_raw_intent_catalogs(session, sketch_id, "raw");

  const auto constraints_before = session.sketch_constraint_catalog(sketch_id).value();
  const auto dimensions_before = session.sketch_dimension_catalog(sketch_id).value();
  const std::size_t bodies_before = session.part_document()->body_count();
  const std::size_t revolves_before = session.part_document()->revolve_feature_count();
  auto axis = AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis,
                                               DatumAxisId("axis.x"));
  auto body = Body::create(body_id, "Raw Revolve Body", BodyKind::Solid);
  REQUIRE(axis);
  REQUIRE(body);

  GuiInteractiveRevolveController controller;
  REQUIRE(controller.begin_with_new_body(
      session, sketch_id, profile_id, feature_id, "Raw Revolve",
      std::move(axis.value()), std::move(body.value())));
  REQUIRE(controller.preview(session));
  REQUIRE(controller.preview_shape(session));
  CHECK(session.part_document()->find_sketch(sketch_id)->profile_count() == 0U);
  CHECK(session.part_document()->body_count() == bodies_before);
  CHECK(session.part_document()->revolve_feature_count() == revolves_before);
  CHECK(session.sketch_constraint_catalog(sketch_id).value() == constraints_before);
  CHECK(session.sketch_dimension_catalog(sketch_id).value() == dimensions_before);

  REQUIRE(controller.apply(session));
  const auto* applied = session.part_document();
  REQUIRE(applied != nullptr);
  CHECK(applied->find_sketch(sketch_id)->profile_count() == 1U);
  CHECK(applied->find_revolve_feature(feature_id) != nullptr);
  CHECK(applied->find_body(body_id) != nullptr);

  const auto constraints_after = session.sketch_constraint_catalog(sketch_id);
  REQUIRE(constraints_after);
  CHECK(constraints_after.value().constraints().empty());
  const auto dimensions_after = session.sketch_dimension_catalog(sketch_id);
  REQUIRE(dimensions_after);
  REQUIRE(dimensions_after.value().dimensions().size() == 1U);
  const auto& remapped = dimensions_after.value().dimensions().front().targets();
  REQUIRE(remapped.size() == 2U);
  CHECK(remapped[0].id() == "entity/raw.a/end");
  CHECK(remapped[1].id() == "entity/raw.b/end");

  REQUIRE(session.undo());
  const auto* undone = session.part_document();
  REQUIRE(undone != nullptr);
  CHECK(undone->find_sketch(sketch_id)->profile_count() == 0U);
  CHECK(undone->find_revolve_feature(feature_id) == nullptr);
  CHECK(undone->find_body(body_id) == nullptr);
  CHECK(session.sketch_constraint_catalog(sketch_id).value() == constraints_before);
  CHECK(session.sketch_dimension_catalog(sketch_id).value() == dimensions_before);

  REQUIRE(session.redo());
  CHECK(session.part_document()->find_sketch(sketch_id)->profile_count() == 1U);
  CHECK(session.part_document()->find_revolve_feature(feature_id) != nullptr);
  CHECK(session.sketch_constraint_catalog(sketch_id).value().constraints().empty());
}

TEST_CASE("A mixed Sketch raw edge extrudes without duplicating its materialized profile",
          "[gui][raw-profile-transaction][shell-features][mixed-profile][undo]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Mixed Profile")));
  const SketchId sketch_id("sketch.mixed");
  const ProfileId existing_profile("profile.existing");
  REQUIRE(window.session().commit_part_transaction(
      "Seed mixed Sketch", [=](PartDocument& part) -> Result<std::size_t> {
        auto sketch = Sketch::create(sketch_id, "Mixed", DatumPlaneId("datum.xy"));
        if (sketch.has_error())
          return Result<std::size_t>::failure(sketch.error());
        const auto existing = add_line_loop(sketch.value(), "existing", {0.0, 0.0},
                                            {10.0, 10.0});
        auto profile = ClosedProfile::create(
            existing_profile,
            {existing[0], existing[1], existing[2], existing[3]});
        if (profile.has_error())
          return Result<std::size_t>::failure(profile.error());
        if (auto added = sketch.value().add_profile(std::move(profile.value()));
            added.has_error())
          return added;
        add_line_loop(sketch.value(), "raw", {20.0, 0.0}, {30.0, 10.0});
        return part.add_sketch(std::move(sketch.value()));
      }));
  seed_raw_intent_catalogs(window.session(), sketch_id, "raw");
  window.refresh();

  const auto constraints_before =
      window.session().sketch_constraint_catalog(sketch_id).value();
  const auto dimensions_before =
      window.session().sketch_dimension_catalog(sketch_id).value();
  const std::string raw_edge = "sketch/" + sketch_id.value() + "/entity/raw.a";
  REQUIRE(window.viewport().select_semantic(raw_edge));
  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.selected_profile()->first == sketch_id);
  CHECK(tools.selected_profile()->second != existing_profile);
  CHECK(tools.selected_profile()->second.value().starts_with("generated.region."));
  REQUIRE(tools.extrude_action().isEnabled());

  const ProfileId generated_profile = tools.selected_profile()->second;
  REQUIRE(tools.begin_extrude());
  const FeatureId feature_id = tools.coordinator().extrude().feature_id();
  const ParameterId parameter_id =
      tools.coordinator().extrude().distance_parameter_id();
  const BodyId body_id = tools.coordinator().extrude().result_body_id();
  const auto* preview_source = window.session().part_document()->find_sketch(sketch_id);
  REQUIRE(preview_source != nullptr);
  CHECK(preview_source->profile_count() == 1U);
  CHECK(preview_source->find_closed_profile(existing_profile) != nullptr);
  CHECK(preview_source->find_closed_profile(generated_profile) == nullptr);
  CHECK(window.session().sketch_constraint_catalog(sketch_id).value() == constraints_before);
  CHECK(window.session().sketch_dimension_catalog(sketch_id).value() == dimensions_before);

  REQUIRE(tools.apply());
  const auto* applied = window.session().part_document();
  REQUIRE(applied != nullptr);
  const auto* applied_sketch = applied->find_sketch(sketch_id);
  REQUIRE(applied_sketch != nullptr);
  CHECK(applied_sketch->profile_count() == 2U);
  CHECK(applied_sketch->find_closed_profile(existing_profile) != nullptr);
  CHECK(applied_sketch->find_closed_profile(generated_profile) != nullptr);
  const auto* feature = applied->find_feature(feature_id);
  REQUIRE(feature != nullptr);
  REQUIRE(feature->profile().has_value());
  CHECK(*feature->profile() == generated_profile);
  CHECK(window.session().sketch_constraint_catalog(sketch_id).value().constraints().empty());

  REQUIRE(window.undo());
  const auto* undone = window.session().part_document();
  REQUIRE(undone != nullptr);
  const auto* undone_sketch = undone->find_sketch(sketch_id);
  REQUIRE(undone_sketch != nullptr);
  CHECK(undone_sketch->profile_count() == 1U);
  CHECK(undone_sketch->find_closed_profile(existing_profile) != nullptr);
  CHECK(undone_sketch->find_closed_profile(generated_profile) == nullptr);
  CHECK(undone->find_feature(feature_id) == nullptr);
  CHECK(undone->find_parameter(parameter_id) == nullptr);
  CHECK(undone->find_body(body_id) == nullptr);
  CHECK(window.session().sketch_constraint_catalog(sketch_id).value() == constraints_before);
  CHECK(window.session().sketch_dimension_catalog(sketch_id).value() == dimensions_before);
}

TEST_CASE("A Sketch node ignores subtractive hole-pattern intent when one additive profile exists",
          "[gui][raw-profile-transaction][shell-features][selection]") {
  ShellWindow window;
  REQUIRE(window.new_part(QStringLiteral("Additive Profile With Hole Pattern")));
  const SketchId sketch_id("sketch.additive_and_pattern");
  const ProfileId additive_profile("profile.additive");
  REQUIRE(window.session().commit_part_transaction(
      "Seed additive profile and pattern", [=](PartDocument& part) -> Result<std::size_t> {
        add_pattern_parameters(part);
        auto sketch =
            Sketch::create(sketch_id, "Additive and Pattern", DatumPlaneId("datum.xy"));
        if (sketch.has_error())
          return Result<std::size_t>::failure(sketch.error());
        const auto contour =
            add_line_loop(sketch.value(), "additive", {5.0, 5.0}, {25.0, 15.0});
        auto profile = ClosedProfile::create(
            additive_profile,
            {contour[0], contour[1], contour[2], contour[3]});
        if (profile.has_error())
          return Result<std::size_t>::failure(profile.error());
        if (auto added = sketch.value().add_profile(std::move(profile.value()));
            added.has_error())
          return added;
        auto pattern = CircularHolePattern::create(
            ProfileId("pattern.holes"), ParameterId("pattern.radius"),
            ParameterId("pattern.count"), ParameterId("pattern.diameter"), {50.0, 0.0});
        if (pattern.has_error())
          return Result<std::size_t>::failure(pattern.error());
        if (auto added = sketch.value().add_profile(std::move(pattern.value()));
            added.has_error())
          return added;
        return part.add_sketch(std::move(sketch.value()));
      }));
  window.refresh();

  auto* sketch_item =
      find_tree_item(window.model_browser(), QString::fromStdString(sketch_id.value()));
  REQUIRE(sketch_item != nullptr);
  window.model_browser().setCurrentItem(sketch_item);
  auto& tools = window.feature_tools();
  REQUIRE(tools.selected_profile().has_value());
  CHECK(tools.selected_profile()->first == sketch_id);
  CHECK(tools.selected_profile()->second == additive_profile);
  REQUIRE(tools.extrude_action().isEnabled());
  REQUIRE(tools.begin_extrude());
  CHECK(tools.active());
  tools.cancel();
}
