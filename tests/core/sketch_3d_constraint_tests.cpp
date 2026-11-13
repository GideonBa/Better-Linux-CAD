#include "../support/engineering_resource_fixture.hpp"
#include "../support/hole_pattern_fixture.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::spatial_fixture;
using json = nlohmann::json;
namespace {
std::string snapshot(const PartDocument& doc) {
  return f::checked(serialize_part_document_to_json(doc));
}
json owner(const Sketch3D& s) {
  auto doc = f::part();
  (void)f::checked(doc.add_sketch_3d(s));
  return json::parse(snapshot(doc)).at("sketches_3d")[0];
}
SpatialConstraint intent(const char* id, SpatialConstraintKind kind,
                         std::vector<SpatialConstraintTarget> targets,
                         std::optional<SpatialDimensionValue> value = {}) {
  return f::checked(SpatialConstraint::create(SketchConstraintId(id), kind, std::move(targets),
                                              std::move(value)));
}
std::vector<SpatialConstraint> matrix() {
  return {
      intent("coincident", SpatialConstraintKind::Coincident, {f::target("p0"), f::target("p1")}),
      f::checked(SpatialConstraint::create(SketchConstraintId("fixed"),
                                           SpatialConstraintKind::Fixed, {f::target("p3")}, {},
                                           Point3{0, 3, 2})),
      intent("axis", SpatialConstraintKind::AxisParallel, {f::curve("line"), f::axis()}),
      intent("plane_parallel", SpatialConstraintKind::PlaneParallel,
             {f::curve("line"), f::plane()}),
      intent("on_line", SpatialConstraintKind::PointOnLine, {f::target("p2"), f::curve("line")}),
      intent("on_plane", SpatialConstraintKind::PointOnPlane, {f::target("p1"), f::plane()}),
      f::checked(SpatialConstraint::create(
          SketchConstraintId("on_curve"), SpatialConstraintKind::PointOnCurve,
          {f::target("p2"), f::curve("spline")}, {}, {}, SpatialAngleConvention::Unsigned, .25)),
      intent("on_face", SpatialConstraintKind::PointOnFace, {f::target("p3"), f::face()}),
      intent("parallel", SpatialConstraintKind::Parallel, {f::curve("line"), f::axis()}),
      intent("perpendicular", SpatialConstraintKind::Perpendicular, {f::curve("line"), f::axis()}),
      intent("tangent", SpatialConstraintKind::Tangent,
             {f::curve("spline", SpatialCurveLocation::Start),
              f::curve("arc", SpatialCurveLocation::End)}),
      intent("smooth", SpatialConstraintKind::Smooth,
             {f::curve("spline", SpatialCurveLocation::Start),
              f::curve("line", SpatialCurveLocation::End)}),
      intent("distance", SpatialConstraintKind::Distance, {f::target("p0"), f::target("p1")},
             f::length(2)),
      intent("angle", SpatialConstraintKind::Angle, {f::curve("line"), f::axis()}, f::angle(90)),
      intent("radius", SpatialConstraintKind::Radius, {f::curve("helix")}, f::length(4)),
      intent("length", SpatialConstraintKind::Length, {f::curve("polyline")}, f::length(10))};
}
} // namespace
TEST_CASE("Spatial intent validates every ordered capability tuple without claiming a solve",
          "[core][parity-b163][spatial-schema]") {
  auto s = f::sketch();
  for (const auto& c : matrix()) {
    INFO(c.id().value());
    CHECK(validate_spatial_constraint(s, c));
    REQUIRE(s.add_constraint(c));
    CHECK_FALSE(s.add_constraint(c));
  }
  CHECK(s.constraints().size() == 16);
  auto doc = f::part();
  REQUIRE(doc.add_sketch_3d(s));
  CHECK(doc.dependency_graph().has_dependency("axis.z", "spatial"));
  CHECK(doc.dependency_graph().has_dependency("datum.xy", "spatial"));
  CHECK(doc.dependency_graph().has_dependency("base.feature", "spatial"));
  CHECK_FALSE(SpatialConstraint::create({}, SpatialConstraintKind::Coincident,
                                        {f::target("p0"), f::target("p1")}));
  CHECK_FALSE(SpatialConstraint::create(SketchConstraintId("bad"),
                                        static_cast<SpatialConstraintKind>(999), {}));
  CHECK_FALSE(SpatialConstraint::create(SketchConstraintId("bad"), SpatialConstraintKind::Smooth,
                                        {f::curve("line"), f::curve("spline")}));
  CHECK_FALSE(SpatialConstraint::create(SketchConstraintId("bad"), SpatialConstraintKind::Fixed,
                                        {f::target("p0")}));
  CHECK_FALSE(SpatialConstraint::create(SketchConstraintId("bad"), SpatialConstraintKind::Fixed,
                                        {f::target("p0")}, {},
                                        Point3{std::numeric_limits<double>::quiet_NaN(), 0, 0}));
  CHECK_FALSE(SpatialConstraintTarget::curve({}, SpatialCurveLocation::Whole));
  CHECK_FALSE(SpatialConstraintTarget::curve(SketchEntityId("line"),
                                             static_cast<SpatialCurveLocation>(99)));
  CHECK_FALSE(SpatialConstraintTarget::axis(f::checked(
      AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis, DatumAxisId("axis.z")))));
  CHECK_FALSE(s.add_constraint(
      intent("bad_direction", SpatialConstraintKind::Parallel, {f::curve("spline"), f::axis()})));
  CHECK_FALSE(s.add_constraint(
      intent("bad_radius", SpatialConstraintKind::Radius, {f::curve("line")}, f::length(2))));
  CHECK_FALSE(s.add_constraint(
      intent("same_point", SpatialConstraintKind::Coincident, {f::target("p0"), f::target("p0")})));
  CHECK_FALSE(s.add_constraint(intent("missing", SpatialConstraintKind::Coincident,
                                      {f::target("p0"), f::target("missing")})));
  CHECK_FALSE(SpatialConstraint::create(SketchConstraintId("bad"), SpatialConstraintKind::Angle,
                                        {f::curve("line"), f::axis()}, f::angle(181)));
  CHECK_FALSE(SpatialConstraint::create(SketchConstraintId("bad"), SpatialConstraintKind::Distance,
                                        {f::target("p0"), f::target("p1")}, f::angle(90)));
  CHECK_FALSE(SpatialConstraint::create(
      SketchConstraintId("bad"), SpatialConstraintKind::PointOnCurve,
      {f::target("p0"), f::curve("spline")}, {}, {}, SpatialAngleConvention::Unsigned, 2));
  REQUIRE(SpatialConstraint::create(SketchConstraintId("signed"), SpatialConstraintKind::Angle,
                                    {f::curve("line"), f::axis(), f::axis()}, f::angle(-90), {},
                                    SpatialAngleConvention::OrientedAboutAxis));
}
TEST_CASE("Spatial variable ordering separates immutable coordinates and integer turns from owned "
          "scalars",
          "[core][parity-b163][spatial-variables]") {
  const auto doc = f::part();
  auto s = f::sketch();
  const auto old = *s.find_helix(SketchEntityId("helix"));
  REQUIRE(s.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
      old.id(), old.axis(),
      f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(4)))),
      old.pitch_binding(), old.turns_parameter(), old.handedness()))));
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("z"), SpatialConstraintKind::PointOnCurve,
      {f::target("p2"), f::curve("spline")}, {}, {}, SpatialAngleConvention::Unsigned, .7))));
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("a"), SpatialConstraintKind::PointOnCurve,
      {f::target("p3"), f::curve("arc")}, {}, {}, SpatialAngleConvention::Unsigned, .2))));
  const auto vars = f::checked(spatial_variable_layout(doc, s));
  const std::vector<std::pair<std::string, SpatialVariableKind>> expected = {
      {"p0", SpatialVariableKind::PointY},        {"p0", SpatialVariableKind::PointZ},
      {"p1", SpatialVariableKind::PointX},        {"p1", SpatialVariableKind::PointY},
      {"p1", SpatialVariableKind::PointZ},        {"p2", SpatialVariableKind::PointX},
      {"p2", SpatialVariableKind::PointY},        {"p2", SpatialVariableKind::PointZ},
      {"p3", SpatialVariableKind::PointX},        {"p3", SpatialVariableKind::PointY},
      {"p3", SpatialVariableKind::PointZ},        {"helix", SpatialVariableKind::HelixRadius},
      {"a", SpatialVariableKind::CurveParameter}, {"z", SpatialVariableKind::CurveParameter}};
  REQUIRE(vars.size() == expected.size());
  for (std::size_t i = 0; i < vars.size(); ++i) {
    CHECK(vars[i].owner_id == expected[i].first);
    CHECK(vars[i].kind == expected[i].second);
  }
  CHECK(vars[2].initial_value == 10);
  CHECK(vars[11].initial_value == 4);
  CHECK(vars[12].initial_value == .2);
  CHECK(vars[13].initial_value == .7);
  CHECK(s.find_point(SketchEntityId("p0"))->x().parameter() == ParameterId("coordinate.x"));
  CHECK(s.find_helix(old.id())->radius_parameter().empty());
  CHECK(s.find_helix(old.id())->pitch_parameter() == ParameterId("pitch"));
  CHECK(s.find_helix(old.id())->turns_parameter() == ParameterId("turns"));
  const auto dependencies = s.parameter_dependencies();
  CHECK(std::find(dependencies.begin(), dependencies.end(), ParameterId("radius")) ==
        dependencies.end());
  CHECK_FALSE(SketchLength3D::create_explicit(f::checked(Quantity::angle_deg(1))));
  CHECK_FALSE(SketchLength3D::create_parameter({}));
}
TEST_CASE("Spatial owner removal and replacements preserve source order and all constraint "
          "identities atomically",
          "[core][parity-b163][spatial-ownership]") {
  auto s = f::sketch("spatial", true);
  REQUIRE(s.add_point(f::point("isolated", {3, 2, 1})));
  REQUIRE(s.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("fixed"), SpatialConstraintKind::Fixed,
                                {f::target("isolated")}, {}, Point3{3, 2, 1}))));
  REQUIRE(s.add_constraint(
      intent("length", SpatialConstraintKind::Length, {f::curve("line")}, f::length(2))));
  REQUIRE(s.add_constraint(
      intent("equation", SpatialConstraintKind::Length, {f::curve("equation")}, f::length(1))));
  const auto before = owner(s);
  CHECK_FALSE(s.remove_point(SketchEntityId("isolated")));
  CHECK_FALSE(s.remove_line(SketchEntityId("line")));
  CHECK_FALSE(s.remove_equation_curve(SketchEntityId("equation")));
  CHECK_FALSE(s.replace_line(f::checked(SketchLine3D::create(
      SketchEntityId("line"), SketchEntityId("p0"), SketchEntityId("missing")))));
  CHECK_FALSE(s.replace_constraint(
      intent("length", SpatialConstraintKind::Radius, {f::curve("line")}, f::length(2))));
  CHECK(owner(s) == before);
  REQUIRE(s.replace_point(f::point("p1", {11, 0, 1})));
  CHECK(s.points()[3].id() == SketchEntityId("p1"));
  CHECK(s.constraints().size() == 3);
  const auto candidate =
      f::checked(Sketch3DInteractionService::move_point(s, SketchEntityId("p2"), {2, 2, 0}));
  CHECK(owner(candidate.sketch)["constraints"] == before["constraints"]);
  CHECK(owner(candidate.sketch)["guide_curves"] == before["guide_curves"]);
  REQUIRE(s.remove_constraint(SketchConstraintId("fixed")));
  REQUIRE(s.remove_point(SketchEntityId("isolated")));
  REQUIRE(s.remove_constraint(SketchConstraintId("equation")));
  REQUIRE(s.remove_equation_curve(SketchEntityId("equation")));
}
TEST_CASE("Spatial typed dimensional bindings roll back invalid parameter changes and formulas",
          "[core][parity-b163][spatial-bindings]") {
  auto doc = f::part();
  auto s = f::sketch();
  REQUIRE(doc.add_expression_parameter(ParameterId("derived.distance"), "DerivedDistance",
                                       ParameterType::Length, "distance / 2"));
  REQUIRE(s.add_constraint(
      intent("distance", SpatialConstraintKind::Distance, {f::target("p0"), f::target("p1")},
             f::checked(SpatialDimensionValue::parameter(ParameterId("derived.distance"))))));
  REQUIRE(
      s.add_constraint(intent("angle", SpatialConstraintKind::Angle, {f::curve("line"), f::axis()},
                              f::checked(SpatialDimensionValue::parameter(ParameterId("angle"))))));
  REQUIRE(doc.add_sketch_3d(s));
  CHECK(doc.dependency_graph().has_dependency("derived.distance", "spatial"));
  doc.mark_all_clean();
  const auto before = snapshot(doc);
  const auto graph = doc.dependency_graph().dependencies();
  CHECK_FALSE(doc.set_parameter_value(ParameterId("angle"), f::checked(Quantity::angle_deg(200))));
  CHECK(snapshot(doc) == before);
  CHECK(doc.dependency_graph().dependencies() == graph);
  CHECK(f::checked(doc.create_recompute_plan()).steps().empty());
  CHECK_FALSE(doc.set_parameter_formula(ParameterId("derived.distance"), "distance / 0"));
  CHECK(snapshot(doc) == before);
  REQUIRE(doc.set_parameter_formula(ParameterId("derived.distance"), "distance / 4"));
  CHECK(doc.find_parameter(ParameterId("derived.distance"))->is_expression());
  CHECK(std::get<ParameterId>(doc.find_sketch_3d(s.id())
                                  ->find_constraint(SketchConstraintId("distance"))
                                  ->value()
                                  ->source()) == ParameterId("derived.distance"));
  REQUIRE(doc.set_parameter_value(ParameterId("coordinate.x"), f::checked(Quantity::length_mm(9))));
  CHECK(doc.find_sketch_3d(s.id())->find_point(SketchEntityId("p0"))->x().parameter() ==
        ParameterId("coordinate.x"));
  auto bad = f::sketch("bad");
  REQUIRE(bad.add_constraint(
      intent("wrong_unit", SpatialConstraintKind::Distance, {f::target("p0"), f::target("p1")},
             f::checked(SpatialDimensionValue::parameter(ParameterId("angle"))))));
  const auto current = snapshot(doc);
  CHECK_FALSE(doc.add_sketch_3d(bad));
  CHECK(snapshot(doc) == current);
}
TEST_CASE("Spatial external targets create scoped dependencies and reject source loss and cycles",
          "[core][parity-b163][spatial-references]") {
  auto doc = f::part();
  auto b = f::sketch("B");
  REQUIRE(doc.add_sketch_3d(b));
  auto a = f::sketch("A");
  auto external = f::checked(
      SpatialConstraintTarget::external_curve(f::checked(PathSegmentReference::create_sketch_3d(
          b.id(), SketchEntityId("line"), Sketch3DPathCurveKind::Line))));
  REQUIRE(a.add_constraint(
      intent("on_B", SpatialConstraintKind::PointOnCurve, {f::target("p2"), external})));
  auto point_ref = f::checked(SpatialConstraintTarget::point(f::checked(
      SketchPointReference3D::create_construction_point(ConstructionPointId("external.point")))));
  REQUIRE(a.add_constraint(
      intent("external_point", SpatialConstraintKind::Coincident, {f::target("p0"), point_ref})));
  auto planar_ref = f::checked(
      SpatialConstraintTarget::point(f::checked(SketchPointReference3D::create_planar_sketch_point(
          SketchId("reference.planar"), f::checked(SketchReferenceTarget::create_line_segment_start(
                                            SketchEntityId("source.line")))))));
  REQUIRE(a.add_constraint(
      intent("planar", SpatialConstraintKind::Coincident, {f::target("p1"), planar_ref})));
  REQUIRE(doc.add_sketch_3d(a));
  doc.mark_all_clean();
  const auto before = snapshot(doc);
  CHECK(doc.dependency_graph().has_dependency("B", "A"));
  CHECK(doc.dependency_graph().has_dependency("reference.planar", "A"));
  CHECK(doc.dependency_graph().has_dependency("external.point", "A"));
  REQUIRE(b.remove_line(SketchEntityId("line")));
  CHECK_FALSE(doc.update_sketch_3d(b));
  CHECK(snapshot(doc) == before);
  auto source = f::checked(
      Sketch::create(SketchId("reference.planar"), "Reference", DatumPlaneId("datum.xy")));
  CHECK_FALSE(doc.update_sketch(source));
  CHECK(snapshot(doc) == before);
  b = *doc.find_sketch_3d(Sketch3DId("B"));
  external = f::checked(
      SpatialConstraintTarget::external_curve(f::checked(PathSegmentReference::create_sketch_3d(
          a.id(), SketchEntityId("line"), Sketch3DPathCurveKind::Line))));
  REQUIRE(b.add_constraint(
      intent("on_A", SpatialConstraintKind::PointOnCurve, {f::target("p2"), external})));
  CHECK_FALSE(doc.update_sketch_3d(b));
  CHECK(snapshot(doc) == before);
  auto same = f::sketch("C");
  external = f::checked(
      SpatialConstraintTarget::external_curve(f::checked(PathSegmentReference::create_sketch_3d(
          same.id(), SketchEntityId("line"), Sketch3DPathCurveKind::Line))));
  CHECK_FALSE(same.add_constraint(
      intent("self", SpatialConstraintKind::PointOnCurve, {f::target("p0"), external})));
}
TEST_CASE(
    "Spatial V9 and Project roundtrips preserve constraints and historical curve records exactly",
    "[core][parity-b163][spatial-json]") {
  auto doc = f::part();
  auto s = f::sketch("spatial", true);
  REQUIRE(doc.add_sketch_3d(s));
  const auto legacy = json::parse(snapshot(doc));
  CHECK(legacy["version"] == 8);
  for (const auto& c : matrix())
    REQUIRE(s.add_constraint(c));
  REQUIRE(doc.update_sketch_3d(s));
  const auto text = snapshot(doc);
  const auto current = json::parse(text);
  CHECK(current["version"] == 9);
  const auto& old = legacy["sketches_3d"][0];
  const auto& now = current["sketches_3d"][0];
  for (const auto* field : {"points", "lines", "polylines", "arcs", "splines", "helices",
                            "guide_curves", "equation_curves"})
    CHECK(now[field] == old[field]);
  CHECK(snapshot(f::checked(deserialize_part_document_from_json(text))) == text);
  auto assembly = f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  auto project = f::checked(Project::create(DocumentId("project"), "Project", std::move(assembly)));
  REQUIRE(project.add_part_document(doc));
  const auto portable = f::checked(serialize_project_to_json(project));
  CHECK(f::checked(serialize_project_to_json(
            f::checked(deserialize_project_from_json(portable)))) == portable);
  for (const auto* name : {"remaining_dof", "sampled_points", "solver_cache"})
    CHECK(text.find(name) == std::string::npos);
}
TEST_CASE("Spatial JSON rejects reserved historical fields and malformed semantic intent",
          "[core][parity-b163][spatial-json]") {
  auto doc = f::part();
  auto s = f::sketch();
  for (const auto& c : matrix())
    REQUIRE(s.add_constraint(c));
  REQUIRE(doc.add_sketch_3d(s));
  const auto valid = json::parse(snapshot(doc));
  const auto reject = [&](auto mutation) {
    auto value = valid;
    mutation(value);
    CHECK_FALSE(deserialize_part_document_from_json(value.dump()));
  };
  reject([](auto& j) { j["sketches_3d"][0]["equation_curves"] = nullptr; });
  reject([](auto& j) { j["sketches_3d"][0]["equation_curves"] = json::object(); });
  reject([](auto& j) {
    auto& h = j["sketches_3d"][0]["helices"][0];
    h["radius"] = {{"source", "parameter"}, {"parameter", "radius"}};
    h["pitch"] = {{"source", "parameter"}, {"parameter", "pitch"}};
    h.erase("radius_parameter");
    h.erase("pitch_parameter");
  });
  reject([](auto& j) { j["version"] = 8; });
  reject([](auto& j) { j["version"] = 7; });
  reject([](auto& j) { j["sketches_3d"][0].erase("constraints"); });
  reject([](auto& j) { j["sketches_3d"][0]["constraints"][0]["remaining_dof"] = 2; });
  reject([](auto& j) {
    j["sketches_3d"][0]["constraints"][0]["targets"][0]["position_mm"] = {1, 2, 3};
  });
  reject([](auto& j) { j["sketches_3d"][0]["constraints"][0]["kind"] = "unknown"; });
  reject([](auto& j) {
    j["sketches_3d"][0]["constraints"][0]["targets"][0]["reference"]["point"] = "missing";
  });
  reject([](auto& j) { j["sketches_3d"][0]["constraints"][6]["parameter_seed"] = 2; });
  reject([](auto& j) { j["sketches_3d"][0]["constraints"][12]["value"]["unit"] = "count"; });
  reject([](auto& j) {
    j["sketches_3d"][0]["constraints"].push_back(j["sketches_3d"][0]["constraints"][0]);
  });
}
TEST_CASE("Owned spatial helix scalars use V9 only when needed and retain positive Count turns",
          "[core][parity-b163][spatial-json]") {
  auto doc = f::part();
  auto s = f::sketch();
  const auto old = owner(s);
  const auto h = *s.find_helix(SketchEntityId("helix"));
  REQUIRE(s.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
      h.id(), h.axis(),
      f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(4)))),
      h.pitch_binding(), h.turns_parameter(), h.handedness()))));
  REQUIRE(doc.add_sketch_3d(s));
  const auto value = json::parse(snapshot(doc));
  CHECK(value["version"] == 9);
  const auto& record = value["sketches_3d"][0]["helices"][0];
  CHECK(record["radius"]["value_mm"] == 4);
  CHECK(record["pitch"]["parameter"] == "pitch");
  CHECK(record["turns_parameter"] == "turns");
  CHECK_FALSE(record.contains("radius_parameter"));
  CHECK(snapshot(f::checked(deserialize_part_document_from_json(value.dump()))) == snapshot(doc));
  auto broken = value;
  broken["sketches_3d"][0]["helices"][0]["radius"]["value_mm"] = 0;
  CHECK_FALSE(deserialize_part_document_from_json(broken.dump()));
  broken = value;
  broken["version"] = 8;
  CHECK_FALSE(deserialize_part_document_from_json(broken.dump()));
  REQUIRE(s.replace_helix(h));
  REQUIRE(doc.update_sketch_3d(s));
  CHECK(json::parse(snapshot(doc))["sketches_3d"][0]["helices"] == old["helices"]);
  CHECK(json::parse(snapshot(doc))["version"] != 9);
}

