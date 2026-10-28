#include "../support/rotor_performance_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_interactive_finishing.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QTimer>
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

// Explicit opt-in: timing observations are not portable correctness budgets.
TEST_CASE("Profile one curved rotor through finishing preview and native publication",
          "[.][gui][complex-part-profile]") {
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
    auto fixture = test::rotor_performance_part(blades);
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
