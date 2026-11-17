#include "../support/part_history_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/core/sketch_text_json.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <nlohmann/json.hpp>
using namespace blcad;
namespace f = blcad::test::history_fixture;
namespace {
PartDocument part() {
  auto p = f::native_base();
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("text.height"), "Text height", f::checked(Quantity::length_mm(4))))));
  return p;
}
FaceReference support() {
  return f::checked(FaceReference::create_planar(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
}
EmbossIntent intent(bool text = false) {
  EmbossSource source =
      f::checked(ProfileRegionReference::create(SketchId("profile"), ProfileId("profile.profile")));
  if (text)
    source = EmbossTextSource{
        f::checked(SketchText::create(SketchTextId("label"), SketchId("profile"), "BO ${n}",
                                      "Barlow", ParameterId("text.height"), {-5, 0}, std::nullopt,
                                      {{"n", ParameterId("count"), 0}})),
        *bundled_outline_font("Barlow")};
  return {std::move(source),
          {support()},
          EmbossOperation::Raise,
          ParameterId("radius"),
          EmbossMapping::Planar,
          {},
          0,
          std::nullopt};
}
SketchDrivenFeature feature(EmbossIntent e = intent()) {
  return f::checked(SketchDrivenFeature::create(FeatureId("emboss"), "Label", BodyId("body.base"),
                                                SketchDrivenDefinition{std::move(e)}));
}
std::string saved(const PartDocument& p) {
  return f::checked(serialize_part_document_to_json(p));
}
} // namespace
TEST_CASE("Emboss and Engrave share typed normal feature intent and pinned text authority",
          "[core][emboss-intent][parity-b182]") {
  auto e = intent(true);
  auto record = feature(e);
  CHECK(record.kind_name() == "Emboss");
  CHECK(record.parameter_dependencies() == std::vector<ParameterId>{ParameterId("radius"),
                                                                    ParameterId("text.height"),
                                                                    ParameterId("count")});
  e.operation = EmbossOperation::Recess;
  CHECK(feature(e).kind_name() == "Engrave");
  const auto& text =
      std::get<EmbossTextSource>(std::get<EmbossIntent>(record.definition().intent).source);
  CHECK(text.text.template_text() == "BO ${n}");
  CHECK(text.font.asset == "Barlow-Regular.ttf");
  CHECK(text.font.sha256.size() == 64);
  CHECK_FALSE(bundled_outline_font("ambient font"));
  CHECK(bundled_outline_font("IBM Plex Mono"));
  e = intent();
  e.supports.push_back(e.supports.front());
  CHECK_FALSE(SketchDrivenFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                          SketchDrivenDefinition{e}));
  e = intent(true);
  std::get<EmbossTextSource>(e.source).font.sha256 = std::string(64, '0');
  CHECK_FALSE(SketchDrivenFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                          SketchDrivenDefinition{e}));
  e = intent();
  e.anchor_mm.x = std::numeric_limits<double>::infinity();
  CHECK_FALSE(SketchDrivenFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                          SketchDrivenDefinition{e}));
  e = intent();
  e.mapping = static_cast<EmbossMapping>(100);
  CHECK_FALSE(SketchDrivenFeature::create(FeatureId("bad"), "Bad", BodyId("body.base"),
                                          SketchDrivenDefinition{e}));
}
TEST_CASE("Emboss parameter and owned source edits publish atomically through normal graph",
          "[core][emboss-intent][parity-b182]") {
  auto p = part();
  const auto before = saved(p);
  REQUIRE(p.add_sketch_driven_feature(feature(intent(true))));
  CHECK(p.dependency_graph().has_dependency("text.height", "emboss"));
  CHECK(p.dependency_graph().has_dependency("count", "emboss"));
  CHECK(p.dependency_graph().has_dependency("base", "emboss"));
  CHECK(p.dependency_graph().has_dependency("emboss", "body:body.base"));
  const auto owned = saved(p);
  CHECK_FALSE(p.set_parameter_formula(ParameterId("text.height"), "height - height"));
  CHECK(saved(p) == owned);
  CHECK_FALSE(p.set_parameter_formula(ParameterId("radius"), "height - 2 * height"));
  CHECK(saved(p) == owned);
  auto e = intent();
  e.orientation = ParameterId("radius");
  CHECK_FALSE(p.update_sketch_driven_feature(feature(e)));
  CHECK(saved(p) == owned);
  REQUIRE(p.set_parameter_value(ParameterId("count"), f::checked(Quantity::count(12))));
  const auto& source = std::get<EmbossTextSource>(
      std::get<EmbossIntent>(p.find_sketch_driven_feature(FeatureId("emboss"))->definition().intent)
          .source);
  CHECK(f::checked(source.text.resolve(p)).text == "BO 12");
  REQUIRE(p.update_sketch_driven_feature(feature()));
  CHECK_FALSE(p.dependency_graph().has_dependency("count", "emboss"));
  REQUIRE(p.remove_sketch_driven_feature(FeatureId("emboss")));
  CHECK(p.sketch_driven_features().empty());
}
TEST_CASE("Emboss V15 persistence retains embedded text source bindings and strict mapping records",
          "[core][emboss-intent][parity-b182]") {
  auto p = part();
  const auto legacy = saved(p);
  REQUIRE(p.add_sketch_driven_feature(feature(intent(true))));
  const auto text = saved(p);
  auto json = nlohmann::json::parse(text);
  CHECK(json["version"] == 15);
  CHECK(json["sketch_driven_features"][0]["mode"] == "emboss");
  CHECK(f::checked(serialize_part_document_to_json(
            f::checked(deserialize_part_document_from_json(text)))) == text);
  for (const auto* field : {"mapping", "mode"}) {
    auto bad = json;
    bad["sketch_driven_features"][0][field] = "unknown";
    CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  }
  auto bad = json;
  bad["sketch_driven_features"][0]["source"]["font"]["asset"] = "../../system.ttf";
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  bad = json;
  bad["sketch_driven_features"][0]["source"]["text"]["extra"] = 1;
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  bad = json;
  bad["sketch_driven_features"][0]["extra"] = 1;
  CHECK_FALSE(deserialize_part_document_from_json(bad.dump()));
  REQUIRE(p.remove_sketch_driven_feature(FeatureId("emboss")));
  CHECK(saved(p) == legacy);
}
TEST_CASE("Emboss charts preserve raw authored anchors and seams without inferred topology",
          "[core][emboss-intent][parity-b182]") {
  auto p = part();
  auto e = intent();
  e.mapping = EmbossMapping::Conical;
  e.anchor_mm = {1e-20, -1.23456789012345};
  e.seam_degrees = 720.00000000001;
  e.supports = {f::checked(FaceReference::create_result(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(ResultFaceReference::create(FeatureId("base"), BodyId("body.base"),
                                             {{FeatureId("base"), "authored.carrier"}}))))};
  REQUIRE(p.add_sketch_driven_feature(feature(e)));
  const auto text = saved(p);
  auto opened = f::checked(deserialize_part_document_from_json(text));
  const auto& restored =
      std::get<EmbossIntent>(opened.sketch_driven_features().front().definition().intent);
  CHECK(restored.anchor_mm == e.anchor_mm);
  CHECK(restored.seam_degrees == e.seam_degrees);
  CHECK(restored.mapping == e.mapping);
  CHECK(saved(opened) == text);
}
TEST_CASE("Emboss history suppression and foreign support failures retain the owned model",
          "[core][emboss-intent][parity-b182]") {
  auto p = part();
  REQUIRE(p.add_sketch_driven_feature(feature()));
  REQUIRE(p.apply_history_intent({{"base", "emboss"}, {"emboss"}}));
  CHECK(f::checked(p.executable_history()).back().family == PartFeatureFamily::SketchDriven);
  CHECK(f::checked(p.create_history_evaluation_plan()).entries.back().state ==
        PartHistoryExecutionState::Suppressed);
  const auto before = saved(p);
  CHECK_FALSE(p.apply_history_intent({{"emboss", "base"}, {}}));
  CHECK(saved(p) == before);
  auto e = intent();
  e.supports = {f::checked(FaceReference::create_result(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(ResultFaceReference::create(FeatureId("base"), BodyId("foreign"),
                                             {{FeatureId("base"), "top"}}))))};
  CHECK_FALSE(p.update_sketch_driven_feature(feature(e)));
  CHECK(saved(p) == before);
}
