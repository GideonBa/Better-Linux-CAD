#include "../../src/gui/shell/sketch_driven_controls.hpp"
#include "../support/part_history_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
TEST_CASE("Sketch-driven controls retain bound dimensions and inactive mode values",
          "[gui][sketch-driven-controls][parity-b184]") {
  QWidget parent;
  detail::SketchDrivenControls ui(&parent);
  auto p = blcad::test::history_fixture::native_base();
  ui.bind_catalog(p, ui.size, ParameterId("width"));
  CHECK(ui.size.value->text() == "20");
  CHECK_FALSE(ui.size.value->isReadOnly());
  ui.size.value->setText("21.23456789012345");
  ui.mode->setCurrentIndex(2);
  ui.source_kind->setCurrentIndex(2);
  ui.text->setText("Part ${n}");
  ui.render();
  ui.mode->setCurrentIndex(0);
  ui.render();
  CHECK(ui.size.value->text() == "21.23456789012345");
  CHECK(ui.text->text() == "Part ${n}");
  ui.bind_catalog(p, ui.draft, ParameterId("angle"));
  CHECK(ui.draft.value->text() == "2");
  CHECK(ui.draft.type == ParameterType::Angle);
  CHECK(ui.size.type == ParameterType::Length);
  CHECK(ui.fields.at("apply")->parentWidget() == ui.panel);
  CHECK(ui.fields.at("cancel")->parentWidget() == ui.panel);
}
