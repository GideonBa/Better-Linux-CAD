#include "blcad/geometry/assembly_interference_analyzer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <limits>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

// A 10 x 20 x 2 plate: volume 400 mm^3.
PartDocument make_plate_part(const char* id = "part.plate") {
  auto part = PartDocument::create(DocumentId(id), id);
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 10.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 20.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 2.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto sketch = Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(part.value().add_feature(feature.value()));
  return part.value();
}

ComponentInstance
make_component(const char* id, const char* part,
               RigidTransform transform = identity_rigid_transform(),
               ComponentVisibility visibility = ComponentVisibility::Visible,
               ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto component =
      ComponentInstance::create(ComponentInstanceId(id), id, DocumentId(part), visibility,
                                suppression, ComponentGroundingState::Free, transform);
  REQUIRE(component);
  return component.value();
}

SubassemblyInstance
make_subassembly(const char* id, const char* child,
                 RigidTransform transform = identity_rigid_transform(),
                 ComponentVisibility visibility = ComponentVisibility::Visible,
                 ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child),
                                              visibility, suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

Project make_two_plate_project(
    RigidTransform second_transform,
    ComponentSuppressionState second_suppression = ComponentSuppressionState::Active,
    bool reverse_insertion = false) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.plate")));

  const ComponentInstance component_a = make_component("component.a", "part.plate");
  const ComponentInstance component_b =
      make_component("component.b", "part.plate", second_transform, ComponentVisibility::Visible,
                     second_suppression);
  if (reverse_insertion) {
    REQUIRE(root.value().add_component_instance(component_b));
    REQUIRE(root.value().add_component_instance(component_a));
  } else {
    REQUIRE(root.value().add_component_instance(component_a));
    REQUIRE(root.value().add_component_instance(component_b));
  }

  auto project = Project::create(DocumentId("project.plates"), "Plates", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_plate_part()));
  return project.value();
}

} // namespace

TEST_CASE("Interference analysis distinguishes disjoint, touching, and overlapping leaves",
          "[geometry][assembly-interference]") {
  const AssemblyInterferenceAnalyzer analyzer;

  SECTION("disjoint leaves report no interference") {
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{50.0, 0.0, 0.0}, Vector3{}});
    const auto analysis = analyzer.analyze(project);

    REQUIRE(analysis);
    CHECK(analysis.value().leaf_count == 2U);
    CHECK(analysis.value().evaluated_pair_count == 1U);
    CHECK(analysis.value().recomputed_part_count == 1U);
    CHECK(analysis.value().interferences.empty());
  }

  SECTION("exact face contact without positive volume is not interference") {
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{0.0, 0.0, 2.0}, Vector3{}});
    const auto analysis = analyzer.analyze(project);

    REQUIRE(analysis);
    CHECK(analysis.value().evaluated_pair_count == 1U);
    CHECK(analysis.value().interferences.empty());
  }

  SECTION("an overlapping pair reports one ordered record with the exact common volume") {
    // Overlap region: 10 x 20 x 1 = 200 mm^3.
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{0.0, 0.0, 1.0}, Vector3{}});
    const auto analysis = analyzer.analyze(project);

    REQUIRE(analysis);
    CHECK(analysis.value().leaf_count == 2U);
    CHECK(analysis.value().evaluated_pair_count == 1U);
    REQUIRE(analysis.value().interferences.size() == 1U);
    const AssemblyLeafInterferenceRecord& record = analysis.value().interferences.front();
    CHECK(record.leaf_a.occurrence_key == "assembly.root/component.a");
    CHECK(record.leaf_b.occurrence_key == "assembly.root/component.b");
    CHECK(record.leaf_a.component_instance.value() == "component.a");
    CHECK(record.leaf_b.component_instance.value() == "component.b");
    CHECK(record.leaf_a.subassembly_path.empty());
    CHECK(record.overlap_volume_mm3 == Approx(200.0).margin(1.0e-6));
  }
}

