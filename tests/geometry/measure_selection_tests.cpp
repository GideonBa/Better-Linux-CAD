#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/measure_selection.hpp"
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::geometry;
TEST_CASE("Measure catalogue resolves current world geometry with disposable identities",
          "[geometry][block136-measure]") {
  auto box = RectangleExtrusionAdapter{}.make_extruded_rectangle(Quantity::length_mm(10).value(),
                                                                 Quantity::length_mm(20).value(),
                                                                 Quantity::length_mm(30).value());
  REQUIRE(box);
  auto shifted = BodyTransformAdapter{}.translate(box.value(), {100, 200, 300});
  REQUIRE(shifted);
  const std::vector<MeasureSelectionOwner> owners{
      {"body/box", "Box", shifted.value(), BodyId("box")}};
  auto selected = MeasureSelectionBuilder::build(owners, 7);
  REQUIRE(selected);
  CHECK(selected.value().size() == 27);
  for (auto kind :
       {MeasureSelectionKind::Point, MeasureSelectionKind::Line, MeasureSelectionKind::Plane}) {
    CHECK(std::count_if(selected.value().begin(), selected.value().end(),
                        [=](const auto& target) { return target.kind == kind; }) ==
          (kind == MeasureSelectionKind::Point  ? 8
           : kind == MeasureSelectionKind::Line ? 12
                                                : 6));
  }
  for (const auto& target : selected.value()) {
    CHECK_FALSE(target.shape.empty());
    if (!target.descriptor) {
      CHECK(target.body == BodyId("box"));
      continue;
    }
    CHECK(target.semantic_id.starts_with("measure/7/"));
    CHECK(target.anchor.x >= 94.999);
    CHECK(target.anchor.y >= 189.999);
    CHECK(target.anchor.z >= 299.999);
  }
  auto later = MeasureSelectionBuilder::build(owners, 8);
  REQUIRE(later);
  CHECK(later.value().at(1).semantic_id != selected.value().at(1).semantic_id);
  CHECK_FALSE(MeasureSelectionBuilder::build({owners.front(), owners.front()}, 7));
  CHECK_FALSE(MeasureSelectionBuilder::build({{"empty", "Empty", {}, {}}}, 7));
}
TEST_CASE("Intersecting measure planes publish a witness on both supporting planes",
          "[geometry][block136-measure]") {
  const AssemblyPlanarTargetDescriptor first{{0, 0, 3}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  const AssemblyPlanarTargetDescriptor second{{7, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
  auto distance = GeometryMeasureQuery{}.distance(first, second);
  REQUIRE(distance);
  CHECK(distance.value().distance_mm == 0);
  CHECK(distance.value().witness_a.x == Catch::Approx(7));
  CHECK(distance.value().witness_a.z == Catch::Approx(3));
  CHECK(distance.value().witness_b.x == Catch::Approx(7));
  CHECK(distance.value().witness_b.z == Catch::Approx(3));
}
