#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::history_fixture;
namespace {
PartDocument part() {
  auto p = f::native_base();
  auto sketch =
      f::checked(Sketch::create(SketchId("rib.sketch"), "Rib skeleton", DatumPlaneId("datum.xy")));
  f::accept(sketch.add_entity(
      f::checked(LineSegment::create(SketchEntityId("skeleton"), {-8, 0}, {8, 0}))));
  f::accept(p.add_sketch(sketch));
  for (const auto& [id, value] : {std::pair{"rib.t", 1.0}, {"rib.l", 12.0}, {"rib.t2", 2.0}})
    f::accept(p.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  f::accept(p.add_parameter(f::checked(
      Parameter::create_angle(ParameterId("rib.a"), "Draft", f::checked(Quantity::angle_deg(5))))));
  return p;
}
RibWebIntent intent() {
  return {RibWebKind::Rib,
          {SketchId("rib.sketch"), {SketchEntityId("skeleton")}, std::nullopt},
          RibExtrusionPlane::PerpendicularToSketch,
          {0, 0, 1},
          RibThicknessSide::Symmetric,
          ParameterId("rib.t"),
          std::nullopt,
          std::nullopt,
          RibDistanceExtent{ParameterId("rib.l")},
          RibCrossingPolicy::Union};
}
SketchDrivenFeature feature(RibWebIntent r = intent()) {
  return f::checked(SketchDrivenFeature::create_rib_web(FeatureId("rib"), "Rib",
                                                        BodyId("body.base"), std::move(r)));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
} // namespace
TEST_CASE("Rib and Web keep explicit typed source side direction draft and termination intent",
          "[core][rib-web][parity-b180]") {
  auto r = intent();
  r.direction = {0, 0, 2.12345678901234};
  r.side = RibThicknessSide::Asymmetric;
  r.second_thickness = ParameterId("rib.t2");
  r.draft = ParameterId("rib.a");
  auto rib = feature(r);
  CHECK(rib.kind_name() == "Rib");
  CHECK(std::get<RibWebIntent>(rib.definition().intent).direction == r.direction);
  CHECK(rib.parameter_dependencies().size() == 4);
  r.kind = RibWebKind::Web;
  auto web = feature(r);
  CHECK(web.kind_name() == "Web");
  r.source.curves.push_back(r.source.curves[0]);
  CHECK_FALSE(SketchDrivenFeature::create_rib_web(FeatureId("bad"), "Bad", BodyId("body.base"), r));
  r = intent();
  r.source.profile = ProfileId("profile");
  CHECK_FALSE(SketchDrivenFeature::create_rib_web(FeatureId("bad"), "Bad", BodyId("body.base"), r));
  r = intent();
  r.direction = {0, 0, std::numeric_limits<double>::infinity()};
  CHECK_FALSE(SketchDrivenFeature::create_rib_web(FeatureId("bad"), "Bad", BodyId("body.base"), r));
  r = intent();
  r.side = static_cast<RibThicknessSide>(100);
  CHECK_FALSE(SketchDrivenFeature::create_rib_web(FeatureId("bad"), "Bad", BodyId("body.base"), r));
}
TEST_CASE("Rib registry owns normal dependencies updates removal and atomic parameter validation",
          "[core][rib-web][parity-b180]") {
  auto p = part();
  const auto before = saved(p);
  REQUIRE(p.add_sketch_driven_feature(feature()));
  CHECK(p.sketch_driven_feature_count() == 1);
  CHECK(p.dependency_graph().has_dependency("rib.sketch", "rib"));
  CHECK(p.dependency_graph().has_dependency("rib.t", "rib"));
  CHECK(p.dependency_graph().has_dependency("base", "rib"));
  CHECK(p.dependency_graph().has_dependency("rib", "body:body.base"));
  const auto owned = saved(p);
  CHECK_FALSE(p.set_parameter_formula(ParameterId("rib.t"), "height - height"));
  CHECK(saved(p) == owned);
  auto r = intent();
  r.side = RibThicknessSide::Asymmetric;
  r.second_thickness = ParameterId("rib.t2");
  r.draft = ParameterId("rib.a");
  REQUIRE(p.update_sketch_driven_feature(feature(r)));
  CHECK(p.dependency_graph().has_dependency("rib.a", "rib"));
  const auto revised = saved(p);
  CHECK_FALSE(p.set_parameter_value(ParameterId("rib.a"), f::checked(Quantity::angle_deg(45))));
  CHECK(saved(p) == revised);
  r = intent();
  r.source.curves = {SketchEntityId("missing")};
  CHECK_FALSE(p.update_sketch_driven_feature(feature(r)));
  CHECK(saved(p) == revised);
  r = intent();
  r.thickness = ParameterId("rib.a");
  r.draft = ParameterId("rib.a");
  CHECK_FALSE(p.update_sketch_driven_feature(feature(r)));
  CHECK(saved(p) == revised);
  REQUIRE(p.remove_sketch_driven_feature(FeatureId("rib")));
  CHECK(saved(p) == before);
}
TEST_CASE(
    "Rib V15 JSON is necessary strict and compatible with advanced finishing and complete replay",
    "[core][rib-web][parity-b180]") {
  auto p = part();
  auto legacy = nlohmann::json::parse(saved(p));
  CHECK_FALSE(legacy.contains("sketch_driven_features"));
  REQUIRE(p.add_sketch_driven_feature(feature()));
  const auto text = saved(p);
  auto root = nlohmann::json::parse(text);
  CHECK(root["version"] == 15);
  auto opened = f::checked(deserialize_part_document_from_json(text));
  CHECK(saved(opened) == text);
  for (const auto* key : {"side", "plane", "mode", "crossings"}) {
    auto malformed = root;
    malformed["sketch_driven_features"][0][key] = "unknown";
    CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  }
  auto malformed = root;
  malformed["sketch_driven_features"][0]["unexpected"] = 1;
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = root;
  malformed["sketch_driven_features"][0]["direction"][0] = true;
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = root;
  malformed["version"] = 14;
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = root;
  malformed.erase("feature_order");
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = root;
  malformed["sketch_driven_features"] = nlohmann::json::array();
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = legacy;
  malformed["version"] = 15;
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  REQUIRE(p.add_fillet_feature(f::checked(FilletFeature::create_variable(
      FeatureId("finish"), "Finish", BodyId("body.base"),
      VariableFilletIntent{{{"group",
                             "Group",
                             {{f::edge(PartFeatureInputRole::FilletEdge), false}},
                             FilletPropagation::Explicit,
                             FilletRadiusInterpolation::Linear,
                             {{"start", FilletStationLocation::Start, 0, ParameterId("radius")},
                              {"end", FilletStationLocation::End, 1, ParameterId("radius")}}}}}))));
  auto advanced = f::checked(deserialize_part_document_from_json(saved(p)));
  CHECK(advanced.find_fillet_feature(FeatureId("finish"))->advanced_intent());
  CHECK(advanced.find_sketch_driven_feature(FeatureId("rib")));
}
TEST_CASE("Rib and Web use ordinary executable order suppression and prefix availability",
          "[core][rib-web][parity-b180]") {
  auto p = part();
  REQUIRE(p.add_sketch_driven_feature(feature()));
  REQUIRE(p.apply_history_intent({{"base", "rib"}, {"rib"}}));
  const auto history = f::checked(p.executable_history());
  REQUIRE(history.size() == 2);
  CHECK(history[1].family == PartFeatureFamily::SketchDriven);
  CHECK(history[1].state == PartFeatureState::Suppressed);
  auto plan = f::checked(p.create_history_evaluation_plan());
  CHECK(plan.complete());
  CHECK(plan.entries[1].state == PartHistoryExecutionState::Suppressed);
  auto opened = f::checked(deserialize_part_document_from_json(saved(p)));
  CHECK(opened.authored_history_intent() == p.authored_history_intent());
  CHECK_FALSE(p.apply_history_intent({{"rib", "base"}, {}}));
  CHECK(p.authored_history_intent()->suppressed == std::vector<std::string>{"rib"});
}
TEST_CASE(
    "Rib termination consumes an earlier derived Body without cycling its current target alias",
    "[core][rib-web][parity-b180]") {
  auto p = part();
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("tol"), "Tolerance", f::checked(Quantity::length_mm(1e-6))))));
  f::accept(p.add_body(f::checked(Body::create(BodyId("limit.body"), "Limit", BodyKind::Solid))));
  const auto top = f::checked(FaceReference::create_planar(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  const auto source = f::checked(SurfaceReference::create_faces(BodyId("body.base"), {top}));
  const auto context = f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("limit.body")));
  const SurfaceOffsetParameters sizes{ParameterId("rib.t"), ParameterId("tol"),
                                      SurfaceOffsetSide::AlongNormal,
                                      SurfaceOffsetApproximation::ExactAnalytic, std::nullopt};
  REQUIRE(p.add_surface_feature(f::checked(
      SurfaceOffsetFeature::create_thicken(FeatureId("limit"), "Limit", source, sizes, context))));
  REQUIRE(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("limit.place"), BodyId("limit.body"), BodyTransformCoordinateSpace::World,
      std::nullopt, {0, 0, 8}, false, false))));
  auto r = intent();
  r.extent = RibBodyExtent{BodyId("limit.body")};
  REQUIRE(p.add_sketch_driven_feature(feature(r)));
  CHECK_FALSE(p.dependency_graph().has_cycle());
  CHECK_FALSE(p.dependency_graph().has_dependency("body:body.base", "limit"));
  CHECK(p.dependency_graph().has_dependency("base", "limit"));
  CHECK(p.dependency_graph().has_dependency("body:limit.body", "rib"));
  const auto text = saved(p);
  auto opened = f::checked(deserialize_part_document_from_json(text));
  CHECK(saved(opened) == text);
  const auto order = f::checked(opened.executable_history());
  CHECK(order.back().node_id == "rib");
  CHECK_FALSE(opened.apply_history_intent({{"base", "rib", "limit", "limit.place"}, {}}));
  CHECK(saved(opened) == text);
}

