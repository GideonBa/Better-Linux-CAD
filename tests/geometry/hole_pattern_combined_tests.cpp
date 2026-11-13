#include "../support/engineering_resource_fixture.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "../support/phase_b_acceptance_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "assembly_hole_geometry_scope.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_interaction.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/thread_adapter.hpp"
#include "imported_body_test_fixture.hpp"
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>
#include <numbers>
#include <set>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::hole_pattern_fixture;
namespace resource = blcad::test::engineering_fixture;
using f::checked;
namespace {
ShapeCache evaluate(const PartDocument& p, StepImportTopologyContext* imports = nullptr) {
  auto c = checked(ShapeCache::create(ShapeCacheId("pattern.combined")));
  auto result = GeometryRecomputeExecutor{imports}.execute_document(p, c);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  REQUIRE(c.validate_complete_geometry_result(p));
  return c;
}
double volume(const GeometryShape& s) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(*OcctShapeView::get(s), p, 1e-9);
  return p.Mass();
}
std::string bytes(const GeometryShape& s) {
  std::ostringstream out;
  BRepTools::Write(*OcctShapeView::get(s), out);
  return out.str();
}
HoleInstanceReference instance(unsigned i = 0, unsigned j = 0,
                               HoleReferenceRole role = HoleReferenceRole::Seat) {
  return checked(
      HoleInstanceReference::create(FeatureId("holes"), HolePlacementId("first"), role,
                                    HolePatternOccurrence{FeatureId("pattern"), {i, j}}));
}
void length(PartDocument& p, const char* id, double v) {
  REQUIRE(p.set_parameter_value(ParameterId(id), checked(Quantity::length_mm(v))));
}
void count(PartDocument& p, unsigned n) {
  REQUIRE(p.set_parameter_value(ParameterId("count"), checked(Quantity::count(n))));
}
HoleFeature altered(const HoleFeature& h, HoleExtent extent, HoleTip tip,
                    HoleManufacturing manufacturing, std::optional<ThreadDefinition> thread = {}) {
  return checked(HoleFeature::create(h.id(), h.name(), h.kind(), h.target_body(), h.support(),
                                     h.placements(), h.diameter(), std::move(extent), h.direction(),
                                     std::move(tip), std::move(manufacturing), std::move(thread)));
}
void near(Point3 actual, Point3 expected) {
  CHECK(actual.x == Catch::Approx(expected.x).margin(1e-6));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1e-6));
  CHECK(actual.z == Catch::Approx(expected.z).margin(1e-6));
}
PartDocument pin() {
  auto p = checked(PartDocument::create(DocumentId("pin"), "Pin"));
  REQUIRE(p.add_datum_plane(checked(DatumPlane::xy())));
  for (const auto& [id, v] : {std::pair{"diameter", 3.}, {"depth", 5.}})
    REQUIRE(p.add_parameter(
        checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(v))))));
  auto sketch =
      checked(Sketch::create(SketchId("profile"), "Pin profile", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      checked(CircleProfile::create(ProfileId("circle"), ParameterId("diameter")))));
  REQUIRE(p.add_sketch(sketch));
  const auto context = blcad::test::history_fixture::new_body(p, "pin");
  REQUIRE(p.add_feature(
      checked(checked(Feature::create_additive_extrude(FeatureId("pin"), "Pin", SketchId("profile"),
                                                       ParameterId("depth"),
                                                       ExtrudeDirection::OppositeSketchNormal))
                  .with_body_result_context(context))));
  resource::annotate(p);
  return p;
}
AssemblyConstraintTarget target(const char* component, std::string spelling) {
  return checked(
      AssemblyConstraintTarget::create(ComponentInstanceId(component), std::move(spelling)));
}
} // namespace
TEST_CASE("Hole operation Patterns follow seed rotation scale and later Body translation",
          "[geometry][parity-b156][hole-pattern-combined]") {
  auto p = f::part();
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("rotate"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, checked(BodyTransformRotationAxis::create_explicit({}, {0, 0, 1})), 90, false,
      false))));
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_uniform_scale(
      BodyTransformId("scale"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, 2, {}, false, false))));
  REQUIRE(p.add_datum_axis(checked(
      DatumAxis::create_explicit(DatumAxisId("negative.x"), "Negative X", {}, {-1, 0, 0}))));
  auto options = f::options();
  options.second_direction->direction = f::axis("negative.x");
  REQUIRE(p.add_linear_pattern_feature(checked(LinearPatternFeature::create(
      FeatureId("pattern"), "Posed Holes", {f::source()}, f::axis("axis.y"), ParameterId("count"),
      LinearPatternExtentMode::Spacing, ParameterId("spacing.x"), PatternDirectionSign::Positive,
      f::context(), options))));
  auto c = evaluate(p);
  near(checked(HoleInstanceGeometry::resolve(p, instance(2, 1), c)).frame.origin, {15, -16, 14});
  CHECK(checked(HoleInstanceGeometry::resolve(p, instance(2, 1), c)).radius_mm == Catch::Approx(3));
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(8 * (38400 - 6 * 17.5 * std::numbers::pi)).margin(1e-5));
  REQUIRE(p.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("move"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {3, 4, 5}, false, false))));
  c = evaluate(p);
  near(checked(HoleInstanceGeometry::resolve(p, instance(2, 1), c)).frame.origin, {18, -12, 19});
  const auto saved = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(saved));
  auto replay = evaluate(reopened);
  near(checked(HoleInstanceGeometry::resolve(reopened, instance(2, 1), replay)).frame.origin,
       {18, -12, 19});
}
TEST_CASE("Hole Patterns copy current cosmetic and modeled Threads with independent volume and "
          "STEP proof",
          "[geometry][parity-b156][hole-pattern-combined][hole-pattern-thread]") {
  for (auto mode : {ThreadRepresentation::Cosmetic, ThreadRepresentation::Modeled}) {
    auto p = f::part();
    count(p, 2);
    REQUIRE(p.set_parameter_value(ParameterId("rows"), checked(Quantity::count(1))));
    auto thread_part = blcad::test::thread_fixture::part();
    for (const auto& parameter : thread_part.parameters())
      if (parameter.id().value().starts_with("thread."))
        REQUIRE(p.add_parameter(parameter));
    length(p, "thread.major", 4);
    length(p, "thread.minor", 3);
    length(p, "thread.pitch", 1.2);
    length(p, "thread.length", 3);
    auto thread = blcad::test::thread_fixture::definition(blcad::test::thread_fixture::custom(),
                                                          true, ThreadHandedness::Left, mode);
    const auto h = *p.find_hole_feature(FeatureId("holes"));
    REQUIRE(p.update_hole_feature(altered(h, h.extent(), h.tip(), h.manufacturing(), thread)));
    resource::annotate(p);
    const auto seed = evaluate(p);
    const auto seed_bytes = bytes(*seed.find_body_shape(BodyId("body.base")));
    REQUIRE(p.add_linear_pattern_feature(f::linear()));
    auto c = evaluate(p);
    const auto& shape = *c.find_body_shape(BodyId("body.base"));
    const double groove = mode == ThreadRepresentation::Modeled
                              ? 4 * std::numbers::pi * std::tan(55 * std::numbers::pi / 360) * 3 /
                                    1.2 * (2 * .5 * .5 / 2 - .5 * .5 * .5 / 3)
                              : 0;
    const double expected = 38400 - 2 * (17.5 * std::numbers::pi + groove);
    CHECK(volume(shape) == Catch::Approx(expected).margin(2e-4));
    CHECK(bytes(*seed.find_body_shape(BodyId("body.base"))) == seed_bytes);
    CHECK(checked(ThreadAdapter::cosmetic_presentations(shape)).size() ==
          (mode == ThreadRepresentation::Cosmetic ? 2 : 0));
    near(checked(HoleInstanceGeometry::resolve(p, instance(1, 0, HoleReferenceRole::Axis), c))
             .frame.origin,
         {-8, -15, 8});
    CHECK(checked(BodyResultInspector{}.inspect_physical_properties(p))
              .data()
              .complete_mass()
              ->mass_kg == Catch::Approx(expected * 4e-6).margin(1e-9));
    imported_body_test::Fixture output;
    REQUIRE(StepExporter{}.write_part_step(p, c, output.path()));
    auto source = checked(StepImportSource::create(
        StepImportSourceId("roundtrip"), "Roundtrip", "supplier.step",
        checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
    auto imported = checked(StepImportReader{}.read(source, output.root));
    REQUIRE(imported.body_count() == 1);
    const auto& body = imported.products.front().bodies.front();
    CHECK(body.volume_mm3 == Catch::Approx(expected).margin(2e-4));
    CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(body.shape), gp_Pnt(-8, -15, 3), 1e-7)
              .State() == TopAbs_OUT);
    for (double z : {.35, 1.05, 2.7})
      for (double theta : {.23, 2.39, 5.31})
        for (double r : {1.65, 1.9}) {
          const double phase = std::remainder(z + 1.2 * theta / (2 * std::numbers::pi), 1.2);
          const double width = (2 - r) * std::tan(55 * std::numbers::pi / 360);
          if (std::abs(std::abs(phase) - width) < .025)
            continue;
          const bool cut = mode == ThreadRepresentation::Modeled && std::abs(phase) < width;
          const gp_Pnt point(-8 + r * std::cos(theta), -15 - r * std::sin(theta), 7 - z);
          for (const auto* actual : {&shape, &body.shape})
            CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(*actual), point, 1e-7).State() ==
                  (cut ? TopAbs_OUT : TopAbs_IN));
        }
    const auto saved = checked(serialize_part_document_to_json(p));
    CHECK(checked(serialize_part_document_to_json(
              checked(deserialize_part_document_from_json(saved)))) == saved);
  }
}
TEST_CASE(
    "Assembly Insert consumes individual Hole Pattern seats and diagnoses only removed occurrences",
    "[geometry][parity-b156][hole-pattern-combined][hole-pattern-assembly]") {
  auto plate = f::part();
  count(plate, 2);
  REQUIRE(plate.set_parameter_value(ParameterId("rows"), checked(Quantity::count(1))));
  REQUIRE(plate.add_linear_pattern_feature(f::linear()));
  resource::annotate(plate);
  auto bolt = pin();
  auto root = checked(AssemblyDocument::create(DocumentId("root"), "Patterned plate and pins"));
  REQUIRE(root.add_member_part(plate.id()));
  REQUIRE(root.add_member_part(bolt.id()));
  REQUIRE(root.add_component_instance(checked(
      ComponentInstance::create(ComponentInstanceId("plate"), "Plate", plate.id(),
                                ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                ComponentGroundingState::Grounded, {{10, 20, 30}, {0, 0, 90}}))));
  for (auto id : {"first", "second"})
    REQUIRE(root.add_component_instance(
        checked(ComponentInstance::create(ComponentInstanceId(id), id, bolt.id()))));
  const auto frame = checked(make_assembly_frame_target_spelling(DatumPlaneId("datum.xy")));
  for (unsigned i = 0; i < 2; ++i) {
    const auto id = i ? "second" : "first";
    REQUIRE(root.add_constraint(checked(AssemblyConstraint::create(
        AssemblyConstraintId(id), id, AssemblyConstraintType::Insert,
        target("plate", checked(make_hole_target_spelling(instance(i)))), target(id, frame)))));
  }
  auto project = checked(Project::create(DocumentId("project"), "Pattern assembly", root));
  REQUIRE(project.add_part_document(plate));
  REQUIRE(project.add_part_document(bolt));
  auto saved = checked(serialize_project_to_json(project));
  project = checked(deserialize_project_from_json(saved));
  const auto solved = checked(AssemblyRigidBodySolver{}.solve(
      project,
      {ComponentInstanceId("first"), ComponentInstanceId("plate"), ComponentInstanceId("second")}));
  REQUIRE(solved.converged());
  REQUIRE(AssemblySolveResultApplier{}.apply(project, solved));
  for (unsigned i = 0; i < 2; ++i) {
    const auto id = i ? "second" : "first";
    const auto* component = project.assembly().find_component_instance(ComponentInstanceId(id));
    near({component->transform().translation_mm.x, component->transform().translation_mm.y,
          component->transform().translation_mm.z},
         {25, 12. * i, 37});
    const auto eq = checked(AssemblyInsertConstraintEquationBuilder{}.build(
        project, *project.assembly().find_constraint(AssemblyConstraintId(id))));
    CHECK(eq.residual.signed_seating_separation_mm == Catch::Approx(0).margin(1e-6));
    CHECK(length(eq.residual.axis_offset_mm) < 1e-6);
  }
  const auto catalog = checked(AssemblyInteractionGeometry::targets(project));
  std::set<std::string> hole_seats;
  for (const auto& item : catalog)
    if (item.resolved.source_metadata.hole_instance &&
        item.resolved.source_metadata.hole_instance->role() == HoleReferenceRole::Seat)
      hole_seats.insert(item.reference.semantic_reference());
  CHECK(hole_seats.size() == 2);
  imported_body_test::Fixture output;
  REQUIRE(AssemblyStructuredStepExporter{}.write_step(project, output.path()));
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("roundtrip"), "Roundtrip", "supplier.step",
      checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
  const auto imported = checked(StepImportReader{}.read(source, output.root));
  CHECK(imported.body_count() == 2); // one shared pin definition and one plate definition
  saved = checked(serialize_project_to_json(project));
  project = checked(deserialize_project_from_json(saved));
  auto* p = project.find_part_document(plate.id());
  count(*p, 1);
  CHECK(AssemblyInsertConstraintEquationBuilder{}.build(
      project, *project.assembly().find_constraint(AssemblyConstraintId("first"))));
  CHECK_FALSE(AssemblyInsertConstraintEquationBuilder{}.build(
      project, *project.assembly().find_constraint(AssemblyConstraintId("second"))));
  CHECK(project.assembly().constraint_count() == 2);
  count(*p, 2);
  CHECK(AssemblyInsertConstraintEquationBuilder{}.build(
      project, *project.assembly().find_constraint(AssemblyConstraintId("second"))));
}

