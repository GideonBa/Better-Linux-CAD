#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_presentation_visibility.hpp"
#include "blcad/gui/gui_sketch_drag.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace blcad;
using namespace blcad::gui;

namespace {
class TreeStyleOptionDelegate final : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::initStyleOption;
};

bool near(Point2 left, Point2 right) {
  return std::hypot(left.x - right.x, left.y - right.y) < 1.0e-5;
}

void settle() {
  for (int index = 0; index < 8; ++index)
    qApp->processEvents();
}

void mouse(QWidget& widget, QEvent::Type type, QPointF point, Qt::MouseButtons buttons,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
  QMouseEvent event(type, point, widget.mapToGlobal(point.toPoint()), button, buttons, modifiers);
  QApplication::sendEvent(&widget, &event);
  settle();
}

void click_plane(ShellWindow& window, Point2 point,
                 Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  const auto screen = window.viewport().sketch_plane_to_screen(point);
  REQUIRE(screen);
  const QPointF position(screen.value().x, screen.value().y);
  mouse(window.viewport(), QEvent::MouseButtonPress, position, Qt::LeftButton, modifiers);
  mouse(window.viewport(), QEvent::MouseButtonRelease, position, Qt::NoButton, modifiers);
}

std::string drag_plane(ShellWindow& window, Point2 from, Point2 to) {
  std::ostringstream trace;
  const auto record = [&](const char* phase) {
    const auto& controller = window.sketch_tools().drag_controller();
    trace << phase << ": task=" << static_cast<int>(window.session().task().stage())
          << " controller=" << controller.has_value();
    if (controller) {
      trace << " active=" << controller->active();
      if (controller->active_handle())
        trace << " handle=" << controller->active_handle()->id;
      if (controller->processed_pointer())
        trace << " processed=" << controller->processed_pointer()->x << ','
              << controller->processed_pointer()->y;
    }
    if (window.viewport().sketch_snap_result()) {
      const auto& snap = *window.viewport().sketch_snap_result();
      trace << " snap=" << to_string(snap.kind) << ':' << snap.snapped_point.x << ','
            << snap.snapped_point.y;
    }
    trace << " status=" << window.statusBar()->currentMessage().toStdString() << '\n';
  };
  const auto start = window.viewport().sketch_plane_to_screen(from);
  const auto end = window.viewport().sketch_plane_to_screen(to);
  REQUIRE(start);
  REQUIRE(end);
  mouse(window.viewport(), QEvent::MouseButtonPress, {start.value().x, start.value().y},
        Qt::LeftButton);
  record("press");
  mouse(window.viewport(), QEvent::MouseMove, {end.value().x, end.value().y}, Qt::LeftButton);
  record("move");
  mouse(window.viewport(), QEvent::MouseButtonRelease, {end.value().x, end.value().y},
        Qt::NoButton);
  record("release");
  for (const auto& diagnostic : window.session().diagnostics())
    trace << diagnostic.object_id << ": " << diagnostic.message << '\n';
  return trace.str();
}

QTreeWidgetItem* tree_item(ShellWindow& window, std::string_view semantic) {
  for (QTreeWidgetItemIterator it(&window.model_browser()); *it; ++it)
    if ((*it)->data(0, Qt::UserRole).toString().toStdString() == semantic)
      return *it;
  return nullptr;
}

void toggle_browser(ShellWindow& window, const std::string& semantic) {
  auto* item = tree_item(window, semantic);
  REQUIRE(item);
  auto& tree = window.model_browser();
  for (auto* parent = item->parent(); parent; parent = parent->parent())
    parent->setExpanded(true);
  tree.scrollToItem(item);
  settle();
  const auto rect = tree.visualItemRect(item);
  REQUIRE_FALSE(rect.isEmpty());
  const auto expected = item->checkState(0) == Qt::Checked ? Qt::Unchecked : Qt::Checked;
  QStyleOptionViewItem option;
  option.initFrom(&tree);
  TreeStyleOptionDelegate delegate;
  delegate.initStyleOption(&option, tree.indexFromItem(item));
  option.rect = rect;
  option.widget = &tree;
  const auto indicator =
      tree.style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, &tree);
  REQUIRE_FALSE(indicator.isEmpty());
  REQUIRE(rect.contains(indicator.center()));
  const QPointF point(indicator.center());
  mouse(*tree.viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton);
  mouse(*tree.viewport(), QEvent::MouseButtonRelease, point, Qt::NoButton);
  item = tree_item(window, semantic);
  REQUIRE(item);
  REQUIRE(item->checkState(0) == expected);
}