TEST_CASE("Rib source ownership construction and termination failures are atomic",
          "[core][rib-web][parity-b180]") {
  auto p = part();
  const auto before = saved(p);
  auto r = intent();
  r.source.sketch = SketchId("foreign");
  CHECK_FALSE(p.add_sketch_driven_feature(feature(r)));
  CHECK(saved(p) == before);
  r = intent();
  r.extent = RibBodyExtent{BodyId("foreign")};
  CHECK_FALSE(p.add_sketch_driven_feature(feature(r)));
  CHECK(saved(p) == before);
  r = intent();
  r.source.profile = ProfileId("missing");
  r.source.curves.clear();
  CHECK_FALSE(p.add_sketch_driven_feature(feature(r)));
  CHECK(saved(p) == before);
  auto construction = f::checked(
      Sketch::create(SketchId("construction"), "Construction", DatumPlaneId("datum.xy")));
  f::accept(construction.add_entity(f::checked(
      LineSegment::create(SketchEntityId("line"), {0, 0}, {4, 0}, SketchLineRole::Construction))));
  f::accept(p.add_sketch(construction));
  r = intent();
  r.source = {construction.id(), {SketchEntityId("line")}, std::nullopt};
  const auto with_construction = saved(p);
  CHECK_FALSE(p.add_sketch_driven_feature(feature(r)));
  CHECK(saved(p) == with_construction);
  auto project = f::checked(Project::create(
      DocumentId("project.rib"), "Rib project",
      f::checked(AssemblyDocument::create(DocumentId("assembly.rib"), "Assembly"))));
  r = intent();
  f::accept(p.add_sketch_driven_feature(feature(r)));
  f::accept(project.add_part_document(p));
  const auto serialized = f::checked(serialize_project_to_json(project));
  CHECK(f::checked(serialize_project_to_json(
            f::checked(deserialize_project_from_json(serialized)))) == serialized);
}
