#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <TopoDS_Shape.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace resources = blcad::test::engineering_fixture;
namespace history = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> value) {
  INFO((value ? std::string{} : value.error().message()));
  REQUIRE(value);
  return std::move(value.value());
}
PartDocument part(bool authored = false) {
  auto p = history::native_base();
  resources::annotate(p);
  if (authored)
    REQUIRE(p.apply_history_intent({{"base"}, {}}));
  return p;
}
ShapeCache cache(const PartDocument& p) {
  auto c = checked(ShapeCache::create(ShapeCacheId("physical.reuse")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(p, c));
  REQUIRE(c.validate_complete_geometry_result(p));
  return c;
}
Catch::Approx approximately(double n) {
  return Catch::Approx(n).epsilon(1e-9).margin(1e-10);
}
} // namespace

TEST_CASE("Material-only refresh retains identical BRep handles and reweights immutable physical "
          "geometry",
          "[geometry][parity-b150][physical-reuse]") {
  for (bool authored : {false, true}) {
    DYNAMIC_SECTION("authored history " << authored) {
      const auto before = part(authored);
      auto c = cache(before);
      const auto old_shape = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
      const auto integrated = checked(BodyResultInspector{}.inspect_physical_properties(before));
      CHECK_FALSE(integrated.data().geometry_reused);
      auto after = before;
      REQUIRE(after.set_default_material(resources::key("synthetic.material", "r2")));
      const auto refreshed =
          checked(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
      CHECK(refreshed.executed_feature_count == 0);
      CHECK(refreshed.reused_complete_geometry);
      REQUIRE(c.validate_complete_geometry_result(after));
      CHECK_FALSE(c.validate_complete_geometry_result(before));
      CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(old_shape));
      const auto result =
          checked(BodyResultInspector{}.inspect_physical_properties(after, integrated));
      CHECK(result.data().geometry_reused);
      CHECK(result.data().complete_mass()->mass_kg == approximately(.0096));
      CHECK(result.data().volume_mm3 == integrated.data().volume_mm3);
      CHECK(result.data().area_mm2 == integrated.data().area_mm2);
      CHECK(result.data().complete_mass()->centroid_mm.z == approximately(4));
      CHECK(result.data().complete_mass()->central_inertia_kg_mm2[0] ==
            approximately(.0096 * 164 / 12));
      REQUIRE(result.validate_current(after));
      REQUIRE(integrated.validate_current(before));
      CHECK_FALSE(integrated.validate_current(after));
      REQUIRE(after.set_default_material(std::nullopt));
      const auto unavailable =
          checked(BodyResultInspector{}.inspect_physical_properties(after, result));
      CHECK(unavailable.data().mass_status == PhysicalMassStatus::Unavailable);
      CHECK_FALSE(unavailable.data().known_mass);
      REQUIRE(after.set_body_material(BodyId("body.base"), resources::key()));
      const auto overridden =
          checked(BodyResultInspector{}.inspect_physical_properties(after, unavailable));
      CHECK(overridden.data().complete_mass()->mass_kg == approximately(.0064));
      REQUIRE(after.set_body_material(BodyId("body.base"), std::nullopt));
      const auto no_density = checked(EngineeringMaterial::create(
          resources::key("no-density"), "No density", resources::provenance(), {}));
      REQUIRE(after.add_engineering_resource(no_density));
      REQUIRE(after.set_default_material(no_density.key()));
      const auto missing =
          checked(BodyResultInspector{}.inspect_physical_properties(after, overridden));
      CHECK(missing.data().mass_status == PhysicalMassStatus::Unavailable);
      CHECK(missing.data().bodies.front().mass_status == BodyPhysicalMassStatus::MissingDensity);
    }
  }
}

TEST_CASE(
    "Material-only reuse crosses resource-free legacy persistence without hiding table changes",
    "[geometry][parity-b150][physical-reuse]") {
  const auto before = history::native_base();
  const auto original = checked(serialize_part_document_to_json(before));
  const auto integrated = checked(BodyResultInspector{}.inspect_physical_properties(before));
  auto after = before;
  REQUIRE(after.add_engineering_resource(resources::material()));
  REQUIRE(after.set_default_material(resources::key()));
  const auto assigned =
      checked(BodyResultInspector{}.inspect_physical_properties(after, integrated));
  CHECK(assigned.data().complete_mass()->mass_kg == approximately(.0064));
  auto c = cache(before);
  REQUIRE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  const auto materialized = after;
  REQUIRE(after.set_default_material(std::nullopt));
  REQUIRE(after.remove_engineering_resource(resources::key()));
  CHECK(checked(serialize_part_document_to_json(after)) == original);
  const auto cleared = checked(BodyResultInspector{}.inspect_physical_properties(after, assigned));
  CHECK(cleared.data().mass_status == PhysicalMassStatus::Unavailable);
  REQUIRE(GeometryRecomputeExecutor{}.refresh_material_metadata(materialized, after, c));
  REQUIRE(c.validate_complete_geometry_result(before));
}