void activate_browser(ShellWindow& window, const std::string& semantic) {
  auto* item = tree_item(window, semantic);
  REQUIRE(item);
  auto& tree = window.model_browser();
  for (auto* parent = item->parent(); parent; parent = parent->parent())
    parent->setExpanded(true);
  tree.scrollToItem(item);
  settle();
  const auto rect = tree.visualItemRect(item);
  REQUIRE_FALSE(rect.isEmpty());
  // The row's right side is clear of both the expansion arrow and checkbox.
  const QPointF point(rect.right() - 8, rect.center().y());
  mouse(*tree.viewport(), QEvent::MouseButtonPress, point, Qt::LeftButton);
  mouse(*tree.viewport(), QEvent::MouseButtonRelease, point, Qt::NoButton);
  mouse(*tree.viewport(), QEvent::MouseButtonDblClick, point, Qt::LeftButton);
  mouse(*tree.viewport(), QEvent::MouseButtonRelease, point, Qt::NoButton);
}

void capture(ShellWindow& window, const char* name) {
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (directory.isEmpty())
    return;
  REQUIRE(QDir{}.mkpath(directory));
  REQUIRE(window.grab().save(QDir(directory).filePath(QString::fromUtf8(name) + ".png")));
}
} // namespace

TEST_CASE(
    "Origin coincidence axis constraints spline dragging and visibility form one editable workflow",
    "[gui][shell-reference-visibility][complex-workflow][spline-junction-regression]") {
  ShellWindow window;
  window.resize(1450, 950);
  window.show();
  settle();
  if (QApplication::platformName() != QStringLiteral("offscreen")) {
    window.raise();
    window.activateWindow();
    QElapsedTimer deadline;
    deadline.start();
    while (!window.viewport().native_viewer_available() && deadline.elapsed() < 5000) {
      qApp->processEvents();
      QThread::msleep(10);
    }
    REQUIRE(window.viewport().native_viewer_available());
  }
  REQUIRE(window.new_part(QStringLiteral("Reference visibility workflow")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  const SketchId sketch_id = *window.active_sketch();
  auto& tools = window.sketch_tools();
  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Line));
  REQUIRE(tools.add_pick({12.0, 8.0}));
  REQUIRE(tools.add_pick({32.0, 8.0}));
  tools.cancel_tool();

  // Use actual point hits, including the fixed origin, to author Coincident.
  window.session().selection().clear();
  click_plane(window, {12.0, 8.0});
  click_plane(window, {0.0, 0.0}, Qt::ControlModifier);
  REQUIRE(window.session().selection().size() == 2U);
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Coincident));
  auto current = [&]() -> const Sketch& {
    return *window.session().part_document()->find_sketch(sketch_id);
  };
  CHECK(near(current().line_segments().front().start(), {0.0, 0.0}));
  window.session().selection().clear();
  const auto& line = current().line_segments().front();
  const Point2 center{(line.start().x + line.end().x) / 2.0, (line.start().y + line.end().y) / 2.0};
  click_plane(window, center);
  click_plane(window, {60.0, 0.0}, Qt::ControlModifier);
  REQUIRE(window.session().selection().size() == 2U);
  REQUIRE(tools.apply_constraint(SketchSolverConstraintKind::Parallel));
  CHECK(std::abs(current().line_segments().front().end().y) < 1.0e-5);

  REQUIRE(tools.begin_tool(GuiSketchCreateTool::Spline));
  for (const auto point :
       {Point2{8.0, 16.0}, Point2{20.0, 28.0}, Point2{36.0, 20.0}, Point2{50.0, 34.0}})
    REQUIRE(tools.add_pick(point));
  REQUIRE(tools.complete_tool());
  REQUIRE(current().spline_segments().size() == 3U);
  const std::string before_drag =
      serialize_part_document_to_json(*window.session().part_document()).value();
  // Move outside the existing endpoint snap radius so the pointer cannot
  // resolve back onto the source junction instead of reaching the target.
  INFO(drag_plane(window, {20.0, 28.0}, {30.0, 40.0}));
  INFO("actual junction=" << current().spline_segments()[0].end().x << ','
                          << current().spline_segments()[0].end().y);
  REQUIRE(near(current().spline_segments()[0].end(), {30.0, 40.0}));
  REQUIRE(near(current().spline_segments()[1].start(), {30.0, 40.0}));
  const std::string after_drag =
      serialize_part_document_to_json(*window.session().part_document()).value();
  CHECK(after_drag != before_drag);
  REQUIRE(window.undo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == before_drag);
  REQUIRE(window.redo());
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after_drag);

  const std::string spline_id = current().spline_segments().front().id().value();
  const std::string browser_id = "sketch/" + sketch_id.value() + "/spline/" + spline_id;
  const std::string presentation_id = "sketch/" + sketch_id.value() + "/entity/" + spline_id;
  const std::string history_label(window.session().undo_label());
  toggle_browser(window, browser_id);
  CHECK_FALSE(window.session().presentation_visible(presentation_id));
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == after_drag);
  CHECK(window.session().undo_label() == history_label);
  CHECK(tree_item(window, browser_id)->checkState(0) == Qt::Unchecked);
  toggle_browser(window, browser_id);
  CHECK(window.session().presentation_visible(presentation_id));
  toggle_browser(window, sketch_id.value());
  CHECK_FALSE(window.session().presentation_visible(presentation_id));
  CHECK(window.viewport().sketch_drag_handle_count() == 0U);
  const auto hidden_model =
      serialize_part_document_to_json(*window.session().part_document()).value();
  drag_plane(window, {30.0, 40.0}, {40.0, 50.0});
  CHECK(serialize_part_document_to_json(*window.session().part_document()).value() == hidden_model);
  CHECK(window.viewport().sketch_drag_handle_count() == 0U);
  capture(window, "hidden-sketch-workflow");
  toggle_browser(window, sketch_id.value());
  CHECK(window.viewport().sketch_drag_handle_count() > 0U);
  capture(window, "origin-spline-visibility-workflow");

  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("reference-spline.blcad.json");
  REQUIRE(window.session().save_as(path.toStdString()));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(reopened.hidden_presentations().empty());
  auto drag = GuiSketchDragController::create(reopened, sketch_id);
  REQUIRE(drag);
  const auto* origin = drag.value().source_topology().find_point(SketchPointId("reference/origin"));
  REQUIRE(origin);
  CHECK(origin->reference());
  CHECK(origin->position() == Point2{0.0, 0.0});
  CHECK(near(reopened.part_document()->find_sketch(sketch_id)->spline_segments()[0].end(),
             reopened.part_document()->find_sketch(sketch_id)->spline_segments()[1].start()));
}

