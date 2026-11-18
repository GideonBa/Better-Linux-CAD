#include "../support/assembly_hole_fixture.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_flexible_subassembly_solver.hpp"
#include "blcad/geometry/assembly_hole_coordinator.hpp"
#include "blcad/geometry/assembly_occurrence_solver.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace a = blcad::test::assembly_hole;
namespace h = blcad::test::history_fixture;
namespace {
std::string saved(const Project& p) {
  return h::checked(serialize_project_to_json(p));
}
std::string saved(const PartDocument& p) {
  return h::checked(serialize_part_document_to_json(p));
}
CoordinatedAssemblyHoleEvaluation bind(const Project& p, AssemblyHoleFeature f) {
  auto r = AssemblyHoleCoordinator{}.bind(p, std::move(f));
  INFO((r ? "ok" : r.error().object_id() + ": " + r.error().message()));
  REQUIRE(r);
  return std::move(r.value());
}
ShapeCache compute(const PartDocument& p) {
  auto c = h::checked(ShapeCache::create(ShapeCacheId("assembly.hole.proof")));
  auto r = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((r ? "ok" : r.error().message()));
  REQUIRE(r);
  return c;
}
double volume(const GeometryShape& shape) {
  GProp_GProps props;
  BRepGProp::VolumeProperties(*OcctShapeView::get(shape), props);
  return props.Mass();
}
double volume(const PartDocument& p) {
  auto c = compute(p);
  return volume(*c.find_body_shape(BodyId("body.base")));
}
bool material(const GeometryShape& shape, Point3 at) {
  for (TopExp_Explorer it(*OcctShapeView::get(shape), TopAbs_SOLID); it.More(); it.Next()) {
    BRepClass3d_SolidClassifier c(it.Current(), gp_Pnt(at.x, at.y, at.z), 1e-7);
    if (c.State() == TopAbs_IN)
      return true;
  }
  return false;
}
Point3 transform(Point3 p, RigidTransform t) {
  const double x = t.rotation_deg.x * std::numbers::pi / 180,
               y = t.rotation_deg.y * std::numbers::pi / 180,
               z = t.rotation_deg.z * std::numbers::pi / 180;
  const Point3 rx{p.x, std::cos(x) * p.y - std::sin(x) * p.z,
                  std::sin(x) * p.y + std::cos(x) * p.z};
  const Point3 ry{std::cos(y) * rx.x + std::sin(y) * rx.z, rx.y,
                  -std::sin(y) * rx.x + std::cos(y) * rx.z};
  return {std::cos(z) * ry.x - std::sin(z) * ry.y + t.translation_mm.x,
          std::sin(z) * ry.x + std::cos(z) * ry.y + t.translation_mm.y, ry.z + t.translation_mm.z};
}
double cover_volume(std::size_t count) {
  return 400 - count * std::numbers::pi * 1.2 * 1.2 * 2;
}
double base_volume(std::size_t count, double depth = 4) {
  return 1600 - count * std::numbers::pi * .8 * .8 * depth;
}
void placements(const Project& p, bool pattern, double x = 2, double second = 3) {
  const auto& cover = *p.find_part_document(DocumentId("cover.part"));
  const auto& base = *p.find_part_document(DocumentId("base.part"));
  const auto* source = cover.find_hole_feature(FeatureId("layout.hole"));
  const auto* target = base.find_hole_feature(FeatureId("mounting.hole"));
  REQUIRE(source);
  REQUIRE(target);
  REQUIRE(source->placements().size() == 1);
  CHECK(source->placements()[0].id == HolePlacementId("seed"));
  CHECK((std::get<Point2>(source->placements()[0].source) == Point2{-3, 0}));
  REQUIRE(target->placements().size() == (pattern ? 2 : 1));
  auto c = compute(base);
  auto effective_cover = cover.evaluate_model_state(primary_model_state_id());
  REQUIRE(effective_cover);
  auto cc = compute(effective_cover.value());
  for (std::size_t i = 0; i < target->placements().size(); ++i) {
    const auto index = pattern ? std::optional(PatternOccurrenceIndex{i, 0}) : std::nullopt;
    const auto key = assembly_hole_placement_id(HolePlacementId("seed"), index);
    const auto* placement = &target->placements()[i];
    CHECK(placement->id == key);
    const auto local = std::get<Point2>(placement->source);
    CHECK(local.x == Catch::Approx(x).margin(1e-8));
    CHECK(local.y == Catch::Approx(i ? second : -3).margin(1e-8));
    auto reference =
        h::checked(HoleInstanceReference::create(target->id(), key, HoleReferenceRole::Axis));
    const auto frame = h::checked(HoleInstanceGeometry::resolve(base, reference, c));
    CHECK(frame.radius_mm == Catch::Approx(.8).margin(1e-8));
    CHECK(frame.frame.origin.x == Catch::Approx(x).margin(1e-8));
    CHECK(frame.frame.origin.y == Catch::Approx(i ? second : -3).margin(1e-8));
    CHECK(frame.frame.origin.z == Catch::Approx(8).margin(1e-8));
    auto source_ref = h::checked(HoleInstanceReference::create(
        source->id(), HolePlacementId("seed"), HoleReferenceRole::Axis,
        pattern ? std::optional(HolePatternOccurrence{FeatureId("layout.pattern"), {i, 0}})
                : std::nullopt));
    const auto cover_frame = h::checked(HoleInstanceGeometry::resolve(cover, source_ref, cc));
    const auto actual =
        transform(transform(cover_frame.frame.origin, {{x, 0, 8}, {0, 0, 90}}), a::parent_pose());
    const auto expected = transform({x, i ? second : -3, 10}, a::parent_pose());
    CHECK(actual.x == Catch::Approx(expected.x).margin(1e-7));
    CHECK(actual.y == Catch::Approx(expected.y).margin(1e-7));
    CHECK(actual.z == Catch::Approx(expected.z).margin(1e-7));
  }
}
struct Directory {
  std::filesystem::path path;
  Directory() {
    char pattern[] = "/tmp/blcad-assembly-hole-XXXXXX";
    auto* value = ::mkdtemp(pattern);
    REQUIRE(value);
    path = value;
  }
  ~Directory() {
    std::error_code e;
    std::filesystem::remove_all(path, e);
  }
};
std::string bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}
} // namespace

