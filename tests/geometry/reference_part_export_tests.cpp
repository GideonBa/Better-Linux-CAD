#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "step_export_transaction.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
std::string bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}
void write(const std::filesystem::path& path, const std::string& content) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << content;
  REQUIRE(stream.good());
}
struct Fixture {
  std::filesystem::path root;
  GeometryShape shape;
  Fixture() {
    char pattern[] = "/tmp/blcad-reference-export-XXXXXX";
    const auto* created = ::mkdtemp(pattern);
    REQUIRE(created != nullptr);
    root = created;
    shape = checked(RectangleExtrusionAdapter{}.make_extruded_rectangle(
        checked(Quantity::length_mm(3)), checked(Quantity::length_mm(4)),
        checked(Quantity::length_mm(5))));
    REQUIRE(StepExporter{}.write_step(shape, source_path().string()));
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  std::filesystem::path source_path() const {
    return root / "supplier.step";
  }
  StepImportSource source() const {
    return checked(StepImportSource::create(
        StepImportSourceId("source"), "Supplier", "supplier.step",
        checked(step_import_sha256(bytes(source_path()))), StepImportMode::Reference));
  }
  PartDocument reference() const {
    const auto captured = source();
    const auto geometry = checked(StepImportReader{}.read(captured, root));
    REQUIRE(geometry.products.size() == 1);
    const auto catalog = checked(
        StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), geometry, 0));
    std::vector<ImportedBodyOutput> outputs;
    for (const auto& entry : catalog.entries())
      if (entry.descriptor.kind == ImportedTopologyKind::Body)
        outputs.push_back({entry.id, BodyId("body"), BodyKind::Solid});
    REQUIRE(outputs.size() == 1);
    return checked(PartDocument::create_reference(
        DocumentId("reference"), "Reference", captured, catalog,
        checked(ImportedPartDefinition::create(captured.id(), catalog.id(), outputs))));
  }
  Project project(const PartDocument& part) const {
    auto assembly = checked(AssemblyDocument::create(DocumentId("assembly"), "Assembly"));
    REQUIRE(assembly.add_member_part(part.id()));
    for (const auto* id : {"first", "second"})
      REQUIRE(assembly.add_component_instance(checked(ComponentInstance::create(
          ComponentInstanceId(id), id, part.id(), ComponentVisibility::Visible,
          ComponentSuppressionState::Active, ComponentGroundingState::Free, {}))));
    auto result = checked(Project::create(DocumentId("project"), "Project", assembly));
    REQUIRE(result.add_part_document(part));
    return result;
  }
  void no_temporary_files() const {
    for (const auto& entry : std::filesystem::directory_iterator(root))
      CHECK_FALSE(entry.path().filename().string().starts_with(".blcad-step-export-"));
  }
};
enum class Mode { Part, Flat, Structured };
Result<std::size_t> export_document(Mode mode, const PartDocument& part, const ShapeCache& cache,
                                    const Project& project, StepImportTopologyContext* context,
                                    const std::filesystem::path& output) {
  switch (mode) {
  case Mode::Part: {
    auto result = StepExporter{context}.write_part_step(part, cache, output.string());
    return result ? Result<std::size_t>::success(result.value().written_bytes)
                  : Result<std::size_t>::failure(result.error());
  }
  case Mode::Flat: {
    auto result = AssemblyStepExporter{context}.write_step(project, output.string());
    return result ? Result<std::size_t>::success(result.value().written_bytes)
                  : Result<std::size_t>::failure(result.error());
  }
  case Mode::Structured: {
    auto result = AssemblyStructuredStepExporter{context}.write_step(project, output.string());
    return result ? Result<std::size_t>::success(result.value().written_bytes)
                  : Result<std::size_t>::failure(result.error());
  }
  }
  FAIL("unknown export mode");
}
} // namespace

