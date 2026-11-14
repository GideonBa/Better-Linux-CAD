#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/gui/gui_document_session.hpp"
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::spatial_fixture;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
std::string snapshot(const GuiDocumentSession& session) {
  return f::checked(serialize_part_document_to_json(*session.part_document()));
}
double volume(const GuiDocumentSession& session) {
  const auto* body = session.part_shape_cache()->find_body_shape(BodyId("driven.body"));
  REQUIRE(body);
  return geometry::RectangleExtrusionAdapter{}.summarize(*body).volume_mm3;
}
} // namespace
TEST_CASE("Spatial parameter edits replay actual geometry and exact source history after reopening",
          "[gui][parity-b165][spatial-parametric-session]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("spatial.part"), "Spatial"));
  REQUIRE(session.commit_part_transaction("Source", [](PartDocument& part) {
    part = sf::straight_sweep();
    return Result<std::size_t>::success(1);
  }));
  const auto before = snapshot(session);
  CHECK(volume(session) == Catch::Approx(.64 * 10).margin(1e-6));
  REQUIRE(session.commit_part_transaction("Length", [](PartDocument& part) {
    const auto changed = part.set_parameter_value(ParameterId("spatial.length"),
                                                  f::checked(Quantity::length_mm(15)));
    return changed ? Result<std::size_t>::success(changed.value().size())
                   : Result<std::size_t>::failure(changed.error());
  }));
  const auto after = snapshot(session);
  CHECK(after != before);
  CHECK(volume(session) == Catch::Approx(.64 * 15).margin(1e-6));
  CHECK(session.part_document()
            ->find_sketch_3d(Sketch3DId("driven"))
            ->find_point(SketchEntityId("b"))
            ->z()
            .explicit_coordinate()
            ->millimeters() == 10);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(volume(session) == Catch::Approx(.64 * 10).margin(1e-6));
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  CHECK(volume(session) == Catch::Approx(.64 * 15).margin(1e-6));
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("driven.blcad.json").toStdString();
  REQUIRE(session.save_as(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path));
  CHECK(snapshot(reopened) == after);
  CHECK(volume(reopened) == Catch::Approx(.64 * 15).margin(1e-6));
}
TEST_CASE("Unconsumed spatial conflicts reject parameter publication without adding history",
          "[gui][parity-b165][spatial-parametric-rollback]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("spatial.part"), "Spatial"));
  const auto empty = snapshot(session);
  REQUIRE(session.commit_part_transaction("Source", [](PartDocument& part) {
    part = f::part();
    auto owner = f::checked(Sketch3D::create(Sketch3DId("fixed.owner"), "Fixed"));
    auto point = f::checked(SketchPoint3D::create(
        SketchEntityId("p"),
        f::checked(SketchCoordinate3D::create_parameter(ParameterId("coordinate.x"))),
        f::coordinate(0), f::coordinate(0)));
    (void)f::checked(owner.add_point(point));
    (void)f::checked(owner.add_constraint(f::checked(
        SpatialConstraint::create(SketchConstraintId("fixed"), SpatialConstraintKind::Fixed,
                                  {f::target("p")}, {}, Point3{8, 0, 0}))));
    return part.add_sketch_3d(owner);
  }));
  const auto before = snapshot(session);
  const auto revision = session.model_revision();
  CHECK_FALSE(session.commit_part_transaction("Impossible", [](PartDocument& part) {
    auto changed =
        part.set_parameter_value(ParameterId("coordinate.x"), f::checked(Quantity::length_mm(9)));
    return changed ? Result<std::size_t>::success(1)
                   : Result<std::size_t>::failure(changed.error());
  }));
  CHECK(snapshot(session) == before);
  // Rejected publication records a diagnostic and invalidates borrowed task contexts.
  // It must not change the document, geometry or undo history.
  CHECK(session.model_revision() > revision);
  CHECK(session.part_shape_cache()->find_feature_shape(FeatureId("base.feature")));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == empty);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == before);
}