TEST_CASE("Coordinated holes use one real layout under nonidentity nested occurrence frames",
          "[geometry][assembly-hole][parity-b197][A03]") {
  const bool pattern = GENERATE(false, true);
  auto p = a::project(pattern);
  const auto original = saved(p);
  const auto cover = saved(*p.find_part_document(DocumentId("cover.part")));
  const auto unrelated = saved(*p.find_part_document(DocumentId("unrelated.part")));
  const auto child = h::checked(
      serialize_assembly_document_to_json(*p.find_assembly_document(DocumentId("mounting.child"))));
  auto result = bind(p, a::feature(pattern));
  CHECK(saved(p) == original);
  CHECK(saved(*result.project.find_part_document(DocumentId("cover.part"))) == cover);
  CHECK(saved(*result.project.find_part_document(DocumentId("unrelated.part"))) == unrelated);
  CHECK(h::checked(serialize_assembly_document_to_json(
            *result.project.find_assembly_document(DocumentId("mounting.child")))) == child);
  CHECK(std::find(result.changed_parts.begin(), result.changed_parts.end(),
                  DocumentId("base.part")) != result.changed_parts.end());
  CHECK(std::find(result.changed_parts.begin(), result.changed_parts.end(),
                  DocumentId("unrelated.part")) == result.changed_parts.end());
  placements(result.project, pattern);
  const std::size_t count = pattern ? 2 : 1;
  CHECK(volume(*result.project.find_part_document(DocumentId("cover.part"))) ==
        Catch::Approx(cover_volume(count)).margin(1e-5));
  CHECK(volume(*result.project.find_part_document(DocumentId("base.part"))) ==
        Catch::Approx(base_volume(count)).margin(1e-5));
  const auto posed = h::checked(AssemblyStepExporter{}.build_posed_shape(result.project));
  for (double y : pattern ? std::vector<double>{-3, 3} : std::vector<double>{-3}) {
    CHECK_FALSE(material(posed, transform({2, y, 9}, a::parent_pose())));
    CHECK_FALSE(material(posed, transform({3.1, y, 9}, a::parent_pose())));
    CHECK(material(posed, transform({3.3, y, 9}, a::parent_pose())));
    CHECK_FALSE(material(posed, transform({2, y, 4.1}, a::parent_pose())));
    CHECK(material(posed, transform({2, y, 3.9}, a::parent_pose())));
    CHECK(material(posed, transform({2.9, y, 6}, a::parent_pose())));
  }
  const auto mass = h::checked(BodyResultInspector{}.inspect_physical_properties(result.project));
  REQUIRE(mass.data().complete_mass());
  const double expected = cover_volume(count) + base_volume(count) + 72;
  CHECK(mass.data().volume_mm3 == Catch::Approx(expected).margin(1e-5));
  CHECK(mass.data().complete_mass()->mass_kg == Catch::Approx(expected * 4e-6).margin(1e-9));
  const auto repeated = h::checked(AssemblyHoleCoordinator{}.evaluate(result.project));
  CHECK(saved(repeated.project) == saved(result.project));
  CHECK(repeated.changed_parts.empty());
  const auto reopened = h::checked(deserialize_project_from_json(saved(result.project)));
  CHECK(saved(reopened) == saved(result.project));
  CHECK(volume(h::checked(AssemblyStepExporter{}.build_posed_shape(reopened))) ==
        Catch::Approx(expected).margin(1e-5));
  Directory dir;
  REQUIRE(AssemblyStepExporter{}.write_step(result.project, (dir.path / "mounting.step").string()));
  REQUIRE(AssemblyStructuredStepExporter{}.write_step(
      result.project, (dir.path / "mounting-structured.step").string()));
  for (const auto* name : {"mounting.step", "mounting-structured.step"}) {
    const auto exported = h::checked(StepImportSource::create(
        StepImportSourceId("export"), "Export", name,
        h::checked(step_import_sha256(bytes(dir.path / name))), StepImportMode::Reference));
    const auto read = h::checked(StepImportReader{}.read(exported, dir.path));
    double actual = 0;
    for (const auto& product : read.products)
      for (const auto& body : product.bodies)
        actual += body.volume_mm3;
    CHECK(actual == Catch::Approx(expected).margin(1e-4));
  }
}

