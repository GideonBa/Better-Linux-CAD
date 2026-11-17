#include "../support/part_history_fixture.hpp"
#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::history_fixture;
namespace {
FaceReference face(SemanticFace side,
                   PartFeatureInputRole role = PartFeatureInputRole::FilletSupportFace,
                   FeatureId source = FeatureId("base")) {
  return f::checked(
      FaceReference::create_planar(role, f::checked(SemanticFaceReference::create(source, side))));
}
FaceBlendIntent blend() {
  return {{"first", "First side", {face(SemanticFace::Top)}},
          {"second", "Second side", {face(SemanticFace::Front)}},
          FilletContinuity::Tangent,
          FaceBlendSizeKind::Radius,
          ParameterId("radius"),
          {}};
}
FullRoundIntent round() {
  return {{"first", "First side", {face(SemanticFace::Left)}},
          {"second", "Second side", {face(SemanticFace::Right)}},
          {"middle", "Middle", {face(SemanticFace::Top, PartFeatureInputRole::FilletMiddleFace)}},
          FilletContinuity::Tangent,
          {}};
}
FilletFeature feature(FaceBlendIntent intent) {
  return f::checked(FilletFeature::create_face_blend(FeatureId("finish"), "Face blend",
                                                     BodyId("body.base"), std::move(intent)));
}
FilletFeature feature(FullRoundIntent intent) {
  return f::checked(FilletFeature::create_full_round(FeatureId("finish"), "Full round",
                                                     BodyId("body.base"), std::move(intent)));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
} // namespace
TEST_CASE(
    "Face blend and full-round intent preserve typed sets immutable payloads and exact dispatch",
    "[core][face-fillet][parity-b177]") {
  for (bool full : {false, true}) {
    auto p = f::native_base();
    auto source = round();
    auto record = full ? feature(source) : feature(blend());
    source.middle.faces.clear();
    REQUIRE(record.advanced_intent());
    CHECK(record.edges().empty());
    CHECK(record.faces().size() == (full ? 3 : 2));
    CHECK(record.parameter_dependencies().size() == (full ? 0 : 1));
    if (full)
      CHECK(record.radius_parameter().empty());
    REQUIRE(p.add_fillet_feature(record));
    CHECK(p.dependency_graph().has_dependency("base", "finish"));
    const auto text = saved(p);
    const auto json = nlohmann::json::parse(text);
    CHECK(json.at("version") == 14);
    const auto& stored = json.at("edge_treatments").at(0);
    CHECK(stored.size() == 6);
    CHECK(stored.at("mode") == (full ? "full_round" : "face_blend"));
    CHECK_FALSE(stored.contains("radius_parameter"));
    CHECK_FALSE(stored.contains("edges"));
    CHECK_FALSE(stored.contains("groups"));
    auto opened = f::checked(deserialize_part_document_from_json(text));
    CHECK(saved(opened) == text);
    auto project = f::checked(
        Project::create(DocumentId("round.project"), "Rounds",
                        f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"))));
    REQUIRE(project.add_part_document(opened));
    const auto json_project = f::checked(serialize_project_to_json(project));
    CHECK(f::checked(serialize_project_to_json(
              f::checked(deserialize_project_from_json(json_project)))) == json_project);
    REQUIRE_FALSE(p.remove_feature(FeatureId("base")));
    CHECK(saved(p) == text);
    auto suppressed = f::checked(p.with_history_intent({{"base", "finish"}, {"finish"}}));
    CHECK(saved(f::checked(deserialize_part_document_from_json(saved(suppressed)))) ==
          saved(suppressed));
    REQUIRE(p.remove_fillet_feature(FeatureId("finish")));
    CHECK_FALSE(p.dependency_graph().has_node("finish"));
  }
}
TEST_CASE("Face Fillet rejects repeated roles invalid capabilities and malformed mode envelopes",
          "[core][face-fillet][parity-b177]") {
  const auto invalid = [](const auto& change) {
    auto x = round();
    change(x);
    CHECK_FALSE(
        FilletFeature::create_full_round(FeatureId("finish"), "Round", BodyId("body.base"), x));
  };
  invalid([](auto& x) { x.middle.faces.clear(); });
  invalid([](auto& x) { x.first.id = x.second.id; });
  invalid([](auto& x) { x.first.faces.push_back(x.first.faces.front()); });
  invalid([](auto& x) { x.second.faces = x.first.faces; });
  invalid([](auto& x) { x.middle.faces = {face(SemanticFace::Top)}; });
  invalid([](auto& x) {
    x.middle.faces = {face(SemanticFace::Left, PartFeatureInputRole::FilletMiddleFace)};
  });
  invalid([](auto& x) { x.continuity = static_cast<FilletContinuity>(99); });
  auto x = blend();
  x.continuity = FilletContinuity::Curvature;
  CHECK_FALSE(
      FilletFeature::create_face_blend(FeatureId("finish"), "Blend", BodyId("body.base"), x));
  x.size_kind = FaceBlendSizeKind::ContactSetback;
  REQUIRE(FilletFeature::create_face_blend(FeatureId("finish"), "Blend", BodyId("body.base"), x));
  x.size_kind = static_cast<FaceBlendSizeKind>(99);
  CHECK_FALSE(
      FilletFeature::create_face_blend(FeatureId("finish"), "Blend", BodyId("body.base"), x));
  auto p = f::native_base();
  REQUIRE(p.add_fillet_feature(feature(round())));
  const auto root = nlohmann::json::parse(saved(p));
  for (const auto* field : {"first", "second", "middle", "continuity", "hold_lines"}) {
    auto bad = root;
    bad["edge_treatments"][0]["definition"].erase(field);
    CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  }
  auto bad = root;
  bad["edge_treatments"][0]["definition"]["first"]["faces"][0]["role"] = "shell_removal_face";
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  bad = root;
  bad["edge_treatments"][0]["definition"]["size"] = {{"parameter", "radius"}};
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  bad = root;
  bad["edge_treatments"][0]["mode"] = "variable";
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  bad = root;
  bad["version"] = 13;
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  bad = root;
  bad["edge_treatments"][0]["definition"]["middle"]["faces"][0]["extra"] = true;
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
}
TEST_CASE(
    "Face blend parameter hold-line and producer edits participate in normal graph and rollback",
    "[core][face-fillet][parity-b177]") {
  auto p = f::native_base();
  f::path(p, "hold", 0);
  auto x = blend();
  x.hold_lines = {{"contact", FilletSide::First, PathCurveId("hold")}};
  REQUIRE(p.add_fillet_feature(feature(x)));
  CHECK(p.dependency_graph().has_dependency("hold", "finish"));
  CHECK(p.dependency_graph().has_dependency("radius", "finish"));
  const auto inventory = f::checked(p.executable_history());
  const auto found = std::find_if(inventory.begin(), inventory.end(),
                                  [](const auto& row) { return row.node_id == "finish"; });
  REQUIRE(found != inventory.end());
  CHECK(found->family == PartFeatureFamily::Fillet);
  CHECK(f::checked(p.create_history_evaluation_plan()).complete());
  CHECK(p.find_fillet_feature(FeatureId("finish"))->hold_lines() ==
        std::vector<PathCurveId>{PathCurveId("hold")});
  const auto before = saved(p);
  auto bad = x;
  bad.hold_lines[0].path = PathCurveId("missing");
  CHECK_FALSE(p.update_fillet_feature(feature(bad)));
  CHECK(saved(p) == before);
  for (const auto* id : {"missing", "count", "angle"}) {
    bad = x;
    bad.size = ParameterId(id);
    CHECK_FALSE(p.update_fillet_feature(feature(bad)));
    CHECK(saved(p) == before);
  }
  bad = x;
  bad.hold_lines.push_back({"other", FilletSide::First, PathCurveId("hold")});
  CHECK_FALSE(
      FilletFeature::create_face_blend(FeatureId("finish"), "Blend", BodyId("body.base"), bad));
  auto affected =
      f::checked(p.set_parameter_value(ParameterId("radius"), f::checked(Quantity::length_mm(2))));
  CHECK(std::find(affected.begin(), affected.end(), "finish") != affected.end());
  const auto edited = saved(p);
  CHECK(saved(f::checked(deserialize_part_document_from_json(edited))) == edited);
  f::rectangle(p, "other");
  const auto context = f::new_body(p, "other");
  REQUIRE(p.add_feature(f::checked(
      f::checked(Feature::create_additive_extrude(FeatureId("other.feature"), "Other",
                                                  SketchId("other"), ParameterId("depth")))
          .with_body_result_context(context))));
  const auto owned = saved(p);
  bad = x;
  bad.first.faces = {
      face(SemanticFace::Top, PartFeatureInputRole::FilletSupportFace, FeatureId("other.feature"))};
  CHECK_FALSE(p.update_fillet_feature(feature(bad)));
  CHECK(saved(p) == owned);
}
TEST_CASE(
    "Typed setback corners preserve constant group identity distance dependency and strict payload",
    "[core][face-fillet][parity-b177]") {
  auto p = f::native_base();
  REQUIRE(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("setback"), "setback", f::checked(Quantity::length_mm(4))))));
  VariableFilletIntent law;
  int index = 0;
  for (auto edge : {SemanticEdge::FrontLeft, SemanticEdge::TopFront, SemanticEdge::TopLeft}) {
    const auto id = "edge." + std::to_string(index++);
    law.groups.push_back(
        {id,
         id,
         {{f::checked(EdgeReference::create_linear(
               PartFeatureInputRole::FilletEdge,
               f::checked(SemanticEdgeReference::create(FeatureId("base"), edge)))),
           false}},
         FilletPropagation::Explicit,
         FilletRadiusInterpolation::Linear,
         {{id + ".start", FilletStationLocation::Start, 0, ParameterId("radius")},
          {id + ".end", FilletStationLocation::End, 1, ParameterId("radius")}}});
  }
  law.setback =
      FilletSetbackCorner{"corner", {"edge.0", "edge.1", "edge.2"}, ParameterId("setback")};
  const auto make = [&](auto value) {
    return FilletFeature::create_variable(FeatureId("finish"), "Corner", BodyId("body.base"),
                                          std::move(value));
  };
  REQUIRE(p.add_fillet_feature(f::checked(make(law))));
  CHECK(p.dependency_graph().has_dependency("setback", "finish"));
  CHECK(p.find_fillet_feature(FeatureId("finish"))->parameter_dependencies() ==
        std::vector<ParameterId>{ParameterId("radius"), ParameterId("setback")});
  const auto before = saved(p);
  CHECK(saved(f::checked(deserialize_part_document_from_json(before))) == before);
  auto bad = law;
  bad.setback->groups[1] = "edge.0";
  CHECK_FALSE(make(bad));
  bad = law;
  bad.groups[1].stations[1].radius = ParameterId("height");
  CHECK_FALSE(make(bad));
  bad = law;
  bad.groups.pop_back();
  CHECK_FALSE(make(bad));
  auto json = nlohmann::json::parse(before);
  json["edge_treatments"][0]["corner"]["groups"][1] = 42;
  CHECK_FALSE(deserialize_part_document_from_json(json.dump()));
  auto changed =
      f::checked(p.set_parameter_value(ParameterId("setback"), f::checked(Quantity::length_mm(5))));
  CHECK(std::find(changed.begin(), changed.end(), "finish") != changed.end());
}

