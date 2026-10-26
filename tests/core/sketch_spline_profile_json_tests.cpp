#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_authoring.hpp"
#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_spline_edit.hpp"
#include "blcad/core/sketch_text.hpp"
#include "blcad/core/sketch_text_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace blcad;

namespace {

Sketch make_spline_profile_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.spline"), "SplineSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto bottom =
      LineSegment::create(SketchEntityId("line.bottom"), Point2{0.0, 0.0}, Point2{20.0, 0.0});
  REQUIRE(bottom);
  REQUIRE(sketch.value().add_entity(bottom.value()));

  auto spline =
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.right"), Point2{20.0, 0.0},
                                         Point2{30.0, 5.0}, Point2{30.0, 15.0}, Point2{20.0, 20.0});
  REQUIRE(spline);
  REQUIRE(sketch.value().add_entity(spline.value()));

  auto left =
      LineSegment::create(SketchEntityId("line.left"), Point2{20.0, 20.0}, Point2{0.0, 0.0});
  REQUIRE(left);
  REQUIRE(sketch.value().add_entity(left.value()));

  auto tangent = SketchTangentContinuity::create_tangent(
      SketchConstraintId("tangent.bottom_spline"), SketchEntityId("line.bottom"),
      SketchEntityId("spline.right"));
  REQUIRE(tangent);
  REQUIRE(sketch.value().add_tangent_continuity(tangent.value()));

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.spline"),
      {SketchEntityId("line.bottom"), SketchEntityId("spline.right"), SketchEntityId("line.left")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

PartDocument make_document() {
  auto document = PartDocument::create(DocumentId("part.spline_json"), "SplineJson");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(make_spline_profile_sketch()));
  return document.value();
}

PartDocument make_constraint_document(Point2 end = {10.0, 3.0}) {
  auto document = PartDocument::create(DocumentId("part.constraints"), "Constraints");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  auto sketch = Sketch::create(SketchId("sketch.constraints"), "Constraints",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(
      LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, end).value()));
  REQUIRE(document.value().add_sketch(std::move(sketch.value())));
  return document.value();
}

Parameter length_parameter(const char* id, const char* name, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

} // namespace

TEST_CASE("Spline sketch profile and tangent metadata roundtrip through JSON",
          "[core][json][spline]") {
  PartDocument document = make_document();

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"spline_segments\"") != std::string::npos);
  CHECK(serialized.value().find("\"tangent_continuities\"") != std::string::npos);
  CHECK(serialized.value().find("\"kind\": \"cubic_bezier\"") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);

  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.spline"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->find_spline_segment(SketchEntityId("spline.right")) != nullptr);
  REQUIRE(sketch->find_tangent_continuity(SketchConstraintId("tangent.bottom_spline")) != nullptr);
  REQUIRE(sketch->find_arc_closed_profile(ProfileId("profile.spline")) != nullptr);

  const SplineSegment* spline = sketch->find_spline_segment(SketchEntityId("spline.right"));
  REQUIRE(spline != nullptr);
  CHECK(spline->control1() == Point2{30.0, 5.0});
  CHECK(spline->control2() == Point2{30.0, 15.0});
}

TEST_CASE("Block 113 control-point editing preserves ids profiles and JSON authority",
          "[core][sketch-spline-edit]") {
  PartDocument document = make_document();
  const Sketch* source = document.find_sketch(SketchId("sketch.spline"));
  REQUIRE(source != nullptr);

  auto model = SketchSplineEditModel::from_segments(
      *source, {SketchEntityId("spline.right")});
  REQUIRE(model);
  REQUIRE(model.value().move_control_point(0U, 1U, Point2{34.0, 6.0}));
  const auto preview = model.value().preview();
  REQUIRE(preview.control_polygons.size() == 1U);
  CHECK(preview.control_polygons.front()[1] == Point2{34.0, 6.0});

  auto candidate = model.value().build_sketch(*source);
  REQUIRE(candidate);
  REQUIRE(candidate.value().find_arc_closed_profile(ProfileId("profile.spline")) != nullptr);
  REQUIRE(document.update_sketch(std::move(candidate.value())));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const auto* spline = restored.value()
                           .find_sketch(SketchId("sketch.spline"))
                           ->find_spline_segment(SketchEntityId("spline.right"));
  REQUIRE(spline != nullptr);
  CHECK(spline->control1() == Point2{34.0, 6.0});
}