TEST_CASE("Identical Hole Patterns retain Through All conical tips and stable Step seats",
          "[geometry][parity-b156][hole-pattern-combined][hole-pattern-extents]") {
  for (int mode = 0; mode < 3; ++mode) {
    CAPTURE(mode);
    auto p = f::part();
    const auto h = *p.find_hole_feature(FeatureId("holes"));
    double cut = 22 * std::numbers::pi;
    if (mode == 0)
      REQUIRE(p.update_hole_feature(
          altered(h, HoleThroughAllExtent{}, HoleFlatTip{}, h.manufacturing())));
    if (mode == 1) {
      REQUIRE(p.add_parameter(checked(Parameter::create_angle(ParameterId("tip.angle"), "Tip angle",
                                                              checked(Quantity::angle_deg(118))))));
      REQUIRE(p.update_hole_feature(
          altered(h, h.extent(), HoleConicalTip{ParameterId("tip.angle")}, h.manufacturing())));
      cut = std::numbers::pi * (17.5 + 2.25 * .5 / std::tan(59 * std::numbers::pi / 180));
    }
    if (mode == 2) {
      REQUIRE(p.add_parameter(checked(Parameter::create_length(
          ParameterId("step.diameter"), "Step diameter", checked(Quantity::length_mm(4))))));
      REQUIRE(p.add_parameter(checked(Parameter::create_length(
          ParameterId("step.depth"), "Step depth", checked(Quantity::length_mm(3))))));
      REQUIRE(p.update_hole_feature(altered(
          h, h.extent(), HoleFlatTip{},
          HoleStepped{{{"wide", ParameterId("seat.diameter"), ParameterId("seat.depth")},
                       {"middle", ParameterId("step.diameter"), ParameterId("step.depth")}}})));
      cut = 21 * std::numbers::pi;
    }
    REQUIRE(p.add_linear_pattern_feature(f::linear()));
    auto c = evaluate(p);
    CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
          Catch::Approx(38400 - 6 * cut).margin(2e-6));
    if (mode == 2) {
      auto step = checked(HoleInstanceReference::create(
          FeatureId("holes"), HolePlacementId("first"), HoleReferenceRole::Step,
          HolePatternOccurrence{FeatureId("pattern"), {2, 1}}, "middle"));
      near(checked(HoleInstanceGeometry::resolve(p, step, c)).frame.origin, {4, 0, 5});
      CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance(2, 1), c));
    }
    auto prefix = checked(ShapeCache::create(ShapeCacheId("prefix")));
    REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(p, 2, prefix));
    CHECK(volume(*prefix.find_body_shape(BodyId("body.base"))) ==
          Catch::Approx(38400 - cut).margin(1e-6));
    REQUIRE(p.apply_history_intent({{"base", "holes", "pattern"}, {"pattern"}}));
    c = evaluate(p);
    CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
          Catch::Approx(38400 - cut).margin(1e-6));
    CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance(1, 0, HoleReferenceRole::Axis), c));
    REQUIRE(p.apply_history_intent({{"base", "holes", "pattern"}, {}}));
    c = evaluate(p);
    CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
          Catch::Approx(38400 - 6 * cut).margin(2e-6));
  }
}

