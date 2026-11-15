#include "../support/engineering_resource_fixture.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::spatial_fixture;
namespace ef = blcad::test::engineering_fixture;
using json = nlohmann::json;
namespace {
PartDocument inputs() {
  auto p = f::part();
  (void)f::checked(p.add_expression_parameter(ParameterId("tolerance"), "tolerance",
                                              ParameterType::Length, "width / 8000000"));
  (void)f::checked(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("deviation"), "deviation", f::checked(Quantity::length_mm(1e-4))))));
  (void)f::checked(
      p.add_body(f::checked(Body::create(BodyId("result"), "Surface", BodyKind::Surface))));
  return p;
}
SurfaceReference target() {
  return f::checked(SurfaceReference::create_semantic_face(
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
void projects(const PartDocument& input) {
  for (const bool owned : {false, true}) {
    auto p = input;
    if (owned)
      ef::annotate(p);
    auto assembly = f::checked(AssemblyDocument::create(DocumentId("root"), "Root"));
    (void)f::checked(assembly.add_member_part(p.id()));
    auto project = f::checked(Project::create(DocumentId("project"), "Project", assembly));
    (void)f::checked(project.add_part_document(p));
    if (!owned)
      (void)f::checked(project.add_engineering_resource(ef::material()));
    const auto bytes = f::checked(serialize_project_to_json(project));
    CHECK(json::parse(bytes).at("parts").at(0).at("version") == 13);
    const auto restored = f::checked(deserialize_project_from_json(bytes));
    CHECK(f::checked(serialize_project_to_json(restored)) == bytes);
    CHECK(saved(*restored.find_part_document(p.id())) == saved(p));
  }
}
} // namespace
TEST_CASE("Advanced Trim options alone require strict V13 and retain explicit side and projection",
          "[core][advanced-surface-intent][parity-b172]") {
  auto p = inputs();
  const auto base_version = json::parse(saved(p)).at("version");
  auto trim = f::checked(TrimSurfaceFeature::create(
      FeatureId("trim"), "Trim", target(),
      f::checked(TrimmingReference::create_profile_region(f::checked(
          ProfileRegionReference::create(SketchId("base.sketch"), ProfileId("base.profile"))))),
      BodyId("result")));
  const SurfaceTrimOptions options{SurfaceTrimSide::RemoveInside,
                                   SurfaceTrimProjection::AlongDirection, Vector3{0, 0, 1},
                                   ParameterId("tolerance")};
  REQUIRE(p.add_surface_feature(f::checked(trim.with_options(options))));
  const auto before = saved(p);
  const auto root = json::parse(before);
  CHECK(root.at("version") == 13);
  CHECK(root.at("surface_features")[0].at("options").at("side") == "remove");
  CHECK(saved(f::checked(deserialize_part_document_from_json(before))) == before);
  projects(p);
  for (int version = 1; version < 13; ++version) {
    auto invalid = root;
    invalid["version"] = version;
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
  for (int mutation = 0; mutation < 8; ++mutation) {
    auto invalid = root;
    auto& o = invalid["surface_features"][0]["options"];
    if (mutation == 0)
      o["side"] = "first";
    if (mutation == 1)
      o["projection"] = "nearest_guess";
    if (mutation == 2)
      o["direction"] = nullptr;
    if (mutation == 3)
      o["direction"] = {{"x", 0}, {"y", 0}, {"z", 0}};
    if (mutation == 4)
      o["tolerance"] = "turns";
    if (mutation == 5)
      o["samples"] = json::array();
    if (mutation == 6)
      o = nullptr;
    if (mutation == 7)
      invalid["surface_features"][0].erase("options");
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
  CHECK_FALSE(p.set_parameter_formula(ParameterId("tolerance"), "width"));
  CHECK(saved(p) == before);
  CHECK_FALSE(
      p.set_parameter_value(ParameterId("width"), f::checked(Quantity::length_mm(20000000))));
  CHECK(saved(p) == before);
  auto inactive = options;
  inactive.projection = SurfaceTrimProjection::OnSurface;
  CHECK_FALSE(trim.with_options(inactive));
  inactive.direction.reset();
  CHECK(trim.with_options(inactive));
  REQUIRE(p.update_surface_feature(trim));
  CHECK(json::parse(saved(p)).at("version") == base_version);
}
TEST_CASE("Advanced Extend persists natural and tangent budgets without changing legacy replay",
          "[core][advanced-surface-intent][parity-b172]") {
  for (const auto mode : {SurfaceExtendMode::Natural, SurfaceExtendMode::Tangent}) {
    auto p = inputs();
    const auto segment = f::checked(PathSegmentReference::create_planar(
        SketchId("reference.planar"), SketchEntityId("source.line"), PlanarPathCurveKind::Line));
    REQUIRE(p.add_path_curve(
        f::checked(PathCurve::create(PathCurveId("boundary"), "Boundary", {segment},
                                     PathClosure::Open, PathOrientationRule::MinimumTwist))));
    auto extend = f::checked(ExtendSurfaceFeature::create(
        FeatureId("extend"), "Extend", target(),
        f::checked(BoundaryCurveReference::create_path_curve(PathCurveId("boundary"))),
        ParameterId("distance"), BodyId("result")));
    const SurfaceExtendOptions options{mode, ParameterId("tolerance"),
                                       mode == SurfaceExtendMode::Tangent
                                           ? std::optional(ParameterId("deviation"))
                                           : std::nullopt};
    REQUIRE(p.add_surface_feature(f::checked(extend.with_options(options))));
    const auto before = saved(p);
    const auto root = json::parse(before);
    CHECK(root.at("version") == 13);
    CHECK(saved(f::checked(deserialize_part_document_from_json(before))) == before);
    projects(p);
    CHECK(p.dependency_graph().has_dependency("tolerance", "extend"));
    CHECK(p.dependency_graph().has_dependency("deviation", "extend") ==
          (mode == SurfaceExtendMode::Tangent));
    CHECK_FALSE(
        p.set_parameter_value(ParameterId("distance"), f::checked(Quantity::length_mm(1e-7))));
    CHECK(saved(p) == before);
    for (int mutation = 0; mutation < 5; ++mutation) {
      auto invalid = root;
      auto& o = invalid["surface_features"][0]["options"];
      if (mutation == 0)
        o["mode"] = "automatic";
      if (mutation == 1)
        o["maximum_deviation"] =
            mode == SurfaceExtendMode::Tangent ? json(nullptr) : json("deviation");
      if (mutation == 2)
        o["tolerance"] = "turns";
      if (mutation == 3)
        o["healed"] = true;
      if (mutation == 4)
        invalid["version"] = 12;
      CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
    }
    REQUIRE(p.update_surface_feature(extend));
    CHECK(json::parse(saved(p)).at("version") != 13);
  }
}