TEST_CASE("Interference analysis is deterministic and insertion-order independent",
          "[geometry][assembly-interference]") {
  const AssemblyInterferenceAnalyzer analyzer;
  const Project forward = make_two_plate_project(RigidTransform{Vector3{0.0, 0.0, 1.0}, Vector3{}});
  const Project reverse = make_two_plate_project(RigidTransform{Vector3{0.0, 0.0, 1.0}, Vector3{}},
                                                 ComponentSuppressionState::Active, true);

  const auto first = analyzer.analyze(forward);
  const auto repeated = analyzer.analyze(forward);
  const auto reversed = analyzer.analyze(reverse);

  REQUIRE(first);
  REQUIRE(repeated);
  REQUIRE(reversed);
  CHECK(first.value() == repeated.value());
  CHECK(first.value() == reversed.value());
}

TEST_CASE("Interference analysis excludes suppressed leaves", "[geometry][assembly-interference]") {
  const AssemblyInterferenceAnalyzer analyzer;
  const Project project = make_two_plate_project(RigidTransform{Vector3{0.0, 0.0, 1.0}, Vector3{}},
                                                 ComponentSuppressionState::Suppressed);
  const auto analysis = analyzer.analyze(project);

  REQUIRE(analysis);
  CHECK(analysis.value().leaf_count == 1U);
  CHECK(analysis.value().evaluated_pair_count == 0U);
  CHECK(analysis.value().interferences.empty());
}

TEST_CASE("Interference analysis covers repeated nested child occurrences",
          "[geometry][assembly-interference]") {
  // Root plate at origin. Two rigid occurrences of one child assembly:
  // subassembly.a overlaps the root plate, subassembly.z is far away.
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(root.value().add_component_instance(make_component("component.root", "part.plate")));
  REQUIRE(root.value().add_subassembly_instance(make_subassembly(
      "subassembly.a", "assembly.child", RigidTransform{Vector3{0.0, 0.0, 1.0}, Vector3{}})));
  REQUIRE(root.value().add_subassembly_instance(make_subassembly(
      "subassembly.z", "assembly.child", RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{}})));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(child.value().add_component_instance(make_component("component.plate", "part.plate")));

  auto project = Project::create(DocumentId("project.nested"), "Nested", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(make_plate_part()));

  const AssemblyInterferenceAnalyzer analyzer;
  const auto analysis = analyzer.analyze(project.value());

  REQUIRE(analysis);
  CHECK(analysis.value().leaf_count == 3U);
  CHECK(analysis.value().evaluated_pair_count == 3U);
  CHECK(analysis.value().recomputed_part_count == 1U);
  REQUIRE(analysis.value().interferences.size() == 1U);
  const AssemblyLeafInterferenceRecord& record = analysis.value().interferences.front();
  CHECK(record.leaf_a.occurrence_key == "assembly.child/subassembly.a/component.plate");
  CHECK(record.leaf_b.occurrence_key == "assembly.root/component.root");
  REQUIRE(record.leaf_a.subassembly_path.size() == 1U);
  CHECK(record.leaf_a.subassembly_path.front().value() == "subassembly.a");
  CHECK(record.overlap_volume_mm3 == Approx(200.0).margin(1.0e-6));
}

TEST_CASE("Interference analysis validates its overlap tolerance",
          "[geometry][assembly-interference]") {
  const AssemblyInterferenceAnalyzer analyzer;
  const Project project =
      make_two_plate_project(RigidTransform{Vector3{50.0, 0.0, 0.0}, Vector3{}});

  for (const double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    AssemblyInterferenceAnalysisOptions options;
    options.minimum_overlap_volume_mm3 = invalid;
    const auto analysis = analyzer.analyze(project, options);
    REQUIRE(analysis.has_error());
    CHECK(analysis.error().message() ==
          "interference overlap volume tolerance must be finite and positive");
  }
}