TEST_CASE(
    "Imported Hole Patterns verify exposed entries and actual local through termination atomically",
    "[geometry][parity-b156][hole-pattern-combined][hole-pattern-import]") {
  for (int mode = 0; mode < 3; ++mode) {
    CAPTURE(mode);
    imported_body_test::Fixture input;
    auto stock = BRepPrimAPI_MakeBox(gp_Pnt(-40, -30, 0), 80, 60, 8).Shape();
    if (mode)
      stock = BRepAlgoAPI_Fuse(
                  stock, BRepPrimAPI_MakeBox(gp_Pnt(0, -30, mode == 1 ? 8 : -2), 40, 60, 2).Shape())
                  .Shape();
    input.write(stock);
    const auto supplier = input.source_bytes();
    auto p = input.editable();
    const auto seed = f::part();
    for (const auto& parameter : seed.parameters())
      REQUIRE(p.add_parameter(parameter));
    for (const auto& axis : seed.datum_axes())
      REQUIRE(p.add_datum_axis(axis));
    REQUIRE(p.add_construction_plane(checked(ConstructionPlane::create_explicit(
        ConstructionPlaneId("entry"), "Entry", {0, 0, 8}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}))));
    const auto body = p.imported_body_features().front().definition().outputs().front().body;
    const auto support = checked(PlaneReference::create_construction_plane(
        PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("entry")));
    const auto h = *seed.find_hole_feature(FeatureId("holes"));
    REQUIRE(p.add_hole_feature(checked(
        HoleFeature::create(h.id(), h.name(), h.kind(), body, support, h.placements(), h.diameter(),
                            HoleThroughAllExtent{}, h.direction(), h.tip(), h.manufacturing()))));
    StepImportTopologyContext context(input.root);
    auto c = evaluate(p, &context);
    const auto original = p;
    const auto prior = bytes(*c.find_body_shape(body));
    const auto immutable =
        bytes(c.validated_imported_body_result(p, p.imported_body_features().front())
                  .value()
                  ->imported.bodies.front()
                  .shape);
    const auto result_context = checked(
        FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, body, std::nullopt));
    REQUIRE(p.add_linear_pattern_feature(checked(LinearPatternFeature::create(
        FeatureId("pattern"), "Imported holes", {f::source()}, f::axis("axis.x"),
        ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("spacing.x"),
        PatternDirectionSign::Positive, result_context, f::options()))));
    const auto evaluated = GeometryRecomputeExecutor{&context}.execute_document(p, c);
    if (mode) {
      REQUIRE_FALSE(evaluated);
      CHECK(c.validate_complete_geometry_result(original));
      CHECK(bytes(*c.find_body_shape(body)) == prior);
      CHECK(evaluated.error().message().find(mode == 1 ? "entry" : "termination") !=
            std::string::npos);
    } else {
      REQUIRE(evaluated);
      CHECK(volume(*c.find_body_shape(body)) ==
            Catch::Approx(38400 - 6 * 22 * std::numbers::pi).margin(1e-6));
      near(checked(HoleInstanceGeometry::resolve(p, instance(2, 1), c)).frame.origin, {4, 0, 7});
      const auto current = bytes(*c.find_body_shape(body));
      input.write_bytes(supplier + "\n");
      StepImportTopologyContext changed(input.root);
      CHECK_FALSE(GeometryRecomputeExecutor{&changed}.execute_document(p, c));
      CHECK(bytes(*c.find_body_shape(body)) == current);
      input.write_bytes(supplier);
    }
    CHECK(bytes(c.validated_imported_body_result(mode ? original : p,
                                                 p.imported_body_features().front())
                    .value()
                    ->imported.bodies.front()
                    .shape) == immutable);
    CHECK(input.source_bytes() == supplier);
  }
}

