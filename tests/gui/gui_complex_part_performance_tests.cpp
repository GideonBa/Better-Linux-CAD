#include "../support/rotor_performance_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/occt_viewport.hpp"
#include "blcad/gui/gui_sketch_dimensions.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QTimer>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>

using namespace blcad;
using namespace blcad::gui;

namespace {
template <typename Function> double measured_ms(Function&& function) {
  QElapsedTimer timer;
  timer.start();
  function();
  return timer.nsecsElapsed() / 1e6;
}
QJsonObject distribution(const std::vector<double>& samples) {
  auto sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  const auto quantile = [&](double value) {
    return sorted[static_cast<std::size_t>(std::ceil(value * sorted.size())) - 1];
  };
  QJsonArray raw;
  for (double value : samples)
    raw.push_back(value);
  QJsonObject result{{"samples_ms", raw},
                     {"first_ms", samples.front()},
                     {"p50_ms", quantile(.5)},
                     {"p95_ms", quantile(.95)}};
  if (samples.size() > 1) {
    std::vector<double> warm(samples.begin() + 1, samples.end());
    std::sort(warm.begin(), warm.end());
    result.insert("warm_p50_ms", warm[static_cast<std::size_t>(std::ceil(.5 * warm.size())) - 1]);
    result.insert("warm_p95_ms", warm[static_cast<std::size_t>(std::ceil(.95 * warm.size())) - 1]);
  }
  return result;
}
} // namespace

TEST_CASE("Complex rotor previews reuse clean producers and recompute changed dependencies",
          "[gui][preview-cache][complex-part-correctness]") {
  const auto fixture = test::rotor_performance_part(20);
  GuiDocumentSession session;
  REQUIRE(session.create_part(fixture.id(), fixture.name()));
  REQUIRE(session.commit_part_transaction("Rotor", [&](PartDocument& part) {
    part = fixture;
    return Result<std::size_t>::success(1);
  }));
  const auto original = serialize_part_document_to_json(*session.part_document()).value();
  const auto cached_hub = geometry::OcctShapeView::get(
      *session.part_shape_cache()->find_feature_shape(FeatureId("feature.hub")));
  const auto cached_blade = geometry::OcctShapeView::get(
      *session.part_shape_cache()->find_feature_shape(FeatureId("feature.blade")));
  auto targets = geometry::FinishingSelectionBuilder::build(
      *session.part_document(), *session.part_shape_cache(), BodyId("body.rotor"),
      PartFeatureInputRole::FilletEdge);
  REQUIRE(targets);
  auto tip = std::find_if(targets.value().begin(), targets.value().end(), [](const auto& item) {
    return item.anchor.x > 85 && std::abs(item.anchor.y) < 20 && item.anchor.z > 9;
  });
  REQUIRE(tip != targets.value().end());
  const auto edge = std::get<EdgeReference>(tip->reference);
  auto candidate = *session.part_document();
  REQUIRE(candidate.add_fillet_feature(FilletFeature::create(FeatureId("feature.fillet"), "Fillet",
                                                             BodyId("body.rotor"), {edge},
                                                             ParameterId("fillet.radius"))
                                           .value()));
  const auto check = [&](const PartDocument& part, bool reuse_hub, bool reuse_blade) {
    auto preview = detail::recompute_preview_geometry(session, part, {"feature.fillet"});
    INFO((preview ? "valid" : preview.error().message()));
    REQUIRE(preview);
    auto cold = geometry::ShapeCache::create(ShapeCacheId("rotor.cold.oracle")).value();
    REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cold));
    const auto actual = geometry::RectangleExtrusionAdapter{}.summarize(
        *preview.value().find_body_shape(BodyId("body.rotor")));
    const auto expected = geometry::RectangleExtrusionAdapter{}.summarize(
        *cold.find_body_shape(BodyId("body.rotor")));
    CHECK(actual.solid_count == 1);
    CHECK(actual.volume_mm3 == Catch::Approx(expected.volume_mm3).epsilon(1e-10).margin(1e-6));
    CHECK((geometry::OcctShapeView::get(*preview.value().find_feature_shape(
               FeatureId("feature.hub"))) == cached_hub) == reuse_hub);
    CHECK((geometry::OcctShapeView::get(*preview.value().find_feature_shape(
               FeatureId("feature.blade"))) == cached_blade) == reuse_blade);
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == original);
  };
  check(candidate, true, true);
  // The mutable Core API can add clean nodes without refreshing session
  // Geometry. A Fresh flag alone must not authorize an incomplete cache.
  GuiDocumentSession incomplete;
  REQUIRE(incomplete.create_part(fixture.id(), fixture.name()));
  *incomplete.part_document() = fixture;
  REQUIRE(incomplete.recompute_status() == GuiRecomputeStatus::Fresh);
  REQUIRE(incomplete.part_shape_cache()->feature_shape_count() == 0);
  auto rebuilt = detail::recompute_preview_geometry(incomplete, candidate, {"feature.fillet"});
  REQUIRE(rebuilt);
  CHECK(rebuilt.value().find_feature_shape(FeatureId("feature.blade")));
  CHECK(rebuilt.value().find_feature_shape(FeatureId("feature.fillet")));
  GuiDocumentSession different_ids;
  REQUIRE(different_ids.create_part(fixture.id(), fixture.name()));
  REQUIRE(different_ids.commit_part_transaction("Rotor", [&](PartDocument& part) {
    part = fixture;
    return Result<std::size_t>::success(1);
  }));
  auto replacement = test::rotor_performance_part(20, "feature.hub.replaced");
  *different_ids.part_document() = replacement;
  REQUIRE(different_ids.part_shape_cache()->feature_shape_count() == 3);
  REQUIRE(replacement.add_fillet_feature(FilletFeature::create(FeatureId("feature.fillet"),
                                                               "Fillet", BodyId("body.rotor"),
                                                               {edge}, ParameterId("fillet.radius"))
                                             .value()));
  auto replaced =
      detail::recompute_preview_geometry(different_ids, replacement, {"feature.fillet"});
  REQUIRE(replaced);
  CHECK(replaced.value().find_feature_shape(FeatureId("feature.hub.replaced")));
  CHECK_FALSE(replaced.value().find_feature_shape(FeatureId("feature.hub")));
  REQUIRE(candidate.set_parameter_value(ParameterId("hub.height"),
                                        Quantity::length_mm(5, "hub.height").value()));
  check(candidate, false, true);
  session.set_derived_results_fresh(false);
  check(candidate, false, false);
  session.set_derived_results_fresh(true);
  GuiInteractiveFinishingController controller;
  REQUIRE(controller.begin_fillet(session, BodyId("body.rotor"), FeatureId("feature.fillet"),
                                  "Fillet", ParameterId("fillet.radius")));
  REQUIRE(controller.add_edge(edge));
  auto preview = controller.preview_shape(session);
  REQUIRE(preview);
  const auto volume = geometry::RectangleExtrusionAdapter{}.summarize(preview.value()).volume_mm3;
  REQUIRE(controller.apply(session));
  CHECK(geometry::RectangleExtrusionAdapter{}
            .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.rotor")))
            .volume_mm3 == Catch::Approx(volume).margin(1e-6));
  const auto applied = serialize_part_document_to_json(*session.part_document()).value();
  REQUIRE(session.undo());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == original);
  REQUIRE(session.redo());
  CHECK(serialize_part_document_to_json(*session.part_document()).value() == applied);
}