TEST_CASE("Coordinated layout spacing updates every participant and detach retains explicit last "
          "snapshots",
          "[geometry][assembly-hole][parity-b197][A03]") {
  auto coordinated = bind(a::project(), a::feature());
  auto changed = coordinated.project;
  REQUIRE(
      changed.find_part_document(DocumentId("cover.part"))
          ->set_parameter_value(ParameterId("layout.spacing"), h::checked(Quantity::length_mm(4))));
  const auto before = saved(changed);
  auto update = h::checked(AssemblyHoleCoordinator{}.evaluate(changed));
  CHECK(saved(changed) == before);
  placements(update.project, true, 2, 1);
  const auto local = saved(*update.project.find_part_document(DocumentId("base.part")));
  auto detached =
      h::checked(AssemblyHoleCoordinator{}.detach(update.project, FeatureId("assembly.mounting")));
  CHECK(detached.assembly_hole_features().empty());
  CHECK(saved(*detached.find_part_document(DocumentId("base.part"))) == local);
  REQUIRE(
      detached.find_part_document(DocumentId("cover.part"))
          ->set_parameter_value(ParameterId("layout.spacing"), h::checked(Quantity::length_mm(5))));
  auto independent = h::checked(AssemblyHoleCoordinator{}.evaluate(detached));
  CHECK(saved(*independent.project.find_part_document(DocumentId("base.part"))) == local);
}

TEST_CASE(
    "One failed assembly participant preserves every original definition resource and root pose",
    "[geometry][assembly-hole][parity-b197][A03]") {
  auto p = a::project();
  REQUIRE(
      p.find_part_document(DocumentId("base.part"))
          ->set_parameter_value(ParameterId("blind.depth"), h::checked(Quantity::length_mm(20))));
  const auto before = saved(p);
  CHECK_FALSE(AssemblyHoleCoordinator{}.bind(p, a::feature()));
  CHECK(saved(p) == before);
  auto valid = bind(a::project(), a::feature());
  auto invalid = valid.project;
  REQUIRE(invalid.find_part_document(DocumentId("cover.part"))
              ->set_parameter_value(ParameterId("layout.count"), h::checked(Quantity::count(4))));
  const auto overrun = saved(invalid);
  CHECK_FALSE(AssemblyHoleCoordinator{}.evaluate(invalid));
  CHECK(saved(invalid) == overrun);
  auto repeated = a::project(true, true);
  CHECK_FALSE(AssemblyHoleCoordinator{}.bind(repeated, a::feature()));
  auto explicit_change =
      bind(repeated, a::feature(true, AssemblyHoleDefinitionPolicy::AffectSharedDefinition));
  CHECK(volume(h::checked(AssemblyStepExporter{}.build_posed_shape(explicit_change.project))) ==
        Catch::Approx(cover_volume(2) + 2 * base_volume(2) + 72).margin(1e-5));
  auto reference = a::project(true, false, true);
  const auto reference_before = saved(reference);
  CHECK_FALSE(AssemblyHoleCoordinator{}.bind(
      reference, a::feature(true, AssemblyHoleDefinitionPolicy::SingleOccurrence,
                            primary_model_state_id(), true)));
  CHECK(saved(reference) == reference_before);
}

