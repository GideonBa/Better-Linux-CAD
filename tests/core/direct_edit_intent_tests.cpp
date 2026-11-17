#include "../support/hole_feature_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/direct_edit_feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>

using namespace blcad;
namespace f = blcad::test::history_fixture;
namespace {
PartDocument part() {
  auto p = f::native_base();
  for (const auto& [id, value] : {std::pair{"dx", 1.0}, {"dy", 1.0}, {"dz", 2.0}})
    f::accept(p.add_parameter(f::checked(
        Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
  return p;
}
std::vector<DirectEditFace> faces() {
  return {{f::face(PartFeatureInputRole::SurfaceSourceFace), DirectEditFaceCapability::Planar}};
}
DirectEditFeature feature(DirectEditOperation operation = DirectFaceOffset{ParameterId("dz")}) {
  return f::checked(DirectEditFeature::create(FeatureId("edit"), "Direct Edit", BodyId("body.base"),
                                              FeatureId("base"), faces(), std::move(operation)));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
PartDocument imported_cylinder() {
  const auto original = f::imported();
  auto entries = original.imported_topology_catalogs().front().entries();
  ImportedTopologyDescriptor wall;
  wall.kind = ImportedTopologyKind::Face;
  wall.geometry = ImportedGeometryClass::Cylinder;
  wall.measure = 100;
  wall.bounds_mm = {-2, -2, 0, 2, 2, 10};
  wall.direction = {0, 0, 1};
  wall.radius_mm = 2;
  wall.geometry_key = std::string(64, 'b');
  entries.push_back({ImportedTopologyId("wall"), ImportedTopologyId("solid"), wall});
  const auto source = original.step_import_sources().front();
  const auto catalog = f::checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), source.id(), source.sha256_digest(),
      std::string(64, '2'), std::move(entries)));
  auto result = f::checked(
      PartDocument::create_editable_import(DocumentId("direct.import"), "Imported cylinder", source,
                                           catalog, original.imported_body_features().front()));
  f::accept(result.add_parameter(f::checked(Parameter::create_length(
      ParameterId("size"), "Diameter", f::checked(Quantity::length_mm(5))))));
  return result;
}
} // namespace

TEST_CASE("Direct Edit owns typed operations explicit frame and conservative healing intent",
          "[core][direct-edit][parity-b185]") {
  auto p = part();
  for (const auto& operation : std::vector<DirectEditOperation>{
           DirectFaceMove{ParameterId("dx"), ParameterId("dy"), ParameterId("dz"), {0, 0, -1}},
           DirectFaceRotate{{1, 0, 0}, ParameterId("angle")}, DirectFaceOffset{ParameterId("dz")},
           DirectFaceDeleteHeal{}}) {
    auto candidate = p;
    f::accept(candidate.add_direct_edit_feature(feature(operation)));
    CHECK(candidate.direct_edit_feature_count() == 1);
    CHECK(candidate.find_direct_edit_feature(FeatureId("edit"))->predecessor() ==
          FeatureId("base"));
    CHECK(saved(f::checked(deserialize_part_document_from_json(saved(candidate)))) ==
          saved(candidate));
    CHECK(candidate.dependency_graph().has_dependency("base", "edit"));
    CHECK(candidate.dependency_graph().has_dependency("edit", "body:body.base"));
    CHECK(f::checked(candidate.executable_history()).back().family ==
          PartFeatureFamily::DirectEdit);
  }
  auto duplicate = faces();
  duplicate.push_back(duplicate.front());
  CHECK_FALSE(DirectEditFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                        FeatureId("base"), duplicate, DirectFaceDeleteHeal{}));
  auto wrong = faces();
  wrong[0].expected = DirectEditFaceCapability::AnySurface;
  CHECK_FALSE(DirectEditFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                        FeatureId("base"), wrong,
                                        DirectFaceOffset{ParameterId("dz")}));
  CHECK_FALSE(DirectEditFeature::create(
      FeatureId("bad"), "Bad", BodyId("body.base"), FeatureId("base"), faces(),
      DirectFaceSize{ParameterId("dz"), DirectEditSizeMode::Radius}));
  DirectEditFrame axes{{1, 2, 3}, {0, 1, 0}, {0, 0, 1}};
  CHECK(DirectEditFeature::create(FeatureId("good"), "Good", BodyId("body.base"), FeatureId("base"),
                                  faces(), DirectFaceOffset{ParameterId("dz")}, axes));
  axes.y_axis = axes.x_axis;
  CHECK_FALSE(DirectEditFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                        FeatureId("base"), faces(),
                                        DirectFaceOffset{ParameterId("dz")}, axes));
  DirectEditPolicy bad;
  bad.tolerance_mm = std::numeric_limits<double>::infinity();
  CHECK_FALSE(DirectEditFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                        FeatureId("base"), faces(), DirectFaceDeleteHeal{}, {},
                                        bad));
  bad = {};
  bad.healing = static_cast<DirectEditHealPolicy>(99);
  CHECK_FALSE(DirectEditFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                        FeatureId("base"), faces(), DirectFaceDeleteHeal{}, {},
                                        bad));
}