TEST_CASE("Document-aware STEP exports protect every registered asset alias",
          "[geometry][assembly-step-reference-part][step-reference-export]") {
  Fixture fixture;
  const auto part = fixture.reference();
  const auto project = fixture.project(part);
  const auto original = bytes(fixture.source_path());
  auto cache = checked(ShapeCache::create(ShapeCacheId("cache")));
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(part, cache));
  const auto mode = GENERATE(Mode::Part, Mode::Flat, Mode::Structured);
  const auto alias = fixture.root / "alias.step";
  SECTION("direct source") {
    CHECK_FALSE(export_document(mode, part, cache, project, &context, fixture.source_path()));
  }
  SECTION("canonical path") {
    REQUIRE(std::filesystem::create_directory(fixture.root / "sub"));
    CHECK_FALSE(export_document(mode, part, cache, project, &context,
                                fixture.root / "sub/../supplier.step"));
  }
  SECTION("hardlink alias") {
    std::filesystem::create_hard_link(fixture.source_path(), alias);
    CHECK_FALSE(export_document(mode, part, cache, project, &context, alias));
    CHECK(bytes(alias) == original);
  }
  SECTION("symlink alias") {
    std::filesystem::create_symlink(fixture.source_path(), alias);
    CHECK_FALSE(export_document(mode, part, cache, project, &context, alias));
    CHECK(bytes(alias) == original);
  }
  SECTION("missing registered target remains missing") {
    std::filesystem::rename(fixture.source_path(), fixture.root / "saved.step");
    CHECK_FALSE(export_document(mode, part, cache, project, &context, fixture.source_path()));
    CHECK_FALSE(std::filesystem::exists(fixture.source_path()));
    std::filesystem::rename(fixture.root / "saved.step", fixture.source_path());
  }
  CHECK(bytes(fixture.source_path()) == original);
  fixture.no_temporary_files();
}

TEST_CASE(
    "Reference STEP export publishes successful output and preserves prior output on stale source",
    "[geometry][assembly-step-reference-part][step-reference-export]") {
  Fixture fixture;
  const auto part = fixture.reference();
  const auto project = fixture.project(part);
  const auto original = bytes(fixture.source_path());
  auto cache = checked(ShapeCache::create(ShapeCacheId("cache")));
  StepImportTopologyContext context(fixture.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(part, cache));
  const auto mode = GENERATE(Mode::Part, Mode::Flat, Mode::Structured);
  const auto output = fixture.root / "export.step";
  write(output, "prior accepted output");
  const auto absent = export_document(mode, part, cache, project, nullptr, output);
  REQUIRE_FALSE(absent);
  CHECK(absent.error().message().find("explicit source-root context") != std::string::npos);
  CHECK(bytes(output) == "prior accepted output");
  write(fixture.source_path(), original + "\n/* unaccepted */\n");
  StepImportTopologyContext stale(fixture.root);
  CHECK_FALSE(export_document(mode, part, cache, project, &stale, output));
  CHECK(bytes(output) == "prior accepted output");
  fixture.no_temporary_files();
  write(fixture.source_path(), original);
  StepImportTopologyContext restored(fixture.root);
  const auto exported = checked(export_document(mode, part, cache, project, &restored, output));
  CHECK(exported == std::filesystem::file_size(output));
  CHECK(bytes(output).starts_with("ISO-10303-21;"));
  CHECK(bytes(fixture.source_path()) == original);
  fixture.no_temporary_files();
}

TEST_CASE(
    "Staged STEP publication rejects a source change after writing and cleans its reserved inode",
    "[geometry][assembly-step-reference-part][step-reference-export]") {
  Fixture fixture;
  const auto part = fixture.reference();
  const auto original = bytes(fixture.source_path());
  StepImportTopologyContext context(fixture.root);
  REQUIRE(observe_imported_part_sources(part, &context));
  const auto output = fixture.root / "export.step";
  write(output, "prior accepted output");
  {
    auto transaction = checked(detail::StepExportTransaction::create(part, &context, output));
    REQUIRE(StepExporter{}.write_step(fixture.shape, transaction.writer_path().string()));
    write(fixture.source_path(), original + "\n/* changed during export */\n");
    CHECK_FALSE(transaction.commit());
    CHECK(bytes(output) == "prior accepted output");
  }
  fixture.no_temporary_files();
  write(fixture.source_path(), original);
  {
    auto transaction = checked(detail::StepExportTransaction::create(part, &context, output));
    REQUIRE(StepExporter{}.write_step(fixture.shape, transaction.writer_path().string()));
    std::filesystem::remove(output);
    std::filesystem::create_hard_link(fixture.source_path(), output);
    CHECK_FALSE(transaction.commit());
    CHECK(bytes(fixture.source_path()) == original);
  }
  fixture.no_temporary_files();
}