TEST_CASE("Presentation visibility filters semantic subtrees and hidden body selection targets",
          "[gui][presentation-visibility][model-browser]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.visibility"), "Visibility"));
  const auto model_revision = session.model_revision();
  const auto presentation_revision = session.presentation_revision();
  REQUIRE(session.set_presentation_visible("body/a", false));
  REQUIRE(session.set_presentation_visible("sketch/s1", false));
  CHECK(session.model_revision() == model_revision);
  CHECK(session.presentation_revision() == presentation_revision + 2U);
  CHECK_FALSE(session.presentation_visible("sketch/s1/entity/a"));
  CHECK(session.presentation_visible("sketch/s10/entity/a"));
  std::vector<geometry::ViewportSceneItem> items{
      {geometry::ViewportSceneKind::SolidBody, "body/a", {}, SketchLineRole::Geometry, "a"},
      {geometry::ViewportSceneKind::Edge, "edge/body-a", {}, SketchLineRole::Geometry, "a"},
      {geometry::ViewportSceneKind::SolidBody, "body/b", {}, SketchLineRole::Geometry, "b"},
      {geometry::ViewportSceneKind::SketchEntity, "sketch/s1/entity/a", {}}};
  filter_presentation_visibility(session, items);
  REQUIRE(items.size() == 1U);
  CHECK(items.front().semantic_id == "body/b");
  CHECK(session.show_all_presentations() == 2U);
  CHECK(session.model_revision() == model_revision);
  CHECK(session.presentation_revision() == presentation_revision + 3U);
  CHECK(session.presentation_visible("body/a"));
  CHECK(session.presentation_visible("sketch/s1/entity/a"));
  REQUIRE(session.set_presentation_visible("sketch/s1", false));
  REQUIRE(session.set_presentation_visible("sketch/s1/entity/a", true));
  CHECK(session.model_revision() == model_revision);
  CHECK(session.presentation_revision() == presentation_revision + 5U);
  CHECK(session.presentation_visible("sketch/s1/entity/a"));
}