TEST_CASE("Assembly Hole uses selected model state and occurrence-local flexible placement without "
          "baking Primary",
          "[geometry][assembly-hole][parity-b197][A03]") {
  auto p = a::project();
  auto* base = p.find_part_document(DocumentId("base.part"));
  REQUIRE(base);
  ModelState state{ModelStateId("short"),
                   "Short blind",
                   {{ParameterId("blind.depth"), h::checked(Quantity::length_mm(3))}},
                   {},
                   {},
                   {},
                   {}};
  REQUIRE(base->replace_model_states({state}));
  REQUIRE(p.find_assembly_document(DocumentId("mounting.child"))
              ->set_component_model_state(ComponentInstanceId("base"), state.id));
  const auto coordinated =
      bind(p, a::feature(true, AssemblyHoleDefinitionPolicy::SingleOccurrence, state.id));
  const auto& primary = *coordinated.project.find_part_document(DocumentId("base.part"));
  CHECK(primary.find_parameter(ParameterId("blind.depth"))->value().millimeters() == 4);
  CHECK(primary.model_states() == std::vector<ModelState>{state});
  CHECK(volume(h::checked(primary.evaluate_model_state(state.id))) ==
        Catch::Approx(base_volume(2, 3)).margin(1e-5));
  const auto posed = h::checked(AssemblyStepExporter{}.build_posed_shape(coordinated.project));
  CHECK(volume(posed) == Catch::Approx(cover_volume(2) + base_volume(2, 3) + 72).margin(1e-5));
  auto flex = a::project();
  const auto definition = h::checked(serialize_assembly_document_to_json(
      *flex.find_assembly_document(DocumentId("mounting.child"))));
  REQUIRE(flex.set_occurrence_pose(h::checked(
      AssemblyOccurrencePose::create({SubassemblyInstanceId("pair")}, DocumentId("mounting.child"),
                                     AssemblyOccurrenceRigidity::Flexible,
                                     {{ComponentInstanceId("cover"), {{1, 0, 8}, {0, 0, 90}}}}))));
  auto flexible = bind(flex, a::feature());
  placements(flexible.project, true, 1, 3);
  CHECK(h::checked(serialize_assembly_document_to_json(
            *flexible.project.find_assembly_document(DocumentId("mounting.child")))) == definition);
  const auto flexible_shape =
      h::checked(AssemblyStepExporter{}.build_posed_shape(flexible.project));
  CHECK_FALSE(material(flexible_shape, transform({1, -3, 6}, a::parent_pose())));
  CHECK(material(flexible_shape, transform({2, -3, 6}, a::parent_pose())));
}