TEST_CASE("Block 113 fit-point insertion removal and conversion are deterministic",
          "[core][sketch-spline-edit]") {
  auto sketch = Sketch::create(SketchId("sketch.fit"), "Fit", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.fit"), {0.0, 0.0}, {3.0, 6.0}, {7.0, 6.0}, {10.0, 0.0}).value()));

  auto model = SketchSplineEditModel::from_segments(
      sketch.value(), {SketchEntityId("spline.fit")});
  REQUIRE(model);
  REQUIRE(model.value().convert_to_fit_points());
  CHECK(model.value().representation() == SketchSplineRepresentation::FitPoints);
  REQUIRE(model.value().fit_points().size() == 3U);
  REQUIRE(model.value().insert_fit_point(2U, Point2{8.0, 4.0}));
  CHECK(model.value().segments().size() == 3U);
  CHECK(model.value().segments()[0].id().value() == "spline.fit");
  CHECK(model.value().segments()[1].id().value() == "spline.fit.fit.2");
  CHECK(model.value().segments()[2].id().value() == "spline.fit.fit.3");

  const auto first_preview = model.value().preview();
  REQUIRE(first_preview.continuity_handles.size() == 2U);
  CHECK(first_preview.continuity_handles[0].position_continuous);
  CHECK(first_preview.continuity_handles[0].tangent_continuous);

  REQUIRE(model.value().remove_fit_point(1U));
  CHECK(model.value().segments().size() == 2U);
  REQUIRE(model.value().convert_to_control_points());
  CHECK(model.value().representation() == SketchSplineRepresentation::ControlPoints);
  CHECK_FALSE(model.value().preview().handles.empty());
}

TEST_CASE("Block 113 connected spline junctions expose one endpoint and reject profile cardinality changes",
          "[core][sketch-spline-edit]") {
  auto sketch = Sketch::create(SketchId("sketch.chain"), "Chain", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.a"), {0.0, 0.0}, {2.0, 0.0}, {8.0, 5.0}, {10.0, 5.0}).value()));
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.b"), {10.0, 5.0}, {12.0, 5.0}, {18.0, 0.0}, {20.0, 0.0}).value()));

  auto chain = SketchSplineEditModel::from_segments(
      sketch.value(), {SketchEntityId("spline.a"), SketchEntityId("spline.b")});
  REQUIRE(chain);
  const auto preview = chain.value().preview();
  const auto endpoint_count = std::count_if(preview.handles.begin(), preview.handles.end(),
                                             [](const auto& handle) {
                                               return handle.kind == SketchSplineHandleKind::Endpoint;
                                             });
  CHECK(endpoint_count == 3);

  PartDocument document = make_document();
  const Sketch* profile_sketch = document.find_sketch(SketchId("sketch.spline"));
  REQUIRE(profile_sketch != nullptr);
  auto profile_edit = SketchSplineEditModel::from_segments(
      *profile_sketch, {SketchEntityId("spline.right")});
  REQUIRE(profile_edit);
  REQUIRE(profile_edit.value().convert_to_fit_points());
  auto rejected = profile_edit.value().build_sketch(*profile_sketch);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().category() == ErrorCategory::Dependency);
}

TEST_CASE("Closed spline endpoint edits preserve the seam in both authoring representations",
          "[core][sketch-spline-edit][spline-junction-regression]") {
  auto fitted = SketchSplineEditModel::from_fit_points(
      SketchId("sketch.closed.seam"), SketchEntityId("spline.closed"),
      {{0.0, 0.0}, {12.0, 0.0}, {8.0, 10.0}, {0.0, 0.0}});
  REQUIRE(fitted);
  REQUIRE(fitted.value().move_fit_point(0U, {-2.0, 1.0}));
  CHECK(fitted.value().fit_points().front() == fitted.value().fit_points().back());
  CHECK(fitted.value().segments().front().start() == fitted.value().segments().back().end());
  REQUIRE(fitted.value().move_fit_point(3U, {-3.0, 2.0}));
  CHECK(fitted.value().fit_points().front() == Point2{-3.0, 2.0});
  REQUIRE(fitted.value().convert_to_control_points());
  REQUIRE(fitted.value().move_control_point(0U, 0U, {-4.0, 3.0}));
  CHECK(fitted.value().segments().back().end() == Point2{-4.0, 3.0});
  REQUIRE(fitted.value().move_control_point(2U, 3U, {-5.0, 4.0}));
  CHECK(fitted.value().segments().front().start() == Point2{-5.0, 4.0});
}

TEST_CASE("Open fitted spline migration shares numbered junctions without welding unrelated curves",
          "[core][sketch-topology][spline-junction-regression]") {
  auto sketch = Sketch::create(SketchId("sketch.open.fit"), "Open fit", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto fit =
      SketchSplineEditModel::from_fit_points(sketch.value().id(), SketchEntityId("spline.chain"),
                                             {{0.0, 0.0}, {10.0, 7.0}, {20.0, 3.0}, {30.0, 9.0}});
  REQUIRE(fit);
  for (const auto& segment : fit.value().segments())
    REQUIRE(sketch.value().add_entity(segment));
  REQUIRE(sketch.value().add_entity(
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.independent"), {10.0, 7.0},
                                         {12.0, 12.0}, {15.0, 12.0}, {18.0, 7.0})
          .value()));
  auto topology = SketchTopology::migrate_legacy(sketch.value());
  REQUIRE(topology);
  const auto* first = topology.value().find_entity("entity/spline.chain");
  const auto* second = topology.value().find_entity("entity/spline.chain.fit.2");
  const auto* third = topology.value().find_entity("entity/spline.chain.fit.3");
  const auto* independent = topology.value().find_entity("entity/spline.independent");
  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(third);
  REQUIRE(independent);
  CHECK(first->points().back() == second->points().front());
  CHECK(second->points().back() == third->points().front());
  CHECK(independent->points().front() != first->points().back());
  const auto* origin = topology.value().find_point(SketchPointId("reference/origin"));
  REQUIRE(origin);
  CHECK(origin->reference());
  CHECK(origin->position() == Point2{0.0, 0.0});
  const auto* axis = topology.value().find_entity("reference/axis/x");
  REQUIRE(axis);
  CHECK(axis->reference());
  auto move_origin = SketchEditCommand::move_point(SketchPointId("reference/origin"), {5.0, 5.0});
  REQUIRE(move_origin);
  CHECK_FALSE(SketchEditCommandExecutor{}.apply(topology.value(), move_origin.value()));
  auto materialized =
      SketchTopologyLegacyMaterializer{}.materialize(sketch.value(), topology.value());
  REQUIRE(materialized);
  CHECK(SketchTopology::migrate_legacy(materialized.value()).value() == topology.value());
}

TEST_CASE("Raw closed fitted families retain one seam through generic point edits and legacy "
          "materialization",
          "[core][sketch-topology][closed-spline-seam-regression]") {
  for (const bool numbered_first : {false, true}) {
    INFO("first segment uses .fit.1=" << numbered_first);
    auto sketch = Sketch::create(SketchId("sketch.raw.closed.fit"), "Raw closed fit",
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    auto fit = SketchSplineEditModel::from_fit_points(
        sketch.value().id(), SketchEntityId("spline.closed.family"),
        {{20.0, 20.0}, {60.0, 20.0}, {60.0, 60.0}, {20.0, 60.0}, {20.0, 20.0}});
    REQUIRE(fit);
    std::vector<SketchEntityId> ids;
    for (std::size_t index = 0; index < fit.value().segments().size(); ++index) {
      const auto& segment = fit.value().segments()[index];
      ids.emplace_back(index == 0U && !numbered_first
                           ? "spline.closed.family"
                           : "spline.closed.family.fit." + std::to_string(index + 1U));
      REQUIRE(sketch.value().add_entity(
          SplineSegment::create_cubic_bezier(ids.back(), segment.start(), segment.control1(),
                                             segment.control2(), segment.end(), segment.weights())
              .value()));
    }
    REQUIRE(sketch.value().add_entity(
        SplineSegment::create_cubic_bezier(SketchEntityId("spline.unrelated"), {20.0, 20.0},
                                           {10.0, 15.0}, {5.0, 30.0}, {0.0, 40.0})
            .value()));
    REQUIRE(sketch.value().profile_count() == 0U);
    auto topology = SketchTopology::migrate_legacy(sketch.value());
    REQUIRE(topology);
    const auto* first = topology.value().find_entity("entity/" + ids.front().value());
    const auto* last = topology.value().find_entity("entity/" + ids.back().value());
    const auto* unrelated = topology.value().find_entity("entity/spline.unrelated");
    REQUIRE(first);
    REQUIRE(last);
    REQUIRE(unrelated);
    REQUIRE(first->points().front() == last->points().back());
    CHECK(first->points().front() != unrelated->points().front());
    auto move = SketchEditCommand::move_point(first->points().front(), {10.0, 10.0});
    REQUIRE(move);
    auto edited = SketchEditCommandExecutor{}.apply(topology.value(), move.value());
    REQUIRE(edited);
    auto materialized =
        SketchTopologyLegacyMaterializer{}.materialize(sketch.value(), edited.value().after());
    REQUIRE(materialized);
    CHECK(materialized.value().profile_count() == 0U);
    CHECK(materialized.value().find_spline_segment(ids.front())->start() == Point2{10.0, 10.0});
    CHECK(materialized.value().find_spline_segment(ids.back())->end() == Point2{10.0, 10.0});
    CHECK(materialized.value().find_spline_segment(SketchEntityId("spline.unrelated"))->start() ==
          Point2{20.0, 20.0});
    auto remigrated = SketchTopology::migrate_legacy(materialized.value());
    REQUIRE(remigrated);
    CHECK(remigrated.value() == edited.value().after());
    auto document =
        PartDocument::create(DocumentId("part.closed.raw.roundtrip"), "Closed raw roundtrip");
    REQUIRE(document);
    REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
    REQUIRE(document.value().add_sketch(materialized.value()));
    auto serialized = serialize_part_document_to_json(document.value());
    REQUIRE(serialized);
    auto reopened = deserialize_part_document_from_json(serialized.value());
    REQUIRE(reopened);
    CHECK(SketchTopology::migrate_legacy(*reopened.value().find_sketch(sketch.value().id()))
              .value() == remigrated.value());
  }
}

TEST_CASE("Fitted family seam inference rejects incomplete or disconnected numbering",
          "[core][sketch-topology][closed-spline-seam-regression]") {
  for (const bool missing_index : {false, true}) {
    INFO("missing numbered member=" << missing_index);
    auto sketch =
        Sketch::create(SketchId("sketch.incomplete.family"), "Incomplete", DatumPlaneId("datum.xy"))
            .value();
    REQUIRE(
        sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("family"), {0.0, 0.0},
                                                             {1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0})
                              .value()));
    if (!missing_index)
      REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("family.fit.2"),
                                                                   {5.0, 0.0}, {6.0, 1.0},
                                                                   {7.0, 1.0}, {8.0, 0.0})
                                    .value()));
    REQUIRE(sketch.add_entity(SplineSegment::create_cubic_bezier(SketchEntityId("family.fit.3"),
                                                                 {8.0, 0.0}, {8.0, 6.0}, {0.0, 6.0},
                                                                 {0.0, 0.0})
                                  .value()));
    auto topology = SketchTopology::migrate_legacy(sketch);
    REQUIRE(topology);
    const auto* first = topology.value().find_entity("entity/family");
    const auto* last = topology.value().find_entity("entity/family.fit.3");
    REQUIRE(first);
    REQUIRE(last);
    CHECK(first->points().front() != last->points().back());
  }
}

