#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/geometry/body_transform_adapter.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
using namespace blcad;
using namespace blcad::geometry;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
} // namespace
TEST_CASE(
    "Assembly interaction resolves stable root targets and posed glyphs without changing intent",
    "[geometry][block136-assembly]") {
  auto part = checked(PartDocument::create(DocumentId("part"), "Part"));
  REQUIRE(part.add_parameter(checked(
      Parameter::create_length(ParameterId("width"), "Width", checked(Quantity::length_mm(10))))));
  REQUIRE(part.add_parameter(checked(
      Parameter::create_length(ParameterId("depth"), "Depth", checked(Quantity::length_mm(2))))));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Sketch", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(
      RectangleProfile::create(ProfileId("profile"), ParameterId("width"), ParameterId("width")))));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_feature(checked(Feature::create_additive_extrude(
      FeatureId("base"), "Base", sketch.id(), ParameterId("depth")))));
  auto assembly = checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  REQUIRE(assembly.add_member_part(part.id()));
  REQUIRE(assembly.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId("placed"), "Placed", part.id(),
                                ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                ComponentGroundingState::Free, {{30, 10, 5}, {0, 0, 90}}))));
  auto project = checked(Project::create(DocumentId("project"), "Project", assembly));
  REQUIRE(project.add_part_document(part));
  const auto before = checked(serialize_project_to_json(project));
  auto targets = checked(AssemblyInteractionGeometry::targets(project));
  REQUIRE_FALSE(targets.empty());
  std::set<std::string> identities;
  bool top = false;
  for (const auto& target : targets) {
    CHECK(identities.insert(target.presentation.semantic_id).second);
    CHECK_FALSE(target.presentation.shape.empty());
    auto* native = OcctShapeView::get(target.presentation.shape);
    REQUIRE(native);
    CHECK(BRepCheck_Analyzer(*native).IsValid());
    CHECK(target.reference.component_instance() == ComponentInstanceId("placed"));
    if (target.reference.semantic_reference() == "base.top") {
      top = true;
      CHECK(target.anchor.z == Catch::Approx(7));
      auto bounds = checked(BodyTransformAdapter{}.bounds(target.presentation.shape));
      CHECK(bounds.minimum.z == Catch::Approx(7).margin(1e-6));
      CHECK(bounds.minimum.x == Catch::Approx(25).margin(1e-6));
      CHECK(bounds.maximum.y == Catch::Approx(15).margin(1e-6));
    }
  }
  CHECK(top);
  CHECK(checked(serialize_project_to_json(project)) == before);
  REQUIRE(project.assembly().set_component_instance_visibility(ComponentInstanceId("placed"),
                                                               ComponentVisibility::Hidden));
  CHECK(checked(AssemblyInteractionGeometry::targets(project)).empty());
}
