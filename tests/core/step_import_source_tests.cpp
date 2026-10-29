#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>
using namespace blcad;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
struct AssetDirectory {
  std::filesystem::path path;
  AssetDirectory() {
    std::string name = "/tmp/blcad-step-source-XXXXXX";
    auto* created = ::mkdtemp(name.data());
    REQUIRE(created);
    path = created;
  }
  ~AssetDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
  void write(std::string_view relative, std::string_view content) {
    const auto target = path / relative;
    std::filesystem::create_directories(target.parent_path());
    std::ofstream output(target, std::ios::binary);
    REQUIRE(output);
    output.write(content.data(), content.size());
    output.close();
    REQUIRE(output);
  }
};
PartDocument part() {
  return checked(PartDocument::create(DocumentId("part"), "Source host"));
}
Project project() {
  return checked(
      Project::create(DocumentId("project"), "Project source host",
                      checked(AssemblyDocument::create(DocumentId("assembly"), "Root"))));
}
StepImportSource source(std::string id = "source", std::string path = "assets/part.step",
                        StepImportMode mode = StepImportMode::Reference,
                        std::string_view bytes = "abc") {
  return checked(StepImportSource::create(StepImportSourceId(std::move(id)), "STEP asset",
                                          std::move(path), checked(step_import_sha256(bytes)),
                                          mode));
}
std::string json(const PartDocument& host) {
  return checked(serialize_part_document_to_json(host));
}
std::string json(const Project& host) {
  return checked(serialize_project_to_json(host));
}
} // namespace
TEST_CASE("STEP source canonical identity requires explicit mode digest and portable path",
          "[core][step-import-source]") {
  auto value = checked(StepImportSource::create(StepImportSourceId("source"), "Bracket",
                                                "./assets//bracket.step", std::string(64, 'A'),
                                                StepImportMode::EditableBody));
  CHECK(value.project_relative_asset_path() == "assets/bracket.step");
  CHECK(value.sha256_digest() == std::string(64, 'a'));
  CHECK(value.mode() == StepImportMode::EditableBody);
  CHECK(to_string(value.mode()) == "editable_body");
  CHECK(to_string(StepImportMode::Reference) == "reference");
  for (const auto& path : std::vector<std::string>{
           "", ".", "..", "../part.step", "assets/../part.step", "/part.step", "C:/part.step",
           "assets\\part.step", "assets/", "assets/.", "assets/con.step", "assets/LPT1.step",
           "assets/trailing.thing.", "assets/name ", "assets/a:b.step", "assets/a?b.step",
           "assets/new\nline.step", std::string("assets/nul\0.step", 16)}) {
    CAPTURE(path);
    CHECK_FALSE(StepImportSource::create(StepImportSourceId("source"), "Name", path,
                                         value.sha256_digest(), value.mode()));
  }
  for (const auto& digest :
       {std::string(), std::string(63, 'a'), std::string(65, 'a'), std::string(64, 'x')})
    CHECK_FALSE(StepImportSource::create(StepImportSourceId("source"), "Name", "asset.step", digest,
                                         value.mode()));
  CHECK_FALSE(StepImportSource::create(StepImportSourceId(""), "Name", "a.step",
                                       value.sha256_digest(), value.mode()));
  CHECK_FALSE(StepImportSource::create(StepImportSourceId("source"), " ", "a.step",
                                       value.sha256_digest(), value.mode()));
  CHECK_FALSE(StepImportSource::create(StepImportSourceId("source"), "Name", "a.step",
                                       value.sha256_digest(), static_cast<StepImportMode>(-1)));
}
TEST_CASE("STEP registries reject duplicate canonical sources and stale mutations atomically",
          "[core][step-import-source]") {
  const auto exercise = [](auto host) {
    const auto original = source();
    const auto legacy = json(host);
    REQUIRE(host.add_step_import_source(original));
    const auto accepted = json(host);
    CHECK_FALSE(host.add_step_import_source(source("source", "assets/other.step")));
    CHECK_FALSE(host.add_step_import_source(
        source("other", "assets//./part.step", StepImportMode::EditableBody)));
    CHECK(json(host) == accepted);
    CHECK_FALSE(host.remove_step_import_source(StepImportSourceId("unknown")));
    CHECK_FALSE(host.refresh_step_import_source(original, "malformed"));
    const auto stale = checked(StepImportSource::create(original.id(), "Other name",
                                                        original.project_relative_asset_path(),
                                                        original.sha256_digest(), original.mode()));
    CHECK_FALSE(host.remove_step_import_source(stale));
    CHECK_FALSE(host.refresh_step_import_source(stale, checked(step_import_sha256("new"))));
    CHECK(json(host) == accepted);
    REQUIRE(host.refresh_step_import_source(original, checked(step_import_sha256("new"))));
    CHECK(host.find_step_import_source(original.id())->sha256_digest() ==
          checked(step_import_sha256("new")));
    CHECK_FALSE(host.refresh_step_import_source(original, original.sha256_digest()));
    REQUIRE(host.remove_step_import_source(original.id()));
    CHECK(json(host) == legacy);
  };
  SECTION("Part") {
    exercise(part());
  }
  SECTION("Project") {
    exercise(project());
  }
}
TEST_CASE("STEP JSON preserves legacy bytes and independent Project and embedded Part registries",
          "[core][step-import-json]") {
  auto standalone = part();
  const auto legacy_part = json(standalone);
  auto assembly = project();
  const auto legacy_project = json(assembly);
  CHECK_FALSE(nlohmann::json::parse(legacy_part).contains("step_import_sources"));
  CHECK_FALSE(nlohmann::json::parse(legacy_project).contains("step_import_sources"));
  CHECK(json(checked(deserialize_part_document_from_json(legacy_part))) == legacy_part);
  CHECK(json(checked(deserialize_project_from_json(legacy_project))) == legacy_project);
  REQUIRE(standalone.add_step_import_source(source()));
  REQUIRE(standalone.add_step_import_source(
      source("editable", "assets/edit.step", StepImportMode::EditableBody)));
  REQUIRE(assembly.add_part_document(standalone));
  REQUIRE(assembly.add_step_import_source(
      source("source", "assets/part.step", StepImportMode::EditableBody)));
  const auto encoded = json(assembly);
  const auto parsed = nlohmann::json::parse(encoded);
  REQUIRE(parsed.at("step_import_sources").size() == 1);
  REQUIRE(parsed.at("parts").at(0).at("step_import_sources").size() == 2);
  CHECK(parsed.at("step_import_sources").at(0).at("mode") == "editable_body");
  CHECK(parsed.at("parts").at(0).at("step_import_sources").at(0).at("mode") == "reference");
  const auto restored = checked(deserialize_project_from_json(encoded));
  CHECK(restored.step_import_sources() == assembly.step_import_sources());
  CHECK(restored.part_documents().front().step_import_sources() ==
        standalone.step_import_sources());
  CHECK(json(restored) == encoded);
  CHECK(json(checked(deserialize_part_document_from_json(json(standalone)))) == json(standalone));
  auto empty = nlohmann::json::parse(legacy_part);
  empty["step_import_sources"] = nlohmann::json::array();
  CHECK(json(checked(deserialize_part_document_from_json(empty.dump()))) == legacy_part);
}
TEST_CASE("STEP source JSON rejects malformed mode types paths digests and duplicate entries",
          "[core][step-import-json]") {
  const bool project_host = GENERATE(false, true);
  auto p = part();
  auto a = project();
  REQUIRE(p.add_step_import_source(source()));
  REQUIRE(a.add_step_import_source(source()));
  auto root = nlohmann::json::parse(project_host ? json(a) : json(p));
  const auto rejects = [&](const auto& candidate) {
    if (project_host)
      CHECK_FALSE(deserialize_project_from_json(candidate.dump()));
    else
      CHECK_FALSE(deserialize_part_document_from_json(candidate.dump()));
  };
  for (const auto& invalid :
       {nlohmann::json(nullptr), nlohmann::json::object(), nlohmann::json("array")}) {
    auto copy = root;
    copy["step_import_sources"] = invalid;
    rejects(copy);
  }
  for (const char* field : {"id", "name", "project_relative_asset_path", "sha256_digest", "mode"}) {
    auto copy = root;
    copy["step_import_sources"][0].erase(field);
    rejects(copy);
    copy = root;
    copy["step_import_sources"][0][field] = 123;
    rejects(copy);
  }
  for (const auto& [field, value] : std::vector<std::pair<std::string, std::string>>{
           {"mode", "Reference"},
           {"mode", "auto"},
           {"sha256_digest", std::string(64, 'A')},
           {"sha256_digest", std::string(63, 'a')},
           {"project_relative_asset_path", "./assets/part.step"},
           {"project_relative_asset_path", "../escape.step"}}) {
    auto copy = root;
    copy["step_import_sources"][0][field] = value;
    rejects(copy);
  }
  auto copy = root;
  copy["step_import_sources"][0]["step_payload"] = "ISO-10303";
  rejects(copy);
  copy = root;
  copy["step_import_sources"].push_back(copy["step_import_sources"][0]);
  rejects(copy);
  copy["step_import_sources"][1]["id"] = "other";
  rejects(copy);
}
TEST_CASE("STEP SHA256 matches published empty short and multiblock vectors",
          "[core][step-import-freshness]") {
  CHECK(checked(step_import_sha256("")) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(checked(step_import_sha256("abc")) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(checked(step_import_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  CHECK(checked(step_import_sha256(std::string(1000000, 'a'))) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
TEST_CASE("STEP freshness distinguishes missing changed unreadable and matching read-only assets",
          "[core][step-import-freshness]") {
  AssetDirectory directory;
  const auto original = source();
  auto report = checked(inspect_step_import_source(original, directory.path));
  CHECK(report.state() == StepImportFreshness::Missing);
  CHECK_FALSE(report.observed_sha256_digest());
  directory.write("assets/part.step", "abc");
  report = checked(inspect_step_import_source(original, directory.path));
  CHECK(report.state() == StepImportFreshness::Available);
  CHECK(report.observed_sha256_digest() == std::optional(original.sha256_digest()));
  const auto asset = checked(inspect_step_import_asset(directory.path, "./assets//part.step"));
  CHECK(asset.project_relative_asset_path() == "assets/part.step");
  CHECK(asset.observed_sha256_digest() == report.observed_sha256_digest());
  directory.write("assets/part.step", std::string("a\0b", 3));
  report = checked(inspect_step_import_source(original, directory.path));
  CHECK(report.state() == StepImportFreshness::Changed);
  CHECK(report.observed_sha256_digest() ==
        std::optional(checked(step_import_sha256(std::string("a\0b", 3)))));
  REQUIRE(::chmod((directory.path / "assets/part.step").c_str(), 0000) == 0);
  CHECK(checked(inspect_step_import_source(original, directory.path)).state() ==
        StepImportFreshness::Unreadable);
  REQUIRE(::chmod((directory.path / "assets/part.step").c_str(), 0600) == 0);
  std::filesystem::remove(directory.path / "assets/part.step");
  std::filesystem::create_directory(directory.path / "assets/part.step");
  CHECK(checked(inspect_step_import_source(original, directory.path)).state() ==
        StepImportFreshness::Unreadable);
  std::filesystem::remove(directory.path / "assets/part.step");
  REQUIRE(::mkfifo((directory.path / "assets/part.step").c_str(), 0600) == 0);
  CHECK(checked(inspect_step_import_source(original, directory.path)).state() ==
        StepImportFreshness::Unreadable);
}
TEST_CASE("STEP freshness rejects external and in-root symlinks without escaping the asset root",
          "[core][step-import-freshness]") {
  AssetDirectory directory, external;
  external.write("outside.step", "secret");
  directory.write("inside.step", "abc");
  std::filesystem::create_directory(directory.path / "assets");
  std::filesystem::create_symlink(external.path / "outside.step",
                                  directory.path / "assets/part.step");
  auto report = checked(inspect_step_import_source(source(), directory.path));
  CHECK(report.state() == StepImportFreshness::Unreadable);
  CHECK_FALSE(report.observed_sha256_digest());
  std::filesystem::remove(directory.path / "assets/part.step");
  std::filesystem::create_symlink(directory.path / "inside.step",
                                  directory.path / "assets/part.step");
  CHECK(checked(inspect_step_import_source(source(), directory.path)).state() ==
        StepImportFreshness::Unreadable);
  std::filesystem::create_directory_symlink(external.path, directory.path / "linked");
  CHECK(checked(inspect_step_import_asset(directory.path, "linked/outside.step")).state() ==
        StepImportFreshness::Unreadable);
  std::filesystem::create_directory_symlink(directory.path, external.path / "root-link");
  CHECK(checked(inspect_step_import_asset(external.path / "root-link", "inside.step")).state() ==
        StepImportFreshness::Unreadable);
  CHECK_FALSE(inspect_step_import_asset(directory.path, "../outside.step"));
  CHECK_FALSE(inspect_step_import_asset(directory.path, external.path.string() + "/outside.step"));
}
TEST_CASE(
    "Verified STEP snapshots retain exact accepted binary bytes without changing source intent",
    "[core][step-import-freshness][step-import-snapshot]") {
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  const auto size = GENERATE(std::size_t(0), std::size_t(3), std::size_t(131079));
  CAPTURE(mode, size);
  std::string bytes(size, '\0');
  for (std::size_t i = 0; i < size; ++i)
    bytes[i] = static_cast<char>(i % 256);
  AssetDirectory directory;
  directory.write("assets/part.step", bytes);
  const auto original = source("source", "assets/part.step", mode, bytes);
  auto standalone = part();
  auto assembly = project();
  REQUIRE(standalone.add_step_import_source(original));
  REQUIRE(assembly.add_step_import_source(original));
  const auto initial_part = json(standalone);
  const auto initial_project = json(assembly);
  struct stat before {
  }, after{};
  REQUIRE(::stat((directory.path / "assets/part.step").c_str(), &before) == 0);
  const auto snapshot = checked(read_verified_step_import_source(original, directory.path));
  CHECK(snapshot == bytes);
  CHECK(snapshot.size() == size);
  REQUIRE(::stat((directory.path / "assets/part.step").c_str(), &after) == 0);
  CHECK(before.st_dev == after.st_dev);
  CHECK(before.st_ino == after.st_ino);
  CHECK(before.st_mode == after.st_mode);
  CHECK(before.st_size == after.st_size);
  CHECK(before.st_mtim.tv_sec == after.st_mtim.tv_sec);
  CHECK(before.st_mtim.tv_nsec == after.st_mtim.tv_nsec);
  CHECK(before.st_ctim.tv_sec == after.st_ctim.tv_sec);
  CHECK(before.st_ctim.tv_nsec == after.st_ctim.tv_nsec);
  CHECK(checked(inspect_step_import_source(original, directory.path)).state() ==
        StepImportFreshness::Available);
  directory.write("assets/part.step", "replacement after snapshot");
  CHECK(snapshot == bytes);
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  std::filesystem::remove(directory.path / "assets/part.step");
  CHECK(snapshot == bytes);
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  CHECK(standalone.step_import_sources() == std::vector{original});
  CHECK(assembly.step_import_sources() == std::vector{original});
  CHECK(json(standalone) == initial_part);
  CHECK(json(assembly) == initial_project);
}
TEST_CASE(
    "Verified STEP snapshots fail closed for changed missing unreadable and nonregular assets",
    "[core][step-import-freshness][step-import-snapshot]") {
  AssetDirectory directory;
  const auto original = source();
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  directory.write("assets/part.step", "abd");
  const auto mismatch = read_verified_step_import_source(original, directory.path);
  REQUIRE_FALSE(mismatch);
  CHECK(mismatch.error().message() ==
        "STEP source content differs from its registered SHA-256 digest");
  CHECK(checked(inspect_step_import_source(original, directory.path)).state() ==
        StepImportFreshness::Changed);
  directory.write("assets/part.step", "abc");
  REQUIRE(::chmod((directory.path / "assets/part.step").c_str(), 0000) == 0);
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  REQUIRE(::chmod((directory.path / "assets/part.step").c_str(), 0600) == 0);
  CHECK(checked(read_verified_step_import_source(original, directory.path)) == "abc");
  std::filesystem::remove(directory.path / "assets/part.step");
  std::filesystem::create_directory(directory.path / "assets/part.step");
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  std::filesystem::remove(directory.path / "assets/part.step");
  REQUIRE(::mkfifo((directory.path / "assets/part.step").c_str(), 0600) == 0);
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  CHECK_FALSE(read_verified_step_import_source(original, {}));
  CHECK_FALSE(read_verified_step_import_source(original, directory.path / "missing-root"));
  directory.write("not-a-root", "abc");
  CHECK_FALSE(read_verified_step_import_source(original, directory.path / "not-a-root"));
}
TEST_CASE("Verified STEP snapshots reject symlink roots ancestors and asset paths",
          "[core][step-import-freshness][step-import-snapshot]") {
  AssetDirectory directory, external;
  const auto original = source();
  external.write("assets/part.step", "abc");
  directory.write("inside.step", "abc");
  std::filesystem::create_directory(directory.path / "assets");
  std::filesystem::create_symlink(external.path / "assets/part.step",
                                  directory.path / "assets/part.step");
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  std::filesystem::remove(directory.path / "assets/part.step");
  std::filesystem::create_symlink(directory.path / "inside.step",
                                  directory.path / "assets/part.step");
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  std::filesystem::remove_all(directory.path / "assets");
  std::filesystem::create_directory_symlink(external.path / "assets", directory.path / "assets");
  CHECK_FALSE(read_verified_step_import_source(original, directory.path));
  std::filesystem::create_directory_symlink(external.path, directory.path / "root-link");
  CHECK_FALSE(read_verified_step_import_source(original, directory.path / "root-link"));
  CHECK_FALSE(read_verified_step_import_source(original, directory.path / "root-link/unused/.."));
  for (const auto& path :
       {"../assets/part.step", "/assets/part.step", "assets/../part.step", "assets\\part.step"})
    CHECK_FALSE(StepImportSource::create(original.id(), original.name(), path,
                                         original.sha256_digest(), original.mode()));
}
TEST_CASE("STEP explicit refresh rechecks source bytes and full intent on both hosts",
          "[core][step-import-freshness]") {
  const auto exercise = [](auto host) {
    AssetDirectory directory, relocated;
    directory.write("assets/part.step", "abc");
    const auto original = source();
    REQUIRE(host.add_step_import_source(original));
    const auto initial = json(host);
    directory.write("assets/part.step", "changed");
    auto report = checked(inspect_step_import_source(original, directory.path));
    REQUIRE(report.state() == StepImportFreshness::Changed);
    CHECK(json(host) == initial);
    directory.write("assets/part.step", "changed again");
    CHECK_FALSE(refresh_step_import_source(host, directory.path, report));
    CHECK(json(host) == initial);
    report = checked(inspect_step_import_source(original, directory.path));
    relocated.write("assets/part.step", "changed again");
    CHECK_FALSE(refresh_step_import_source(host, relocated.path, report));
    CHECK(json(host) == initial);
    REQUIRE(host.remove_step_import_source(original));
    auto different = checked(StepImportSource::create(
        original.id(), "Changed name", original.project_relative_asset_path(),
        original.sha256_digest(), StepImportMode::EditableBody));
    REQUIRE(host.add_step_import_source(different));
    const auto changed_intent = json(host);
    CHECK_FALSE(refresh_step_import_source(host, directory.path, report));
    CHECK(json(host) == changed_intent);
    REQUIRE(host.remove_step_import_source(different));
    REQUIRE(host.add_step_import_source(original));
    REQUIRE(refresh_step_import_source(host, directory.path, report));
    CHECK(host.find_step_import_source(original.id())->sha256_digest() ==
          checked(step_import_sha256("changed again")));
    CHECK(host.find_step_import_source(original.id())->mode() == original.mode());
    const auto accepted = json(host);
    CHECK_FALSE(refresh_step_import_source(host, directory.path, report));
    CHECK(json(host) == accepted);
    const auto matching = checked(
        inspect_step_import_source(*host.find_step_import_source(original.id()), directory.path));
    CHECK(checked(refresh_step_import_source(host, directory.path, matching)) == 0);
    CHECK(checked(inspect_step_import_source(*host.find_step_import_source(original.id()),
                                             relocated.path))
              .state() == StepImportFreshness::Available);
    std::ifstream input(directory.path / "assets/part.step", std::ios::binary);
    CHECK(std::string(std::istreambuf_iterator<char>(input), {}) == "changed again");
    std::filesystem::remove(directory.path / "assets/part.step");
    CHECK_FALSE(refresh_step_import_source(host, directory.path, matching));
    CHECK(json(host) == accepted);
    std::filesystem::create_symlink(relocated.path / "assets/part.step",
                                    directory.path / "assets/part.step");
    CHECK_FALSE(refresh_step_import_source(host, directory.path, matching));
    CHECK(json(host) == accepted);
    std::filesystem::remove(directory.path / "assets/part.step");
    const auto missing = checked(
        inspect_step_import_source(*host.find_step_import_source(original.id()), directory.path));
    CHECK_FALSE(refresh_step_import_source(host, directory.path, missing));
    CHECK(json(host) == accepted);
  };
  SECTION("Part") {
    exercise(part());
  }
  SECTION("Project") {
    exercise(project());
  }
}
TEST_CASE("STEP registry survives existing Part parameter feature update and remove rebuilds",
          "[core][step-import-source][step-import-json]") {
  auto host = part();
  const auto registered = source();
  REQUIRE(host.add_step_import_source(registered));
  for (const char* id : {"width", "height", "depth"})
    REQUIRE(host.add_parameter(
        checked(Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(10))))));
  REQUIRE(host.add_datum_plane(checked(DatumPlane::xy())));
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Profile", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(host.add_sketch(sketch));
  auto feature = checked(Feature::create_additive_extrude(FeatureId("extrude"), "Extrude",
                                                          sketch.id(), ParameterId("depth")));
  REQUIRE(host.add_feature(feature));
  REQUIRE(host.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(12))));
  auto replacement = checked(Feature::create_additive_extrude(feature.id(), "Updated extrusion",
                                                              sketch.id(), ParameterId("depth")));
  REQUIRE(host.update_feature(replacement));
  CHECK(host.step_import_sources() == std::vector{registered});
  const auto encoded = json(host);
  CHECK(checked(deserialize_part_document_from_json(encoded)).step_import_sources() ==
        std::vector{registered});
  REQUIRE(host.remove_feature(feature.id()));
  CHECK(host.step_import_sources() == std::vector{registered});
}
