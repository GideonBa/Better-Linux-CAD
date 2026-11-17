#include "../../src/gui/shell/advanced_fillet_controls.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include <QApplication>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::history_fixture;
namespace {
EdgeReference seed() {
  return f::checked(EdgeReference::create_linear(
      PartFeatureInputRole::FilletEdge,
      f::checked(SemanticEdgeReference::create(FeatureId("base"), SemanticEdge::TopFront))));
}
FilletFeature variable() {
  VariableFilletIntent value{
      {{"group",
        "Outer",
        {{seed(), false}},
        FilletPropagation::Explicit,
        FilletRadiusInterpolation::Linear,
        {{"start", FilletStationLocation::Start, 0, ParameterId("radius")},
         {"tiny", FilletStationLocation::Fraction, 1e-20, ParameterId("radius")},
         {"end", FilletStationLocation::End, 1, ParameterId("radius")}}}}};
  return f::checked(
      FilletFeature::create_variable(FeatureId("finish"), "Variable", BodyId("body.base"), value));
}
FaceReference face(SemanticFace side, PartFeatureInputRole role) {
  return f::checked(FaceReference::create_planar(
      role, f::checked(SemanticFaceReference::create(FeatureId("base"), side))));
}
FilletFeature round() {
  FullRoundIntent value{
      {"first", "Front", {face(SemanticFace::Front, PartFeatureInputRole::FilletSupportFace)}},
      {"second", "Back", {face(SemanticFace::Back, PartFeatureInputRole::FilletSupportFace)}},
      {"middle", "Top", {face(SemanticFace::Top, PartFeatureInputRole::FilletMiddleFace)}},
      FilletContinuity::Curvature,
      {}};
  return f::checked(FilletFeature::create_full_round(FeatureId("finish"), "Full round",
                                                     BodyId("body.base"), value));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
} // namespace
TEST_CASE("Owned Fillet controls preserve exact untouched stations and stable groups",
          "[gui][advanced-fillet-controls][parity-b179]") {
  QWidget parent;
  QComboBox mode(&parent);
  mode.addItems({"Constant", "Variable", "Face", "Full Round"});
  int changed = 0;
  AdvancedFilletControls controls(&parent, &mode, [&] { ++changed; }, [] {});
  auto p = f::native_base();
  REQUIRE(p.add_fillet_feature(variable()));
  const auto before = saved(p);
  controls.load(p, *p.find_fillet_feature(FeatureId("finish")));
  REQUIRE(mode.currentIndex() == 1);
  REQUIRE(controls.stations->rowCount() == 3);
  auto* fraction = qobject_cast<QLineEdit*>(controls.stations->cellWidget(1, 1));
  REQUIRE(fraction);
  CHECK(fraction->text().toDouble() == 1e-20);
  REQUIRE(QMetaObject::invokeMethod(fraction, "editingFinished", Qt::DirectConnection));
  CHECK(changed == 0);
  auto draft = f::checked(controls.build());
  auto candidate = p;
  REQUIRE(draft->apply(candidate));
  CHECK(saved(candidate) == before);
  auto& group = controls.groups[0];
  group = reverse_fillet_group(group);
  group = reverse_fillet_group(group);
  controls.configure();
  draft = f::checked(controls.build());
  candidate = p;
  REQUIRE(draft->apply(candidate));
  CHECK(saved(candidate) == before);
  controls.corner->setChecked(true);
  auto missing_groups = controls.build();
  REQUIRE_FALSE(missing_groups);
  CHECK_FALSE(controls.live);
  CHECK(missing_groups.error().message().find("three edge groups") != std::string::npos);
}
TEST_CASE("Fillet mode controls retain face roles and exclude inactive scalar bindings",
          "[gui][advanced-fillet-controls][parity-b179]") {
  QWidget parent;
  QComboBox mode(&parent);
  mode.addItems({"Constant", "Variable", "Face", "Full Round"});
  AdvancedFilletControls controls(&parent, &mode, [] {}, [] {});
  auto p = f::native_base();
  REQUIRE(p.add_fillet_feature(round()));
  const auto before = saved(p);
  controls.load(p, *p.find_fillet_feature(FeatureId("finish")));
  CHECK(mode.currentIndex() == 3);
  auto initial = f::checked(controls.build());
  CHECK(initial->pending.empty());
  CHECK(initial->record.parameter_dependencies().empty());
  auto candidate = p;
  REQUIRE(initial->apply(candidate));
  CHECK(saved(candidate) == before);
  mode.setCurrentIndex(2);
  controls.configure();
  auto face = f::checked(controls.build());
  CHECK(std::holds_alternative<FaceBlendIntent>(face->record.advanced_intent()->definition));
  CHECK(face->record.faces().size() == 2);
  CHECK(face->pending.size() == 1);
  mode.setCurrentIndex(3);
  controls.configure();
  auto full = f::checked(controls.build());
  CHECK(full->pending.empty());
  CHECK(full->values.empty());
  CHECK(full->record.parameter_dependencies().empty());
  candidate = p;
  REQUIRE(full->apply(candidate));
  CHECK(saved(candidate) == before);
}
TEST_CASE("Read-only law direction guides draw labels without intercepting station drags",
          "[gui][advanced-fillet-controls][parity-b179]") {
  GuiViewportCameraBookmark camera;
  camera.eye = {0, 0, 100};
  camera.target = {};
  camera.up_direction = {0, 1, 0};
  camera.scale = 200;
  camera.projection = GuiViewportProjection::Orthographic;
  GuiViewportManipulatorLayer layer;
  REQUIRE(layer.set_mapping(
      f::checked(GuiViewportManipulatorMapping::create_camera(camera, 200, 200))));
  GuiViewportManipulatorHandle direction;
  direction.id = "direction";
  direction.interactive = false;
  direction.label = "Law Direction";
  GuiViewportManipulatorHandle radius;
  radius.id = "radius";
  radius.axis = {0, 1, 0};
  radius.reference_value = 1;
  REQUIRE(layer.set_handles({direction, radius}));
  const auto presentations = layer.presentations();
  REQUIRE(presentations.size() == 2);
  CHECK_FALSE(presentations[0].interactive);
  CHECK(presentations[0].value_text == "Law Direction");
  CHECK(layer.hits_at(presentations[0].endpoint).empty());
  CHECK_FALSE(layer.begin_drag(presentations[0].endpoint));
  auto hits = layer.hits_at(presentations[0].anchor);
  REQUIRE(hits.size() == 1);
  CHECK(hits[0].handle_id == "radius");
}
