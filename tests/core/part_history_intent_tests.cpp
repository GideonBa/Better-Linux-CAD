#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
namespace fixture = blcad::test::history_fixture;
namespace {
template <typename T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string json(const PartDocument& part) {
  return checked(serialize_part_document_to_json(part));
}
PartFeatureHistoryIntent intent(const PartDocument& part) {
  PartFeatureHistoryIntent result;
  for (const auto& entry : checked(part.executable_history())) {
    result.order.push_back(entry.node_id);
    if (entry.state == PartFeatureState::Suppressed)
      result.suppressed.push_back(entry.node_id);
  }
  return result;
}
void second(PartDocument& part, const char* source = "profile") {
  const auto context = fixture::new_body(part, "second");
  const auto feature = checked(Feature::create_additive_extrude(
      FeatureId("second"), "Second", SketchId(source), ParameterId("depth")));
  REQUIRE(part.add_feature(checked(feature.with_body_result_context(context))));
}
FilletFeature fillet() {
  return checked(FilletFeature::create(FeatureId("fillet"), "Fillet", BodyId("body.base"),
                                       {fixture::edge(PartFeatureInputRole::FilletEdge)},
                                       ParameterId("radius")));
}
std::vector<std::pair<std::string, InvalidationStatus>> statuses(const PartDocument& part) {
  std::vector<std::pair<std::string, InvalidationStatus>> result;
  for (const auto& entry : part.invalidation_state().entries())
    result.emplace_back(entry.node_id, entry.status);
  return result;
}
} // namespace

TEST_CASE("Authored history retains all families definitions and sparse suppression intent",
          "[core][parity-b146][history-intent]") {
  const auto part = fixture::all_families();
  const auto original = json(part);
  auto edit = intent(part);
  for (const auto& id : edit.order)
    if (id != "imported")
      edit.suppressed.push_back(id);
  const auto candidate = checked(part.with_history_intent(edit));
  CHECK(json(part) == original);
  REQUIRE(candidate.authored_history_intent());
  CHECK(*candidate.authored_history_intent() == edit);
  const auto rows = checked(candidate.executable_history());
  REQUIRE(rows.size() == 15);
  for (const auto& row : rows)
    CHECK(row.state ==
          (row.node_id == "imported" ? PartFeatureState::Active : PartFeatureState::Suppressed));
  auto before = nlohmann::json::parse(original);
  auto after = nlohmann::json::parse(json(candidate));
  CHECK(after["version"] == 2);
  CHECK(after["feature_order"] == edit.order);
  CHECK(after["suppressed_features"] == edit.suppressed);
  after["version"] = 1;
  after.erase("suppressed_features");
  CHECK(after == before);
  const auto restored = checked(deserialize_part_document_from_json(json(candidate)));
  CHECK(restored.authored_history_intent() == candidate.authored_history_intent());
  CHECK(json(restored) == json(candidate));
}

TEST_CASE(
    "Authored independent order survives queries copies and JSON without changing family arrays",
    "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  second(part);
  auto edit = intent(part);
  REQUIRE(edit.order == std::vector<std::string>{"imported", "base", "second"});
  std::swap(edit.order[1], edit.order[2]);
  REQUIRE(part.apply_history_intent(edit));
  CHECK(intent(part) == edit);
  CHECK(part.features()[0].id().value() == "base");
  CHECK(part.features()[1].id().value() == "second");
  CHECK(intent(PartDocument(part)) == edit);
  const auto serialized = json(part);
  const auto restored = checked(deserialize_part_document_from_json(serialized));
  CHECK(intent(restored) == edit);
  CHECK(restored.features()[0].id().value() == "base");
  CHECK(json(restored) == serialized);
  CHECK(nlohmann::json::parse(serialized)["suppressed_features"].empty());
}

TEST_CASE("Legal modifier reorder replaces the previous same Body predecessor chain",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  REQUIRE(part.add_fillet_feature(fillet()));
  REQUIRE(part.add_chamfer_feature(checked(ChamferFeature::create_equal_distance(
      FeatureId("chamfer"), "Chamfer", BodyId("body.base"),
      {fixture::edge(PartFeatureInputRole::ChamferEdge)}, ParameterId("radius")))));
  auto edit = intent(part);
  REQUIRE(edit.order == std::vector<std::string>{"imported", "base", "fillet", "chamfer"});
  std::swap(edit.order[2], edit.order[3]);
  REQUIRE(part.apply_history_intent(edit));
  CHECK(part.dependency_graph().has_dependency("base", "chamfer"));
  CHECK(part.dependency_graph().has_dependency("chamfer", "fillet"));
  CHECK_FALSE(part.dependency_graph().has_dependency("fillet", "chamfer"));
  CHECK(part.fillet_features()[0].id().value() == "fillet");
  CHECK(json(checked(deserialize_part_document_from_json(json(part)))) == json(part));
}