TEST_CASE(
    "Fresh closed spline seams stay joined during mouse dragging before Finish and after Save Open",
    "[gui][shell-reference-visibility][spline-junction-regression][closed-spline-seam-"
    "regression]") {
  ShellWindow window;
  window.resize(1450, 950);
  window.show();
  settle();
  if (QApplication::platformName() != QStringLiteral("offscreen")) {
    window.raise();
    window.activateWindow();
    QElapsedTimer deadline;
    deadline.start();
    while (!window.viewport().native_viewer_available() && deadline.elapsed() < 5000) {
      qApp->processEvents();
      QThread::msleep(10);
    }
    REQUIRE(window.viewport().native_viewer_available());
  }
  REQUIRE(window.new_part(QStringLiteral("Raw closed spline seam")));
  REQUIRE(window.viewport().select_semantic("datum-plane/datum.xy"));
  REQUIRE(window.create_sketch_on_selection());
  const SketchId sketch_id = *window.active_sketch();
  REQUIRE(window.sketch_tools().begin_tool(GuiSketchCreateTool::Spline));
  for (const Point2 point : {Point2{-20.0, -20.0}, Point2{20.0, -20.0}, Point2{20.0, 20.0},
                             Point2{-20.0, 20.0}, Point2{-20.0, -20.0}})
    click_plane(window, point);
  REQUIRE(window.sketch_tools().complete_tool());
  auto sketch = [&]() -> const Sketch& {
    return *window.session().part_document()->find_sketch(sketch_id);
  };
  REQUIRE(sketch().spline_segments().size() == 4U);
  REQUIRE(sketch().profile_count() == 0U); // No profile can supply seam connectivity yet.
  const SketchEntityId first = sketch().spline_segments().front().id();
  const SketchEntityId last = sketch().spline_segments().back().id();
  auto check_seam = [&](Point2 expected) {
    const auto* start = sketch().find_spline_segment(first);
    const auto* end = sketch().find_spline_segment(last);
    REQUIRE(start);
    REQUIRE(end);
    CHECK(near(start->start(), expected));
    CHECK(near(end->end(), expected));
    auto topology = SketchTopology::migrate_legacy(sketch());
    REQUIRE(topology);
    REQUIRE(topology.value().find_entity("entity/" + first.value())->points().front() ==
            topology.value().find_entity("entity/" + last.value())->points().back());
  };
  check_seam({-20.0, -20.0});
  INFO(drag_plane(window, {-20.0, -20.0}, {-30.0, -30.0}));
  check_seam({-30.0, -30.0});
  REQUIRE(sketch().profile_count() == 0U);
  capture(window, "raw-closed-spline-seam-drag");
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("raw-closed-spline.blcad.json");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  REQUIRE_FALSE(window.active_sketch());
  REQUIRE(sketch().profile_count() == 0U);
  activate_browser(window, sketch_id.value());
  REQUIRE(window.active_sketch() == sketch_id);
  check_seam({-30.0, -30.0});
  // Keep the target clear of other fit-point alignments so the grid snap is unambiguous.
  INFO(drag_plane(window, {-30.0, -30.0}, {-40.0, -40.0}));
  check_seam({-40.0, -40.0});
  REQUIRE(sketch().profile_count() == 0U);
  REQUIRE(window.undo());
  check_seam({-30.0, -30.0});
  REQUIRE(window.redo());
  check_seam({-40.0, -40.0});
  capture(window, "raw-closed-spline-seam-reopened");
  REQUIRE(window.finish_active_sketch());
  CHECK(sketch().profile_count() >= 1U);
}