TEST_CASE("A Hole Pattern retains axes and seats through old Fillet Chamfer and diameter edits",
          "[geometry][parity-b156][hole-pattern-combined][hole-pattern-finishing]") {
  auto p = f::part();
  length(p, "radius", .5);
  REQUIRE(p.add_fillet_feature(checked(
      FilletFeature::create(FeatureId("fillet"), "Outer Fillet", BodyId("body.base"),
                            {blcad::test::history_fixture::edge(PartFeatureInputRole::FilletEdge)},
                            ParameterId("radius")))));
  const auto before = evaluate(p);
  const auto oldvolume = volume(*before.find_body_shape(BodyId("body.base")));
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  auto c = evaluate(p);
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(oldvolume - 5 * 17.5 * std::numbers::pi).margin(1e-6));
  const auto targets = checked(FinishingSelectionBuilder::build(p, c, BodyId("body.base"),
                                                                PartFeatureInputRole::ChamferEdge));
  const auto rim = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    const auto* witness =
        std::get_if<ResultEdgeReference>(&std::get<EdgeReference>(target.reference).source());
    return witness && witness->curve_type() == 1 && std::abs(target.anchor.z - 8) < 1e-7 &&
           std::abs(target.anchor.x + 8) < 3 && std::abs(target.anchor.y + 15) < 3;
  });
  REQUIRE(rim != targets.end());
  length(p, "radius", .25);
  REQUIRE(p.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
      FeatureId("chamfer"), "Copied Hole rim", BodyId("body.base"),
      {std::get<EdgeReference>(rim->reference)}, ParameterId("radius")))));
  c = evaluate(p);
  near(checked(HoleInstanceGeometry::resolve(p, instance(1, 0), c)).frame.origin, {-8, -15, 7});
  CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance(1, 0, HoleReferenceRole::Entry), c));
  length(p, "diameter", 4);
  const auto plan =
      checked(RecomputePlan::from_graph_and_changed_nodes(p.dependency_graph(), {"diameter"}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(p, plan, c));
  near(checked(HoleInstanceGeometry::resolve(p, instance(1, 0), c)).frame.origin, {-8, -15, 7});
  CHECK(checked(HoleInstanceGeometry::resolve(p, instance(1, 0), c)).radius_mm == Catch::Approx(2));
  const auto fresh = evaluate(p);
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(volume(*fresh.find_body_shape(BodyId("body.base")))).margin(1e-6));
  const auto saved = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(saved));
  const auto replay = evaluate(reopened);
  near(checked(HoleInstanceGeometry::resolve(reopened, instance(1, 0), replay)).frame.origin,
       {-8, -15, 7});
}