TEST_CASE("Block 113 Sketch text resolves expression parameters and roundtrips sidecar JSON",
          "[core][sketch-text]") {
  auto document = PartDocument::create(DocumentId("part.text"), "Text");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(
      Sketch::create(SketchId("sketch.text"), "Text", DatumPlaneId("datum.xy")).value()));
  REQUIRE(document.value().add_parameter(length_parameter("text.height", "text_height", 7.0)));
  REQUIRE(document.value().add_parameter(length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_expression_parameter(ParameterId("text.value"), "text_value",
                                                    ParameterType::Length, "width / 2"));

  auto text = SketchText::create(
      SketchTextId("text.width"), SketchId("sketch.text"), "WIDTH=${value}", "DIN 1451",
      ParameterId("text.height"), Point2{5.0, 8.0}, std::nullopt,
      {SketchTextBinding{"value", ParameterId("text.value"), 1U}});
  REQUIRE(text);
  auto resolved = text.value().resolve(document.value());
  REQUIRE(resolved);
  CHECK(resolved.value().text == "WIDTH=20 mm");
  CHECK(resolved.value().height_mm == Catch::Approx(7.0));

  auto catalog = SketchTextCatalog::create(document.value().id(), {text.value()});
  REQUIRE(catalog);
  auto serialized = serialize_sketch_text_catalog_to_json(catalog.value());
  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.sketch_text.mvp8") != std::string::npos);
  auto restored = deserialize_sketch_text_catalog_from_json(serialized.value());
  REQUIRE(restored);
  const SketchText* restored_text = restored.value().find(SketchTextId("text.width"));
  REQUIRE(restored_text != nullptr);
  auto restored_resolved = restored_text->resolve(document.value());
  REQUIRE(restored_resolved);
  CHECK(restored_resolved.value().text == "WIDTH=20 mm");
}