TEST_CASE("Spatial V9 retains equation Pattern Hole history and shared Project resources",
          "[core][parity-b163][spatial-json-resources]") {
  auto member = blcad::test::hole_pattern_fixture::part();
  REQUIRE(member.add_linear_pattern_feature(blcad::test::hole_pattern_fixture::linear()));
  blcad::test::engineering_fixture::annotate(member);
  auto sketch = f::checked(Sketch3D::create(Sketch3DId("spatial.extra"), "Spatial"));
  REQUIRE(sketch.add_point(f::point("owned", {1, 2, 3})));
  REQUIRE(sketch.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("fixed"), SpatialConstraintKind::Fixed,
                                {f::target("owned")}, {}, Point3{1, 2, 3}))));
  REQUIRE(sketch.add_equation_curve(f::checked(EquationCurve::create(
      SketchEntityId("equation"), EquationCurveMode::Parametric3D, {"u", CurveUnit::Scalar}, "0",
      "1", {"u*1mm", "u*u*1mm", "u*u*u*1mm"}, .001))));
  REQUIRE(member.add_sketch_3d(sketch));
  const auto encoded = snapshot(member);
  CHECK(json::parse(encoded).at("version") == 9);
  CHECK(snapshot(f::checked(deserialize_part_document_from_json(encoded))) == encoded);
  auto assembly = f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
  auto project = f::checked(Project::create(DocumentId("project"), "Spatial", assembly));
  REQUIRE(project.add_part_document(member));
  const auto portable = f::checked(serialize_project_to_json(project));
  const auto root = json::parse(portable);
  CHECK(root.at("version") == 2);
  CHECK(root.at("parts")[0].at("version") == 9);
  CHECK(root.at("parts")[0].contains("engineering_resource_refs"));
  CHECK_FALSE(root.at("parts")[0].contains("engineering_resources"));
  CHECK(f::checked(serialize_project_to_json(
            f::checked(deserialize_project_from_json(portable)))) == portable);
}
