#include "../support/engineering_resource_fixture.hpp"
#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;
namespace f = blcad::test::spatial_fixture;
namespace ef = blcad::test::engineering_fixture;
namespace {
std::string saved(const PartDocument& part) {
  return f::checked(serialize_part_document_to_json(part));
}
ProfileSectionReference section(const char* sketch, const char* profile) {
  return f::checked(
      ProfileSectionReference::create_closed_region(f::checked(ProfileRegionReference::create(
          SketchId(sketch), ProfileId(profile), PartFeatureInputRole::LoftSection))));
}
PartDocument part() {
  auto part = f::part();
  (void)f::checked(part.add_parameter(f::checked(Parameter::create_length(
      ParameterId("loft.tolerance"), "Loft tolerance", f::checked(Quantity::length_mm(1e-5))))));
  auto sketch = f::checked(Sketch::create(SketchId("last"), "Last", DatumPlaneId("datum.xy")));
  (void)f::checked(sketch.add_profile(f::checked(RectangleProfile::create(
      ProfileId("last.profile"), ParameterId("width"), ParameterId("height")))));
  (void)f::checked(part.add_sketch(sketch));
  (void)f::checked(
      part.add_body(f::checked(Body::create(BodyId("loft.body"), "Loft", BodyKind::Surface))));
  return part;
}
LoftFeature loft() {
  return f::checked(LoftFeature::create_loft_surface(
      FeatureId("loft"), "Loft",
      {section("base.sketch", "base.profile"), section("last", "last.profile")},
      f::checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, {},
                                                  BodyId("loft.body"))),
      {}, {}, LoftContinuity::G1));
}
LoftConstraintIntent intent() {
  auto support = f::checked(FaceReference::create_planar(
      PartFeatureInputRole::LoftBoundarySupport,
      f::checked(SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))));
  return {
      ParameterId("loft.tolerance"),
      1e-5,
      2e-5,
      LoftBoundaryCondition{
          loft().sections().front().identity(), support, {}, LoftBoundaryContinuity::G2, true, 1.5},
      {},
      {}};
}
void project_roundtrip(const PartDocument& input) {
  for (bool resources : {false, true}) {
    auto part = input;
    if (resources)
      ef::annotate(part);
    auto assembly = f::checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
    (void)f::checked(assembly.add_member_part(part.id()));
    auto project = f::checked(Project::create(DocumentId("project"), "Project", assembly));
    (void)f::checked(project.add_part_document(part));
    if (!resources)
      (void)f::checked(project.add_engineering_resource(ef::material()));
    const auto source = f::checked(serialize_project_to_json(project));
    const auto member = json::parse(source).at("parts").at(0);
    CHECK(member.at("version") == 12);
    CHECK_FALSE(member.contains("engineering_resources"));
    CHECK(member.at("engineering_resource_refs").size() == (resources ? 3U : 0U));
    const auto restored = f::checked(deserialize_project_from_json(source));
    CHECK(f::checked(serialize_project_to_json(restored)) == source);
    CHECK(saved(*restored.find_part_document(part.id())) == saved(part));
  }
}
} // namespace

TEST_CASE("Loft boundary intent preserves longitudinal meaning and strict necessary V12",
          "[core][loft-constraints][parity-b168]") {
  auto document = part();
  const auto feature = f::checked(loft().with_constraints(intent()));
  REQUIRE(document.add_loft_feature(feature));
  CHECK(feature.continuity() == LoftContinuity::G1);
  REQUIRE(feature.constraints()->start);
  CHECK(feature.constraints()->start->continuity == LoftBoundaryContinuity::G2);
  const auto source = saved(document);
  const auto encoded = json::parse(source);
  CHECK(encoded.at("version") == 12);
  CHECK(encoded.at("history_version") == 1);
  CHECK_FALSE(encoded.contains("suppressed_features"));
  const auto restored = f::checked(deserialize_part_document_from_json(source));
  CHECK(saved(restored) == source);
  CHECK(restored.find_loft_feature(FeatureId("loft"))->constraints()->start->reverse_normal);
  CHECK(document.dependency_graph().has_dependency("base.feature", "loft"));
  CHECK(document.dependency_graph().has_dependency("loft.tolerance", "loft"));
  for (int version = 1; version < 12; ++version) {
    auto old = encoded;
    old["version"] = version;
    CHECK_FALSE(deserialize_part_document_from_json(old.dump()));
  }
  auto redundant = encoded;
  redundant["loft_features"][0].erase("constraints");
  CHECK_FALSE(deserialize_part_document_from_json(redundant.dump()));
  const auto legacy = f::checked(feature.with_constraints({}));
  REQUIRE(document.update_loft_feature(legacy));
  CHECK(json::parse(saved(document)).at("version") == 1);
  CHECK_FALSE(json::parse(saved(document)).at("loft_features")[0].contains("constraints"));
  project_roundtrip(restored);
}