TEST_CASE("Block 114 topological constraint intent solves and roundtrips deterministically",
          "[core][sketch-constraints]") {
  PartDocument document = make_constraint_document();
  auto catalog = SketchConstraintCatalog::create(document.id(), SketchId("sketch.constraints"));
  REQUIRE(catalog);
  auto entity = SketchConstraintIntentTarget::entity("entity/line.a");
  REQUIRE(entity);
  auto horizontal = SketchConstraintIntent::create(
      SketchConstraintId("constraint.horizontal"), SketchSolverConstraintKind::Horizontal,
      {entity.value()});
  REQUIRE(horizontal);

  auto preview = SketchConstraintAuthoringService{}.preview(
      document, catalog.value(), horizontal.value());
  REQUIRE(preview);
  CHECK(preview.value().accepted);
  CHECK(preview.value().solve.status == SketchSolveStatus::UnderConstrained);
  REQUIRE(preview.value().solved_sketch.has_value());
  const auto* solved_line =
      preview.value().solved_sketch->find_line_segment(SketchEntityId("line.a"));
  REQUIRE(solved_line != nullptr);
  CHECK(solved_line->start().y == Catch::Approx(solved_line->end().y).margin(1.0e-7));

  auto serialized =
      serialize_sketch_constraint_catalog_to_json(preview.value().catalog_after);
  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.sketch_constraints.mvp8") != std::string::npos);
  auto restored = deserialize_sketch_constraint_catalog_from_json(serialized.value());
  REQUIRE(restored);
  REQUIRE(restored.value().constraints().size() == 1U);
  CHECK(restored.value().constraints().front().id().value() == "constraint.horizontal");
  CHECK(restored.value().constraints().front().source() ==
        SketchConstraintIntentSource::Manual);
}