TEST_CASE("Direct Edit mutations validate dimension provenance and history atomically",
          "[core][direct-edit][parity-b185]") {
  auto p = part();
  const auto before = saved(p);
  CHECK_FALSE(p.add_direct_edit_feature(feature(DirectFaceOffset{ParameterId("angle")})));
  CHECK(saved(p) == before);
  auto missing =
      f::checked(DirectEditFeature::create(FeatureId("edit"), "Missing", BodyId("body.base"),
                                           FeatureId("missing"), faces(), DirectFaceDeleteHeal{}));
  CHECK_FALSE(p.add_direct_edit_feature(missing));
  CHECK(saved(p) == before);
  f::accept(p.add_direct_edit_feature(feature(DirectFaceRotate{{1, 0, 0}, ParameterId("angle")})));
  const auto owned = saved(p);
  CHECK_FALSE(p.add_direct_edit_feature(feature()));
  CHECK_FALSE(p.set_parameter_value(ParameterId("angle"), f::checked(Quantity::angle_deg(90))));
  CHECK(saved(p) == owned);
  CHECK_FALSE(p.remove_feature(FeatureId("base")));
  CHECK_FALSE(p.remove_body(BodyId("body.base")));
  CHECK_FALSE(p.apply_history_intent({{"edit", "base"}, {}}));
  CHECK(saved(p) == owned);
  f::accept(p.apply_history_intent({{"base", "edit"}, {"edit"}}));
  const auto plan = f::checked(p.create_history_evaluation_plan());
  CHECK(plan.entries.back().state == PartHistoryExecutionState::Suppressed);
  CHECK(plan.bodies.front().producer_id == std::optional<std::string>("base"));
  f::accept(p.apply_history_intent({{"base", "edit"}, {}}));
  f::accept(p.update_direct_edit_feature(feature(DirectFaceOffset{ParameterId("dz")})));
  CHECK(p.dependency_graph().has_dependency("dz", "edit"));
  CHECK_FALSE(p.dependency_graph().has_dependency("angle", "edit"));
  f::accept(p.remove_direct_edit_feature(FeatureId("edit")));
  CHECK(p.direct_edit_feature_count() == 0);
}