TEST_CASE("Loft malformed boundaries and parameter edits remain atomic",
          "[core][loft-constraints][parity-b168]") {
  auto document = part();
  const auto feature = f::checked(loft().with_constraints(intent()));
  REQUIRE(document.add_loft_feature(feature));
  const auto before = saved(document);
  for (int mutation = 0; mutation < 8; ++mutation) {
    auto invalid = intent();
    if (mutation == 0)
      invalid.start->section_identity = loft().sections().back().identity();
    if (mutation == 1)
      invalid.start->tangent_scale = 0;
    if (mutation == 2)
      invalid.start->tangent_scale = std::numeric_limits<double>::quiet_NaN();
    if (mutation == 3)
      invalid.start->imported_source_body = BodyId("base.body");
    if (mutation == 4)
      invalid.start->continuity = static_cast<LoftBoundaryContinuity>(99);
    if (mutation == 5)
      invalid.angular_tolerance_rad = 0;
    if (mutation == 6)
      invalid.curvature_tolerance_per_mm = std::numeric_limits<double>::infinity();
    if (mutation == 7)
      invalid.start.reset();
    CHECK_FALSE(loft().with_constraints(invalid));
    CHECK(saved(document) == before);
  }
  auto wrong_type = intent();
  wrong_type.linear_tolerance = ParameterId("angle");
  CHECK_FALSE(document.update_loft_feature(f::checked(loft().with_constraints(wrong_type))));
  CHECK(saved(document) == before);
  auto missing = intent();
  missing.linear_tolerance = ParameterId("missing");
  CHECK_FALSE(document.update_loft_feature(f::checked(loft().with_constraints(missing))));
  CHECK(saved(document) == before);
  CHECK_FALSE(document.set_parameter_value(ParameterId("loft.tolerance"),
                                           f::checked(Quantity::length_mm(2))));
  CHECK(saved(document) == before);
  auto edited = intent();
  edited.start->tangent_scale = 2;
  edited.start->reverse_normal = false;
  REQUIRE(document.update_loft_feature(f::checked(feature.with_constraints(edited))));
  CHECK(document.find_loft_feature(FeatureId("loft"))->constraints()->start->tangent_scale == 2);
  REQUIRE(document.set_parameter_value(ParameterId("loft.tolerance"),
                                       f::checked(Quantity::length_mm(2e-5))));
  CHECK(saved(f::checked(deserialize_part_document_from_json(saved(document)))) == saved(document));
  for (const auto* key : {"start", "end", "guides", "linear_tolerance", "angular_tolerance_rad",
                          "curvature_tolerance_per_mm"}) {
    auto invalid = json::parse(before);
    invalid["loft_features"][0]["constraints"].erase(key);
    CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  }
  auto invalid = json::parse(before);
  invalid["loft_features"][0]["constraints"]["start"]["support"]["role"] = "thread_support_face";
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
  invalid = json::parse(before);
  invalid["loft_features"][0]["constraints"]["sampled_sections"] = json::array();
  CHECK_FALSE(deserialize_part_document_from_json(invalid.dump()));
}

