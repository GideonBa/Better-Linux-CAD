#include "../support/adaptive_project_fixture.hpp"
#include "../support/geometry_volume_oracle.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/adaptive_project.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
using namespace blcad::test;
using Catch::Approx;

TEST_CASE("Adaptive cover uses an exact transformed source face and one target definition",
          "[parity-b195][adaptive-project]") {
  auto project = adaptive_project();
  const auto irrelevant =
      serialize_part_document_to_json(*project.find_part_document(DocumentId("irrelevant")));
  REQUIRE(irrelevant);
  auto linked = AdaptiveProjectEvaluator{}.bind(project, adaptive_plane_reference());
  REQUIRE(linked);
  REQUIRE(linked.value().changed_parts.size() == 1);
  CHECK(linked.value().changed_parts[0] == DocumentId("cover"));
  auto& candidate = linked.value().project;
  auto frame = WorkplaneResolver{}.resolve(*candidate.find_part_document(DocumentId("cover")),
                                           DatumPlaneId("linked.plane"));
  REQUIRE(frame);
  CHECK(frame.value().origin.x == Approx(-30).margin(1e-8));
  CHECK(frame.value().origin.y == Approx(-25).margin(1e-8));
  CHECK(frame.value().origin.z == Approx(4).margin(1e-8));
  CHECK(frame.value().x_axis.y == Approx(-1).margin(1e-8));
  auto shape = AssemblyStepExporter{}.build_posed_shape(candidate);
  REQUIRE(shape);
  CHECK(classify_solid_point(shape.value(), {20, 30, 6}) == SolidPointLocation::Inside);
  CHECK(classify_solid_point(shape.value(), {-10, 5, 2}) == SolidPointLocation::Outside);
  CHECK(serialize_part_document_to_json(*candidate.find_part_document(DocumentId("irrelevant")))
            .value() == irrelevant.value());
  auto new_length = Quantity::length_mm(8);
  REQUIRE(new_length);
  REQUIRE(candidate.find_part_document(DocumentId("source"))
              ->set_parameter_value(ParameterId("length"), new_length.value()));
  auto changed = AdaptiveProjectEvaluator{}.evaluate(candidate);
  REQUIRE(changed);
  auto shifted =
      WorkplaneResolver{}.resolve(*changed.value().project.find_part_document(DocumentId("cover")),
                                  DatumPlaneId("linked.plane"));
  REQUIRE(shifted);
  CHECK(shifted.value().origin.z == Approx(8).margin(1e-8));
  auto physical = BodyResultInspector{}.inspect_physical_properties(changed.value().project);
  REQUIRE(physical);
  CHECK(physical.value().data().volume_mm3 == Approx(4800).margin(1e-6));
  const auto step_path = std::filesystem::temp_directory_path() / "blcad-adaptive-cover-proof.step";
  REQUIRE(AssemblyStepExporter{}.write_step(changed.value().project, step_path.string()));
  STEPControl_Reader reader;
  REQUIRE(reader.ReadFile(step_path.c_str()) == IFSelect_RetDone);
  REQUIRE(reader.TransferRoots() > 0);
  GProp_GProps native;
  BRepGProp::VolumeProperties(reader.OneShape(), native);
  CHECK(native.Mass() == Approx(4800).margin(1e-6));
  CHECK(native.CentreOfMass().X() == Approx(276.6666666666667).margin(1e-6));
  CHECK(native.CentreOfMass().Y() == Approx(15).margin(1e-6));
  CHECK(native.CentreOfMass().Z() == Approx(4.6666666666667).margin(1e-6));
  auto saved = serialize_project_to_json(changed.value().project);
  REQUIRE(saved);
  auto reopened = deserialize_project_from_json(saved.value());
  REQUIRE(reopened);
  auto replayed = AdaptiveProjectEvaluator{}.evaluate(reopened.value());
  REQUIRE(replayed);
  CHECK(replayed.value().changed_parts.empty());
  const auto generation =
      replayed.value().project.find_part_document(DocumentId("cover"))->executable_generation();
  auto repeated = AdaptiveProjectEvaluator{}.evaluate(replayed.value().project);
  REQUIRE(repeated);
  CHECK(repeated.value().changed_parts.empty());
  CHECK(repeated.value().project.find_part_document(DocumentId("cover"))->executable_generation() ==
        generation);
  CHECK(serialize_project_to_json(replayed.value().project).value() == saved.value());
  auto detached = AdaptiveProjectEvaluator{}.detach(changed.value().project,
                                                    ProjectDesignReferenceId("cover.support"));
  REQUIRE(detached);
  REQUIRE(detached.value()
              .find_part_document(DocumentId("source"))
              ->set_parameter_value(ParameterId("length"), Quantity::length_mm(12).value()));
  auto independent = AdaptiveProjectEvaluator{}.evaluate(detached.value());
  REQUIRE(independent);
  CHECK(independent.value()
            .project.find_part_document(DocumentId("cover"))
            ->find_construction_plane(ConstructionPlaneId("linked.plane"))
            ->origin()
            .z == Approx(8));
}

