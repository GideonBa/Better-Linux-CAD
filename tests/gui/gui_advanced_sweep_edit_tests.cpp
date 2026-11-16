#include "../support/curved_surface_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "../support/spatial_constraint_feature_fixture.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
using namespace blcad;
using namespace blcad::gui;
namespace f = blcad::test::curved_surface;
namespace sf = blcad::test::spatial_feature_fixture;
namespace {
std::string saved(ShellWindow& w) {
  return blcad::test::surface_offset::saved(*w.session().part_document());
}
template <class T> T& control(ShellWindow& w, const char* name) {
  auto* p = w.findChild<T*>(name);
  REQUIRE(p);
  return *p;
}
} // namespace
TEST_CASE("Shared Sweep editing retains material policy and precise up vector across independent "
          "twist overrides",
          "[gui][advanced-sweep-edit][parity-b173]") {
  ShellWindow w;
  REQUIRE(w.new_part("Sweep edit"));
  REQUIRE(w.session().commit_part_transaction("Inputs", [](PartDocument& p) {
    p = sf::straight_sweep();
    f::checked(p.set_path_execution_policy(
        PathCurveId("driven.path"), f::checked(PathExecutionPolicy::curve_preserving(1e-7))));
    f::checked(p.add_parameter(
        f::checked(Parameter::create_angle(ParameterId("twist.original"), "original_twist",
                                           f::checked(Quantity::angle_deg(12.3456789012345))))));
    const auto old = *p.find_sweep_feature(FeatureId("driven.sweep"));
    const auto profile = f::checked(SweepProfileReference::create_closed_region(
        std::get<ProfileRegionReference>(old.profile().source()),
        SweepRegionPolicy::MaterialRegion));
    return p.update_sweep_feature(f::checked(SweepFeature::create_sweep(
        old.id(), old.name(), profile, old.path(), old.body_result_context(),
        PathOrientationRule::FixedUpVector, Vector3{1, 0, 1e-17}, ParameterId("twist.original"))));
  }));
  w.refresh();
  const auto before = saved(w);
  REQUIRE(w.feature_edit_tools().begin(FeatureId("driven.sweep")));
  REQUIRE(w.feature_tools().active());
  CHECK(control<QComboBox>(w, "blcad.sweep.region_policy").currentIndex() == 1);
  CHECK(control<QLineEdit>(w, "blcad.spatial.up_2").text().toDouble() == 1e-17);
  REQUIRE(w.feature_tools().apply());
  CHECK(saved(w) == before);
  for (double angle : {15.00000000012345, 16.00000000012345}) {
    REQUIRE(w.feature_edit_tools().begin(FeatureId("driven.sweep")));
    const auto accepted = saved(w);
    control<QCheckBox>(w, "blcad.sweep.independent_twist").setChecked(true);
    REQUIRE(w.feature_tools().set_numeric_value(angle));
    CHECK(saved(w) == accepted);
    REQUIRE(w.feature_tools().apply());
    const auto* p = w.session().part_document();
    const auto& sweep = *p->find_sweep_feature(FeatureId("driven.sweep"));
    REQUIRE(sweep.twist_parameter());
    CHECK(p->find_parameter(*sweep.twist_parameter())->value().degrees() == angle);
    CHECK(sweep.profile().region_policy() == SweepRegionPolicy::MaterialRegion);
    CHECK(p->find_path_curve(sweep.path())->execution_policy().kind() ==
          PathGeometryPolicyKind::CurvePreserving);
    CHECK(sweep.fixed_up_vector_override() == Vector3{1, 0, 1e-17});
    CHECK(p->find_parameter(ParameterId("twist.original"))->value().degrees() == 12.3456789012345);
    CHECK(blcad::test::integrated_solid_volume(
              *w.session().part_shape_cache()->find_body_shape(BodyId("driven.body")))
              .volume_mm3 == Catch::Approx(6.4).margin(1e-5));
  }
  std::set<std::string> names;
  for (const auto& parameter : w.session().part_document()->parameters())
    CHECK(names.insert(parameter.name()).second);
  const auto after = saved(w);
  REQUIRE(w.undo());
  CHECK(saved(w) != after);
  REQUIRE(w.session().redo());
  CHECK(saved(w) == after);
}