TEST_CASE("Authored guide stations retain logical section identity and explicit direction",
          "[core][loft-constraints][parity-b168]") {
  auto document = part();
  const auto line = f::checked(PathSegmentReference::create_planar(
      SketchId("reference.planar"), SketchEntityId("source.line"), PlanarPathCurveKind::Line));
  REQUIRE(document.add_path_curve(f::checked(PathCurve::create(
      PathCurveId("rail"), "Rail", {line}, PathClosure::Open, PathOrientationRule::MinimumTwist))));
  const auto base = loft();
  const auto feature = f::checked(LoftFeature::create_loft_surface(
      base.id(), base.name(), base.sections(), base.body_result_context(), {},
      {PathCurveId("rail")}, LoftContinuity::C0));
  LoftConstraintIntent authored{ParameterId("loft.tolerance"),
                                1e-5,
                                1e-5,
                                {},
                                {},
                                {{PathCurveId("rail"),
                                  true,
                                  {{feature.sections()[0].identity(), {}, .25, 1},
                                   {feature.sections()[1].identity(), {}, .75, 0}}}}};
  REQUIRE(document.add_loft_feature(f::checked(feature.with_constraints(authored))));
  const auto before = saved(document);
  CHECK(saved(f::checked(deserialize_part_document_from_json(before))) == before);
  CHECK(document.dependency_graph().has_dependency("rail", "loft"));
  for (int mutation = 0; mutation < 6; ++mutation) {
    auto invalid = authored;
    if (mutation == 0)
      invalid.guides[0].reversed = false;
    if (mutation == 1)
      invalid.guides[0].anchors.pop_back();
    if (mutation == 2)
      invalid.guides[0].anchors[0].section_identity = feature.sections()[1].identity();
    if (mutation == 3)
      invalid.guides[0].anchors[1].guide_fraction = 1;
    if (mutation == 4)
      invalid.guides[0].anchors[0].boundary_fraction = -.01;
    if (mutation == 5)
      invalid.guides[0].guide = PathCurveId("other");
    CHECK_FALSE(feature.with_constraints(invalid));
  }
  auto invalid_entity = authored;
  invalid_entity.guides[0].anchors[0].boundary_entity = SketchEntityId("source.line");
  CHECK_FALSE(document.update_loft_feature(f::checked(feature.with_constraints(invalid_entity))));
  CHECK(saved(document) == before);
  // Alignment/rotation/flip and compatibility subdivisions do not enter the logical key.
  const auto region = std::get<ProfileRegionReference>(feature.sections()[0].source());
  const auto flipped = f::checked(
      ProfileSectionReference::create_closed_region(region, {}, true, ParameterId("angle")));
  CHECK(flipped.identity() == feature.sections()[0].identity());
  CHECK(flipped.identity() != feature.sections()[1].identity());
}