TEST_CASE("Failed adaptive downstream cut and pose feedback preserve every Project asset",
          "[parity-b195][adaptive-project]") {
  auto project = adaptive_project();
  auto* cover = project.find_part_document(DocumentId("cover"));
  REQUIRE(cover->add_parameter(
      Parameter::create_length(ParameterId("bore"), "Bore", Quantity::length_mm(2).value())
          .value()));
  auto sketch = Sketch::create(SketchId("cut.sketch"), "Cut", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto circle = CircleProfile::create(ProfileId("cut"), ParameterId("bore"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));
  REQUIRE(cover->add_sketch(sketch.value()));
  auto feature = Feature::create_subtractive_extrude(FeatureId("cut"), "Static bore",
                                                     SketchId("cut.sketch"), FeatureId("base"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                  BodyId("body.base"), std::nullopt);
  REQUIRE(context);
  auto owned = feature.value().with_body_result_context(context.value());
  REQUIRE(owned);
  REQUIRE(cover->add_feature(owned.value()));
  auto before = serialize_project_to_json(project);
  REQUIRE(before);
  auto rejected = AdaptiveProjectEvaluator{}.bind(project, adaptive_plane_reference());
  CHECK_FALSE(rejected);
  CHECK(serialize_project_to_json(project).value() == before.value());

  auto feedback = adaptive_project();
  REQUIRE(feedback.assembly().set_component_instance_grounding_state(
      ComponentInstanceId("cover"), ComponentGroundingState::Free));
  auto first = AssemblyConstraintTarget::create(ComponentInstanceId("source"), "base.top");
  REQUIRE(first);
  auto second = AssemblyConstraintTarget::create(ComponentInstanceId("cover"), "base.top");
  REQUIRE(second);
  auto relation =
      AssemblyConstraint::create(AssemblyConstraintId("feedback"), "Pose feedback",
                                 AssemblyConstraintType::Distance, first.value(), second.value(),
                                 AssemblyConstraintState::Active, Quantity::length_mm(4).value());
  REQUIRE(relation);
  REQUIRE(feedback.assembly().add_constraint(relation.value()));
  auto snapshot = serialize_project_to_json(feedback);
  REQUIRE(snapshot);
  CHECK_FALSE(AdaptiveProjectEvaluator{}.bind(feedback, adaptive_plane_reference()));
  CHECK(serialize_project_to_json(feedback).value() == snapshot.value());
}

namespace {
SketchCoordinate3D coordinate(double value) {
  return SketchCoordinate3D::create_explicit(Quantity::linear_displacement_mm(value).value())
      .value();
}
void source_paths(PartDocument& source) {
  auto sketch = Sketch3D::create(Sketch3DId("source.curves"), "Source exact curves");
  REQUIRE(sketch);
  for (const auto offset : {0, 4}) {
    std::vector<SketchPointReference3D> refs;
    for (int i = 0; i < 4; ++i) {
      const SketchEntityId id("p" + std::to_string(offset) + "." + std::to_string(i));
      auto z = i == 0 ? SketchCoordinate3D::create_parameter(ParameterId("length")).value()
                      : coordinate(4 + i * 20.0 / 3.0);
      auto point = SketchPoint3D::create(id, coordinate(offset), coordinate(0), z);
      REQUIRE(point);
      REQUIRE(sketch.value().add_point(point.value()));
      refs.push_back(SketchPointReference3D::create_local_point(id).value());
    }
    const SketchEntityId id("spline" + std::to_string(offset));
    auto curve = SketchSpline3D::create(id, SketchSpline3DRepresentation::ControlPoints, refs, 3,
                                        SketchSpline3DContinuity::Positional);
    REQUIRE(curve);
    REQUIRE(sketch.value().add_spline(curve.value()));
  }
  REQUIRE(source.add_sketch_3d(sketch.value()));
  for (const auto offset : {0, 4}) {
    auto segment = PathSegmentReference::create_sketch_3d(
        sketch.value().id(), SketchEntityId("spline" + std::to_string(offset)),
        Sketch3DPathCurveKind::Spline);
    REQUIRE(segment);
    auto path = PathCurve::create(PathCurveId("source.path" + std::to_string(offset)),
                                  "Exact spine", {segment.value()}, PathClosure::Open,
                                  PathOrientationRule::MinimumTwist, std::nullopt, std::nullopt,
                                  1e-7, PathExecutionPolicy::curve_preserving(1e-6).value());
    REQUIRE(path);
    REQUIRE(source.add_path_curve(path.value()));
  }
}
ProjectDesignReference path_reference(int offset) {
  auto ref = ProjectDesignReference::create(
      ProjectDesignReferenceId("curve" + std::to_string(offset)), "Linked exact spine",
      {{}, ComponentInstanceId("source"), DocumentId("source")}, primary_model_state_id(),
      ProjectDesignPathSource{PathCurveId("source.path" + std::to_string(offset))},
      {{}, ComponentInstanceId("cover"), DocumentId("cover")},
      ProjectDerivedPathTarget{PathCurveId("linked.path" + std::to_string(offset)),
                               Sketch3DId("linked.curve" + std::to_string(offset))});
  REQUIRE(ref);
  return ref.value();
}
FeatureBodyResultContext new_body(const char* id) {
  auto context =
      FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, BodyId(id));
  REQUIRE(context);
  return context.value();
}
double volume(PartDocument& part, const char* id) {
  auto cache = ShapeCache::create(ShapeCacheId("adaptive.oracles"));
  REQUIRE(cache);
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache.value()));
  const auto* shape = cache.value().find_body_shape(BodyId(id));
  REQUIRE(shape);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
} // namespace

