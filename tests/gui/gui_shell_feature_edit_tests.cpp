#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
std::string snapshot(ShellWindow& w) {
  return checked(serialize_part_document_to_json(*w.session().part_document()));
}
void fixture(ShellWindow& window, bool fillet = true, bool formula = false, bool offset = false) {
  REQUIRE(window.new_part("Feature editing"));
  auto setup = window.session().commit_part_transaction("Edit inputs", [=](PartDocument& part) {
    for (auto [id, scalar] : std::vector<std::pair<std::string, double>>{
             {"width", 20}, {"height", 10}, {"depth", 5}, {"depth2", 8}, {"radius", 1}})
      REQUIRE(part.add_parameter(checked(
          Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(scalar))))));
    if (formula)
      REQUIRE(part.add_parameter(checked(
          Parameter::create_expression(ParameterId("computed"), "Computed", ParameterType::Length,
                                       "depth * 2", checked(Quantity::length_mm(10))))));
    auto sketch =
        checked(Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(
        RectangleProfile::create(ProfileId("profile.base"), ParameterId("width"),
                                 ParameterId("height"), offset ? Point2{30, 0} : Point2{}))));
    REQUIRE(part.add_sketch(sketch));
    REQUIRE(part.add_body(checked(Body::create(BodyId("body.base"), "Base", BodyKind::Solid))));
    auto feature = checked(Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                            sketch.id(), ParameterId("depth")));
    REQUIRE(part.add_feature(
        checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
            FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.base")))))));
    if (fillet) {
      auto edge = checked(EdgeReference::create_linear(
          PartFeatureInputRole::FilletEdge,
          checked(SemanticEdgeReference::create(feature.id(), SemanticEdge::TopFront))));
      REQUIRE(part.add_fillet_feature(
          checked(FilletFeature::create(FeatureId("feature.fillet"), "Fillet", BodyId("body.base"),
                                        {edge}, ParameterId("radius")))));
    }
    return Result<std::size_t>::success(1);
  });
  REQUIRE(setup);
  window.refresh();
  window.resize(1280, 800);
  window.show();
  qApp->processEvents();
  window.viewport().fit_all();
  qApp->processEvents();
}
QPushButton& apply(ShellWindow& w) {
  auto* button = w.findChild<QPushButton*>("blcad.feature_edit.apply");
  REQUIRE(button);
  return *button;
}
QDoubleSpinBox& number(ShellWindow& w, const QString& path) {
  auto* spin = qobject_cast<QDoubleSpinBox*>(w.feature_edit_tools().field(path));
  REQUIRE(spin);
  return *spin;
}
void name(ShellWindow& w, const QString& text) {
  auto* input = qobject_cast<QLineEdit*>(w.feature_edit_tools().field("/name"));
  REQUIRE(input);
  input->setText(text);
  REQUIRE(QMetaObject::invokeMethod(input, "editingFinished"));
}
void history(ShellWindow& w, const std::string& before) {
  const auto after = snapshot(w);
  REQUIRE(w.undo());
  CHECK(snapshot(w) == before);
  REQUIRE(w.redo());
  CHECK(snapshot(w) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  REQUIRE(w.save_to(directory.filePath("edited.blcad.json")));
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(directory.filePath("edited.blcad.json").toStdString()));
  CHECK(checked(serialize_part_document_to_json(*opened.part_document())) == after);
}
void capture(ShellWindow& w, const QString& name) {
  auto dir = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (dir.isEmpty())
    return;
  REQUIRE(QDir().mkpath(dir));
  CHECK(w.grab().save(QDir(dir).filePath(name + ".png")));
}
QTreeWidgetItem* tree_item(QTreeWidgetItem* item, const QString& id) {
  if (item->data(0, Qt::UserRole).toString() == id)
    return item;
  for (int i = 0; i < item->childCount(); ++i)
    if (auto* found = tree_item(item->child(i), id))
      return found;
  return nullptr;
}
FeatureId family_fixture(ShellWindow& w, GuiFeatureEditKind kind) {
  using K = GuiFeatureEditKind;
  fixture(w, false, false, true);
  if (kind == K::Extrude)
    return FeatureId("feature.base");
  auto result = w.session().commit_part_transaction("Family input", [kind](PartDocument& part) {
    const FeatureId id("feature.edit");
    const BodyId base("body.base"), output("body.edit");
    auto axis = [&](PartFeatureInputRole role, const char* direction) {
      return checked(AxisReference::create_datum_axis(role, DatumAxisId(direction)));
    };
    auto plane = [&](PartFeatureInputRole role, const char* direction) {
      return checked(PlaneReference::create_datum_plane(role, DatumPlaneId(direction)));
    };
    auto new_body = [&](BodyKind type = BodyKind::Solid) {
      REQUIRE(part.add_body(checked(Body::create(output, "Result", type))));
      return checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                      std::nullopt, output));
    };
    auto edge = [&](PartFeatureInputRole role) {
      return checked(EdgeReference::create_linear(
          role, checked(SemanticEdgeReference::create(FeatureId("feature.base"),
                                                      SemanticEdge::TopFront))));
    };
    auto face = [&](PartFeatureInputRole role, SemanticFace side) {
      return checked(FaceReference::create_planar(
          role, checked(SemanticFaceReference::create(FeatureId("feature.base"), side))));
    };
    auto profile = [&](PartFeatureInputRole role, const char* sketch = "sketch.base",
                       const char* name = "profile.base") {
      return checked(ProfileRegionReference::create(SketchId(sketch), ProfileId(name), role));
    };
    if (kind == K::Fillet)
      REQUIRE(part.add_fillet_feature(checked(FilletFeature::create(
          id, "Edit", base, {edge(PartFeatureInputRole::FilletEdge)}, ParameterId("radius")))));
    else if (kind == K::Chamfer)
      REQUIRE(part.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
          id, "Edit", base, {edge(PartFeatureInputRole::ChamferEdge)}, ParameterId("radius")))));
    else if (kind == K::Shell)
      REQUIRE(part.add_shell_feature(checked(ShellFeature::create(
          id, "Edit", base, {face(PartFeatureInputRole::ShellRemovalFace, SemanticFace::Top)},
          ParameterId("radius"), ShellDirection::Inward))));
    else if (kind == K::Draft) {
      REQUIRE(part.add_parameter(checked(Parameter::create_angle(
          ParameterId("angle"), "Angle", checked(Quantity::angle_deg(3))))));
      REQUIRE(part.add_draft_feature(checked(DraftFeature::create(
          id, "Edit", base, {face(PartFeatureInputRole::DraftFace, SemanticFace::Front)},
          axis(PartFeatureInputRole::DraftPullDirection, "datum.axis.z"),
          plane(PartFeatureInputRole::DraftNeutralPlane, "datum.xy"), ParameterId("angle")))));
    } else if (kind == K::Revolve) {
      REQUIRE(part.add_revolve_feature(checked(RevolveFeature::create_revolve(
          id, "Edit", profile(PartFeatureInputRole::RevolveProfile),
          axis(PartFeatureInputRole::RevolveAxis, "datum.axis.y"),
          checked(RevolveAngleExtent::angle(180, RevolveSide::Positive)), new_body()))));
    } else if (kind == K::LinearPattern || kind == K::CircularPattern) {
      REQUIRE(part.add_parameter(checked(
          Parameter::create_count(ParameterId("count"), "Count", checked(Quantity::count(3))))));
      REQUIRE(part.add_parameter(checked(Parameter::create_length(
          ParameterId("spacing"), "Spacing", checked(Quantity::length_mm(30))))));
      auto source = checked(PatternSourceReference::body(base));
      if (kind == K::LinearPattern)
        REQUIRE(part.add_linear_pattern_feature(checked(LinearPatternFeature::create(
            id, "Edit", {source}, axis(PartFeatureInputRole::PatternAxis, "datum.axis.x"),
            ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("spacing"),
            PatternDirectionSign::Positive, new_body()))));
      else
        REQUIRE(part.add_circular_pattern_feature(checked(CircularPatternFeature::create(
            id, "Edit", {source}, axis(PartFeatureInputRole::PatternAxis, "datum.axis.z"),
            ParameterId("count"), 360, true, new_body()))));
    } else if (kind == K::Mirror) {
      REQUIRE(part.add_mirror_feature(checked(MirrorFeature::create(
          id, "Edit", {checked(MirrorSourceReference::body(base))},
          plane(PartFeatureInputRole::MirrorPlane, "datum.yz"), new_body()))));
    } else if (kind == K::BodyBoolean) {
      REQUIRE(part.add_body(checked(Body::create(BodyId("tool"), "Tool", BodyKind::Solid))));
      auto tool = checked(Feature::create_additive_extrude(
          FeatureId("feature.tool"), "Tool", SketchId("sketch.base"), ParameterId("depth")));
      REQUIRE(part.add_feature(
          checked(tool.with_body_result_context(checked(FeatureBodyResultContext::create(
              FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("tool")))))));
      (void)new_body();
      REQUIRE(part.add_body_boolean_feature(
          checked(BodyBooleanFeature::create(id, BodyBooleanOperation::Add, base, {BodyId("tool")},
                                             BodyBooleanResultMode::NewBody, output, false))));
    } else if (kind == K::Sweep) {
      REQUIRE(part.add_parameter(checked(Parameter::create_angle(
          ParameterId("twist"), "Twist", checked(Quantity::angle_deg(0))))));
      REQUIRE(part.add_construction_point(checked(
          ConstructionPoint::create_explicit(ConstructionPointId("p0"), "Start", {30, 0, 0}))));
      REQUIRE(part.add_construction_point(checked(
          ConstructionPoint::create_explicit(ConstructionPointId("p1"), "End", {30, 0, 10}))));
      auto relation = checked(ConstructionRelation::create_line_through_two_points(
          ConstructionRelationId("relation"), ConstructionPointId("p0"),
          ConstructionPointId("p1")));
      REQUIRE(part.add_construction_line(checked(ConstructionLine::create_through_two_points(
          ConstructionLineId("line"), "Path", relation))));
      REQUIRE(part.add_path_curve(checked(PathCurve::create(
          PathCurveId("path"), "Path",
          {checked(PathSegmentReference::create_construction_line(ConstructionLineId("line")))},
          PathClosure::Open, PathOrientationRule::MinimumTwist))));
      REQUIRE(part.add_sweep_feature(
          checked(SweepFeature::create_sweep(id, "Edit",
                                             checked(SweepProfileReference::create_closed_region(
                                                 profile(PartFeatureInputRole::SweepProfile))),
                                             PathCurveId("path"), new_body()))));
    } else if (kind == K::Loft) {
      REQUIRE(part.add_construction_plane(checked(
          ConstructionPlane::create_explicit(ConstructionPlaneId("upper.plane"), "Upper",
                                             {0, 0, 10}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
      auto upper = checked(Sketch::create(SketchId("upper"), "Upper", DatumPlaneId("upper.plane")));
      REQUIRE(upper.add_profile(checked(RectangleProfile::create(
          ProfileId("upper.profile"), ParameterId("width"), ParameterId("height"), {30, 0}))));
      REQUIRE(part.add_sketch(upper));
      REQUIRE(part.add_loft_feature(checked(LoftFeature::create_loft(
          id, "Edit",
          {checked(ProfileSectionReference::create_closed_region(
               profile(PartFeatureInputRole::LoftSection))),
           checked(ProfileSectionReference::create_closed_region(
               profile(PartFeatureInputRole::LoftSection, "upper", "upper.profile")))},
          new_body()))));
    } else if (kind == K::Surface) {
      auto sketch =
          checked(Sketch::create(SketchId("boundary"), "Boundary", DatumPlaneId("datum.xy")));
      const std::vector<Point2> points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
      std::vector<PathSegmentReference> segments;
      for (int i = 0; i < 4; ++i) {
        SketchEntityId line("line." + std::to_string(i));
        REQUIRE(
            sketch.add_entity(checked(LineSegment::create(line, points[i], points[(i + 1) % 4]))));
        segments.push_back(checked(
            PathSegmentReference::create_planar(sketch.id(), line, PlanarPathCurveKind::Line)));
      }
      REQUIRE(part.add_sketch(sketch));
      REQUIRE(part.add_path_curve(
          checked(PathCurve::create(PathCurveId("surface.boundary"), "Boundary", segments,
                                    PathClosure::Closed, PathOrientationRule::MinimumTwist))));
      (void)new_body(BodyKind::Surface);
      REQUIRE(part.add_surface_feature(checked(FillSurfaceFeature::create(
          id, "Edit",
          {checked(BoundaryCurveReference::create_path_curve(PathCurveId("surface.boundary")))},
          output))));
    }
    return Result<std::size_t>::success(1);
  });
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  w.refresh();
  return FeatureId("feature.edit");
}
} // namespace
TEST_CASE("Shell feature editor preloads and updates all thirteen persistent feature families",
          "[gui][block136-feature-edit]") {
  const auto kind =
      static_cast<GuiFeatureEditKind>(GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12));
  CAPTURE(static_cast<int>(kind));
  ShellWindow w;
  const auto id = family_fixture(w, kind);
  const auto before = snapshot(w);
  auto& tools = w.feature_edit_tools();
  auto begun = tools.begin(id);
  INFO((begun.has_error() ? begun.error().message() : "ok"));
  REQUIRE(begun);
  REQUIRE(apply(w).isEnabled());
  CHECK(snapshot(w) == before);
  if (kind == GuiFeatureEditKind::BodyBoolean) {
    auto* keep = qobject_cast<QCheckBox*>(tools.field("/keep_tool_bodies"));
    REQUIRE(keep);
    keep->setChecked(true);
  } else
    name(w, "Edited feature");
  REQUIRE(tools.apply());
  CHECK(snapshot(w) != before);
  history(w, before);
}
TEST_CASE(
    "Shell feature edit preloads bindings and recomputes upstream and downstream in one Apply",
    "[gui][block136-feature-edit]") {
  ShellWindow w;
  fixture(w);
  const auto before = snapshot(w);
  auto& tools = w.feature_edit_tools();
  REQUIRE(tools.begin(FeatureId("feature.base")));
  CHECK(number(w, "/length_parameter").value() == 5);
  number(w, "/length_parameter").setValue(8);
  name(w, "Updated base");
  CHECK(snapshot(w) == before);
  REQUIRE(w.viewport().feature_preview_active());
  REQUIRE(apply(w).isEnabled());
  capture(w, "feature-edit-upstream");
  REQUIRE(tools.apply());
  const auto& part = *w.session().part_document();
  CHECK(part.features().size() == 1);
  CHECK(part.fillet_features().size() == 1);
  CHECK(part.features().front().id() == FeatureId("feature.base"));
  CHECK(part.features().front().name() == "Updated base");
  CHECK(part.find_parameter(ParameterId("depth"))->value().millimeters() == 8);
  REQUIRE(w.session().part_shape_cache()->find_body_result(BodyId("body.base")));
  history(w, before);
}
TEST_CASE("Shell feature edit rejects downstream geometry failures and restores filters on Cancel",
          "[gui][block136-feature-edit]") {
  ShellWindow w;
  fixture(w);
  const auto before = snapshot(w);
  const auto mask = w.viewport().selection_filter_mask();
  const auto scope = w.viewport().selection_semantic_scope();
  auto& tools = w.feature_edit_tools();
  REQUIRE(tools.begin(FeatureId("feature.base")));
  number(w, "/length_parameter").setValue(0.1);
  CHECK_FALSE(apply(w).isEnabled());
  CHECK_FALSE(tools.apply());
  CHECK(snapshot(w) == before);
  CHECK(tools.active());
  number(w, "/length_parameter").setValue(9);
  REQUIRE(apply(w).isEnabled());
  tools.cancel();
  CHECK(snapshot(w) == before);
  CHECK_FALSE(w.viewport().feature_preview_active());
  CHECK(w.viewport().selection_filter_mask() == mask);
  CHECK(w.viewport().selection_semantic_scope() == scope);
}
TEST_CASE("Shell feature edit rebinds parameters and preserves expression driven values",
          "[gui][block136-feature-edit]") {
  const bool rebind = GENERATE(false, true);
  ShellWindow w;
  fixture(w, false, true);
  auto& tools = w.feature_edit_tools();
  const auto before = snapshot(w);
  REQUIRE(tools.begin(FeatureId("feature.base")));
  number(w, "/length_parameter").setValue(6);
  REQUIRE(apply(w).isEnabled());
  // A display name matching a parameter id is not a parameter dependency.
  name(w, "depth");
  if (!rebind) {
    REQUIRE(tools.apply());
    CHECK(w.session()
              .part_document()
              ->find_parameter(ParameterId("computed"))
              ->value()
              .millimeters() == 12);
    history(w, before);
    return;
  }
  auto* binding = w.findChild<QComboBox*>("blcad.feature_edit.binding/length_parameter");
  REQUIRE(binding);
  binding->setCurrentIndex(binding->findData("computed"));
  CHECK_FALSE(number(w, "/length_parameter").isEnabled());
  CHECK(number(w, "/length_parameter").value() == 10);
  REQUIRE(tools.apply());
  CHECK(w.session().part_document()->features().front().length_parameter() ==
        ParameterId("computed"));
  CHECK(w.session().part_document()->find_parameter(ParameterId("depth"))->value().millimeters() ==
        5);
  history(w, before);
}
TEST_CASE("Shell feature edit changes ordered sections and edge membership through typed controls",
          "[gui][block136-feature-edit]") {
  bool loft = false;
  SECTION("Loft section order") {
    loft = true;
  }
  SECTION("Fillet edge membership") {}
  ShellWindow w;
  auto id = family_fixture(w, loft ? GuiFeatureEditKind::Loft : GuiFeatureEditKind::Fillet);
  const auto before = snapshot(w);
  auto& tools = w.feature_edit_tools();
  REQUIRE(tools.begin(id));
  if (loft) {
    auto* down = w.findChild<QPushButton*>(QString::fromUtf8("blcad.feature_edit.↓/sections/0"));
    REQUIRE(down);
    down->click();
    qApp->processEvents();
    auto* sketch = qobject_cast<QComboBox*>(tools.field("/sections/0/sketch"));
    REQUIRE(sketch);
    CHECK(sketch->currentData().toString() == "upper");
  } else {
    auto* add = w.findChild<QPushButton*>("blcad.feature_edit.add/edges");
    REQUIRE(add);
    add->click();
    qApp->processEvents();
    CHECK_FALSE(apply(w).isEnabled());
    auto* edge = qobject_cast<QComboBox*>(tools.field("/edges/1/edge"));
    REQUIRE(edge);
    const int back =
        edge->findData(QString::fromStdString(std::string(to_string(SemanticEdge::TopBack))));
    REQUIRE(back >= 0);
    edge->setCurrentIndex(back);
    REQUIRE(apply(w).isEnabled());
    auto* remove = w.findChild<QPushButton*>("blcad.feature_edit.Entfernen/edges/0");
    REQUIRE(remove);
    remove->click();
    qApp->processEvents();
    // An empty required list must still permit recovery inside the same edit.
    remove = w.findChild<QPushButton*>("blcad.feature_edit.Entfernen/edges/0");
    REQUIRE(remove);
    remove->click();
    qApp->processEvents();
    CHECK_FALSE(apply(w).isEnabled());
    add = w.findChild<QPushButton*>("blcad.feature_edit.add/edges");
    REQUIRE(add);
    REQUIRE(add->isEnabled());
    add->click();
    qApp->processEvents();
    REQUIRE(apply(w).isEnabled());
  }
  CHECK(snapshot(w) == before);
  REQUIRE(tools.apply());
  if (loft)
    CHECK(std::get<ProfileRegionReference>(
              w.session().part_document()->loft_features().front().sections().front().source())
              .sketch() == SketchId("upper"));
  else
    CHECK(std::get<SemanticEdgeReference>(
              w.session().part_document()->fillet_features().front().edges().front().source())
              .edge() == SemanticEdge::TopBack);
  history(w, before);
}
TEST_CASE(
    "Shell feature edit enables an optional Sweep twist without changing unrelated parameters",
    "[gui][block136-feature-edit]") {
  ShellWindow w;
  auto id = family_fixture(w, GuiFeatureEditKind::Sweep);
  const auto before = snapshot(w);
  auto& tools = w.feature_edit_tools();
  REQUIRE(tools.begin(id));
  auto* enable = w.findChild<QCheckBox*>("blcad.feature_edit.enable/twist_parameter");
  REQUIRE(enable);
  CHECK_FALSE(enable->isChecked());
  enable->setChecked(true);
  qApp->processEvents();
  number(w, "/twist_parameter").setValue(30);
  REQUIRE(apply(w).isEnabled());
  CHECK(snapshot(w) == before);
  REQUIRE(tools.apply());
  CHECK(w.session().part_document()->sweep_features().front().twist_parameter() ==
        ParameterId("twist"));
  CHECK(w.session().part_document()->find_parameter(ParameterId("twist"))->value().degrees() == 30);
  history(w, before);
}
TEST_CASE("Shell feature edit rejects stale documents and refuses conflicting tasks",
          "[gui][block136-feature-edit]") {
  ShellWindow w;
  fixture(w);
  auto& tools = w.feature_edit_tools();
  CHECK_FALSE(tools.begin(FeatureId("missing")));
  REQUIRE(tools.begin(FeatureId("feature.fillet")));
  CHECK_FALSE(w.feature_tools().begin_extrude());
  const auto before = snapshot(w);
  REQUIRE(w.session().commit_part_transaction("External edit", [](PartDocument& part) {
    auto result = part.set_parameter_value(ParameterId("width"), Quantity::length_mm(21).value());
    return result ? Result<std::size_t>::success(1) : Result<std::size_t>::failure(result.error());
  }));
  const auto external = snapshot(w);
  CHECK(before != external);
  CHECK_FALSE(tools.apply());
  w.refresh();
  CHECK_FALSE(tools.active());
  CHECK(snapshot(w) == external);
  REQUIRE(tools.begin(FeatureId("feature.fillet")));
  number(w, "/radius_parameter").setValue(1.2);
  REQUIRE(w.undo());
  CHECK_FALSE(tools.active());
  CHECK(snapshot(w) == before);
}
TEST_CASE("Native browser double click edits existing feature with drag numeric Escape and "
          "reachable Apply",
          "[gui][block136-feature-edit][native-feature-edit]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT feature edit handles");
  ShellWindow w;
  fixture(w);
  const auto before = snapshot(w);
  auto& tree = w.model_browser();
  tree.expandAll();
  auto* item = tree_item(tree.topLevelItem(0), "feature.base");
  REQUIRE(item);
  tree.scrollToItem(item);
  qApp->processEvents();
  const QPointF point = tree.visualItemRect(item).center();
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease,
                    QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, point, tree.viewport()->mapToGlobal(point.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(tree.viewport(), &event);
    qApp->processEvents();
  }
  auto& tools = w.feature_edit_tools();
  REQUIRE(tools.active());
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  auto presentations = tools.manipulators().presentations();
  REQUIRE(presentations.size() == 1);
  auto endpoint = presentations.front().endpoint;
  auto send = [&](QEvent::Type type, GuiSketchScreenPoint p) {
    const QPointF local(p.x, p.y);
    QMouseEvent event(type, local, w.viewport().mapToGlobal(local.toPoint()),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&w.viewport(), &event);
    qApp->processEvents();
  };
  send(QEvent::MouseButtonPress, endpoint);
  REQUIRE_FALSE(tools.manipulators().active_handle_id().empty());
  send(QEvent::MouseMove, {endpoint.x, endpoint.y - 12});
  REQUIRE(tools.set_drag_text("9 mm"));
  send(QEvent::MouseButtonRelease, {endpoint.x, endpoint.y - 12});
  CHECK(number(w, "/length_parameter").value() == 9);
  endpoint = tools.manipulators().presentations().front().endpoint;
  send(QEvent::MouseButtonPress, endpoint);
  REQUIRE(tools.set_drag_text("12 mm"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&w.viewport(), &escape);
  CHECK(number(w, "/length_parameter").value() == 9);
  CHECK(snapshot(w) == before);
  auto& button = apply(w);
  REQUIRE(button.isVisible());
  CHECK(w.childAt(button.mapTo(&w, button.rect().center())) == &button);
  capture(w, "native-feature-edit");
  button.click();
  REQUIRE_FALSE(tools.active());
  history(w, before);
}