TEST_CASE("Assembly Hole rejects a recursive bore-dependent axial pose before staging",
          "[geometry][assembly-hole][parity-b197][assembly-hole-feedback]") {
  auto coordinated = bind(a::project(false), a::feature(false));
  auto& p = coordinated.project;
  auto* child = p.find_assembly_document(DocumentId("mounting.child"));
  REQUIRE(child);
  const auto entry = h::checked(make_hole_target_spelling(h::checked(HoleInstanceReference::create(
      FeatureId("mounting.hole"), HolePlacementId("seed"), HoleReferenceRole::Entry))));
  const auto fixed =
      h::checked(AssemblyConstraintTarget::create(ComponentInstanceId("base"), entry));
  const auto moving =
      h::checked(AssemblyConstraintTarget::create(ComponentInstanceId("cover"), "base.bottom"));
  REQUIRE(child->add_constraint(h::checked(AssemblyConstraint::create(
      AssemblyConstraintId("bore.feedback"), "Recursive bore-controlled cover pose",
      AssemblyConstraintType::Distance, fixed, moving, AssemblyConstraintState::Active,
      h::checked(Quantity::length_mm(4))))));
  const auto before = saved(p);
  // An independent local solve demonstrates that the consumed frame really
  // changes along the bore axis. Line-collinearity alone would still pass.
  auto local =
      h::checked(Project::create(DocumentId("local.proof"), "Independent pose proof", *child));
  for (const auto& part : p.part_documents())
    REQUIRE(local.add_part_document(part));
  auto solve = h::checked(AssemblyRigidBodySolver{}.solve(
      local, {ComponentInstanceId("base"), ComponentInstanceId("cover")}));
  REQUIRE(solve.converged());
  REQUIRE(AssemblySolveResultApplier{}.apply(local, solve));
  const auto pose =
      local.assembly().find_component_instance(ComponentInstanceId("cover"))->transform();
  CHECK(pose.translation_mm.x == Catch::Approx(2).margin(1e-6));
  CHECK(pose.translation_mm.y == Catch::Approx(0).margin(1e-6));
  CHECK(pose.translation_mm.z == Catch::Approx(4).margin(1e-6));
  const auto result = AssemblyHoleCoordinator{}.evaluate(p);
  REQUIRE_FALSE(result);
  CHECK(result.error().message().find("Recursive directed shape/pose dependency") !=
        std::string::npos);
  CHECK(saved(p) == before);
}

TEST_CASE("An independently constrained layout remains valid for directed Assembly machining",
          "[geometry][assembly-hole][parity-b197][assembly-hole-feedback]") {
  auto p = a::project(false);
  auto* child = p.find_assembly_document(DocumentId("mounting.child"));
  REQUIRE(child);
  const auto fixed =
      h::checked(AssemblyConstraintTarget::create(ComponentInstanceId("unrelated"), "base.top"));
  const auto moving =
      h::checked(AssemblyConstraintTarget::create(ComponentInstanceId("cover"), "base.bottom"));
  REQUIRE(child->add_constraint(h::checked(AssemblyConstraint::create(
      AssemblyConstraintId("independent.pose"), "Independent layout support",
      AssemblyConstraintType::Distance, fixed, moving, AssemblyConstraintState::Active,
      h::checked(Quantity::length_mm(1))))));
  const auto before = saved(p);
  auto result = bind(p, a::feature(false));
  const auto pose = result.project.find_assembly_document(DocumentId("mounting.child"))
                        ->find_component_instance(ComponentInstanceId("cover"))
                        ->transform();
  CHECK(pose.translation_mm.z == Catch::Approx(4).margin(1e-6));
  CHECK(volume(*result.project.find_part_document(DocumentId("base.part"))) ==
        Catch::Approx(base_volume(1)).margin(1e-5));
  CHECK(saved(p) == before);
}

TEST_CASE("Assembly Hole detach retains the current checked result after a headless driver edit",
          "[geometry][assembly-hole][parity-b197][assembly-hole-normalization]") {
  auto coordinated = bind(a::project(), a::feature());
  auto& raw = coordinated.project;
  REQUIRE(
      raw.find_part_document(DocumentId("cover.part"))
          ->set_parameter_value(ParameterId("layout.spacing"), h::checked(Quantity::length_mm(4))));
  const auto before = saved(raw);
  const auto detached =
      h::checked(AssemblyHoleCoordinator{}.detach(raw, FeatureId("assembly.mounting")));
  REQUIRE(detached.assembly_hole_features().empty());
  placements(detached, true, 2, 1);
  CHECK(saved(raw) == before);
  CHECK(volume(*detached.find_part_document(DocumentId("base.part"))) ==
        Catch::Approx(base_volume(2)).margin(1e-5));
}