TEST_CASE("Adaptive native spline paths drive real Sweep Loft and Thicken consumers",
          "[parity-b195][adaptive-project][adaptive-path-consumers]") {
  auto project = adaptive_project();
  source_paths(*project.find_part_document(DocumentId("source")));
  auto plane = AdaptiveProjectEvaluator{}.bind(project, adaptive_plane_reference());
  REQUIRE(plane);
  auto first = AdaptiveProjectEvaluator{}.bind(plane.value().project, path_reference(0));
  REQUIRE(first);
  auto second = AdaptiveProjectEvaluator{}.bind(first.value().project, path_reference(4));
  REQUIRE(second);
  auto& candidate = second.value().project;
  auto* cover = candidate.find_part_document(DocumentId("cover"));
  REQUIRE(
      cover->add_parameter(Parameter::create_length(ParameterId("duct.diameter"), "Duct diameter",
                                                    Quantity::length_mm(2).value())
                               .value()));
  const auto* support_plane = cover->find_construction_plane(ConstructionPlaneId("linked.plane"));
  REQUIRE(support_plane);
  auto profile =
      Sketch::create(SketchId("duct.profile"), "Duct section", support_plane->workplane_id());
  REQUIRE(profile);
  REQUIRE(profile.value().add_profile(
      CircleProfile::create(ProfileId("duct.circle"), ParameterId("duct.diameter")).value()));
  REQUIRE(cover->add_sketch(profile.value()));
  REQUIRE(cover->add_body(Body::create(BodyId("duct"), "Duct", BodyKind::Solid).value()));
  auto region = ProfileRegionReference::create(profile.value().id(), ProfileId("duct.circle"),
                                               PartFeatureInputRole::SweepProfile);
  REQUIRE(region);
  auto section = SweepProfileReference::create_closed_region(region.value());
  REQUIRE(section);
  auto sweep = SweepFeature::create_sweep(FeatureId("duct"), "Adaptive duct", section.value(),
                                          PathCurveId("linked.path0"), new_body("duct"));
  REQUIRE(sweep);
  REQUIRE(cover->add_sweep_feature(sweep.value()));
  REQUIRE(cover->add_body(Body::create(BodyId("loft"), "Loft skin", BodyKind::Surface).value()));
  auto a = ProfileSectionReference::create_open_path(PathCurveId("linked.path0"));
  REQUIRE(a);
  auto b = ProfileSectionReference::create_open_path(PathCurveId("linked.path4"));
  REQUIRE(b);
  auto loft = LoftFeature::create_loft_surface(FeatureId("loft"), "Adaptive cover skin",
                                               {a.value(), b.value()}, new_body("loft"));
  REQUIRE(loft);
  REQUIRE(cover->add_loft_feature(loft.value()));
  REQUIRE(cover->add_body(Body::create(BodyId("thick"), "Thick cover", BodyKind::Solid).value()));
  REQUIRE(cover->add_parameter(
      Parameter::create_length(ParameterId("wall"), "Wall", Quantity::length_mm(1).value())
          .value()));
  REQUIRE(cover->add_parameter(
      Parameter::create_length(ParameterId("tol"), "Tolerance", Quantity::length_mm(1e-6).value())
          .value()));
  REQUIRE(cover->add_parameter(Parameter::create_length(ParameterId("deviation"), "Offset budget",
                                                        Quantity::length_mm(.001).value())
                                   .value()));
  auto support = SurfaceReference::create_body(BodyId("loft"));
  REQUIRE(support);
  auto thick = SurfaceOffsetFeature::create_thicken(
      FeatureId("thick"), "Adaptive thick cover", support.value(),
      {ParameterId("wall"), ParameterId("tol"), SurfaceOffsetSide::Symmetric,
       SurfaceOffsetApproximation::VerifiedApproximation, ParameterId("deviation")},
      new_body("thick"));
  REQUIRE(thick);
  REQUIRE(cover->add_surface_feature(thick.value()));
  auto checked = AdaptiveProjectEvaluator{}.evaluate(candidate);
  INFO((checked ? std::string{} : checked.error().message()));
  REQUIRE(checked);
  CHECK(volume(*checked.value().project.find_part_document(DocumentId("cover")), "duct") ==
        Approx(20 * std::numbers::pi).epsilon(1e-7));
  CHECK(volume(*checked.value().project.find_part_document(DocumentId("cover")), "thick") ==
        Approx(80).epsilon(1e-7));
  REQUIRE(checked.value()
              .project.find_part_document(DocumentId("source"))
              ->set_parameter_value(ParameterId("length"), Quantity::length_mm(8).value()));
  auto changed = AdaptiveProjectEvaluator{}.evaluate(checked.value().project);
  INFO((changed ? std::string{} : changed.error().message()));
  REQUIRE(changed);
  CHECK(volume(*changed.value().project.find_part_document(DocumentId("cover")), "duct") ==
        Approx(16 * std::numbers::pi).epsilon(1e-7));
  CHECK(volume(*changed.value().project.find_part_document(DocumentId("cover")), "thick") ==
        Approx(64).epsilon(1e-7));
}