TEST_CASE("Pinned Hole revisions and Assembly bindings drive the existing Pattern dependency chain",
          "[geometry][parity-b156][hole-pattern-combined][hole-pattern-resource]") {
  namespace holes = blcad::test::hole_fixture;
  auto p = f::part();
  REQUIRE(p.add_engineering_resource(holes::table("r1", 3)));
  const auto h = *p.find_hole_feature(FeatureId("holes"));
  REQUIRE(p.update_hole_feature(checked(HoleFeature::create(
      h.id(), h.name(), h.kind(), h.target_body(), h.support(), h.placements(), holes::standard(),
      h.extent(), h.direction(), h.tip(), h.manufacturing()))));
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  resource::annotate(p);
  auto c = evaluate(p);
  REQUIRE(p.adopt_engineering_resource_revision(holes::key(), holes::table("r2", 4)));
  auto plan = checked(RecomputePlan::from_graph_and_changed_nodes(p.dependency_graph(), {"holes"}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(p, plan, c));
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(38400 - 6 * 26.25 * std::numbers::pi).margin(1e-6));
  CHECK(checked(HoleInstanceGeometry::resolve(p, instance(2, 1), c)).radius_mm == Catch::Approx(2));
  auto root = checked(AssemblyDocument::create(DocumentId("bindings"), "Pattern count driver"));
  REQUIRE(root.add_member_part(p.id()));
  REQUIRE(root.add_parameter(checked(Parameter::create_count(
      ParameterId("driver"), "Count", checked(Quantity::count(2)), ParameterScope::Assembly))));
  REQUIRE(root.add_binding(checked(ParameterBinding::create(
      ParameterBindingId("count.binding"), p.id(), ParameterId("count"), ParameterId("driver")))));
  REQUIRE(root.apply_bindings_to(p));
  c = evaluate(p);
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(38400 - 4 * 26.25 * std::numbers::pi).margin(1e-6));
  CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance(2, 1), c));
  near(checked(HoleInstanceGeometry::resolve(p, instance(1, 1), c)).frame.origin, {-8, 0, 7});
  const auto mass = checked(BodyResultInspector{}.inspect_physical_properties(p));
  CHECK(mass.data().complete_mass()->mass_kg ==
        Catch::Approx((38400 - 4 * 26.25 * std::numbers::pi) * 4e-6));
  const auto saved = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(saved));
  const auto replay = evaluate(reopened);
  CHECK(volume(*replay.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(volume(*c.find_body_shape(BodyId("body.base")))).margin(1e-6));
}

