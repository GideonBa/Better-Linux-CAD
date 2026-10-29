#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"

#include "../support/rotor_performance_fixture.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {
template <class T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
struct ProfileDirectory {
  std::filesystem::path path;
  ProfileDirectory() {
    char pattern[] = "/tmp/blcad-step-import-profile-XXXXXX";
    const auto* created = ::mkdtemp(pattern);
    REQUIRE(created);
    path = created;
  }
  ~ProfileDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
struct Sample {
  double snapshot_ms{}, reader_ms{}, preview_ms{};
};
double elapsed(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}
} // namespace

// Explicit opt-in observation; timings never become portable correctness budgets.
TEST_CASE("Profile STEP inspection of a connected twenty-blade curved rotor",
          "[.][geometry][step-import-profile]") {
  ProfileDirectory directory;
  const auto part = test::rotor_performance_part(20);
  auto cache = checked(ShapeCache::create(ShapeCacheId("inspection.profile")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto* original = cache.find_body_shape(BodyId("body.rotor"));
  REQUIRE(original);
  const double volume = RectangleExtrusionAdapter{}.summarize(*original).volume_mm3;
  const auto output = checked(
      StepExporter{}.write_part_step(part, cache, (directory.path / "rotor.step").string()));
  REQUIRE(output.exported_body_count == 1);
  const auto asset = checked(inspect_step_import_asset(directory.path, "rotor.step"));
  REQUIRE(asset.observed_sha256_digest());
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("rotor"), "Twenty-blade curved rotor", "rotor.step",
      *asset.observed_sha256_digest(), StepImportMode::Reference));
  const StepImportReader reader;
  std::vector<Sample> samples;
  std::size_t products = 0, faces = 0, occurrences = 0;
  for (int iteration = 0; iteration < 6; ++iteration) {
    auto start = std::chrono::steady_clock::now();
    const auto bytes = checked(read_verified_step_import_source(source, directory.path));
    const auto snapshot_ms = elapsed(start);
    CHECK(bytes.size() == output.written_bytes);
    start = std::chrono::steady_clock::now();
    const auto imported = checked(reader.read(source, directory.path));
    const auto reader_ms = elapsed(start);
    start = std::chrono::steady_clock::now();
    const auto posed = checked(reader.preview_shapes(imported));
    const auto preview_ms = elapsed(start);
    REQUIRE(imported.body_count() == 1);
    REQUIRE(posed.size() == 1);
    CHECK(RectangleExtrusionAdapter{}.summarize(posed.front()).volume_mm3 ==
          Catch::Approx(volume).epsilon(1e-7));
    products = imported.products.size();
    occurrences = imported.occurrences.size();
    faces = 0;
    for (const auto& product : imported.products)
      for (const auto& body : product.bodies)
        faces += body.face_count;
    samples.push_back({snapshot_ms, reader_ms, preview_ms});
  }
  CHECK(checked(inspect_step_import_source(source, directory.path)).state() ==
        StepImportFreshness::Available);
  std::ostringstream json;
  json.imbue(std::locale::classic());
  json << std::setprecision(10)
       << "{\"workload\":\"connected20BladeCurvedRotor\",\"samplePolicy\":"
          "\"first recorded plus five repeated inspections; exported source and OS cache warm; "
          "fixture construction and export excluded\",\"sourceSha256\":\""
       << source.sha256_digest() << "\",\"bytes\":" << output.written_bytes
       << ",\"products\":" << products << ",\"bodies\":1,\"faces\":" << faces
       << ",\"occurrences\":" << occurrences << ",\"volumeMm3\":" << volume << ",\"samples\":[";
  for (std::size_t i = 0; i < samples.size(); ++i) {
    if (i)
      json << ',';
    json << "{\"snapshotMs\":" << samples[i].snapshot_ms << ",\"readerMs\":" << samples[i].reader_ms
         << ",\"previewMs\":" << samples[i].preview_ms << '}';
  }
  json << "]}\n";
  std::cout << json.str();
  if (const auto* path = std::getenv("BLCAD_STEP_IMPORT_PROFILE_OUTPUT")) {
    std::ofstream file(path);
    REQUIRE(file);
    file << json.str();
    REQUIRE(file.good());
  }
}