TEST_CASE("Occurrence solve and apply normalize headless layout changes and reject stale results",
          "[geometry][assembly-hole][parity-b197][assembly-hole-normalization]") {
  auto initial = a::project();
  REQUIRE(initial.set_occurrence_pose(h::checked(
      AssemblyOccurrencePose::create({SubassemblyInstanceId("pair")}, DocumentId("mounting.child"),
                                     AssemblyOccurrenceRigidity::Flexible))));
  auto coordinated = bind(initial, a::feature());
  auto raw = coordinated.project;
  const std::vector<SubassemblyInstanceId> path{SubassemblyInstanceId("pair")};
  const std::vector<ComponentInstanceId> group{ComponentInstanceId("base")};
  AssemblyOccurrenceSolver solver;
  auto old = h::checked(solver.solve(raw, path, group));
  REQUIRE(old.converged());
  REQUIRE(
      raw.find_part_document(DocumentId("cover.part"))
          ->set_parameter_value(ParameterId("layout.spacing"), h::checked(Quantity::length_mm(4))));
  const auto before = saved(raw);
  CHECK_FALSE(solver.apply(raw, old));
  CHECK(saved(raw) == before);
  const auto current = h::checked(solver.solve(raw, path, group));
  REQUIRE(current.converged());
  CHECK(saved(raw) == before);
  REQUIRE(solver.apply(raw, current));
  placements(raw, true, 2, 1);
}

TEST_CASE("Occurrence motion validates all newly driven Hole geometry before atomic publication",
          "[geometry][assembly-hole][parity-b197][assembly-hole-normalization]") {
  auto p = a::project(false);
  for (const auto& id : {DocumentId("cover.part"), DocumentId("unrelated.part")}) {
    auto* part = p.find_part_document(id);
    REQUIRE(part->add_datum_plane(
        h::checked(DatumPlane::yz(DatumPlaneId("slide.frame"), "Independent slide frame"))));
  }
  auto* child = p.find_assembly_document(DocumentId("mounting.child"));
  REQUIRE(child);
  REQUIRE(
      child->set_component_instance_transform(ComponentInstanceId("unrelated"), a::cover_pose()));
  const auto frame = h::checked(make_assembly_frame_target_spelling(DatumPlaneId("slide.frame")));
  const auto fixed =
      h::checked(AssemblyConstraintTarget::create(ComponentInstanceId("unrelated"), frame));
  const auto moving =
      h::checked(AssemblyConstraintTarget::create(ComponentInstanceId("cover"), frame));
  const auto slot = h::checked(AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Translation, AssemblyJointCoordinateKind::Linear,
      h::checked(Quantity::linear_displacement_mm(0)),
      h::checked(Quantity::linear_displacement_mm(0)),
      h::checked(Quantity::linear_displacement_mm(100))));
  REQUIRE(child->add_joint(h::checked(
      AssemblyJoint::create(AssemblyJointId("layout.slide"), "Independent source layout slide",
                            AssemblyJointType::Prismatic, fixed, moving, AssemblyJointState::Active,
                            std::vector<AssemblyJointCoordinateSlot>{slot}))));
  const std::vector<SubassemblyInstanceId> path{SubassemblyInstanceId("pair")};
  REQUIRE(p.set_occurrence_pose(h::checked(
      AssemblyOccurrencePose::create(path, child->id(), AssemblyOccurrenceRigidity::Flexible))));
  auto coordinated = bind(p, a::feature(false));
  auto raw = coordinated.project;
  const auto before = saved(raw);
  AssemblyOccurrenceSolver solver;
  auto motion = h::checked(solver.move(raw, path,
                                       {AssemblyJointId("layout.slide"),
                                        {{AssemblyJointCoordinateRole::Translation,
                                          h::checked(Quantity::linear_displacement_mm(20))}}}));
  REQUIRE(motion.converged());
  auto rejected = solver.apply(raw, motion);
  REQUIRE_FALSE(rejected);
  CHECK(saved(raw) == before);
  auto valid = h::checked(solver.move(raw, path,
                                      {AssemblyJointId("layout.slide"),
                                       {{AssemblyJointCoordinateRole::Translation,
                                         h::checked(Quantity::linear_displacement_mm(1))}}}));
  REQUIRE(valid.converged());
  REQUIRE(solver.apply(raw, valid));
  const auto* hole = raw.find_part_document(DocumentId("base.part"))
                         ->find_hole_feature(FeatureId("mounting.hole"));
  REQUIRE(hole);
  REQUIRE(hole->placements().size() == 1);
  const auto placement = std::get<Point2>(hole->placements().front().source);
  CHECK(placement.x == Catch::Approx(2).margin(1e-6));
  CHECK(placement.y == Catch::Approx(-2).margin(1e-6));
  const auto body = compute(*raw.find_part_document(DocumentId("base.part")));
  CHECK_FALSE(material(*body.find_body_shape(BodyId("body.base")), {2, -2, 6}));
  CHECK(material(*body.find_body_shape(BodyId("body.base")), {2, -3, 6}));
}