TEST_CASE("Authored order rejects producer after a workplane Sketch consumer with typed identities",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  REQUIRE(part.add_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("support"), "Support",
      checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))))));
  fixture::rectangle(part, "consumer", "support");
  second(part, "consumer");
  auto edit = intent(part);
  std::swap(edit.order[1], edit.order[2]);
  const auto before = json(part);
  const auto edges = part.dependency_graph().dependencies();
  const auto state = statuses(part);
  const auto diagnostics = part.diagnose_history_intent(edit);
  REQUIRE_FALSE(diagnostics.empty());
  CHECK(diagnostics.front().code == PartHistoryDiagnosticCode::DependencyAfterConsumer);
  CHECK(diagnostics.front().producer_id == "base");
  CHECK(diagnostics.front().consumer_id == "second");
  CHECK_FALSE(part.apply_history_intent(edit));
  CHECK(json(part) == before);
  CHECK(part.dependency_graph().dependencies() == edges);
  CHECK(statuses(part) == state);
}

TEST_CASE("Malformed history candidates report typed reasons and preserve complete state",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  const bool already_authored = GENERATE(false, true);
  if (already_authored)
    REQUIRE(part.apply_history_intent({{"imported", "base"}, {"base"}}));
  const auto before = json(part);
  const auto state = statuses(part);
  auto edit = intent(part);
  auto code = PartHistoryDiagnosticCode::UnknownExecutable;
  SECTION("unknown") {
    edit.order[0] = "ghost";
  }
  SECTION("duplicate") {
    edit.order.push_back("base");
    code = PartHistoryDiagnosticCode::DuplicateExecutable;
  }
  SECTION("missing") {
    edit.order.pop_back();
    code = PartHistoryDiagnosticCode::MissingExecutable;
  }
  SECTION("unknown suppression") {
    edit.suppressed = {"ghost"};
    code = PartHistoryDiagnosticCode::UnknownSuppressedExecutable;
  }
  SECTION("duplicate suppression") {
    edit.suppressed = {"base", "base"};
    code = PartHistoryDiagnosticCode::DuplicateSuppression;
  }
  SECTION("indispensable imported base") {
    edit.suppressed = {"imported"};
    code = PartHistoryDiagnosticCode::ImportedBaseRequired;
  }
  const auto diagnostics = part.diagnose_history_intent(edit);
  REQUIRE_FALSE(diagnostics.empty());
  CHECK(diagnostics.front().code == code);
  CHECK_FALSE(part.with_history_intent(edit));
  CHECK_FALSE(part.apply_history_intent(edit));
  CHECK(json(part) == before);
  CHECK(statuses(part) == state);
  CHECK(part.authored_history_intent().has_value() == already_authored);
}

TEST_CASE(
    "Explicit history appends fresh features and removes old order and suppression on ID reuse",
    "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  REQUIRE(part.add_fillet_feature(fillet()));
  auto edit = intent(part);
  edit.suppressed = {"fillet"};
  REQUIRE(part.apply_history_intent(edit));
  REQUIRE(part.remove_fillet_feature(FeatureId("fillet")));
  CHECK(intent(part).suppressed.empty());
  CHECK(intent(part).order == std::vector<std::string>{"imported", "base"});
  second(part);
  REQUIRE(part.add_fillet_feature(fillet()));
  CHECK(intent(part).order == std::vector<std::string>{"imported", "base", "second", "fillet"});
  CHECK(intent(part).suppressed.empty());
  CHECK(json(checked(deserialize_part_document_from_json(json(part)))) == json(part));
}