TEST_CASE("G2 sizes cylindrical supports and opaque Loft face sets survive typed persistence",
          "[core][face-fillet][parity-b177]") {
  auto p = f::native_base();
  auto sketch = f::checked(Sketch::create(SketchId("hole"), "Hole", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(
      f::checked(CircleProfile::create(ProfileId("circle"), ParameterId("radius")))));
  REQUIRE(p.add_sketch(sketch));
  auto cut = f::checked(
      Feature::create_subtractive_extrude(FeatureId("cut"), "Cut", sketch.id(), FeatureId("base")));
  cut = f::checked(cut.with_body_result_context(f::checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("body.base"), {}))));
  REQUIRE(p.add_feature(cut));
  auto value = blend();
  value.continuity = FilletContinuity::Curvature;
  value.size_kind = FaceBlendSizeKind::ContactSetback;
  value.second.faces = {f::checked(FaceReference::create_cylindrical(
      PartFeatureInputRole::FilletSupportFace, f::checked(SemanticCylindricalFaceReference::create(
                                                   FeatureId("cut"), ProfileId("circle")))))};
  REQUIRE(p.add_fillet_feature(feature(value)));
  auto text = saved(p);
  CHECK(saved(f::checked(deserialize_part_document_from_json(text))) == text);
  auto decoded = nlohmann::json::parse(text);
  CHECK(decoded["edge_treatments"][0]["definition"]["size"]["kind"] == "contact_setback");
  CHECK(decoded["edge_treatments"][0]["definition"]["continuity"] == "g2");
  auto full = round();
  full.continuity = FilletContinuity::Curvature;
  REQUIRE(p.update_fillet_feature(feature(full)));
  text = saved(p);
  CHECK(saved(f::checked(deserialize_part_document_from_json(text))) == text);
  CHECK(p.find_fillet_feature(FeatureId("finish"))->parameter_dependencies().empty());

  auto loft_part = f::native_base();
  f::rectangle(loft_part, "section");
  const auto body = f::new_body(loft_part, "loft");
  REQUIRE(loft_part.add_loft_feature(f::checked(
      LoftFeature::create_loft(FeatureId("loft"), "Loft",
                               {f::checked(ProfileSectionReference::create_closed_region(
                                    f::profile(PartFeatureInputRole::LoftSection, "profile"))),
                                f::checked(ProfileSectionReference::create_closed_region(
                                    f::profile(PartFeatureInputRole::LoftSection, "section")))},
                               body))));
  auto opaque = blend();
  opaque.first.faces = {f::checked(FaceReference::create_result(
      PartFeatureInputRole::FilletSupportFace,
      f::checked(ResultFaceReference::create(FeatureId("loft"), BodyId("body.loft"),
                                             {{FeatureId("loft"), "spline.side.first"}}))))};
  opaque.second.faces = {f::checked(FaceReference::create_result(
      PartFeatureInputRole::FilletSupportFace,
      f::checked(ResultFaceReference::create(FeatureId("loft"), BodyId("body.loft"),
                                             {{FeatureId("loft"), "spline.side.second"}}))))};
  REQUIRE(loft_part.add_fillet_feature(f::checked(FilletFeature::create_face_blend(
      FeatureId("finish"), "Opaque supports", BodyId("body.loft"), opaque))));
  text = saved(loft_part);
  CHECK(saved(f::checked(deserialize_part_document_from_json(text))) == text);
}

