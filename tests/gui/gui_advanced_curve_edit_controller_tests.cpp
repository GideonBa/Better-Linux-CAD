#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/geometry/loft_adapter.hpp"
#include "blcad/gui/gui_interactive_path_sweep_loft.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
std::string saved(const GuiDocumentSession& s) {
  return blcad::test::surface_offset::saved(*s.part_document());
}
} // namespace
TEST_CASE("Shared Sweep editing retains every Body result operation and exact source intent",
          "[gui][advanced-curve-edit-controller][parity-b173]") {
  for (const auto mode : {FeatureBodyOperationMode::NewBody, FeatureBodyOperationMode::Join,
                          FeatureBodyOperationMode::Cut, FeatureBodyOperationMode::Intersect}) {
    GuiDocumentSession session;
    REQUIRE(session.create_part(DocumentId("curve.edit"), "Curve edit"));
    REQUIRE(session.commit_part_transaction("Inputs", [&](PartDocument& p) {
      p = sf::straight_sweep();
      const auto old = *p.find_sweep_feature(FeatureId("driven.sweep"));
      const auto context =
          mode == FeatureBodyOperationMode::NewBody
              ? old.body_result_context()
              : f::checked(FeatureBodyResultContext::create(mode, BodyId("base.body"), {}));
      const auto create = mode == FeatureBodyOperationMode::Cut ? SweepFeature::create_sweep_cut
                                                                : SweepFeature::create_sweep;
      f::checked(p.update_sweep_feature(f::checked(
          create(old.id(), old.name(), old.profile(), old.path(), context, {}, {}, {}, {}))));
      if (mode != FeatureBodyOperationMode::NewBody)
        f::checked(p.remove_body(BodyId("driven.body")));
      return Result<std::size_t>::success(1);
    }));
    const auto before = saved(session);
    GuiInteractivePathSweepController edit;
    REQUIRE(edit.begin_edit(session, FeatureId("driven.sweep")));
    CHECK(edit.editing());
    const auto shape = f::checked(edit.preview_shape(session));
    const double expected = mode == FeatureBodyOperationMode::NewBody ? 6.4
                            : mode == FeatureBodyOperationMode::Join  ? 243.2
                            : mode == FeatureBodyOperationMode::Cut   ? 236.8
                                                                      : 3.2;
    CHECK(blcad::test::integrated_solid_volume(shape).volume_mm3 ==
          Catch::Approx(expected).margin(1e-5));
    CHECK(saved(session) == before);
    REQUIRE(edit.apply(session));
    CHECK(saved(session) == before);
    CHECK(session.part_document()
              ->find_sweep_feature(FeatureId("driven.sweep"))
              ->body_result_context()
              .operation_mode() == mode);
    REQUIRE(session.undo());
    CHECK(saved(session) == before);
  }
}
TEST_CASE(
    "Sweep editing retains orientation and bound twist until explicit independent replacement",
    "[gui][advanced-curve-edit-controller][parity-b173]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("twist.edit"), "Twist edit"));
  REQUIRE(session.commit_part_transaction("Inputs", [](PartDocument& p) {
    p = sf::straight_sweep();
    f::checked(p.add_parameter(
        f::checked(Parameter::create_angle(ParameterId("twist.driver"), "twist_driver",
                                           f::checked(Quantity::angle_deg(12.3456789012345))))));
    const auto old = *p.find_sweep_feature(FeatureId("driven.sweep"));
    return p.update_sweep_feature(f::checked(SweepFeature::create_sweep(
        old.id(), old.name(), old.profile(), old.path(), old.body_result_context(),
        PathOrientationRule::FixedUpVector, Vector3{1, 0, 0}, ParameterId("twist.driver"))));
  }));
  const auto before = saved(session);
  GuiInteractivePathSweepController edit;
  REQUIRE(edit.begin_edit(session, FeatureId("driven.sweep")));
  CHECK(edit.twist() == 12.3456789012345);
  REQUIRE(edit.apply(session));
  CHECK(saved(session) == before);
  REQUIRE(edit.begin_edit(session, FeatureId("driven.sweep")));
  edit.set_twist(ParameterId("twist.driver"), 15);
  const auto independent = f::checked(Parameter::create_angle(
      ParameterId("twist.independent"), "independent_twist", f::checked(Quantity::angle_deg(15))));
  edit.set_pending_twist(independent);
  edit.set_twist(independent.id(), 15);
  REQUIRE(edit.apply(session));
  const auto* p = session.part_document();
  const auto& sweep = *p->find_sweep_feature(FeatureId("driven.sweep"));
  CHECK(sweep.orientation_override() == PathOrientationRule::FixedUpVector);
  CHECK(sweep.fixed_up_vector_override() == Vector3{1, 0, 0});
  CHECK(sweep.twist_parameter() == independent.id());
  CHECK(p->find_parameter(ParameterId("twist.driver"))->value().degrees() == 12.3456789012345);
}
TEST_CASE("Loft editing retains verified boundary constraints and rejects global G2 atomically",
          "[gui][advanced-curve-edit-controller][parity-b173]") {
  GuiDocumentSession session;
  REQUIRE(session.create_part(DocumentId("loft.edit"), "Loft edit"));
  REQUIRE(session.commit_part_transaction("Inputs", [](PartDocument& p) {
    p = blcad::test::constrained_airfoil_fixture();
    return Result<std::size_t>::success(1);
  }));
  const auto before = saved(session);
  GuiInteractiveLoftController edit;
  REQUIRE(edit.begin_edit(session, FeatureId("main")));
  CHECK(edit.editing());
  CHECK_FALSE(edit.auto_alignment());
  REQUIRE(edit.sections().size() == 3);
  REQUIRE(edit.set_sections(edit.sections()));
  const auto shape = f::checked(edit.preview_shape(session));
  const auto report = geometry::LoftAdapter::verification(shape);
  REQUIRE(report);
  CHECK(report->g2_joins == 4);
  CHECK(report->normal_error_rad <= report->angular_tolerance_rad);
  CHECK(report->curvature_error_per_mm <= report->curvature_tolerance_per_mm);
  REQUIRE(edit.apply(session));
  CHECK(saved(session) == before);
  REQUIRE(edit.begin_edit(session, FeatureId("main")));
  edit.set_continuity(LoftContinuity::G2);
  CHECK_FALSE(edit.apply(session));
  CHECK(saved(session) == before);
  CHECK(edit.active());
  edit.set_continuity(LoftContinuity::C0);
  REQUIRE(edit.apply(session));
  CHECK(saved(session) == before);
}