TEST_CASE("Assembly solve scopes reuse unchanged Hole geometry and reject stale or failed intent",
          "[geometry][parity-b156][hole-pattern-combined][hole-pattern-assembly]") {
  auto p = f::part();
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  std::shared_ptr<const ShapeCache> last;
  {
    const detail::AssemblyHoleGeometryScope scope;
    const auto first = checked(detail::assembly_hole_part_geometry(p));
    CHECK(checked(detail::assembly_hole_part_geometry(p)) == first);
    {
      const detail::AssemblyHoleGeometryScope nested;
      CHECK(checked(detail::assembly_hole_part_geometry(p)) == first);
    }
    CHECK(checked(detail::assembly_hole_part_geometry(p)) == first);
    const auto original = p;
    count(p, 2);
    last = checked(detail::assembly_hole_part_geometry(p));
    CHECK(last != first);
    CHECK(first->validate_complete_geometry_result(original));
    CHECK_FALSE(first->validate_complete_geometry_result(p));
    CHECK(last->validate_complete_geometry_result(p));
    CHECK(volume(*last->find_body_shape(BodyId("body.base"))) ==
          Catch::Approx(38400 - 4 * 17.5 * std::numbers::pi).margin(1e-6));
    count(p, 1024);
    CHECK_FALSE(detail::assembly_hole_part_geometry(p));
    count(p, 2);
    CHECK(checked(detail::assembly_hole_part_geometry(p)) == last);
  }
  // No process-global geometry is retained between requests.
  CHECK(checked(detail::assembly_hole_part_geometry(p)) != last);
}

TEST_CASE("Explicit two-direction whole Body copies retain their separate positive operation path",
          "[geometry][parity-b156][pattern-placement][hole-pattern-combined]") {
  auto p = f::part();
  length(p, "spacing.x", 100);
  length(p, "spacing.y", 80);
  REQUIRE(p.add_body(checked(Body::create(BodyId("body.copy"), "Copies", BodyKind::Solid))));
  const auto context = checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                                std::nullopt, BodyId("body.copy")));
  const auto source = checked(PatternSourceReference::body(BodyId("body.base")));
  const auto create = [&](PatternPlacementOptions options) {
    return checked(LinearPatternFeature::create(
        FeatureId("pattern"), "Whole Body copies", {source}, f::axis("axis.x"),
        ParameterId("count"), LinearPatternExtentMode::Spacing, ParameterId("spacing.x"),
        PatternDirectionSign::Positive, context, std::move(options)));
  };
  REQUIRE(p.add_linear_pattern_feature(create(f::options())));
  auto c = evaluate(p);
  CHECK(volume(*c.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(38400 - 17.5 * std::numbers::pi).margin(1e-6));
  CHECK(volume(*c.find_body_shape(BodyId("body.copy"))) ==
        Catch::Approx(6 * (38400 - 17.5 * std::numbers::pi)).margin(2e-6));
  const auto origin =
      copy_result_face_origin(FeatureId("pattern"), pattern_source_occurrence_key(source, {2, 1}),
                              {FeatureId("holes"), "hole/5:first/seat/bottom"});
  const auto reference =
      checked(ResultFaceReference::create(FeatureId("pattern"), BodyId("body.copy"), {origin}));
  const auto seat = checked(ResultFaceSupportGeometry::resolve_face(p, c, reference));
  GProp_GProps props;
  BRepGProp::SurfaceProperties(*OcctShapeView::get(seat), props);
  const auto center = props.CentreOfMass();
  near({center.X(), center.Y(), center.Z()}, {180, 65, 7});
  CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance(2, 1), c));
  auto options = f::options();
  options.skipped = {{1, 0}};
  REQUIRE(p.update_linear_pattern_feature(create(options)));
  c = evaluate(p);
  CHECK(volume(*c.find_body_shape(BodyId("body.copy"))) ==
        Catch::Approx(5 * (38400 - 17.5 * std::numbers::pi)).margin(2e-6));
  CHECK(ResultFaceSupportGeometry::resolve_face(p, c, reference));
  count(p, 1);
  REQUIRE(p.set_parameter_value(ParameterId("rows"), checked(Quantity::count(1))));
  c = evaluate(p);
  CHECK(volume(*c.find_body_shape(BodyId("body.copy"))) ==
        Catch::Approx(38400 - 17.5 * std::numbers::pi).margin(1e-6));
  CHECK_FALSE(ResultFaceSupportGeometry::resolve_face(p, c, reference));
}