TEST_CASE("Adaptive references explicitly select a source state without baking Primary",
          "[parity-b195][adaptive-project][adaptive-state]") {
  auto project = adaptive_project();
  ModelState thick{ModelStateId("thick"),
                   "Thick source",
                   {{ParameterId("length"), Quantity::length_mm(8).value()}},
                   {},
                   {},
                   {},
                   {}};
  REQUIRE(project.find_part_document(DocumentId("source"))->replace_model_states({thick}));
  auto primary = serialize_part_document_to_json(*project.find_part_document(DocumentId("source")));
  REQUIRE(primary);
  auto reference = ProjectDesignReference::create(
      ProjectDesignReferenceId("cover.state"), "Selected state support",
      {{}, ComponentInstanceId("source"), DocumentId("source")}, thick.id,
      ProjectDesignSemanticSource{"base.top"},
      {{}, ComponentInstanceId("cover"), DocumentId("cover")}, ConstructionPlaneId("linked.plane"));
  REQUIRE(reference);
  auto result = AdaptiveProjectEvaluator{}.bind(project, reference.value());
  REQUIRE(result);
  const auto* cover = result.value().project.find_part_document(DocumentId("cover"));
  REQUIRE(cover);
  CHECK(cover->find_construction_plane(ConstructionPlaneId("linked.plane"))->origin().z ==
        Approx(8).margin(1e-8));
  CHECK(serialize_part_document_to_json(
            *result.value().project.find_part_document(DocumentId("source")))
            .value() == primary.value());
  CHECK(result.value()
            .project.find_part_document(DocumentId("source"))
            ->find_parameter(ParameterId("length"))
            ->value()
            .millimeters() == 4);
  auto saved = serialize_project_to_json(result.value().project);
  REQUIRE(saved);
  auto reopened = deserialize_project_from_json(saved.value());
  REQUIRE(reopened);
  CHECK(reopened.value().design_references()[0].source_state() == thick.id);
}

