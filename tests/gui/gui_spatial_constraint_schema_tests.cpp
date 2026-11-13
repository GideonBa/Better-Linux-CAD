#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace {
void seed(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("spatial.part"), "Spatial"));
  REQUIRE(session.commit_part_transaction("Spatial source", [](PartDocument& part) {
    part = f::part();
    auto sketch = f::sketch();
    const auto h = *sketch.find_helix(SketchEntityId("helix"));
    auto radius = f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(4))));
    auto result = sketch.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
        h.id(), h.axis(), radius, h.pitch_binding(), h.turns_parameter(), h.handedness())));
    if (!result)
      return result;
    result = sketch.add_constraint(f::checked(
        SpatialConstraint::create(SketchConstraintId("radius"), SpatialConstraintKind::Radius,
                                  {f::curve("helix")}, f::length(4))));
    return result ? part.add_sketch_3d(std::move(sketch)) : result;
  }));
}
std::string snapshot(const GuiDocumentSession& session) {
  return f::checked(serialize_part_document_to_json(*session.part_document()));
}
} // namespace
TEST_CASE("Spatial V9 intent and owned values survive exact session history and portable reopen",
          "[gui][parity-b163][spatial-session]") {
  GuiDocumentSession session;
  seed(session);
  const auto before = snapshot(session);
  REQUIRE(session.commit_part_transaction("Move spatial point", [](PartDocument& part) {
    auto sketch = *part.find_sketch_3d(Sketch3DId("spatial"));
    auto changed = sketch.replace_point(f::point("p2", {2, 2, 0}));
    return changed ? part.update_sketch_3d(std::move(sketch)) : changed;
  }));
  const auto after = snapshot(session);
  CHECK(after != before);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("spatial.blcad.json").toStdString();
  REQUIRE(session.save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(snapshot(reopened) == after);
  REQUIRE(reopened.part_document()->find_sketch_3d(Sketch3DId("spatial"))->constraints().size() ==
          1);
  CHECK(reopened.part_document()
            ->find_sketch_3d(Sketch3DId("spatial"))
            ->find_helix(SketchEntityId("helix"))
            ->radius_binding()
            .explicit_length()
            ->millimeters() == 4);
}
TEST_CASE("Native spatial schema renders owned helix values and preserves intent through existing "
          "editor cancel",
          "[gui][parity-b163][spatial-native-schema]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native spatial presentation");
  ShellWindow window;
  seed(window.session());
  window.resize(1280, 800);
  const auto available =
      QGuiApplication::primaryScreen()->availableGeometry().adjusted(12, 12, -12, -12);
  window.move(available.topLeft());
  window.resize(std::min(1280, available.width()), std::min(800, available.height()));
  window.show();
  window.refresh();
  QGuiApplication::sync();
  qApp->processEvents();
  REQUIRE(window.viewport().native_viewer_available());
  window.viewport().fit_all();
  qApp->processEvents();
  const auto before = snapshot(window.session());
  bool shown = false;
  for (const auto& id : window.viewport().scene_semantic_ids())
    if (id.starts_with("sketch3d/spatial/") && id.ends_with("/helix")) {
      CHECK(window.viewport().scene_presentation_visible(id));
      shown = true;
    }
  REQUIRE(shown);
  REQUIRE(window.sketch_3d_tools().begin_edit(Sketch3DId("spatial")));
  CHECK_FALSE(window.sketch_3d_tools().preview());
  auto* z = window.findChild<QDoubleSpinBox*>("blcad.sketch3d.z");
  auto* accept = window.findChild<QPushButton*>("blcad.sketch3d.stage_point");
  REQUIRE(z);
  REQUIRE(accept);
  z->setValue(2);
  accept->click();
  const auto preview = window.sketch_3d_tools().preview();
  INFO((preview ? "Valid spatial preview" : preview.error().message()));
  REQUIRE(preview);
  CHECK(snapshot(window.session()) == before);
  const auto root = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!root.isEmpty()) {
    REQUIRE(QDir().mkpath(root));
    QGuiApplication::sync();
    qApp->processEvents();
    REQUIRE(window.grab().save(root + "/spatial-schema.png"));
  }
  window.sketch_3d_tools().cancel();
  CHECK(snapshot(window.session()) == before);
}
