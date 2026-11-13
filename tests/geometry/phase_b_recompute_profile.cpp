#include "../support/phase_b_acceptance_fixture.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/utsname.h>
#include <thread>
#include <vector>

namespace {
using namespace blcad;
using namespace blcad::geometry;
using test::history_fixture::checked;
using Clock = std::chrono::steady_clock;
void require(bool valid, const char* message) {
  if (!valid)
    throw std::runtime_error(message);
}
double milliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
bool digest(const std::string& value) {
  return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
double volume(const ShapeCache& cache, const char* body) {
  const auto* shape = cache.find_body_shape(BodyId(body));
  require(shape, "profile result Body unavailable");
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
nlohmann::json range(std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  return {{"min_ms", samples.front()},
          {"median_ms", samples[samples.size() / 2]},
          {"max_ms", samples.back()}};
}
std::string cpu_model() {
  std::ifstream input("/proc/cpuinfo");
  std::string line;
  while (std::getline(input, line))
    if (line.starts_with("model name")) {
      const auto colon = line.find(':');
      if (colon != std::string::npos)
        return line.substr(colon + 2);
    }
  return "unavailable";
}
nlohmann::json profile(bool migrated) {
  const auto source = test::phase_b_fixture::complete(migrated);
  const auto source_json = checked(serialize_part_document_to_json(source));
  const GeometryRecomputeExecutor executor;
  auto seed = checked(ShapeCache::create(ShapeCacheId("phase-b.profile.seed")));
  auto start = Clock::now();
  auto seed_result = executor.execute_document(source, seed);
  const auto seed_ms = milliseconds(start);
  const auto seeded = checked(std::move(seed_result));
  require(std::abs(volume(seed, "body.base") - test::phase_b_fixture::base_volume(10, migrated)) <
              1e-5,
          "seed volume differs from independent material oracle");
  require(std::abs(volume(seed, "body.unrelated") - 48) < 1e-7, "hidden Body changed");
  auto changed = source;
  (void)checked(
      changed.set_parameter_value(ParameterId("diameter"), checked(Quantity::length_mm(4))));
  const auto plan = checked(
      RecomputePlan::from_graph_and_changed_nodes(changed.dependency_graph(), {"diameter"}));
  nlohmann::json samples = nlohmann::json::array();
  std::vector<double> incremental_times, fresh_times;
  for (unsigned iteration = 0; iteration < 7; ++iteration) {
    auto incremental = seed;
    auto fresh = checked(ShapeCache::create(ShapeCacheId("phase-b.profile.fresh")));
    GeometryRecomputeSummary incremental_summary, fresh_summary;
    double incremental_ms{}, fresh_ms{};
    const auto run_incremental = [&] {
      const auto started = Clock::now();
      auto result = executor.execute_plan(changed, plan, incremental);
      incremental_ms = milliseconds(started);
      incremental_summary = checked(std::move(result));
    };
    const auto run_fresh = [&] {
      const auto started = Clock::now();
      auto result = executor.execute_document(changed, fresh);
      fresh_ms = milliseconds(started);
      fresh_summary = checked(std::move(result));
    };
    if (iteration % 2 == 0) {
      run_incremental();
      run_fresh();
    } else {
      run_fresh();
      run_incremental();
    }
    const auto expected = test::phase_b_fixture::base_volume(10, migrated, 4);
    require(std::abs(volume(incremental, "body.base") - expected) < 1e-5,
            "incremental material mismatch");
    require(std::abs(volume(fresh, "body.base") - expected) < 1e-5,
            "empty-cache material mismatch");
    require(std::abs(volume(incremental, "body.unrelated") - 48) < 1e-7,
            "incremental hidden Body mismatch");
    require(std::abs(volume(fresh, "body.unrelated") - 48) < 1e-7, "fresh hidden Body mismatch");
    (void)checked(incremental.validate_complete_geometry_result(changed));
    (void)checked(fresh.validate_complete_geometry_result(changed));
    require(std::isfinite(incremental_ms) && std::isfinite(fresh_ms), "nonfinite measurement");
    incremental_times.push_back(incremental_ms);
    fresh_times.push_back(fresh_ms);
    samples.push_back(
        {{"iteration", iteration},
         {"first_measured_pair", iteration == 0},
         {"order", iteration % 2 == 0 ? "incremental-first" : "fresh-first"},
         {"incremental_ms", incremental_ms},
         {"fresh_ms", fresh_ms},
         {"incremental_executed_features", incremental_summary.executed_feature_count},
         {"fresh_executed_features", fresh_summary.executed_feature_count},
         {"incremental_reused_complete_geometry", incremental_summary.reused_complete_geometry},
         {"fresh_reused_complete_geometry", fresh_summary.reused_complete_geometry},
         {"expected_base_volume_mm3", expected},
         {"hidden_body_volume_mm3", 48}});
  }
  require(checked(serialize_part_document_to_json(source)) == source_json, "source intent changed");
  require(std::abs(volume(seed, "body.base") - test::phase_b_fixture::base_volume(10, migrated)) <
              1e-5,
          "baseline cache changed during paired runs");
  return {{"variant", migrated ? "migrated-v3-with-legacy-cut" : "fresh"},
          {"history_mode", source.authored_history_intent() ? "authored" : "implicit"},
          {"source_intent_sha256", checked(step_import_sha256(source_json))},
          {"changed_intent_sha256",
           checked(step_import_sha256(checked(serialize_part_document_to_json(changed))))},
          {"bodies", source.bodies().size()},
          {"retained_holes", 10},
          {"seed_ms", seed_ms},
          {"seed_executed_features", seeded.executed_feature_count},
          {"policy", "First measured pair and six repeats; seed and kernel warm before "
                     "paired runs; alternating paired order; no portable budget"},
          {"incremental_summary", range(incremental_times)},
          {"fresh_summary", range(fresh_times)},
          {"samples", samples}};
}
} // namespace
int main(int argc, char** argv) {
  try {
    if (argc != 4 || !digest(argv[2]) || !digest(argv[3])) {
      std::cerr << "Usage: blcad_phase_b_profile output.json "
                   "runtime_manifest_sha256 executable_sha256\n";
      return 2;
    }
    struct utsname operating_system {};
    require(::uname(&operating_system) == 0, "OS identity unavailable");
    const auto fresh = profile(false);
    const auto migrated = profile(true);
    nlohmann::json result{
        {"schema", "blcad.phase-ab-recompute-profile.v1"},
        {"scope", "Synchronous GeometryRecomputeExecutor calls only; mutation, "
                  "plan creation, cache copying, fixture setup, assertions, "
                  "external serialization and IO excluded from paired samples; no "
                  "UI/history/pointer-latency claim"},
        {"seed_policy", "First fresh seed is the first Geometry executor call in this "
                        "process; migrated seed follows the fresh workload"},
        {"incremental_policy",
         "Existing Hole/Thread execute_plan takes conservative "
         "complete-history fallback; executed counts and reuse flags retained"},
        {"runtime_manifest_sha256", argv[2]},
        {"executable_sha256", argv[3]},
        {"build_type", BLCAD_PHASE_B_PROFILE_BUILD_TYPE},
        {"compiler", __VERSION__},
        {"occt_version", BLCAD_PHASE_B_PROFILE_OCCT_VERSION},
        {"os", operating_system.sysname},
        {"kernel", operating_system.release},
        {"machine", operating_system.machine},
        {"cpu_model", cpu_model()},
        {"hardware_threads", std::thread::hardware_concurrency()},
        {"workloads", nlohmann::json::array({fresh, migrated})}};
    std::ofstream output(argv[1]);
    require(output.good(), "profile output cannot be opened");
    output << result.dump(2) << '\n';
    require(output.good(), "profile output publication failed");
    std::cout << "Recorded two independently checked workloads with seven "
                 "paired samples each.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