TEST_CASE("Regular solve freshness observes an equal-geometry adaptive driving authority change",
          "[parity-b195][adaptive-project][assembly-context-freshness]") {
  auto project = adaptive_project();
  const auto source_pose =
      project.assembly().find_component_instance(ComponentInstanceId("source"))->transform();
  REQUIRE(project.assembly().set_component_instance_transform(ComponentInstanceId("source.copy"),
                                                              source_pose));
  auto linked = AdaptiveProjectEvaluator{}.bind(project, adaptive_plane_reference());
  REQUIRE(linked);
  auto solved =
      AssemblyRigidBodySolver{}.solve(linked.value().project, {ComponentInstanceId("cover")});
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK_FALSE(solved.value().occurrence_context_snapshot.empty());
  const auto& old = linked.value().project.design_references().front();
  auto different_driver = ProjectDesignReference::create(
      old.id(), old.name(), {{}, ComponentInstanceId("source.copy"), DocumentId("source")},
      old.source_state(), old.source_geometry(), old.driver(), old.target());
  REQUIRE(different_driver);
  auto rebound = AdaptiveProjectEvaluator{}.bind(linked.value().project, different_driver.value());
  REQUIRE(rebound);
  CHECK(serialize_part_document_to_json(
            *rebound.value().project.find_part_document(DocumentId("cover")))
            .value() == serialize_part_document_to_json(
                            *linked.value().project.find_part_document(DocumentId("cover")))
                            .value());
  const auto before = serialize_project_to_json(rebound.value().project);
  REQUIRE(before);
  CHECK_FALSE(AssemblySolveResultApplier{}.apply(rebound.value().project, solved.value()));
  CHECK(serialize_project_to_json(rebound.value().project).value() == before.value());
}

TEST_CASE("Legacy regular solve retains selective freshness for unrelated Part edits",
          "[parity-b195][adaptive-project][assembly-context-freshness]") {
  auto project = adaptive_project();
  auto solved = AssemblyRigidBodySolver{}.solve(project, {ComponentInstanceId("cover")});
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK(solved.value().occurrence_context_snapshot.empty());
  REQUIRE(project.find_part_document(DocumentId("irrelevant"))
              ->set_parameter_value(ParameterId("width"), Quantity::length_mm(21).value()));
  REQUIRE(AssemblySolveResultApplier{}.apply(project, solved.value()));
  CHECK(project.find_part_document(DocumentId("irrelevant"))
            ->find_parameter(ParameterId("width"))
            ->value()
            .millimeters() == 21);
}