TEST_CASE("B04 plate and cover share bound Hole counts physical seats mass and structured STEP",
          "[geometry][parity-b156][hole-pattern-combined][hole-pattern-b04]") {
  auto base = f::part(true);
  const auto seed = *base.find_hole_feature(FeatureId("holes"));
  REQUIRE(base.update_hole_feature(
      altered(seed, HoleThroughAllExtent{}, seed.tip(), seed.manufacturing())));
  REQUIRE(base.add_linear_pattern_feature(f::linear()));
  resource::annotate(base);
  auto data = nlohmann::json::parse(checked(serialize_part_document_to_json(f::part(true))));
  data["document"]["id"] = "cover";
  auto cover = checked(deserialize_part_document_from_json(data.dump()));
  length(cover, "depth", 4);
  length(cover, "seat.depth", .5);
  REQUIRE(cover.add_construction_plane(checked(
      ConstructionPlane::create_explicit(ConstructionPlaneId("underside"), "Cover underside", {},
                                         {1, 0, 0}, {0, -1, 0}, {0, 0, -1}))));
  const auto underside = checked(PlaneReference::create_construction_plane(
      PartFeatureInputRole::HoleSupportPlane, ConstructionPlaneId("underside")));
  REQUIRE(cover.update_hole_feature(checked(HoleFeature::create(
      seed.id(), seed.name(), seed.kind(), seed.target_body(), underside,
      {{HolePlacementId("first"), Point2{-20, 15}}, {HolePlacementId("other"), Point2{-20, 6}}},
      seed.diameter(), HoleThroughAllExtent{}, seed.direction(), seed.tip(),
      seed.manufacturing()))));
  REQUIRE(cover.add_linear_pattern_feature(f::linear()));
  resource::annotate(cover);
  REQUIRE(cover.set_default_material(resource::key("synthetic.material", "r2")));
  auto root = checked(AssemblyDocument::create(DocumentId("b04.root"), "Plate and cover"));
  REQUIRE(root.add_parameter(
      checked(Parameter::create_count(ParameterId("driver"), "Hole count",
                                      checked(Quantity::count(3)), ParameterScope::Assembly))));
  for (const auto* p : {&base, &cover}) {
    REQUIRE(root.add_member_part(p->id()));
    REQUIRE(root.add_binding(
        checked(ParameterBinding::create(ParameterBindingId("binding." + p->id().value()), p->id(),
                                         ParameterId("count"), ParameterId("driver")))));
  }
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("base"), "Base", base.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Grounded))));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("cover"), "Cover", cover.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, {{0, 0, 11}, {}}))));
  for (unsigned i : {1U, 2U}) {
    const auto id = i == 1 ? "retained" : "removed";
    const auto spelling =
        checked(make_hole_target_spelling(instance(i, 1, HoleReferenceRole::Entry)));
    REQUIRE(root.add_constraint(checked(
        AssemblyConstraint::create(AssemblyConstraintId(id), id, AssemblyConstraintType::Insert,
                                   target("base", spelling), target("cover", spelling)))));
  }
  auto project = checked(Project::create(DocumentId("b04.project"), "B04", root));
  REQUIRE(project.add_part_document(base));
  REQUIRE(project.add_part_document(cover));
  const auto solved = checked(AssemblyRigidBodySolver{}.solve(
      project, {ComponentInstanceId("base"), ComponentInstanceId("cover")}));
  REQUIRE(solved.converged());
  REQUIRE(AssemblySolveResultApplier{}.apply(project, solved));
  const auto pose =
      project.assembly().find_component_instance(ComponentInstanceId("cover"))->transform();
  const auto center = AssemblyHierarchyTransformEvaluator{}.evaluate_point({pose}, {4, 0, 0});
  near(center, {4, 0, 8});
  const auto expected_mass = [](unsigned n) {
    return (38400 - n * 22 * std::numbers::pi) * 4e-6 + (19200 - n * 11 * std::numbers::pi) * 6e-6;
  };
  CHECK(checked(BodyResultInspector{}.inspect_physical_properties(project))
            .data()
            .complete_mass()
            ->mass_kg == Catch::Approx(expected_mass(12)).margin(1e-9));
  REQUIRE(project.set_assembly_parameter_value(ParameterId("driver"), checked(Quantity::count(2))));
  REQUIRE(AssemblyInsertConstraintEquationBuilder{}.build(
      project, *project.assembly().find_constraint(AssemblyConstraintId("retained"))));
  CHECK_FALSE(AssemblyInsertConstraintEquationBuilder{}.build(
      project, *project.assembly().find_constraint(AssemblyConstraintId("removed"))));
  const auto saved = checked(serialize_project_to_json(project));
  CHECK_FALSE(AssemblySolveResultApplier{}.apply(project, solved));
  CHECK(checked(serialize_project_to_json(project)) == saved);
  project = checked(deserialize_project_from_json(saved));
  CHECK(checked(BodyResultInspector{}.inspect_physical_properties(project))
            .data()
            .complete_mass()
            ->mass_kg == Catch::Approx(expected_mass(8)).margin(1e-9));
  imported_body_test::Fixture output;
  REQUIRE(AssemblyStructuredStepExporter{}.write_step(project, output.path()));
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("b04"), "B04", "supplier.step",
      checked(step_import_sha256(output.source_bytes())), StepImportMode::EditableBody));
  const auto imported = checked(StepImportReader{}.read(source, output.root));
  REQUIRE(imported.body_count() == 2);
  std::vector<double> volumes;
  for (const auto& product : imported.products)
    for (const auto& body : product.bodies) {
      volumes.push_back(body.volume_mm3);
      CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(body.shape), gp_Pnt(-8, 0, 2), 1e-7)
                .State() == TopAbs_OUT);
      CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(body.shape), gp_Pnt(10, 20, 2), 1e-7)
                .State() == TopAbs_IN);
    }
  std::sort(volumes.begin(), volumes.end());
  CHECK(volumes[0] == Catch::Approx(19200 - 8 * 11 * std::numbers::pi).margin(1e-6));
  CHECK(volumes[1] == Catch::Approx(38400 - 8 * 22 * std::numbers::pi).margin(1e-6));
  CHECK(std::any_of(
      imported.occurrences.begin(), imported.occurrences.end(),
      [](const auto& occurrence) { return std::abs(occurrence.transform.matrix[11] - 8) < 1e-6; }));
  CHECK(checked(serialize_project_to_json(project)) == saved);
}

