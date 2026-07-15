#include "blcad/geometry/rectangle_extrusion_adapter.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/occt_viewport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>

#include <optional>

using namespace blcad;
using namespace blcad::geometry;
using namespace blcad::gui;

namespace {

GeometryShape box_shape() {
  auto width = Quantity::length_mm(20.0);
  auto height = Quantity::length_mm(12.0);
  auto depth = Quantity::length_mm(8.0);
  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(depth);
  auto shape = RectangleExtrusionAdapter{}.make_extruded_rectangle(width.value(), height.value(),
                                                                   depth.value());
  REQUIRE(shape);
  return std::move(shape.value());
}

} // namespace

TEST_CASE("OCCT viewport owns transient display and navigation state",
          "[gui][viewport][gui][navigation]") {
  OcctViewport viewport;
  viewport.show();
  qApp->processEvents();
  CHECK(viewport.objectName() == QStringLiteral("blcad.occt_viewport"));
  if (QApplication::platformName() == QStringLiteral("xcb"))
    CHECK(viewport.native_viewer_available());
  CHECK(viewport.display_mode() == GuiViewportDisplayMode::ShadedWithEdges);
  CHECK(viewport.projection() == GuiViewportProjection::Perspective);
  CHECK_FALSE(viewport.sketch_focus_active());

  std::vector<ViewportSceneItem> scene;
  scene.push_back({ViewportSceneKind::SolidBody, "body/main", box_shape()});
  REQUIRE(viewport.set_scene(std::move(scene)));
  CHECK(viewport.presentation_count() == 1);

  viewport.set_display_mode(GuiViewportDisplayMode::Wireframe);
  CHECK(viewport.display_mode() == GuiViewportDisplayMode::Wireframe);
  viewport.set_projection(GuiViewportProjection::Orthographic);
  CHECK(viewport.projection() == GuiViewportProjection::Orthographic);
  viewport.set_standard_view(GuiStandardView::Front);
  REQUIRE(viewport.set_plane_camera({0.0, 0.0, 5.0}, {0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}));
  REQUIRE(viewport.plane_camera().has_value());
  CHECK(viewport.plane_camera()->target == Point3{0.0, 0.0, 5.0});
  const auto bookmark = viewport.camera_bookmark();
  viewport.set_projection(GuiViewportProjection::Perspective);
  viewport.set_standard_view(GuiStandardView::Isometric);
  CHECK_FALSE(viewport.plane_camera().has_value());
  REQUIRE(viewport.restore_camera_bookmark(bookmark));
  CHECK(viewport.projection() == GuiViewportProjection::Orthographic);
  REQUIRE(viewport.plane_camera().has_value());
  CHECK(viewport.plane_camera()->target == Point3{0.0, 0.0, 5.0});
  CHECK_FALSE(viewport.restore_camera_bookmark(
      GuiViewportCameraBookmark{{1.0, 1.0, 1.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, 0.0}));
  CHECK_FALSE(viewport.set_plane_camera({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0},
                                        {0.0, 0.0, 1.0}));

  viewport.set_sketch_focus("sketch.path");
  CHECK(viewport.sketch_focus_active());
  CHECK(viewport.sketch_focus_id() == "sketch.path");
  CHECK(viewport.sketch_surroundings_mode() == GuiSketchSurroundingsMode::Dim);
  viewport.set_sketch_focus("sketch.path", GuiSketchSurroundingsMode::Isolate);
  CHECK(viewport.sketch_surroundings_mode() == GuiSketchSurroundingsMode::Isolate);
  viewport.clear_sketch_focus();
  CHECK_FALSE(viewport.sketch_focus_active());
  CHECK(viewport.sketch_focus_id().empty());
  viewport.fit_all();
}

TEST_CASE("OCCT viewport semantic bridge filters selection and publishes stable ids",
          "[gui][viewport][gui][semantic-picking]") {
  OcctViewport viewport;
  std::optional<GuiSelection> published;
  viewport.set_selection_callback(
      [&published](std::optional<GuiSelection> selection) { published = std::move(selection); });

  std::vector<ViewportSceneItem> scene;
  scene.push_back({ViewportSceneKind::SolidBody, "body/plate", box_shape()});
  REQUIRE(viewport.set_scene(std::move(scene)));
  REQUIRE(viewport.select_semantic("body/plate"));
  REQUIRE(viewport.selected_semantic().has_value());
  CHECK(viewport.selected_semantic()->kind == GuiSelectionKind::Body);
  CHECK(viewport.selected_semantic()->semantic_id == "body/plate");
  REQUIRE(published.has_value());
  CHECK(published->semantic_id == "body/plate");

  viewport.set_selection_filter_mask(selection_kind_bit(GuiSelectionKind::Component));
  CHECK_FALSE(viewport.selected_semantic().has_value());
  CHECK_FALSE(viewport.select_semantic("body/plate"));
  CHECK_FALSE(viewport.select_semantic("missing"));
}

TEST_CASE("OCCT viewport rejects duplicate semantic owners without replacing its scene",
          "[gui][viewport][gui][semantic-picking]") {
  OcctViewport viewport;
  REQUIRE(viewport.set_scene({{ViewportSceneKind::SolidBody, "body/valid", box_shape()}}));

  std::vector<ViewportSceneItem> duplicate;
  duplicate.push_back({ViewportSceneKind::SolidBody, "body/duplicate", box_shape()});
  duplicate.push_back({ViewportSceneKind::SolidBody, "body/duplicate", box_shape()});
  const auto rejected = viewport.set_scene(std::move(duplicate));
  REQUIRE(rejected.has_error());
  CHECK(viewport.presentation_count() == 1);
  CHECK(viewport.select_semantic("body/valid"));
}

TEST_CASE("Viewport scene builder emits stable datum and planar sketch identities",
          "[gui][viewport][gui][semantic-picking]") {
  auto part = PartDocument::create(DocumentId("part.viewport_scene"), "Viewport Scene");
  REQUIRE(part);
  auto plane = DatumPlane::xy();
  REQUIRE(plane);
  REQUIRE(part.value().add_datum_plane(std::move(plane.value())));
  auto sketch = Sketch::create(SketchId("sketch.path"), "Path", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto line = LineSegment::create(SketchEntityId("line.main"), {0.0, 0.0}, {25.0, 10.0});
  REQUIRE(line);
  REQUIRE(sketch.value().add_entity(std::move(line.value())));
  REQUIRE(part.value().add_sketch(std::move(sketch.value())));
  auto cache = ShapeCache::create(ShapeCacheId("cache.viewport_scene"));
  REQUIRE(cache);

  auto scene = ViewportSceneBuilder{}.build_part(part.value(), cache.value());
  REQUIRE(scene);
  CHECK(scene.value().size() == 2);
  CHECK(scene.value()[0].semantic_id == "datum-plane/datum.xy");
  CHECK(scene.value()[1].semantic_id == "sketch/sketch.path/entity/line.main");
}