TEST_CASE("Imported Loft support validates base output ownership and exact source protection",
          "[core][loft-constraints][parity-b168]") {
  auto document = part();
  const auto source = f::checked(
      StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                               std::string(64, 'a'), StepImportMode::EditableBody));
  ImportedTopologyDescriptor body;
  body.kind = ImportedTopologyKind::Body;
  body.geometry = ImportedGeometryClass::Opaque;
  body.body_kind = BodyKind::Solid;
  body.bounds_mm = {0, 0, 0, 10, 10, 10};
  body.centroid = {5, 5, 5};
  body.measure = 1000;
  body.geometry_key = std::string(64, 'b');
  ImportedTopologyDescriptor face;
  face.kind = ImportedTopologyKind::Face;
  face.geometry = ImportedGeometryClass::Plane;
  face.bounds_mm = {0, 0, 10, 10, 10, 10};
  face.centroid = {5, 5, 10};
  face.direction = {0, 0, 1};
  face.measure = 100;
  face.geometry_key = std::string(64, 'c');
  const auto catalog = f::checked(ImportedTopologyCatalog::create(
      ImportedTopologyCatalogId("catalog"), source.id(), source.sha256_digest(),
      std::string(64, 'd'),
      {{ImportedTopologyId("solid"), {}, body},
       {ImportedTopologyId("face"), ImportedTopologyId("solid"), face}}));
  REQUIRE(document.add_step_import_source(source));
  REQUIRE(document.add_imported_topology_catalog(catalog));
  REQUIRE(document.add_body(
      f::checked(Body::create(BodyId("supplier.body"), "Supplier", BodyKind::Solid))));
  const auto definition = f::checked(ImportedPartDefinition::create(
      source.id(), catalog.id(),
      {{ImportedTopologyId("solid"), BodyId("supplier.body"), BodyKind::Solid}}));
  REQUIRE(document.add_imported_body_feature(
      f::checked(ImportedBodyFeature::create(FeatureId("supplier.base"), "Supplier", definition))));
  auto authored = intent();
  const auto reference = f::checked(ImportedTopologyReference::create(
      catalog.id(), ImportedTopologyId("face"), ImportedTopologyCapability::Face));
  authored.start->support = f::checked(
      FaceReference::create_imported(PartFeatureInputRole::LoftBoundarySupport, reference));
  CHECK_FALSE(loft().with_constraints(authored));
  authored.start->imported_source_body = BodyId("base.body");
  const auto before = saved(document);
  CHECK_FALSE(document.add_loft_feature(f::checked(loft().with_constraints(authored))));
  CHECK(saved(document) == before);
  authored.start->imported_source_body = BodyId("supplier.body");
  REQUIRE(document.add_loft_feature(f::checked(loft().with_constraints(authored))));
  const auto persisted = saved(document);
  CHECK(document.dependency_graph().has_dependency("body:supplier.body", "loft"));
  CHECK(saved(f::checked(deserialize_part_document_from_json(persisted))) == persisted);
  CHECK_FALSE(document.remove_imported_body_feature(FeatureId("supplier.base")));
  CHECK(saved(document) == persisted);
  for (const auto mode : {FeatureBodyOperationMode::Join, FeatureBodyOperationMode::Cut,
                          FeatureBodyOperationMode::Intersect}) {
    auto in_place = f::checked(deserialize_part_document_from_json(before));
    const auto context =
        f::checked(FeatureBodyResultContext::create(mode, BodyId("supplier.body"), {}));
    const auto base =
        mode == FeatureBodyOperationMode::Cut
            ? LoftFeature::create_loft_cut(loft().id(), "Cut", loft().sections(), context)
            : LoftFeature::create_loft(loft().id(), "Solid", loft().sections(), context);
    REQUIRE(in_place.add_loft_feature(f::checked(f::checked(base).with_constraints(authored))));
    CHECK(in_place.dependency_graph().has_dependency("supplier.base", "loft"));
    CHECK_FALSE(in_place.dependency_graph().has_cycle());
    CHECK(saved(f::checked(deserialize_part_document_from_json(saved(in_place)))) ==
          saved(in_place));
  }
}

TEST_CASE("Surface result support is typed and a downstream support update rejects its cycle",
          "[core][loft-constraints][parity-b168]") {
  auto document = part();
  REQUIRE(document.add_loft_feature(loft()));
  REQUIRE(document.add_body(
      f::checked(Body::create(BodyId("support.body"), "Support", BodyKind::Surface))));
  const auto stitch = f::checked(SurfaceStitchFeature::create(
      FeatureId("support"), "Support",
      {f::checked(SurfaceReference::create_body(BodyId("loft.body"))),
       f::checked(SurfaceReference::create_semantic_face(f::checked(
           SemanticFaceReference::create(FeatureId("base.feature"), SemanticFace::Top))))},
      ParameterId("loft.tolerance"), BodyId("support.body")));
  REQUIRE(document.add_surface_feature(stitch));
  const auto support = f::checked(ResultFaceReference::create(
      FeatureId("support"), BodyId("support.body"), {{FeatureId("support"), "authored/support"}}));
  REQUIRE(validate_result_face_reference(document, support));
  CHECK_FALSE(validate_result_face_workplane_reference(
      document,
      f::checked(ResultFaceWorkplane::create(DatumPlaneId("solid.only.plane"), "Plane", support))));
  auto authored = intent();
  authored.start->support =
      f::checked(FaceReference::create_result(PartFeatureInputRole::LoftBoundarySupport, support));
  const auto before = saved(document);
  CHECK_FALSE(document.update_loft_feature(f::checked(loft().with_constraints(authored))));
  CHECK(saved(document) == before);
  CHECK(document.dependency_graph().has_dependency("body:loft.body", "support"));
}