TEST_CASE("Phase B fresh and migrated material history matches independent finished Hole solids",
          "[geometry][parity-b158][phase-b-integration]") {
  namespace integrated = blcad::test::phase_b_fixture;
  const bool migrated = GENERATE(false, true);
  CAPTURE(migrated);
  auto part = integrated::complete(migrated);
  const auto saved = checked(serialize_part_document_to_json(part));
  CHECK(nlohmann::json::parse(saved).at("version") == 7);
  REQUIRE(part.hole_features().front().thread());
  CHECK(part.hole_features().front().thread()->representation() == ThreadRepresentation::Cosmetic);
  auto cache = evaluate(part);
  const auto* result = cache.find_body_shape(BodyId("body.base"));
  REQUIRE(result);
  CHECK(volume(*result) == Catch::Approx(integrated::base_volume(10, migrated)).margin(1e-5));
  for (const auto& [point, state] : {std::pair{gp_Pnt(0, 29.99, 7.99), TopAbs_OUT},
                                     {gp_Pnt(0, 29.5, 7.99), TopAbs_IN},
                                     {gp_Pnt(0, -29.99, .01), TopAbs_OUT},
                                     {gp_Pnt(0, -29.7, .01), TopAbs_IN},
                                     {gp_Pnt(-8, 0, 4), TopAbs_OUT},
                                     {gp_Pnt(4, 0, 4), TopAbs_IN},
                                     {gp_Pnt(30, 20, 4), migrated ? TopAbs_OUT : TopAbs_IN}}) {
    CAPTURE(point.X(), point.Y(), point.Z());
    CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(*result), point, 1e-7).State() == state);
  }
  const auto* unrelated = cache.find_body_shape(BodyId("body.unrelated"));
  REQUIRE(unrelated);
  const auto unrelated_owner = *unrelated;
  CHECK(volume(*unrelated) == Catch::Approx(48).margin(1e-7));
  CHECK(part.find_body(BodyId("body.unrelated"))->visibility() == BodyVisibility::Hidden);
  const auto mass = checked(BodyResultInspector{}.inspect_physical_properties(part));
  REQUIRE(mass.data().complete_mass());
  CHECK(mass.data().complete_mass()->mass_kg ==
        Catch::Approx((integrated::base_volume(10, migrated) + 48) * 4e-6).margin(1e-9));

  length(part, "diameter", 4);
  const auto plan =
      checked(RecomputePlan::from_graph_and_changed_nodes(part.dependency_graph(), {"diameter"}));
  const auto execution = checked(GeometryRecomputeExecutor{}.execute_plan(part, plan, cache));
  CHECK_FALSE(execution.reused_complete_geometry);
  CHECK(execution.executed_feature_count == (migrated ? 7 : 6));
  CHECK(volume(*cache.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(integrated::base_volume(10, migrated, 4)).margin(1e-5));
  REQUIRE(cache.find_body_shape(BodyId("body.unrelated")));
  CHECK(volume(*cache.find_body_shape(BodyId("body.unrelated"))) ==
        Catch::Approx(volume(unrelated_owner)).margin(1e-7));
  const auto fresh = evaluate(part);
  CHECK(volume(*fresh.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(volume(*cache.find_body_shape(BodyId("body.base")))).margin(1e-7));
  length(part, "diameter", 3);

  std::vector<std::string> order;
  for (const auto& row : checked(part.executable_history()))
    order.push_back(row.node_id);
  const auto suppressed = checked(part.with_history_intent({order, {"pattern"}}));
  const auto bypassed = evaluate(suppressed);
  CHECK(volume(*bypassed.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(integrated::base_volume(2, migrated)).margin(1e-5));
  REQUIRE(bypassed.find_body_shape(BodyId("body.unrelated")));
  const auto creator_suppressed = checked(part.with_history_intent({order, {"unrelated"}}));
  const auto unavailable = evaluate(creator_suppressed);
  CHECK_FALSE(unavailable.find_body_shape(BodyId("body.unrelated")));
  CHECK(volume(*unavailable.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(integrated::base_volume(10, migrated)).margin(1e-5));
  const auto fillet = std::find(order.begin(), order.end(), "fillet");
  const auto chamfer = std::find(order.begin(), order.end(), "chamfer");
  REQUIRE(fillet != order.end());
  REQUIRE(chamfer != order.end());
  std::iter_swap(fillet, chamfer);
  const auto reordered = checked(part.with_history_intent({order, {}}));
  const auto reordered_shapes = evaluate(reordered);
  CHECK(volume(*reordered_shapes.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(integrated::base_volume(10, migrated)).margin(1e-5));
  const auto pattern = std::find(order.begin(), order.end(), "pattern");
  REQUIRE(pattern != order.end());
  std::rotate(order.begin(), pattern, pattern + 1);
  CHECK_FALSE(part.with_history_intent({order, {}}));
  CHECK(checked(serialize_part_document_to_json(part)) == saved);
  auto reopened = checked(deserialize_part_document_from_json(saved));
  CHECK(checked(serialize_part_document_to_json(reopened)) == saved);
  const auto replay = evaluate(reopened);
  CHECK(volume(*replay.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(integrated::base_volume(10, migrated)).margin(1e-5));
}