TEST_CASE("Visibility controls respect Core-hidden bodies and nested occurrence definitions",
          "[gui][presentation-visibility][model-browser]") {
  GuiDocumentSession session;
  REQUIRE(session.create_project(DocumentId("project.visibility.nested"), "Nested visibility"));
  REQUIRE(session.commit_project_transaction(
      "Seed nested visibility", [](Project& project) -> Result<std::size_t> {
        auto part = PartDocument::create(DocumentId("part.visibility.member"), "Member").value();
        auto body = Body::create(BodyId("body.visibility.hidden"), "Hidden body", BodyKind::Solid,
                                 BodyVisibility::Hidden)
                        .value();
        auto added = part.add_body(body);
        if (!added)
          return added;
        added = project.add_part_document(std::move(part));
        if (!added)
          return added;
        auto child =
            AssemblyDocument::create(DocumentId("assembly.visibility.child"), "Child").value();
        added = child.add_member_part(DocumentId("part.visibility.member"));
        if (!added)
          return added;
        auto component =
            ComponentInstance::create(ComponentInstanceId("component.visibility.member"), "Member",
                                      DocumentId("part.visibility.member"))
                .value();
        added = child.add_component_instance(component);
        if (!added)
          return added;
        added = project.add_child_assembly_document(std::move(child));
        if (!added)
          return added;
        auto occurrence =
            SubassemblyInstance::create(SubassemblyInstanceId("subassembly.visibility.child"),
                                        "Child occurrence", DocumentId("assembly.visibility.child"))
                .value();
        return project.assembly().add_subassembly_instance(occurrence);
      }));
  auto browser = GuiDocumentBrowser::build(session);
  auto* body = browser.find("body.visibility.hidden");
  auto* component = browser.find("component.visibility.member");
  auto* subassembly = browser.find("subassembly.visibility.child");
  REQUIRE(body);
  REQUIRE(component);
  REQUIRE(subassembly);
  CHECK_FALSE(body->presentation_visible);
  REQUIRE_FALSE(subassembly->visibility_semantic_id.empty());
  REQUIRE(set_browser_presentation_visible(session, *body, true));
  CHECK(session.project()
            ->find_part_document(DocumentId("part.visibility.member"))
            ->find_body(BodyId("body.visibility.hidden"))
            ->visibility() == BodyVisibility::Visible);
  REQUIRE(set_browser_presentation_visible(session, *component, false));
  CHECK(session.project()
            ->find_assembly_document(DocumentId("assembly.visibility.child"))
            ->find_component_instance(ComponentInstanceId("component.visibility.member"))
            ->visibility() == ComponentVisibility::Hidden);
  REQUIRE(set_browser_presentation_visible(session, *component, true));
  REQUIRE(set_browser_presentation_visible(session, *subassembly, false));
  CHECK(session.project()
            ->assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.visibility.child"))
            ->visibility() == ComponentVisibility::Hidden);
  browser = GuiDocumentBrowser::build(session);
  CHECK_FALSE(browser.find("subassembly.visibility.child")->presentation_visible);
  REQUIRE(session.undo());
  browser = GuiDocumentBrowser::build(session);
  CHECK(browser.find("subassembly.visibility.child")->presentation_visible);
}

TEST_CASE("Unrelated hidden bodies preserve visible sketch intersection snaps",
          "[gui][presentation-visibility][sketch-interaction]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("part.visibility.intersections"), "Intersections"));
  REQUIRE(session.set_presentation_visible("body/other", false));
  GuiSketchInteractionScene scene;
  scene.sketch = SketchId("sketch.visible");
  scene.curves.push_back(
      {"sketch/sketch.visible/entity/a", "a", GuiSketchCurveKind::Line, {{0, 0}, {10, 10}}});
  scene.intersections.push_back({5, 5});
  filter_presentation_visibility(session, scene);
  REQUIRE(scene.intersections.size() == 1U);
  REQUIRE(session.set_presentation_visible("sketch/sketch.visible/entity/a", false));
  filter_presentation_visibility(session, scene);
  CHECK(scene.intersections.empty());
  scene.points.push_back(
      {"sketch/sketch.visible/entity/a" + std::string(kGuiSketchHitRoleMarker) + "a:start",
       "a:start",
       {0, 0}});
  CHECK_FALSE(session.presentation_visible(scene.points.front().semantic_id));
  filter_presentation_visibility(session, scene);
  CHECK(scene.points.empty());
}