TEST_CASE("Fillet hold-line removal and a derived self-dependency preserve the original document",
          "[core][face-fillet][parity-b177]") {
  auto p = f::native_base();
  f::path(p, "hold", 0);
  auto value = blend();
  value.hold_lines = {{"first.hold", FilletSide::First, PathCurveId("hold")}};
  REQUIRE(p.add_fillet_feature(feature(value)));
  auto before = saved(p);
  CHECK_FALSE(p.remove_path_curve(PathCurveId("hold")));
  CHECK(saved(p) == before);
  std::array<Point3, ResultEdgeReference::sample_count> samples;
  for (std::size_t i = 0; i < samples.size(); ++i)
    samples[i] = {10. * i / (samples.size() - 1), 0, 0};
  auto edge = f::checked(EdgeReference::create_result(
      PartFeatureInputRole::PathSegment,
      f::checked(ResultEdgeReference::create(FeatureId("finish"), 0, 10, samples))));
  auto segment = f::checked(PathSegmentReference::create_feature_edge(edge));
  REQUIRE(p.add_path_curve(f::checked(
      PathCurve::create(PathCurveId("after.finish"), "Derived", {segment}, PathClosure::Open,
                        PathOrientationRule::MinimumTwist, {}, {}, 1e-7,
                        f::checked(PathExecutionPolicy::curve_preserving(1e-7))))));
  before = saved(p);
  const auto graph = p.dependency_graph().dependencies();
  value.hold_lines.push_back({"derived", FilletSide::Second, PathCurveId("after.finish")});
  CHECK_FALSE(p.update_fillet_feature(feature(value)));
  CHECK(saved(p) == before);
  CHECK(p.dependency_graph().dependencies() == graph);
}