TEST_CASE("Existing updates cannot contradict explicit history through later support resources",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  fixture::rectangle(part, "independent");
  second(part, "independent");
  REQUIRE(part.apply_history_intent(intent(part)));
  REQUIRE(part.add_derived_workplane(checked(DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("late.support"), "Late support",
      checked(SemanticFaceReference::create(FeatureId("second"), SemanticFace::Top))))));
  const auto before = json(part);
  auto sketch =
      checked(Sketch::create(SketchId("profile"), "Profile", DatumPlaneId("late.support")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile.profile"), ParameterId("width"), ParameterId("height")))));
  CHECK_FALSE(part.update_sketch(std::move(sketch)));
  CHECK(json(part) == before);
  fixture::rectangle(part, "late.profile", "late.support");
  const auto before_feature = json(part);
  const auto replacement = checked(Feature::create_additive_extrude(
      FeatureId("base"), "Base", SketchId("late.profile"), ParameterId("depth")));
  REQUIRE(part.find_feature(FeatureId("base"))->body_result_context());
  CHECK_FALSE(part.update_feature(checked(replacement.with_body_result_context(
      *part.find_feature(FeatureId("base"))->body_result_context()))));
  CHECK(json(part) == before_feature);
}

TEST_CASE("Versioned history rejects malformed and unversioned suppression intent",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  REQUIRE(part.apply_history_intent(intent(part)));
  auto root = nlohmann::json::parse(json(part));
  SECTION("missing order") {
    root.erase("feature_order");
  }
  SECTION("missing suppression") {
    root.erase("suppressed_features");
  }
  SECTION("wrong order container") {
    root["feature_order"] = nlohmann::json::object();
  }
  SECTION("wrong suppression container") {
    root["suppressed_features"] = nlohmann::json::object();
  }
  SECTION("wrong order element") {
    root["feature_order"][0] = 1;
  }
  SECTION("wrong suppression element") {
    root["suppressed_features"] = {false};
  }
  SECTION("unknown executable") {
    root["feature_order"][0] = "ghost";
  }
  SECTION("missing executable") {
    root["feature_order"].erase(0);
  }
  SECTION("duplicate executable") {
    root["feature_order"][0] = root["feature_order"][1];
  }
  SECTION("unknown suppressed executable") {
    root["suppressed_features"] = {"ghost"};
  }
  SECTION("duplicate suppression") {
    root["suppressed_features"] = {"base", "base"};
  }
  SECTION("suppressed imported base") {
    root["suppressed_features"] = {"imported"};
  }
  SECTION("version 1 cannot ignore new state") {
    root["version"] = 1;
  }
  SECTION("future version") {
    root["version"] = 3;
  }
  SECTION("fractional version") {
    root["version"] = 2.5;
  }
  CHECK_FALSE(deserialize_part_document_from_json(root.dump()));
  CHECK_FALSE(
      deserialize_part_document_from_json(root.dump(), PartDocumentReadMode::ReferenceRepair));
}

TEST_CASE("Immutable Reference Part rejects even empty authored history",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::imported(true);
  const auto before = json(part);
  const auto diagnostics = part.diagnose_history_intent({});
  REQUIRE(diagnostics.size() == 1);
  CHECK(diagnostics[0].code == PartHistoryDiagnosticCode::ReferencePartReadOnly);
  CHECK_FALSE(part.apply_history_intent({}));
  auto root = nlohmann::json::parse(before);
  root["version"] = 2;
  root["feature_order"] = nlohmann::json::array();
  root["suppressed_features"] = nlohmann::json::array();
  CHECK_FALSE(deserialize_part_document_from_json(root.dump()));
  CHECK(json(part) == before);
}

TEST_CASE("Specialized imported and Surface removals clear history before the same IDs are reused",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::all_families();
  const auto imported = part.imported_body_features().front();
  const auto surface = part.surface_features().front();
  auto edit = intent(part);
  edit.suppressed = {"surface"};
  REQUIRE(part.apply_history_intent(edit));
  REQUIRE(part.remove_surface_feature(FeatureId("surface")));
  REQUIRE(part.remove_imported_body_feature(FeatureId("imported")));
  CHECK(intent(part).suppressed.empty());
  REQUIRE(part.add_imported_body_feature(imported));
  REQUIRE(part.add_surface_feature(surface));
  const auto current = intent(part);
  REQUIRE(current.order.size() == 15);
  CHECK(current.order[13] == "imported");
  CHECK(current.order[14] == "surface");
  CHECK(current.suppressed.empty());
  CHECK(json(checked(deserialize_part_document_from_json(json(part)))) == json(part));
}

TEST_CASE("Versioned history survives result face prelude recursion and Project embedding",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::base();
  const auto reference = checked(ResultFaceReference::create(FeatureId("base"), BodyId("body.base"),
                                                             {{FeatureId("base"), "cap.end"}}));
  REQUIRE(part.add_result_face_workplane(
      checked(ResultFaceWorkplane::create(DatumPlaneId("result.support"), "Support", reference))));
  fixture::rectangle(part, "consumer", "result.support");
  second(part, "consumer");
  auto edit = intent(part);
  edit.suppressed = {"second"};
  REQUIRE(part.apply_history_intent(edit));
  const auto text = json(part);
  const auto restored = checked(deserialize_part_document_from_json(text));
  CHECK(intent(restored) == edit);
  CHECK(json(restored) == text);
  CHECK(restored.dependency_graph().has_dependency("base", "result.support"));
  CHECK(restored.dependency_graph().has_dependency("result.support", "consumer"));
  CHECK(json(checked(deserialize_part_document_from_json(
            text, PartDocumentReadMode::ReferenceRepair))) == text);
  auto project = checked(
      Project::create(DocumentId("project"), "Project",
                      checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"))));
  REQUIRE(project.add_part_document(part));
  const auto saved = checked(serialize_project_to_json(project));
  const auto opened = checked(deserialize_project_from_json(saved));
  REQUIRE(opened.part_documents().size() == 1);
  CHECK(intent(opened.part_documents().front()) == edit);
  CHECK(checked(serialize_project_to_json(opened)) == saved);
}

TEST_CASE("Typed creator dependencies reject an unavailable Body before replaying its modifiers",
          "[core][parity-b146][history-intent]") {
  auto part = fixture::all_families();
  auto edit = intent(part);
  std::erase(edit.order, "base");
  edit.order.push_back("base");
  const auto before = json(part);
  const auto diagnostics = part.diagnose_history_intent(edit);
  REQUIRE(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
    return diagnostic.code == PartHistoryDiagnosticCode::DependencyAfterConsumer &&
           diagnostic.producer_id == "base" && diagnostic.consumer_id == "translate";
  }));
  CHECK_FALSE(part.apply_history_intent(edit));
  CHECK(json(part) == before);
}

TEST_CASE("Independent executable families can cross the historical family loading order",
          "[core][parity-b146][history-intent]") {
  const std::string family = GENERATE("revolve", "sweep", "loft", "surface");
  auto part = fixture::all_families();
  auto edit = intent(part);
  std::erase(edit.order, family);
  edit.order.insert(edit.order.begin() + 1, family);
  REQUIRE(part.apply_history_intent(edit));
  CHECK(intent(part) == edit);
  const auto text = json(part);
  const auto reopened = checked(deserialize_part_document_from_json(text));
  CHECK(intent(reopened) == edit);
  CHECK(json(reopened) == text);
}

TEST_CASE("A modifying operation with a distinct result Body is a required output creator",
          "[core][parity-b146][history-intent]") {
  const auto mode = GENERATE(FeatureBodyOperationMode::Join, FeatureBodyOperationMode::Cut,
                             FeatureBodyOperationMode::Intersect);
  auto part = fixture::native_base();
  REQUIRE(part.add_body(checked(Body::create(BodyId("body.copy"), "Copy", BodyKind::Solid))));
  const auto operation = checked(Feature::create_additive_extrude(
      FeatureId("copy"), "Copy", SketchId("profile"), ParameterId("depth")));
  const auto context =
      checked(FeatureBodyResultContext::create(mode, BodyId("body.base"), BodyId("body.copy")));
  REQUIRE(part.add_feature(checked(operation.with_body_result_context(context))));
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("copy.move"), BodyId("body.copy"), BodyTransformCoordinateSpace::World,
      std::nullopt, {1, 0, 0}, false, false))));
  auto edit = intent(part);
  REQUIRE(edit.order == std::vector<std::string>{"base", "copy", "copy.move"});
  std::swap(edit.order[1], edit.order[2]);
  const auto diagnostics = part.diagnose_history_intent(edit);
  REQUIRE(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
    return diagnostic.code == PartHistoryDiagnosticCode::DependencyAfterConsumer &&
           diagnostic.producer_id == "copy" && diagnostic.consumer_id == "copy.move";
  }));
  CHECK_FALSE(part.with_history_intent(edit));
}

TEST_CASE("Every executable family appends as active while explicit history already exists",
          "[core][parity-b146][history-intent]") {
  // Begin explicit history at the imported base, before adding any native family.
  auto part = fixture::all_families(true);
  REQUIRE(part.authored_history_intent());
  CHECK(intent(part).order == std::vector<std::string>{"imported", "base", "translate", "revolve",
                                                       "sweep", "loft", "surface", "linear",
                                                       "circular", "mirror", "fillet", "chamfer",
                                                       "shell", "draft", "boolean"});
  CHECK(intent(part).suppressed.empty());
  auto edit = intent(part);
  edit.suppressed = {"surface"};
  REQUIRE(part.apply_history_intent(edit));
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("late.move"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {1, 0, 0}, false, false))));
  CHECK(intent(part).order.back() == "late.move");
  CHECK(intent(part).suppressed == std::vector<std::string>{"surface"});
  CHECK(checked(part.executable_history()).back().state == PartFeatureState::Active);
  const auto text = json(part);
  CHECK(json(checked(deserialize_part_document_from_json(text))) == text);
}