TEST_CASE(
    "Physical reuse rejects geometry, table and lifetime changes without modifying prior results",
    "[geometry][parity-b150][physical-reuse]") {
  const auto before = part();
  auto c = cache(before);
  const auto integrated = checked(BodyResultInspector{}.inspect_physical_properties(before));
  const auto old_shape = *OcctShapeView::get(*c.find_body_shape(BodyId("body.base")));
  auto after = before;
  REQUIRE(after.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(12))));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(after, integrated));
  after = before;
  REQUIRE(after.remove_engineering_resource(resources::key("synthetic.table")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(after, integrated));
  after = before;
  const auto feature = *after.find_feature(FeatureId("base"));
  REQUIRE(after.remove_feature(feature.id()));
  REQUIRE(after.add_feature(feature));
  CHECK(checked(serialize_part_document_to_json(after)) ==
        checked(serialize_part_document_to_json(before)));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(after, integrated));
  REQUIRE(c.validate_complete_geometry_result(before));
  REQUIRE(integrated.validate_current(before));
  CHECK(OcctShapeView::get(*c.find_body_shape(BodyId("body.base")))->IsSame(old_shape));
}

TEST_CASE("Physical reuse rejects publicly modified, uncertified and prefix caches including the "
          "last row",
          "[geometry][parity-b150][physical-reuse]") {
  const auto before = part(true);
  auto after = before;
  REQUIRE(after.set_default_material(resources::key("synthetic.material", "r2")));
  const auto complete = cache(before);
  auto c = complete;
  const auto shape = *c.find_feature_shape(FeatureId("base"));
  REQUIRE(c.store_feature_shape(FeatureId("base"), shape));
  CHECK_FALSE(c.validate_complete_geometry_result(before));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  c = complete;
  c.clear_final_shape();
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  c = checked(ShapeCache::create(ShapeCacheId("uncertified")));
  REQUIRE(c.store_body_shape(BodyId("body.base"), FeatureId("base"), shape));
  REQUIRE(c.store_feature_shape(FeatureId("base"), shape));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  c = checked(ShapeCache::create(ShapeCacheId("prefix")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(before, 1, c));
  CHECK_FALSE(c.validate_complete_geometry_result(before));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(before, after, c));
  REQUIRE(complete.validate_complete_geometry_result(before));
}

TEST_CASE("Physical Project reuse reweights repeated hidden occurrences and recomputes authored "
          "poses without integration",
          "[geometry][parity-b150][physical-reuse]") {
  auto member = part();
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Root"));
  REQUIRE(root.add_member_part(member.id()));
  REQUIRE(root.add_component_instance(
      checked(ComponentInstance::create(ComponentInstanceId("first"), "First", member.id()))));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("second"), "Second", member.id(), ComponentVisibility::Hidden,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, {{30, 0, 0}, {}}))));
  auto p = checked(Project::create(DocumentId("physical.project"), "Physical", root));
  REQUIRE(p.add_part_document(member));
  const auto before = p;
  const auto integrated = checked(BodyResultInspector{}.inspect_physical_properties(p));
  REQUIRE(p.find_part_document(member.id())
              ->set_default_material(resources::key("synthetic.material", "r2")));
  REQUIRE(p.assembly().set_component_instance_transform(ComponentInstanceId("second"),
                                                        {{60, 0, 0}, {0, 0, 0}}));
  const auto reused = checked(BodyResultInspector{}.inspect_physical_properties(p, integrated));
  CHECK(reused.data().computed_part_count == 0);
  CHECK(reused.data().reused_part_count == 1);
  CHECK(reused.data().occurrences.size() == 2);
  REQUIRE(reused.data().complete_mass());
  const auto& m = *reused.data().complete_mass();
  CHECK(m.mass_kg == approximately(.0192));
  CHECK(m.centroid_mm.x == approximately(30));
  CHECK(m.centroid_mm.z == approximately(4));
  CHECK(m.central_inertia_kg_mm2[0] == approximately(.2624));
  CHECK(m.central_inertia_kg_mm2[4] == approximately(18.0224));
  CHECK(m.central_inertia_kg_mm2[8] == approximately(18.08));
  REQUIRE(reused.validate_current(p));
  REQUIRE(integrated.validate_current(before));
  CHECK_FALSE(integrated.validate_current(p));
  const auto cold = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(cold.data().volume_mm3 == reused.data().volume_mm3);
  for (std::size_t i = 0; i < 9; ++i)
    CHECK(cold.data().complete_mass()->central_inertia_kg_mm2[i] ==
          approximately(m.central_inertia_kg_mm2[i]));
  REQUIRE(p.find_part_document(member.id())
              ->set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(12))));
  CHECK_FALSE(BodyResultInspector{}.inspect_physical_properties(p, reused));
}

TEST_CASE("Legacy execution cannot certify an untrusted prefilled reference-transform cache",
          "[geometry][parity-b150][physical-reuse][physical-untrusted-origin]") {
  const auto native = part();
  auto untrusted = checked(ShapeCache::create(ShapeCacheId("untrusted.reference")));
  REQUIRE(untrusted.store_reference_transform("profile", BodyId("body.base"),
                                              BodyTransformId("foreign"),
                                              GeometryAffineTransform::translation({1000, 0, 0})));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(native, untrusted));
  const auto posed =
      checked(inspect_shape_physical_properties(*untrusted.find_body_shape(BodyId("body.base"))));
  REQUIRE(posed.volume);
  CHECK(posed.volume->centroid_mm.x == approximately(1000));
  CHECK_FALSE(untrusted.validate_complete_geometry_result(native));
  auto after = native;
  REQUIRE(after.set_default_material(resources::key("synthetic.material", "r2")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.refresh_material_metadata(native, after, untrusted));
  const auto trusted = cache(native);
  const auto fresh =
      checked(inspect_shape_physical_properties(*trusted.find_body_shape(BodyId("body.base"))));
  CHECK(fresh.volume->centroid_mm.x == approximately(0));
}