TEST_CASE("Deep curved rotor dimension and drag sequences preserve history and stale guards",
          "[gui][preview-cache][complex-part-sequence]") {
  for (const int blades : {5, 20, 40}) {
    CAPTURE(blades);
    auto fixture = test::rotor_performance_sequence_part(blades);
    GuiDocumentSession session;
    REQUIRE(session.create_part(fixture.id(), fixture.name()));
    REQUIRE(session.commit_part_transaction("Deep rotor", [&](PartDocument& part) {
      part = fixture;
      return Result<std::size_t>::success(1);
    }));
    const auto original = serialize_part_document_to_json(*session.part_document()).value();
    const auto blade = geometry::OcctShapeView::get(
        *session.part_shape_cache()->find_feature_shape(FeatureId("feature.blade")));
    auto edited = *session.part_document();
    REQUIRE(edited.set_parameter_value(ParameterId("hub.height"),
                                       Quantity::length_mm(5.9, "hub.height").value()));
    auto cached = detail::recompute_preview_geometry(session, edited, {"hub.height"});
    REQUIRE(cached);
    CHECK(geometry::OcctShapeView::get(
              *cached.value().find_feature_shape(FeatureId("feature.blade"))) == blade);
    auto cold = geometry::ShapeCache::create(ShapeCacheId("deep.rotor.oracle")).value();
    REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(edited, cold));
    CHECK(geometry::RectangleExtrusionAdapter{}
              .summarize(*cached.value().find_body_shape(BodyId("body.rotor")))
              .volume_mm3 ==
          Catch::Approx(geometry::RectangleExtrusionAdapter{}
                            .summarize(*cold.find_body_shape(BodyId("body.rotor")))
                            .volume_mm3)
              .epsilon(1e-10)
              .margin(1e-6));
    for (const auto& transform : fixture.body_transforms()) {
      REQUIRE(cached.value().find_feature_shape(FeatureId(transform.id().value())));
      CHECK(geometry::OcctShapeView::get(
                *cached.value().find_feature_shape(FeatureId(transform.id().value()))) !=
            geometry::OcctShapeView::get(*session.part_shape_cache()->find_feature_shape(
                FeatureId(transform.id().value()))));
    }
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == original);

    const auto intent = SketchDimensionIntent::create(
                            SketchDimensionId("dimension.tip"), SketchDimensionKind::Length,
                            SketchDimensionMode::Driving,
                            {SketchConstraintIntentTarget::entity("entity/blade.tip").value()},
                            ParameterId("blade.tip.dimension"))
                            .value();
    auto dimension =
        GuiSketchDimensionController::create(session, SketchId("sketch.blade"), intent);
    REQUIRE(dimension);
    REQUIRE(dimension.value().preview().authoring.accepted);
    REQUIRE(dimension.value().commit(session));
    REQUIRE(GuiSketchDimensionController::edit_literal(
        session, SketchId("sketch.blade"), SketchDimensionId("dimension.tip"),
        Quantity::length_mm(4.05, "blade.tip.dimension").value()));
    CHECK(session.part_document()
              ->find_parameter(ParameterId("blade.tip.dimension"))
              ->value()
              .millimeters() == Catch::Approx(4.05));
    const auto dimensioned = serialize_part_document_to_json(*session.part_document()).value();
    auto drag = GuiSketchDragController::create(session, SketchId("sketch.blade"));
    REQUIRE(drag);
    auto handle = std::find_if(
        drag.value().handles().begin(), drag.value().handles().end(), [](const auto& item) {
          return item.kind == GuiSketchDragHandleKind::Endpoint && item.position.x < 45;
        });
    REQUIRE(handle != drag.value().handles().end());
    const auto target = Point2{handle->position.x + .1, handle->position.y + .1};
    REQUIRE(drag.value().begin(handle->id));
    for (int index = 0; index < 32; ++index)
      REQUIRE(drag.value().queue_pointer(index == 31 ? target : handle->position));
    REQUIRE(drag.value().process_pending());
    CHECK(drag.value().solve_count() == 1);
    CHECK(drag.value().processed_pointer() == target);
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == dimensioned);
    REQUIRE(drag.value().commit_session_constraints(session));
    const auto dragged = serialize_part_document_to_json(*session.part_document()).value();
    CHECK(dragged != dimensioned);
    CHECK(geometry::RectangleExtrusionAdapter{}
              .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.rotor")))
              .solid_count == 1);
    REQUIRE(session.undo());
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == dimensioned);
    REQUIRE(session.redo());
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == dragged);
    auto stale = GuiSketchDragController::create(session, SketchId("sketch.blade"));
    REQUIRE(stale);
    REQUIRE(stale.value().begin(stale.value().handles().front().id));
    REQUIRE(stale.value().flush(stale.value().handles().front().position));
    REQUIRE(session.undo());
    CHECK_FALSE(stale.value().commit_session_constraints(session));
    CHECK(serialize_part_document_to_json(*session.part_document()).value() == dimensioned);
    REQUIRE(session.redo());
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = directory.filePath("deep-rotor.blcad").toStdString();
    REQUIRE(session.save_as(path));
    GuiDocumentSession reopened;
    REQUIRE(reopened.open_file(path));
    CHECK(serialize_part_document_to_json(*reopened.part_document()).value() == dragged);
    CHECK(reopened.sketch_dimension_catalogs() == session.sketch_dimension_catalogs());
  }
}

