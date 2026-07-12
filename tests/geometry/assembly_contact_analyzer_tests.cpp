#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <utility>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

Parameter length_parameter(const char* id, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument contact_part() {
  auto part = PartDocument::create(DocumentId("part.contact_plate"), "ContactPlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(length_parameter("part.width", 10.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.height", 10.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.thickness", 2.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(part.value().add_feature(feature.value()));
  return part.value();
}

ComponentInstance component(const char* id, double x_mm = 0.0) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.contact_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, RigidTransform{Vector3{x_mm, 0.0, 0.0}, Vector3{}});
  REQUIRE(value);
  return value.value();
}

Project two_component_project(double second_x_mm, bool reverse_insertion = false) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.contact_plate")));
  if (!reverse_insertion) {
    REQUIRE(assembly.value().add_component_instance(component("component.a")));
    REQUIRE(assembly.value().add_component_instance(component("component.b", second_x_mm)));
  } else {
    REQUIRE(assembly.value().add_component_instance(component("component.b", second_x_mm)));
    REQUIRE(assembly.value().add_component_instance(component("component.a")));
  }
  auto project = Project::create(DocumentId("project.contact"), "Contact", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(contact_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

SubassemblyInstance occurrence(const char* id, const char* child, double x_mm) {
  auto value = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId(child), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, RigidTransform{Vector3{x_mm, 0.0, 0.0}, Vector3{}});
  REQUIRE(value);
  return value.value();
}

Project repeated_child_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.right", "assembly.child", 20.0)));
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.left", "assembly.child", -20.0)));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.contact_plate")));
  REQUIRE(child.value().add_component_instance(component("component.child")));

  auto project = Project::create(DocumentId("project.contact.repeated"), "Repeated", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(contact_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

const AssemblyContactRecord& only_contact(const AssemblyContactAnalysis& analysis) {
  REQUIRE(analysis.contacts.size() == 1U);
  return analysis.contacts.front();
}

} // namespace

TEST_CASE("Assembly contact classification freezes separated touching and interfering semantics",
          "[geometry][assembly-contact]") {
  const AssemblyContactAnalyzer analyzer;

  SECTION("Separated") {
    auto result = analyzer.analyze(two_component_project(11.0));
    REQUIRE(result);
    const auto& contact = only_contact(result.value());
    CHECK(contact.classification == AssemblyContactClassification::Separated);
    CHECK(contact.overlap_volume_mm3 <= 1.0e-6);
    REQUIRE(contact.minimum_distance_mm.has_value());
    CHECK(*contact.minimum_distance_mm == Approx(1.0).margin(1.0e-7));
  }

  SECTION("Exact touching") {
    auto result = analyzer.analyze(two_component_project(10.0));
    REQUIRE(result);
    const auto& contact = only_contact(result.value());
    CHECK(contact.classification == AssemblyContactClassification::Touching);
    REQUIRE(contact.minimum_distance_mm.has_value());
    CHECK(*contact.minimum_distance_mm == Approx(0.0).margin(1.0e-9));
  }

  SECTION("Near touching uses explicit tolerance") {
    auto result = analyzer.analyze(
        two_component_project(10.005), AssemblyContactAnalysisOptions{0.01, 1.0e-6});
    REQUIRE(result);
    const auto& contact = only_contact(result.value());
    CHECK(contact.classification == AssemblyContactClassification::Touching);
    REQUIRE(contact.minimum_distance_mm.has_value());
    CHECK(*contact.minimum_distance_mm == Approx(0.005).margin(1.0e-7));
  }

  SECTION("Positive-volume overlap is interfering before distance classification") {
    auto result = analyzer.analyze(two_component_project(9.0));
    REQUIRE(result);
    const auto& contact = only_contact(result.value());
    CHECK(contact.classification == AssemblyContactClassification::Interfering);
    CHECK(contact.overlap_volume_mm3 == Approx(20.0).margin(1.0e-6));
    CHECK_FALSE(contact.minimum_distance_mm.has_value());
  }
}

TEST_CASE("Assembly contact uses exact rooted component occurrence pair identity",
          "[geometry][assembly-contact]") {
  Project project = repeated_child_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);

  auto result = AssemblyContactAnalyzer{}.analyze(project);
  REQUIRE(result);
  CHECK(result.value().component_occurrence_count == 2U);
  CHECK(result.value().evaluated_pair_count == 1U);
  CHECK(result.value().recomputed_part_count == 1U);
  const auto& pair = only_contact(result.value()).pair;
  REQUIRE(pair.occurrence_a.containing_assembly_occurrence.occurrence_path.size() == 1U);
  REQUIRE(pair.occurrence_b.containing_assembly_occurrence.occurrence_path.size() == 1U);
  CHECK(pair.occurrence_a.containing_assembly_occurrence.occurrence_path.front().value() ==
        "subassembly.left");
  CHECK(pair.occurrence_b.containing_assembly_occurrence.occurrence_path.front().value() ==
        "subassembly.right");
  CHECK(pair.occurrence_a.component_instance.value() == "component.child");
  CHECK(pair.occurrence_b.component_instance.value() == "component.child");

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Assembly contact pair ordering is independent of component insertion order",
          "[geometry][assembly-contact]") {
  auto forward = AssemblyContactAnalyzer{}.analyze(two_component_project(11.0, false));
  auto reverse = AssemblyContactAnalyzer{}.analyze(two_component_project(11.0, true));
  REQUIRE(forward);
  REQUIRE(reverse);
  CHECK(forward.value() == reverse.value());
  const auto& pair = only_contact(forward.value()).pair;
  CHECK(pair.occurrence_a.component_instance.value() == "component.a");
  CHECK(pair.occurrence_b.component_instance.value() == "component.b");
}

TEST_CASE("Assembly contact validates explicit classification tolerances",
          "[geometry][assembly-contact]") {
  const Project project = two_component_project(11.0);
  const AssemblyContactAnalyzer analyzer;
  CHECK(analyzer.analyze(project, AssemblyContactAnalysisOptions{-1.0, 1.0e-6}).has_error());
  CHECK(analyzer
            .analyze(project, AssemblyContactAnalysisOptions{
                                  std::numeric_limits<double>::quiet_NaN(), 1.0e-6})
            .has_error());
  CHECK(analyzer.analyze(project, AssemblyContactAnalysisOptions{0.0, 0.0}).has_error());
  CHECK(analyzer
            .analyze(project, AssemblyContactAnalysisOptions{
                                  0.0, std::numeric_limits<double>::infinity()})
            .has_error());
}