TEST_CASE("Direct Edit V16 operation dispatch is strict and necessary",
          "[core][direct-edit][parity-b185]") {
  auto p = part();
  auto legacy = nlohmann::json::parse(saved(p));
  CHECK_FALSE(legacy.contains("direct_edit_features"));
  f::accept(p.add_direct_edit_feature(feature()));
  auto root = nlohmann::json::parse(saved(p));
  REQUIRE(root["version"] == 16);
  REQUIRE(root["direct_edit_features"][0]["operation_version"] == 1);
  for (const auto& value :
       {nlohmann::json(2), nlohmann::json(1.0), nlohmann::json("1"), nlohmann::json(nullptr)}) {
    auto invalid = root;
    invalid["direct_edit_features"][0]["operation_version"] = value;
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
  for (const auto* field : {"operation", "policy", "frame"}) {
    auto invalid = root;
    invalid["direct_edit_features"][0][field]["unknown"] = true;
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
  auto invalid = root;
  invalid["direct_edit_features"][0]["operation"]["mode"] = "future_operation";
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["direct_edit_features"][0]["predecessor"] = "missing";
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["direct_edit_features"][0]["faces"].push_back(
      invalid["direct_edit_features"][0]["faces"][0]);
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["direct_edit_features"][0]["policy"]["blends"] = "guess";
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["direct_edit_features"] = nlohmann::json::array();
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["version"] = 15;
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  legacy["version"] = 16;
  CHECK_FALSE(deserialize_part_document_from_json(legacy.dump()));
}

TEST_CASE("Direct Edit preserves result provenance and protects authored Hole Thread semantics",
          "[core][direct-edit][parity-b185]") {
  auto p = part();
  f::accept(p.add_direct_edit_feature(feature()));
  const auto ref = f::checked(ResultFaceReference::create(FeatureId("edit"), BodyId("body.base"),
                                                          {{FeatureId("base"), "cap/top"}}));
  CHECK(validate_result_face_reference(p, ref));
  auto support =
      f::checked(FaceReference::create_result(PartFeatureInputRole::SurfaceSourceFace, ref));
  auto next = f::checked(DirectEditFeature::create(
      FeatureId("next"), "Next", BodyId("body.base"), FeatureId("edit"),
      {{support, DirectEditFaceCapability::Planar}}, DirectFaceOffset{ParameterId("dz")}));
  f::accept(p.add_direct_edit_feature(next));
  CHECK(saved(f::checked(deserialize_part_document_from_json(saved(p)))) == saved(p));
  auto stale =
      f::checked(DirectEditFeature::create(FeatureId("stale"), "Stale", BodyId("body.base"),
                                           FeatureId("base"), faces(), DirectFaceDeleteHeal{}));
  CHECK_FALSE(p.add_direct_edit_feature(stale));
  auto hole_part = blcad::test::hole_fixture::part("protected", false, false);
  const auto bore = f::checked(ResultFaceReference::create(FeatureId("hole"), BodyId("body.base"),
                                                           {{FeatureId("hole"), "bore/center"}}));
  const auto bore_face =
      f::checked(FaceReference::create_result(PartFeatureInputRole::SurfaceSourceFace, bore));
  auto resize = f::checked(DirectEditFeature::create(
      FeatureId("resize"), "Resize", BodyId("body.base"), FeatureId("hole"),
      {{bore_face, DirectEditFaceCapability::Cylindrical}},
      DirectFaceSize{ParameterId("radius"), DirectEditSizeMode::Radius}));
  const auto protected_before = saved(hole_part);
  CHECK_FALSE(hole_part.add_direct_edit_feature(resize));
  CHECK(saved(hole_part) == protected_before);
  auto reference = f::imported(true);
  CHECK_FALSE(reference.add_direct_edit_feature(feature()));
}

TEST_CASE("Direct Edit imported selections retain catalog ownership and immutable base records",
          "[core][direct-edit][parity-b185]") {
  auto p = imported_cylinder();
  const auto original = nlohmann::json::parse(saved(p));
  const auto reference = f::checked(FaceReference::create_imported(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(ImportedTopologyReference::create(ImportedTopologyCatalogId("catalog"),
                                                   ImportedTopologyId("wall"),
                                                   ImportedTopologyCapability::Face))));
  auto operation = f::checked(DirectEditFeature::create(
      FeatureId("size"), "Resize cylinder", BodyId("imported.body"), FeatureId("imported"),
      {{reference, DirectEditFaceCapability::Cylindrical}},
      DirectFaceSize{ParameterId("size"), DirectEditSizeMode::Diameter}));
  // Parameter and feature identities share the dependency namespace.
  CHECK_FALSE(p.add_direct_edit_feature(operation));
  operation = f::checked(DirectEditFeature::create(
      FeatureId("resize"), "Resize cylinder", BodyId("imported.body"), FeatureId("imported"),
      {{reference, DirectEditFaceCapability::Cylindrical}},
      DirectFaceSize{ParameterId("size"), DirectEditSizeMode::Diameter}));
  f::accept(p.add_direct_edit_feature(operation));
  const auto root = nlohmann::json::parse(saved(p));
  for (const auto* field :
       {"step_import_sources", "imported_topology_catalogs", "imported_body_features"})
    CHECK(root.at(field) == original.at(field));
  CHECK(saved(f::checked(deserialize_part_document_from_json(saved(p)))) == saved(p));
  auto invalid = root;
  invalid["direct_edit_features"][0]["faces"][0]["expected"] = "planar";
  invalid["direct_edit_features"][0]["operation"] = {
      {"mode", "offset"}, {"distance", "size"}, {"reverse", false}};
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["imported_topology_catalogs"] = nlohmann::json::array();
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = root;
  invalid["step_import_sources"] = nlohmann::json::array();
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  auto project =
      f::checked(Project::create(DocumentId("direct.project"), "Direct Edit project",
                                 f::checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  f::accept(project.add_part_document(p));
  const auto text = f::checked(serialize_project_to_json(project));
  const auto opened = f::checked(deserialize_project_from_json(text));
  CHECK(f::checked(serialize_project_to_json(opened)) == text);
}