// Explicit opt-in: timing observations are not portable correctness budgets.
TEST_CASE("Profile one curved rotor through finishing preview and native publication",
          "[.][gui][complex-part-profile]") {
  const bool sequence = qEnvironmentVariableIsSet("BLCAD_PROFILE_SEQUENCE");
  const int samples = std::max(1, qEnvironmentVariableIntValue("BLCAD_PROFILE_SAMPLES"));
  const int requested_blades = qEnvironmentVariableIntValue("BLCAD_PROFILE_BLADES");
  const std::vector<int> sizes =
      requested_blades > 0 ? std::vector{requested_blades} : std::vector{5, 20, 40};
  QJsonArray models;
  // The shell keeps one viewport while replacing Parts. Reuse that lifetime
  // here too, and measure window/context lifecycle in its separate regressions.
  OcctViewport viewport;
  viewport.resize(1100, 700);
  viewport.show();
  qApp->processEvents();
  if (QApplication::platformName() == "xcb")
    REQUIRE(viewport.native_viewer_available());
  for (int blades : sizes) {
    viewport.clear_feature_preview();
    viewport.clear_scene();
    CAPTURE(blades);
    auto fixture = sequence ? test::rotor_performance_sequence_part(blades)
                            : test::rotor_performance_part(blades);
    GuiDocumentSession session;
    REQUIRE(session.create_part(fixture.id(), fixture.name()));
    REQUIRE(session.commit_part_transaction("Rotor", [&](PartDocument& part) {
      part = fixture;
      return Result<std::size_t>::success(1);
    }));
    const auto solid = geometry::RectangleExtrusionAdapter{}.summarize(
        *session.part_shape_cache()->find_body_shape(BodyId("body.rotor")));
    REQUIRE(solid.solid_count == 1);
    CHECK(solid.volume_mm3 > 3.141592653589793 * 45 * 45 * 6);
    const auto before = serialize_part_document_to_json(*session.part_document()).value();
    std::vector<geometry::FinishingSelectionTarget> targets;
    std::vector<double> selection, preview, recompute, json, scene, publication, redraw,
        event_delay, cold_candidate, full_publication, cached_publication, full_redraw,
        cached_redraw;
    std::vector<double> full_events, cached_events, full_readback, cached_readback;
    std::vector<double> solver, dimension, drag, picking, apply, undo, redo, complete_preview,
        complete_event_delay;
    for (int iteration = 0; iteration < samples; ++iteration) {
      selection.push_back(measured_ms([&] {
        auto result = geometry::FinishingSelectionBuilder::build(
            *session.part_document(), *session.part_shape_cache(), BodyId("body.rotor"),
            PartFeatureInputRole::FilletEdge);
        REQUIRE(result);
        targets = std::move(result.value());
      }));
      REQUIRE_FALSE(targets.empty());
      GuiInteractiveFinishingController fillet;
      REQUIRE(fillet.begin_fillet(session, BodyId("body.rotor"), FeatureId("feature.fillet"),
                                  "Fillet", ParameterId("fillet.radius")));
      const auto edge = std::find_if(targets.begin(), targets.end(), [](const auto& item) {
        return item.anchor.x > 85 && std::abs(item.anchor.y) < 20 && item.anchor.z > 9;
      });
      REQUIRE(edge != targets.end());
      REQUIRE(fillet.add_edge(std::get<EdgeReference>(edge->reference)));
      std::optional<geometry::GeometryShape> cached_shape, full_shape;
      QElapsedTimer event_timer;
      event_timer.start();
      bool delivered = false;
      QTimer::singleShot(0, &viewport, [&] {
        event_delay.push_back(event_timer.nsecsElapsed() / 1e6);
        delivered = true;
      });
      preview.push_back(measured_ms([&] {
        auto result = fillet.preview_shape(session);
        INFO((result ? "valid" : result.error().message()));
        REQUIRE(result);
        cached_shape = std::move(result.value());
      }));
      qApp->processEvents();
      REQUIRE(delivered);
      cold_candidate.push_back(measured_ms([&] {
        auto candidate = fixture;
        REQUIRE(candidate.add_fillet_feature(
            FilletFeature::create(FeatureId("feature.fillet"), "Fillet", BodyId("body.rotor"),
                                  {std::get<EdgeReference>(edge->reference)},
                                  ParameterId("fillet.radius"))
                .value()));
        auto cache = geometry::ShapeCache::create(ShapeCacheId("profile.full.candidate")).value();
        REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(candidate, cache));
        full_shape = *cache.find_body_shape(BodyId("body.rotor"));
      }));
      REQUIRE(cached_shape);
      REQUIRE(full_shape);
      CHECK(geometry::RectangleExtrusionAdapter{}.summarize(*cached_shape).volume_mm3 ==
            Catch::Approx(geometry::RectangleExtrusionAdapter{}.summarize(*full_shape).volume_mm3)
                .epsilon(1e-10)
                .margin(1e-6));
      recompute.push_back(measured_ms([&] {
        auto cache = geometry::ShapeCache::create(ShapeCacheId("profile.cold")).value();
        REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(fixture, cache));
      }));
      json.push_back(measured_ms([&] {
        auto encoded = serialize_part_document_to_json(fixture);
        REQUIRE(encoded);
        REQUIRE(deserialize_part_document_from_json(encoded.value()));
      }));
      std::vector<geometry::ViewportSceneItem> items;
      scene.push_back(measured_ms([&] {
        auto built = geometry::ViewportSceneBuilder{}.build_part(*session.part_document(),
                                                                 *session.part_shape_cache());
        REQUIRE(built);
        items = std::move(built.value());
      }));
      publication.push_back(measured_ms([&] { REQUIRE(viewport.set_scene(std::move(items))); }));
      redraw.push_back(measured_ms([&] {
        viewport.fit_all();
        qApp->processEvents();
        REQUIRE_FALSE(viewport.grab().isNull());
      }));
      const auto render = [&](auto& events, auto& readback) {
        events.push_back(measured_ms([&] { qApp->processEvents(); }));
        readback.push_back(measured_ms([&] { REQUIRE_FALSE(viewport.grab().isNull()); }));
      };
      full_publication.push_back(measured_ms(
          [&] { REQUIRE(viewport.set_feature_preview(*full_shape, "body/body.rotor")); }));
      full_redraw.push_back(measured_ms([&] { render(full_events, full_readback); }));
      viewport.clear_feature_preview();
      cached_publication.push_back(measured_ms(
          [&] { REQUIRE(viewport.set_feature_preview(*cached_shape, "body/body.rotor")); }));
      cached_redraw.push_back(measured_ms([&] { render(cached_events, cached_readback); }));
      viewport.clear_feature_preview();
      CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
      if (sequence) {
        auto handles = GuiSketchDragController::create(session, SketchId("sketch.blade"));
        REQUIRE(handles);
        solver.push_back(measured_ms([&] {
          auto solved = SketchConstraintSolver{}.solve(handles.value().source_topology(),
                                                       handles.value().source_system());
          REQUIRE(solved);
          CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
        }));
        auto intent = SketchDimensionIntent::create(
                          SketchDimensionId("dimension.tip"), SketchDimensionKind::Length,
                          SketchDimensionMode::Driving,
                          {SketchConstraintIntentTarget::entity("entity/blade.tip").value()},
                          ParameterId("blade.tip.dimension"))
                          .value();
        dimension.push_back(measured_ms([&] {
          auto result =
              GuiSketchDimensionController::create(session, SketchId("sketch.blade"), intent);
          INFO((result ? "valid dimension" : result.error().message()));
          REQUIRE(result);
          CHECK(result.value().preview().authoring.accepted);
        }));
        auto handle = std::find_if(handles.value().handles().begin(),
                                   handles.value().handles().end(), [](const auto& item) {
                                     return item.kind == GuiSketchDragHandleKind::Endpoint &&
                                            item.position.x > 85;
                                   });
        REQUIRE(handle != handles.value().handles().end());
        REQUIRE(handles.value().begin(handle->id));
        drag.push_back(measured_ms([&] {
          // Exercise the existing latest-pointer coalescing on curved topology.
          for (int step = 1; step <= 32; ++step)
            REQUIRE(handles.value().queue_pointer(
                {handle->position.x + step * .001, handle->position.y + step * .001}));
          REQUIRE(handles.value().process_pending());
        }));
        CHECK(handles.value().solve_count() == 1);
        handles.value().cancel();

        if (viewport.native_viewer_available()) {
          viewport.set_standard_view(GuiStandardView::Top);
          viewport.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Body));
          auto screen = viewport.model_to_screen({10, 10, 6});
          REQUIRE(screen);
          const QPointF point(screen.value().x, screen.value().y);
          picking.push_back(measured_ms([&] {
            QMouseEvent move(QEvent::MouseMove, point, viewport.mapToGlobal(point), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&viewport, &move);
            QMouseEvent release(QEvent::MouseButtonRelease, point, viewport.mapToGlobal(point),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&viewport, &release);
          }));
          REQUIRE(viewport.selected_semantic());
          CHECK(viewport.selected_semantic()->semantic_id == "body/body.rotor");
          viewport.clear_selection();
        }
        QElapsedTimer complete;
        complete.start();
        bool event_delivered = false;
        QTimer::singleShot(0, &viewport, [&] {
          complete_event_delay.push_back(complete.nsecsElapsed() / 1e6);
          event_delivered = true;
        });
        complete_preview.push_back(measured_ms([&] {
          auto result = fillet.preview_shape(session);
          REQUIRE(result);
          REQUIRE(viewport.set_feature_preview(result.value(), "body/body.rotor"));
          qApp->processEvents();
          REQUIRE_FALSE(viewport.grab().isNull());
        }));
        REQUIRE(event_delivered);
        viewport.clear_feature_preview();
        apply.push_back(measured_ms([&] { REQUIRE(fillet.apply(session)); }));
        CHECK(
            geometry::RectangleExtrusionAdapter{}
                .summarize(*session.part_shape_cache()->find_body_shape(BodyId("body.rotor")))
                .volume_mm3 ==
            Catch::Approx(geometry::RectangleExtrusionAdapter{}.summarize(*cached_shape).volume_mm3)
                .epsilon(1e-10)
                .margin(1e-6));
        undo.push_back(measured_ms([&] { REQUIRE(session.undo()); }));
        redo.push_back(measured_ms([&] { REQUIRE(session.redo()); }));
        REQUIRE(session.undo());
        CHECK(serialize_part_document_to_json(*session.part_document()).value() == before);
      }
    }
    models.push_back(
        QJsonObject{{"blades", blades},
                    {"solid_count", static_cast<int>(solid.solid_count)},
                    {"volume_mm3", solid.volume_mm3},
                    {"offered_edges", static_cast<int>(targets.size())},
                    {"selection", distribution(selection)},
                    {"preview", distribution(preview)},
                    {"cold_recompute", distribution(recompute)},
                    {"cold_candidate", distribution(cold_candidate)},
                    {"json_roundtrip", distribution(json)},
                    {"scene_build", distribution(scene)},
                    {"publication", distribution(publication)},
                    {"redraw_and_readback", distribution(redraw)},
                    {"full_preview_publication", distribution(full_publication)},
                    {"cached_preview_publication", distribution(cached_publication)},
                    {"full_preview_redraw_and_readback", distribution(full_redraw)},
                    {"cached_preview_redraw_and_readback", distribution(cached_redraw)},
                    {"full_preview_events", distribution(full_events)},
                    {"cached_preview_events", distribution(cached_events)},
                    {"full_preview_readback", distribution(full_readback)},
                    {"cached_preview_readback", distribution(cached_readback)},
                    {"queued_event_delay", distribution(event_delay)}});
    if (sequence) {
      auto model = models.last().toObject();
      model.insert("serial_transforms", static_cast<int>(fixture.body_transforms().size()));
      model.insert("producer_count",
                   static_cast<int>(session.part_shape_cache()->feature_shape_count()));
      model.insert("solver", distribution(solver));
      model.insert("dimension_preview", distribution(dimension));
      model.insert("coalesced_drag_32_points", distribution(drag));
      if (!picking.empty())
        model.insert("native_mouse_pick", distribution(picking));
      model.insert("apply_including_history_and_recompute", distribution(apply));
      model.insert("undo_including_recompute", distribution(undo));
      model.insert("redo_including_recompute", distribution(redo));
      model.insert("preview_publication_events_and_capture", distribution(complete_preview));
      model.insert("complete_preview_queued_event_delay", distribution(complete_event_delay));
      models.replace(models.size() - 1, model);
    }
  }
  QFile cpuinfo("/proc/cpuinfo");
  QString cpu;
  if (cpuinfo.open(QIODevice::ReadOnly))
    for (const auto& line : cpuinfo.readAll().split('\n'))
      if (line.startsWith("model name")) {
        cpu = QString::fromUtf8(line.mid(line.indexOf(':') + 1)).trimmed();
        break;
      }
  const QJsonDocument report(QJsonObject{{"platform", QApplication::platformName()},
                                         {"deep_sequence", sequence},
                                         {"viewport_lifetime", "persistent"},
                                         {"viewport_width_dip", viewport.width()},
                                         {"viewport_height_dip", viewport.height()},
                                         {"device_pixel_ratio", viewport.devicePixelRatioF()},
                                         {"build_type", BLCAD_PROFILE_BUILD_TYPE},
                                         {"cpu", cpu},
                                         {"qt", qVersion()},
                                         {"architecture", QSysInfo::currentCpuArchitecture()},
                                         {"kernel", QSysInfo::kernelVersion()},
                                         {"samples", samples},
                                         {"models", models}});
  std::cout << report.toJson().toStdString();
  const auto path = qEnvironmentVariable("BLCAD_PROFILE_OUTPUT");
  if (!path.isEmpty()) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(report.toJson()) > 0);
  }
}
