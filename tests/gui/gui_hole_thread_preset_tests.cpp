#include "../support/hole_manufacturing_fixture.hpp"
#include "../support/thread_feature_fixture.hpp"
#include "blcad/gui/gui_hole_thread_preset.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace m = blcad::test::manufacturing_fixture;
namespace t = blcad::test::thread_fixture;
namespace {
template <class T> T ok(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string json(const PartDocument& part) {
  return ok(serialize_part_document_to_json(part));
}
nlohmann::json record(const PartDocument& part, FeatureId id) {
  const auto root = nlohmann::json::parse(json(part));
  for (const auto* collection : {"hole_features", "thread_features"})
    for (const auto& item : root.value(collection, nlohmann::json::array()))
      if (item.at("id") == id.value())
        return item;
  FAIL("Missing preset feature");
  return {};
}
HoleFeature other(const HoleFeature& hole, HoleExtent extent) {
  return ok(HoleFeature::create(
      FeatureId("destination"), "Destination name", hole.kind(), hole.target_body(), hole.support(),
      {{HolePlacementId("destination.position"),
        (std::holds_alternative<HoleSlot>(hole.manufacturing()) ? Point2{} : Point2{3, 2})}},
      hole.diameter(), extent, hole.direction(), hole.tip(), hole.manufacturing(), hole.thread()));
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
} // namespace

TEST_CASE("Hole value presets round trip every recipe without transferring document identities",
          "[gui][parity-b157][hole-thread-presets]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
  auto source = m::part(mode);
  const auto before = json(source);
  const auto preset = ok(GuiHoleThreadPresetCommands::capture(source, FeatureId("hole"), "Recipe"));
  CHECK(json(source) == before);
  CHECK(preset.values.id() == DocumentId("preset.document"));
  CHECK_FALSE(preset.values.find_parameter(ParameterId("diameter")));
  CHECK_FALSE(preset.values.find_sketch(SketchId("slot")));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto path = dir.filePath("nested/presets.json");
  CHECK(ok(GuiHoleThreadPresetCommands::save(path, preset)) == 1);
  const auto loaded = ok(GuiHoleThreadPresetCommands::load(path));
  REQUIRE(loaded.size() == 1);
  CHECK(json(loaded.front().values) == json(preset.values));
  auto destination = m::seed();
  const auto feature =
      other(*source.find_hole_feature(FeatureId("hole")), HoleBlindExtent{ParameterId("blind")});
  REQUIRE(destination.add_hole_feature(feature));
  const auto candidate = ok(GuiHoleThreadPresetCommands::apply(loaded.front(), destination,
                                                               feature.id(), "current.preset"));
  const auto& result = *candidate.find_hole_feature(feature.id());
  CHECK(result.id() == feature.id());
  CHECK(result.name() == feature.name());
  CHECK(result.target_body() == feature.target_body());
  CHECK(record(candidate, feature.id()).at("support") ==
        record(destination, feature.id()).at("support"));
  CHECK(record(candidate, feature.id()).at("placements") ==
        record(destination, feature.id()).at("placements"));
  CHECK(result.manufacturing().index() == source.hole_features().front().manufacturing().index());
  CHECK(result.extent().index() == source.hole_features().front().extent().index());
  CHECK(result.tip().index() == source.hole_features().front().tip().index());
  CHECK(std::get<ParameterId>(result.diameter()) != ParameterId("diameter"));
  CHECK(candidate.find_parameter(std::get<ParameterId>(result.diameter()))->value().millimeters() ==
        2);
  if (mode == 8)
    CHECK(record(candidate, feature.id()).at("manufacturing").at("profile") ==
          record(destination, feature.id()).at("manufacturing").at("profile"));
  if (mode == 4) {
    const auto& steps = std::get<HoleStepped>(result.manufacturing()).steps;
    CHECK(steps.front().id == "outer");
    CHECK(steps.back().id == "inner");
  }
}

TEST_CASE("Hole preset limits stay explicit and unsupported current roles fail atomically",
          "[gui][parity-b157][hole-thread-presets]") {
  const int mode = GENERATE(0, 1, 2, 3, 4, 5);
  const auto face = ok(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      ok(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Bottom))));
  const std::vector<HoleExtent> extents{HoleBlindExtent{ParameterId("blind")},
                                        HoleThroughAllExtent{},
                                        HoleToNextExtent{},
                                        HoleSymmetricExtent{ParameterId("blind")},
                                        HoleToFaceExtent{face},
                                        HoleUpToBodyExtent{BodyId("body.limit")}};
  auto source = m::seed();
  REQUIRE(source.add_body(ok(Body::create(BodyId("body.limit"), "Limit", BodyKind::Solid))));
  REQUIRE(source.add_hole_feature(m::feature(0, extents[mode])));
  const auto preset = ok(GuiHoleThreadPresetCommands::capture(source, FeatureId("hole"), "Extent"));
  auto destination = source;
  auto current_face = ok(FaceReference::create_planar(
      PartFeatureInputRole::HoleLimitFace,
      ok(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  REQUIRE(destination.add_body(
      ok(Body::create(BodyId("body.current.limit"), "Current", BodyKind::Solid))));
  const HoleExtent extent = mode == 4 ? HoleExtent{HoleToFaceExtent{current_face}}
                            : mode == 5
                                ? HoleExtent{HoleUpToBodyExtent{BodyId("body.current.limit")}}
                                : HoleExtent{HoleThroughAllExtent{}};
  const auto feature = other(source.hole_features().front(), extent);
  REQUIRE(destination.add_hole_feature(feature));
  const auto before = json(destination);
  const auto candidate =
      ok(GuiHoleThreadPresetCommands::apply(preset, destination, feature.id(), "loaded"));
  const auto& result = *candidate.find_hole_feature(feature.id());
  CHECK(result.extent().index() == extents[mode].index());
  if (mode == 4)
    CHECK(record(candidate, feature.id()).at("extent") ==
          record(destination, feature.id()).at("extent"));
  if (mode == 5)
    CHECK(std::get<HoleUpToBodyExtent>(result.extent()).body == BodyId("body.current.limit"));
  CHECK(json(destination) == before);
  if (mode >= 4) {
    REQUIRE(destination.update_hole_feature(
        other(source.hole_features().front(), HoleThroughAllExtent{})));
    const auto failed =
        GuiHoleThreadPresetCommands::apply(preset, destination, feature.id(), "failed");
    CHECK_FALSE(failed);
    CHECK(failed.error().message().find("Choose") != std::string::npos);
  }
}

TEST_CASE("Preset values snapshot expressions and preserve dimensional aliasing",
          "[gui][parity-b157][hole-thread-presets]") {
  auto source = m::seed();
  REQUIRE(source.add_parameter(ok(
      Parameter::create_expression(ParameterId("expression"), "Expression", ParameterType::Length,
                                   "diameter", ok(Quantity::length_mm(2))))));
  const auto base = m::feature(0);
  auto hole = ok(HoleFeature::create(base.id(), base.name(), base.kind(), base.target_body(),
                                     base.support(), base.placements(), ParameterId("expression"),
                                     HoleBlindExtent{ParameterId("expression")}, base.direction(),
                                     base.tip()));
  REQUIRE(source.add_hole_feature(hole));
  const auto preset = ok(GuiHoleThreadPresetCommands::capture(source, hole.id(), "Aliased"));
  const auto& saved = preset.values.hole_features().front();
  const auto id = std::get<ParameterId>(saved.diameter());
  CHECK(std::get<HoleBlindExtent>(saved.extent()).depth == id);
  CHECK_FALSE(preset.values.find_parameter(id)->formula());
  CHECK(preset.values.find_parameter(id)->value().millimeters() == 2);
  REQUIRE(source.set_parameter_value(ParameterId("diameter"), ok(Quantity::length_mm(3))));
  const auto result = ok(GuiHoleThreadPresetCommands::apply(preset, source, hole.id(), "new"));
  const auto& applied = result.hole_features().front();
  const auto copied = std::get<ParameterId>(applied.diameter());
  CHECK(copied == std::get<HoleBlindExtent>(applied.extent()).depth);
  CHECK(result.find_parameter(copied)->value().millimeters() == 2);
  CHECK_FALSE(result.find_parameter(copied)->formula());
  CHECK(result.find_parameter(ParameterId("expression"))->formula());
}

TEST_CASE(
    "Thread presets retain full custom or pinned definitions and import only required revisions",
    "[gui][parity-b157][hole-thread-presets]") {
  const bool internal = GENERATE(false, true);
  const bool standard = GENERATE(false, true);
  auto source = t::pair_part();
  const auto definition = t::definition(
      standard ? ThreadProfile{t::selected("r2", true)} : ThreadProfile{t::custom(true)}, true,
      ThreadHandedness::Left, ThreadRepresentation::Modeled);
  FeatureId id(internal ? "internal" : "external");
  if (internal)
    REQUIRE(source.add_hole_feature(t::internal(definition)));
  else
    REQUIRE(source.add_thread_feature(t::external(definition)));
  const auto preset = ok(GuiHoleThreadPresetCommands::capture(source, id, "Full definition"));
  CHECK(preset.values.engineering_resources().entries().size() == (standard ? 1 : 0));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto path = dir.filePath("presets.json");
  REQUIRE(GuiHoleThreadPresetCommands::save(path, preset));
  const auto loaded = ok(GuiHoleThreadPresetCommands::load(path));
  auto destination = t::pair_part("other.document", false);
  if (internal)
    REQUIRE(destination.add_hole_feature(t::internal()));
  else
    REQUIRE(destination.add_thread_feature(t::external()));
  const auto result =
      ok(GuiHoleThreadPresetCommands::apply(loaded.front(), destination, id, "new"));
  const auto& thread =
      internal ? *result.find_hole_feature(id)->thread() : result.find_thread_feature(id)->thread();
  CHECK(thread.handedness() == ThreadHandedness::Left);
  CHECK(thread.representation() == ThreadRepresentation::Modeled);
  CHECK(thread.start());
  CHECK(result.find_parameter(thread.length())->value().millimeters() == 4);
  CHECK(result.find_parameter(*thread.start())->value().millimeters() == 1);
  CHECK(result.engineering_resources() == preset.values.engineering_resources());
  if (standard)
    CHECK(record(result, id).at("thread").at("profile") ==
          record(source, id).at("thread").at("profile"));
  else {
    const auto& custom = std::get<ThreadParameterProfile>(thread.profile());
    CHECK(custom.system == t::custom().system);
    CHECK(custom.designation == t::custom().designation);
    CHECK(custom.tolerance == t::custom().tolerance);
    CHECK(result.find_parameter(custom.major_diameter)->value().millimeters() == 10);
    CHECK(result.find_parameter(*custom.taper_angle)->value().degrees() == -2);
  }
}

TEST_CASE("Preset storage preserves other kinds and rejects corrupt or conflicting snapshots",
          "[gui][parity-b157][hole-thread-presets]") {
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto path = dir.filePath("presets.json");
  CHECK(ok(GuiHoleThreadPresetCommands::load(path)).empty());
  auto source = m::seed();
  REQUIRE(source.add_engineering_resource(m::table()));
  REQUIRE(source.add_hole_feature(m::feature(1, HoleThroughAllExtent{}, true)));
  auto hole = ok(GuiHoleThreadPresetCommands::capture(source, FeatureId("hole"), "Shared name"));
  auto threaded = t::pair_part();
  REQUIRE(threaded.add_thread_feature(t::external()));
  const auto thread =
      ok(GuiHoleThreadPresetCommands::capture(threaded, FeatureId("external"), "Shared name"));
  CHECK(ok(GuiHoleThreadPresetCommands::save(path, hole)) == 1);
  CHECK(ok(GuiHoleThreadPresetCommands::save(path, thread)) == 2);
  CHECK(ok(GuiHoleThreadPresetCommands::save(path, hole)) == 2);
  CHECK(ok(GuiHoleThreadPresetCommands::remove(path, hole.name, true)) == 1);
  CHECK_FALSE(ok(GuiHoleThreadPresetCommands::load(path)).front().hole());
  REQUIRE(GuiHoleThreadPresetCommands::save(path, hole));
  auto conflict = m::part(0);
  REQUIRE(conflict.add_engineering_resource(m::table("r1", false, 6)));
  const auto before = json(conflict);
  CHECK_FALSE(GuiHoleThreadPresetCommands::apply(hole, conflict, FeatureId("hole"), "conflict"));
  CHECK(json(conflict) == before);
  QFile damaged(path);
  REQUIRE(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
  REQUIRE(damaged.write("{broken}") == 8);
  damaged.close();
  CHECK_FALSE(GuiHoleThreadPresetCommands::load(path));
  CHECK_FALSE(GuiHoleThreadPresetCommands::save(path, hole));
  CHECK_FALSE(GuiHoleThreadPresetCommands::remove(path, hole.name, true));
  CHECK(bytes(path) == "{broken}");
}