TEST_CASE("Opaque source metadata still exports authored geometry with an explicit protective root",
          "[geometry][assembly-step-reference-part][step-reference-export]") {
  Fixture fixture;
  auto part = checked(PartDocument::create(DocumentId("authored"), "Authored"));
  REQUIRE(part.add_body(checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  REQUIRE(part.add_step_import_source(fixture.source()));
  auto cache = checked(ShapeCache::create(ShapeCacheId("cache")));
  REQUIRE(cache.store_body_shape(BodyId("body"), FeatureId("authored.feature"), fixture.shape));
  const auto output = fixture.root / "export.step";
  write(output, "prior output");
  CHECK_FALSE(StepExporter{}.write_part_step(part, cache, output.string()));
  CHECK(bytes(output) == "prior output");
  std::filesystem::remove(fixture.source_path());
  StepImportTopologyContext context(fixture.root);
  REQUIRE(StepExporter{&context}.write_part_step(part, cache, output.string()));
  CHECK(bytes(output).starts_with("ISO-10303-21;"));
  CHECK_FALSE(StepExporter{&context}.write_part_step(part, cache, fixture.source_path().string()));
  CHECK_FALSE(std::filesystem::exists(fixture.source_path()));
  fixture.no_temporary_files();
}

TEST_CASE("Document-aware STEP export preserves the historical bodyless result contract",
          "[geometry][assembly-step-reference-part][step-reference-export]") {
  Fixture fixture;
  auto part = checked(PartDocument::create(DocumentId("historical"), "Historical"));
  auto cache = checked(ShapeCache::create(ShapeCacheId("cache")));
  REQUIRE(cache.set_final_shape(FeatureId("historical.feature"), fixture.shape));
  const auto output = fixture.root / "historical.step";
  REQUIRE(StepExporter{}.write_document_step(part, cache, output.string()));
  CHECK(bytes(output).starts_with("ISO-10303-21;"));
  REQUIRE(part.add_step_import_source(fixture.source()));
  const auto original = bytes(fixture.source_path());
  write(output, "prior output");
  CHECK_FALSE(StepExporter{}.write_document_step(part, cache, output.string()));
  CHECK(bytes(output) == "prior output");
  StepImportTopologyContext context(fixture.root);
  REQUIRE(StepExporter{&context}.write_document_step(part, cache, output.string()));
  CHECK_FALSE(
      StepExporter{&context}.write_document_step(part, cache, fixture.source_path().string()));
  CHECK(bytes(fixture.source_path()) == original);
  fixture.no_temporary_files();
}

TEST_CASE(
    "Rejected STEP publication cleanup preserves a foreign replacement of the temporary entry",
    "[geometry][assembly-step-reference-part][step-reference-export]") {
  Fixture fixture;
  const auto part = fixture.reference();
  StepImportTopologyContext context(fixture.root);
  REQUIRE(observe_imported_part_sources(part, &context));
  const auto output = fixture.root / "export.step";
  write(output, "prior output");
  std::filesystem::path reserved;
  std::filesystem::path moved;
  {
    auto transaction = checked(detail::StepExportTransaction::create(part, &context, output));
    REQUIRE(StepExporter{}.write_step(fixture.shape, transaction.writer_path().string()));
    for (const auto& entry : std::filesystem::directory_iterator(fixture.root))
      if (entry.path().filename().string().starts_with(".blcad-step-export-"))
        reserved = entry.path();
    REQUIRE_FALSE(reserved.empty());
    moved = fixture.root / "moved-owned-inode.step";
    std::filesystem::rename(reserved, moved);
    write(reserved, "foreign replacement sentinel");
    CHECK_FALSE(transaction.commit());
    CHECK(bytes(output) == "prior output");
  }
  REQUIRE(std::filesystem::exists(reserved));
  CHECK(bytes(reserved) == "foreign replacement sentinel");
  REQUIRE(std::filesystem::exists(moved));
  CHECK(bytes(moved).starts_with("ISO-10303-21;"));
  CHECK(bytes(output) == "prior output");
}