TEST_CASE("Loft logical identities cannot alias separator-containing source IDs",
          "[core][loft-constraints][parity-b168]") {
  const auto a = section("a:b", "c");
  const auto b = section("a", "b:c");
  CHECK(a.identity() != b.identity());
  CHECK(LoftFeature::create_loft_surface(loft().id(), "Distinct", {a, b},
                                         loft().body_result_context()));
}

TEST_CASE(
    "Suppressed Loft support is a required history input and formula tolerance edits roll back",
    "[core][loft-constraints][parity-b168]") {
  auto document = part();
  REQUIRE(document.add_expression_parameter(ParameterId("loft.computed_tolerance"),
                                            "Computed tolerance", ParameterType::Length,
                                            "width / 1000000"));
  auto authored = intent();
  authored.linear_tolerance = ParameterId("loft.computed_tolerance");
  REQUIRE(document.add_loft_feature(f::checked(loft().with_constraints(authored))));
  REQUIRE(
      document.set_parameter_formula(ParameterId("loft.computed_tolerance"), "width / 2000000"));
  const auto before = saved(document);
  CHECK_FALSE(
      document.set_parameter_value(ParameterId("width"), f::checked(Quantity::length_mm(4000000))));
  CHECK(saved(document) == before);
  CHECK_FALSE(document.set_parameter_formula(ParameterId("loft.computed_tolerance"), "width"));
  CHECK(saved(document) == before);
  PartFeatureHistoryIntent history;
  for (const auto& row : f::checked(document.executable_history()))
    history.order.push_back(row.node_id);
  history.suppressed = {"base.feature"};
  REQUIRE(document.apply_history_intent(history));
  const auto plan = f::checked(document.create_history_evaluation_plan());
  const auto row = std::find_if(plan.entries.begin(), plan.entries.end(),
                                [](const auto& entry) { return entry.feature.node_id == "loft"; });
  REQUIRE(row != plan.entries.end());
  CHECK(row->state == PartHistoryExecutionState::Blocked);
  REQUIRE(std::any_of(row->issues.begin(), row->issues.end(), [](const auto& issue) {
    return issue.code == PartHistoryInputIssueCode::SuppressedProducer &&
           issue.producer_id == "base.feature";
  }));
  CHECK(saved(f::checked(deserialize_part_document_from_json(saved(document)))) == saved(document));
}

TEST_CASE("Loft guide anchors distinguish seam aliases from separate original edge endpoints",
          "[core][loft-constraints][parity-b168]") {
  const auto base = loft();
  const auto feature = f::checked(LoftFeature::create_loft_surface(
      base.id(), base.name(), base.sections(), base.body_result_context(), {},
      {PathCurveId("a"), PathCurveId("b")}));
  LoftConstraintIntent authored{ParameterId("loft.tolerance"), 1e-5, 1e-5, {}, {}, {}};
  for (const auto& guide : feature.guide_curves())
    authored.guides.push_back({guide,
                               false,
                               {{feature.sections()[0].identity(), {}, 0, 0},
                                {feature.sections()[1].identity(), {}, 0, 1}}});
  CHECK_FALSE(feature.with_constraints(authored));
  for (auto& anchor : authored.guides[1].anchors)
    anchor.boundary_fraction = 1;
  CHECK_FALSE(feature.with_constraints(authored)); // Same closed seam.
  for (auto& binding : authored.guides)
    for (auto& anchor : binding.anchors)
      anchor.boundary_entity = SketchEntityId("original.edge");
  REQUIRE(feature.with_constraints(authored)); // Different endpoints on one original edge.
  authored.guides[1].anchors.push_back(authored.guides[1].anchors.back());
  CHECK_FALSE(
      feature.with_constraints(authored)); // Surplus logical section never indexes past sections.
}