TEST_CASE("Block 114 conflicting additions remain preview-only with stable ids",
          "[core][sketch-constraints][core][sketch-conflict-diagnostics]") {
  PartDocument document = make_constraint_document({10.0, 0.0});
  auto entity = SketchConstraintIntentTarget::entity("entity/line.a");
  REQUIRE(entity);
  auto fixed = SketchConstraintIntent::create(
      SketchConstraintId("constraint.fixed"), SketchSolverConstraintKind::Fixed,
      {entity.value()});
  auto vertical = SketchConstraintIntent::create(
      SketchConstraintId("constraint.vertical"), SketchSolverConstraintKind::Vertical,
      {entity.value()});
  REQUIRE(fixed);
  REQUIRE(vertical);
  auto catalog = SketchConstraintCatalog::create(
      document.id(), SketchId("sketch.constraints"), {fixed.value()});
  REQUIRE(catalog);

  const auto before = document.find_sketch(SketchId("sketch.constraints"))
                          ->find_line_segment(SketchEntityId("line.a"))->end();
  auto preview = SketchConstraintAuthoringService{}.preview(
      document, catalog.value(), vertical.value());
  REQUIRE(preview);
  CHECK_FALSE(preview.value().accepted);
  CHECK_FALSE(preview.value().solved_sketch.has_value());
  CHECK(preview.value().solve.status == SketchSolveStatus::Conflicting);
  CHECK(std::find(preview.value().conflict_ids.begin(), preview.value().conflict_ids.end(),
                  "constraint.vertical") != preview.value().conflict_ids.end());
  CHECK(catalog.value().constraints().size() == 1U);
  CHECK(document.find_sketch(SketchId("sketch.constraints"))
            ->find_line_segment(SketchEntityId("line.a"))->end() == before);

  auto rejected_dimension = SketchConstraintIntent::create(
      SketchConstraintId("constraint.distance"),
      SketchSolverConstraintKind::AlignedDistance,
      {SketchConstraintIntentTarget::point(SketchPointId("a")).value(),
       SketchConstraintIntentTarget::point(SketchPointId("b")).value()});
  CHECK_FALSE(rejected_dimension);
}