TEST_CASE("Legacy shared-child solve normalizes current directed machining before local snapshots",
          "[geometry][assembly-hole][parity-b197][legacy-flex-normalization]") {
  auto coordinated = bind(a::project(), a::feature());
  auto raw = coordinated.project;
  const auto child = h::checked(serialize_assembly_document_to_json(
      *raw.find_assembly_document(DocumentId("mounting.child"))));
  REQUIRE(
      raw.find_part_document(DocumentId("cover.part"))
          ->set_parameter_value(ParameterId("layout.spacing"), h::checked(Quantity::length_mm(4))));
  const auto before = saved(raw);
  const auto result = h::checked(AssemblyFlexibleSubassemblySolver{}.solve(
      raw, {SubassemblyInstanceId("pair")}, {ComponentInstanceId("base")}));
  REQUIRE(result.converged());
  CHECK_FALSE(result.occurrence_context_snapshot.empty());
  CHECK(saved(raw) == before);
  REQUIRE(AssemblyFlexibleSubassemblySolveResultApplier{}.apply(raw, result));
  placements(raw, true, 2, 1);
  CHECK(h::checked(serialize_assembly_document_to_json(
            *raw.find_assembly_document(DocumentId("mounting.child")))) == child);
  CHECK(raw.occurrence_poses().empty());
}

TEST_CASE("Legacy shared-child free-pose proposals validate complete machining before publication",
          "[geometry][assembly-hole][parity-b197][legacy-flex-normalization]") {
  auto p = a::project(false);
  auto repeat = a::parent_pose();
  repeat.translation_mm.x += 100;
  REQUIRE(p.assembly().add_subassembly_instance(h::checked(SubassemblyInstance::create(
      SubassemblyInstanceId("repeat"), "Repeated shared child", DocumentId("mounting.child"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active, repeat))));
  const auto feature = a::feature(false);
  auto participants = feature.participants();
  for (auto& participant : participants)
    participant.definition_policy = AssemblyHoleDefinitionPolicy::AffectSharedDefinition;
  auto coordinated =
      bind(p, h::checked(AssemblyHoleFeature::create(feature.id(), feature.name(), feature.layout(),
                                                     std::move(participants))));
  auto raw = coordinated.project;
  const auto before = saved(raw);
  const auto free = h::checked(AssemblyFlexibleSubassemblySolver{}.solve(
      raw, {SubassemblyInstanceId("pair")}, {ComponentInstanceId("cover")}));
  REQUIRE(free.converged());
  REQUIRE(free.local_result.proposed_transforms.size() == 1);
  // With no local residuals, every finite cover pose is a local solution.
  // The complete Project must still validate its dependent machining.
  auto invalid = free;
  invalid.local_result.proposed_transforms.front().proposed_transform.translation_mm.x = 100;
  CHECK_FALSE(AssemblyFlexibleSubassemblySolveResultApplier{}.apply(raw, invalid));
  CHECK(saved(raw) == before);
  auto valid = free;
  valid.local_result.proposed_transforms.front().proposed_transform.translation_mm.x = 3;
  REQUIRE(AssemblyFlexibleSubassemblySolveResultApplier{}.apply(raw, valid));
  CHECK(raw.occurrence_poses().empty());
  const auto leaves = h::checked(
      AssemblyLeafOccurrenceResolver{}.resolve(raw, AssemblyLeafParticipation::PhysicalActive));
  std::size_t covers = 0;
  for (const auto& leaf : leaves)
    if (leaf.component_instance == ComponentInstanceId("cover")) {
      ++covers;
      CHECK(leaf.transforms_inner_to_outer.front().translation_mm.x ==
            Catch::Approx(3).margin(1e-6));
    }
  CHECK(covers == 2);
  CHECK(raw.find_assembly_document(DocumentId("mounting.child"))
            ->find_component_instance(ComponentInstanceId("cover"))
            ->transform()
            .translation_mm.x == 3);
  placements(raw, false, 3);
}