TEST_CASE("Clearance analysis reports near pairs, touching pairs, and separates interference",
          "[geometry][assembly-clearance]") {
  const AssemblyClearanceAnalyzer analyzer;

  SECTION("a pair above the threshold is not a clearance violation") {
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{50.0, 0.0, 0.0}, Vector3{}});
    const auto analysis = analyzer.analyze(project);

    REQUIRE(analysis);
    CHECK(analysis.value().leaf_count == 2U);
    CHECK(analysis.value().evaluated_pair_count == 1U);
    CHECK(analysis.value().clearance_violations.empty());
    CHECK(analysis.value().interferences.empty());
  }

  SECTION("a pair below the threshold reports the exact minimum distance") {
    // Plates are 10 mm wide: x-offset 12 leaves a 2 mm gap.
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{12.0, 0.0, 0.0}, Vector3{}});
    AssemblyClearanceAnalysisOptions options;
    options.clearance_threshold_mm = 5.0;
    const auto analysis = analyzer.analyze(project, options);

    REQUIRE(analysis);
    REQUIRE(analysis.value().clearance_violations.size() == 1U);
    const AssemblyLeafClearanceRecord& record = analysis.value().clearance_violations.front();
    CHECK(record.leaf_a.occurrence_key == "assembly.root/component.a");
    CHECK(record.leaf_b.occurrence_key == "assembly.root/component.b");
    CHECK(record.minimum_distance_mm == Approx(2.0).margin(1.0e-6));
    CHECK(analysis.value().interferences.empty());
  }

  SECTION("exact touching is a zero-distance clearance violation, not interference") {
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{10.0, 0.0, 0.0}, Vector3{}});
    const auto analysis = analyzer.analyze(project);

    REQUIRE(analysis);
    REQUIRE(analysis.value().clearance_violations.size() == 1U);
    CHECK(analysis.value().clearance_violations.front().minimum_distance_mm ==
          Approx(0.0).margin(1.0e-9));
    CHECK(analysis.value().interferences.empty());
  }

  SECTION("an interfering pair is reported as interference and never as clearance") {
    const Project project =
        make_two_plate_project(RigidTransform{Vector3{0.0, 0.0, 1.0}, Vector3{}});
    const auto analysis = analyzer.analyze(project);

    REQUIRE(analysis);
    CHECK(analysis.value().clearance_violations.empty());
    REQUIRE(analysis.value().interferences.size() == 1U);
    CHECK(analysis.value().interferences.front().overlap_volume_mm3 ==
          Approx(200.0).margin(1.0e-6));
  }
}

TEST_CASE("Clearance analysis is deterministic and validates its thresholds",
          "[geometry][assembly-clearance]") {
  const AssemblyClearanceAnalyzer analyzer;
  const Project forward =
      make_two_plate_project(RigidTransform{Vector3{12.0, 0.0, 0.0}, Vector3{}});
  const Project reverse = make_two_plate_project(RigidTransform{Vector3{12.0, 0.0, 0.0}, Vector3{}},
                                                 ComponentSuppressionState::Active, true);

  AssemblyClearanceAnalysisOptions options;
  options.clearance_threshold_mm = 5.0;
  const auto first = analyzer.analyze(forward, options);
  const auto repeated = analyzer.analyze(forward, options);
  const auto reversed = analyzer.analyze(reverse, options);

  REQUIRE(first);
  REQUIRE(repeated);
  REQUIRE(reversed);
  CHECK(first.value() == repeated.value());
  CHECK(first.value() == reversed.value());

  for (const double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    AssemblyClearanceAnalysisOptions invalid_options;
    invalid_options.clearance_threshold_mm = invalid;
    const auto analysis = analyzer.analyze(forward, invalid_options);
    REQUIRE(analysis.has_error());
    CHECK(analysis.error().message() == "clearance threshold must be finite and positive");
  }
}
