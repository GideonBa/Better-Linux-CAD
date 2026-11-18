#include "../support/engineering_resource_fixture.hpp"
#include "../support/part_history_fixture.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
using namespace blcad;
using namespace blcad::geometry;
namespace f = test::history_fixture;
namespace e = test::engineering_fixture;
using Clock = std::chrono::steady_clock;
constexpr unsigned warm_repeats = 10;

void require(bool condition, const std::string& message) {
  if (!condition)
    throw std::runtime_error(message);
}
void close(double actual, double expected, const char* message) {
  require(std::isfinite(actual) &&
              std::abs(actual - expected) <= 1e-8 * std::max(1.0, std::abs(expected)),
          message);
}
double milliseconds(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
std::string read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "cannot read " + path.string());
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path& path, const nlohmann::json& value, bool compact = false) {
  const auto temporary = path.string() + ".tmp";
  {
    std::ofstream file(temporary);
    require(file.good(), "cannot open " + temporary);
    file << value.dump(compact ? -1 : 2) << '\n';
    require(file.good(), "cannot publish " + temporary);
  }
  std::filesystem::rename(temporary, path);
}
std::string sha256(std::string_view bytes) {
  return f::checked(step_import_sha256(bytes));
}
bool digest(const std::string& value) {
  return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
nlohmann::json range(std::vector<double> values) {
  require(!values.empty(), "empty timing range");
  std::sort(values.begin(), values.end());
  const auto middle = values.size() / 2;
  const auto median =
      values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
  const auto percentile_rank = static_cast<std::size_t>(std::ceil(.95 * values.size()));
  return {{"sample_count", values.size()}, {"min_ms", values.front()},
          {"median_ms", median},           {"p95_ms", values[percentile_rank - 1]},
          {"p95_method", "nearest-rank"},  {"max_ms", values.back()}};
}
std::string system_line(const char* path, const char* prefix) {
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.starts_with(prefix))
      continue;
    const auto colon = line.find(':');
    return colon == std::string::npos ? line : line.substr(colon + 1);
  }
  return "unavailable";
}
std::string utc_now() {
  const auto now = std::time(nullptr);
  std::tm value{};
  require(::gmtime_r(&now, &value), "UTC timestamp unavailable");
  char output[32]{};
  require(std::strftime(output, sizeof(output), "%Y-%m-%dT%H:%M:%SZ", &value),
          "UTC timestamp formatting failed");
  return output;
}
double volume(const ShapeCache& cache) {
  const auto* shape = cache.find_body_shape(BodyId("body.base"));
  require(shape, "state Body result unavailable");
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}
struct Workload {
  nlohmann::json source;
  nlohmann::json result;
};

Workload states(unsigned count) {
  const auto seed = f::native_base();
  auto primary = f::checked(PartDocument::create(seed.id(), seed.name()));
  // Height is authored as an expression from the start. Literal parameters
  // cannot be converted by the formula-edit API.
  for (const auto& parameter : seed.parameters())
    if (parameter.id() != ParameterId("height"))
      (void)f::checked(primary.add_parameter(parameter));
  (void)f::checked(primary.add_expression_parameter(ParameterId("height"), "height",
                                                    ParameterType::Length, "width / 2"));
  for (const auto& plane : seed.datum_planes())
    (void)f::checked(primary.add_datum_plane(plane));
  for (const auto& axis : seed.datum_axes())
    (void)f::checked(primary.add_datum_axis(axis));
  for (const auto& sketch : seed.sketches())
    (void)f::checked(primary.add_sketch(sketch));
  for (const auto& body : seed.bodies())
    (void)f::checked(primary.add_body(body));
  for (const auto& feature : seed.features())
    (void)f::checked(primary.add_feature(feature));
  e::annotate(primary);
  std::vector<ModelState> definitions;
  for (unsigned index = 0; index < count; ++index) {
    const double width = 20 + index / 2;
    definitions.push_back({ModelStateId("state." + std::to_string(index)),
                           "State " + std::to_string(index),
                           {{ParameterId("width"), f::checked(Quantity::length_mm(width))}},
                           {},
                           {{std::nullopt, e::key("synthetic.material", index % 2 ? "r2" : "r1")}},
                           {},
                           {}});
  }
  (void)f::checked(primary.replace_model_states(definitions));
  const auto source = f::checked(serialize_part_document_to_json(primary));
  std::vector<ShapeCache> caches;
  caches.reserve(count);
  for (unsigned index = 0; index < count; ++index)
    caches.push_back(
        f::checked(ShapeCache::create(ShapeCacheId("state.profile." + std::to_string(index)))));
  const GeometryRecomputeExecutor executor;
  std::optional<ShapeCache> primary_cache;
  std::vector<double> evaluation_warm, geometry_warm;
  nlohmann::json samples = nlohmann::json::array();
  for (unsigned iteration = 0; iteration <= warm_repeats; ++iteration) {
    std::vector<PartDocument> effective;
    effective.reserve(count);
    const auto evaluate_start = Clock::now();
    for (const auto& definition : definitions)
      effective.push_back(f::checked(primary.evaluate_model_state(definition.id)));
    const auto evaluation_ms = milliseconds(evaluate_start);
    std::vector<GeometryRecomputeSummary> summaries;
    summaries.reserve(count);
    const auto geometry_start = Clock::now();
    for (unsigned index = 0; index < count; ++index)
      summaries.push_back(f::checked(executor.execute_document(effective[index], caches[index])));
    const auto geometry_ms = milliseconds(geometry_start);
    std::size_t executed = 0, reused = 0;
    for (unsigned index = 0; index < count; ++index) {
      const double width = 20 + index / 2;
      const double expected_volume = 4 * width * width;
      close(volume(caches[index]), expected_volume, "state volume differs from analytic box");
      (void)f::checked(caches[index].validate_complete_geometry_result(effective[index]));
      require(!caches[index].validate_complete_geometry_result(primary),
              "named state cache incorrectly accepted Primary");
      require(!caches[index].validate_complete_geometry_result(effective[index ^ 1U]),
              "equal-valued sibling state incorrectly accepted another state "
              "cache");
      require(effective[index].effective_model_state() == definitions[index].id,
              "effective state identity changed");
      if (iteration == 0) {
        const auto physical =
            f::checked(BodyResultInspector{}.inspect_physical_properties(effective[index]));
        const auto* mass = physical.data().complete_mass();
        require(mass, "state mass unavailable");
        close(physical.data().volume_mm3, expected_volume, "state physical volume mismatch");
        close(mass->mass_kg, expected_volume * (index % 2 ? 6e-6 : 4e-6),
              "state material override mismatch");
        (void)f::checked(physical.validate_current(effective[index]));
        require(!physical.validate_current(effective[index ^ 1U]),
                "state physical snapshot accepted another state");
      }
      executed += summaries[index].executed_feature_count;
      reused += summaries[index].reused_complete_geometry ? 1 : 0;
    }
    if (!primary_cache) {
      primary_cache = f::checked(ShapeCache::create(ShapeCacheId("state.profile.primary")));
      (void)f::checked(executor.execute_document(primary, *primary_cache));
    }
    close(volume(*primary_cache), 1600, "state workload changed retained Primary geometry");
    (void)f::checked(primary_cache->validate_complete_geometry_result(primary));
    require(!primary_cache->validate_complete_geometry_result(effective.front()),
            "retained Primary cache incorrectly accepted a named sibling");
    require(f::checked(serialize_part_document_to_json(primary)) == source,
            "effective evaluation or Geometry mutated authored Primary");
    require(std::isfinite(evaluation_ms) && std::isfinite(geometry_ms), "nonfinite state timing");
    if (iteration) {
      evaluation_warm.push_back(evaluation_ms);
      geometry_warm.push_back(geometry_ms);
    }
    samples.push_back({{"iteration", iteration},
                       {"phase", iteration ? "warm" : "first-cold"},
                       {"effective_evaluation_batch_ms", evaluation_ms},
                       {"geometry_batch_ms", geometry_ms},
                       {"executed_feature_count", executed},
                       {"reused_complete_geometry_state_count", reused},
                       {"checked_state_count", count}});
  }
  return {nlohmann::json::parse(source),
          {{"scene", "shared native box with sparse width and material sibling states"},
           {"named_states", count},
           {"geometry_cache_policy", "one independent persistent ShapeCache per named state"},
           {"equal_geometry_pair_policy", "consecutive state pairs share width "
                                          "but differ in identity and density"},
           {"cold_definition", "first measured batch after Core catalog validation; all state "
                               "Geometry caches empty; no OS cache flush"},
           {"timed",
            {"effective evaluation of every state", "Geometry execute_document of every state"}},
           {"excluded_from_timing",
            {"fixture and catalog validation", "source serialization",
             "independent volume/mass/freshness checks and retained Primary cache"}},
           {"unmeasured", {"native UI state switching", "undo/history transaction", "STEP export"}},
           {"effective_evaluation_warm_summary", range(evaluation_warm)},
           {"geometry_warm_summary", range(geometry_warm)},
           {"samples", std::move(samples)}}};
}

RigidTransform group_transform(unsigned index) {
  return {{20.0 * index, 5.0 * (index % 3), 0}, {0, 0, 90.0 * (index % 4)}};
}
RigidTransform moving_transform(unsigned index) {
  return {{0, 0, 3.0 + index % 5}, {}};
}
Point3 rotate_z(Point3 point, double degrees) {
  const double angle = degrees * std::numbers::pi / 180;
  return {std::cos(angle) * point.x - std::sin(angle) * point.y,
          std::sin(angle) * point.x + std::cos(angle) * point.y, point.z};
}
Point3 transformed(Point3 point, const RigidTransform& transform) {
  // The generator deliberately uses only Z rotation. This independent oracle
  // does not call the production transform composition or hierarchy resolver.
  require(transform.rotation_deg.x == 0 && transform.rotation_deg.y == 0,
          "generator oracle received unsupported rotation");
  point = rotate_z(point, transform.rotation_deg.z);
  return {point.x + transform.translation_mm.x, point.y + transform.translation_mm.y,
          point.z + transform.translation_mm.z};
}
Project assembly_source(unsigned leaves) {
  auto part = f::native_base();
  (void)f::checked(
      part.set_parameter_value(ParameterId("width"), f::checked(Quantity::length_mm(2))));
  (void)f::checked(
      part.set_parameter_value(ParameterId("height"), f::checked(Quantity::length_mm(2))));
  (void)f::checked(
      part.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(1))));
  e::annotate(part);
  auto module = f::checked(AssemblyDocument::create(DocumentId("profile.module"), "Module"));
  (void)f::checked(module.add_member_part(part.id()));
  for (const bool moving : {false, true}) {
    const std::string id = moving ? "moving" : "fixed";
    (void)f::checked(module.add_component_instance(f::checked(ComponentInstance::create(
        ComponentInstanceId(id), id, part.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active,
        moving ? ComponentGroundingState::Free : ComponentGroundingState::Grounded,
        moving ? moving_transform(0) : identity_rigid_transform()))));
  }
  auto wrapper = f::checked(AssemblyDocument::create(DocumentId("profile.wrapper"), "Wrapper"));
  (void)f::checked(wrapper.add_subassembly_instance(f::checked(SubassemblyInstance::create(
      SubassemblyInstanceId("module"), "Module", module.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, {{3, 4, 2}, {0, 0, 30}}))));
  auto root = f::checked(AssemblyDocument::create(DocumentId("profile.root"), "Root"));
  for (unsigned index = 0; index < leaves / 2; ++index) {
    const std::string id = "group." + std::to_string(index);
    (void)f::checked(root.add_subassembly_instance(f::checked(SubassemblyInstance::create(
        SubassemblyInstanceId(id), id, wrapper.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, group_transform(index)))));
  }
  auto project = f::checked(Project::create(DocumentId("profile.project"), "Profile", root));
  (void)f::checked(project.add_part_document(part));
  (void)f::checked(project.add_child_assembly_document(module));
  (void)f::checked(project.add_child_assembly_document(wrapper));
  for (unsigned index = 0; index < leaves / 2; ++index)
    (void)f::checked(project.set_occurrence_pose(f::checked(AssemblyOccurrencePose::create(
        {SubassemblyInstanceId("group." + std::to_string(index)), SubassemblyInstanceId("module")},
        module.id(), AssemblyOccurrenceRigidity::Flexible,
        {{ComponentInstanceId("moving"), moving_transform(index)}}))));
  return project;
}
Point3 expected_center(unsigned index, bool moving) {
  const auto module =
      transformed({0, 0, .5}, moving ? moving_transform(index) : identity_rigid_transform());
  return transformed(transformed(module, {{3, 4, 2}, {0, 0, 30}}), group_transform(index));
}
Workload assembly(unsigned count) {
  const auto project = assembly_source(count);
  const auto source = f::checked(serialize_project_to_json(project));
  const auto module_source = f::checked(serialize_assembly_document_to_json(
      *project.find_assembly_document(DocumentId("profile.module"))));
  const auto wrapper_source = f::checked(serialize_assembly_document_to_json(
      *project.find_assembly_document(DocumentId("profile.wrapper"))));
  Point3 expected_centroid{};
  for (unsigned index = 0; index < count / 2; ++index)
    for (const bool moving : {false, true}) {
      const auto center = expected_center(index, moving);
      expected_centroid.x += center.x / count;
      expected_centroid.y += center.y / count;
      expected_centroid.z += center.z / count;
    }
  const AssemblyLeafOccurrenceResolver resolver;
  const BodyResultInspector inspector;
  std::optional<ProjectPhysicalProperties> previous;
  std::vector<double> resolver_warm, physical_warm;
  nlohmann::json samples = nlohmann::json::array();
  for (unsigned iteration = 0; iteration <= warm_repeats; ++iteration) {
    const auto resolver_start = Clock::now();
    auto resolved = resolver.resolve(project);
    const auto resolver_ms = milliseconds(resolver_start);
    const auto descriptors = f::checked(std::move(resolved));
    const auto physical_start = Clock::now();
    auto physical_result = previous ? inspector.inspect_physical_properties(project, *previous)
                                    : inspector.inspect_physical_properties(project);
    const auto physical_ms = milliseconds(physical_start);
    const auto physical = f::checked(std::move(physical_result));
    require(descriptors.size() == count, "resolver dropped a nested occurrence");
    require(physical.data().occurrences.size() == count,
            "physical properties dropped an occurrence");
    for (const auto& descriptor : descriptors) {
      require(descriptor.subassembly_path.size() == 2, "nested occurrence path lost a level");
      require(descriptor.transforms_inner_to_outer.size() == 3,
              "nested occurrence lost a transform");
      require(descriptor.subassembly_path[1] == SubassemblyInstanceId("module"),
              "nested module path changed");
      const auto name = descriptor.subassembly_path[0].value();
      require(name.starts_with("group."), "unexpected group identity");
      const auto index = static_cast<unsigned>(std::stoul(name.substr(6)));
      require(index < count / 2, "resolved group outside authored scene");
      const bool moving = descriptor.component_instance == ComponentInstanceId("moving");
      require(moving || descriptor.component_instance == ComponentInstanceId("fixed"),
              "unknown leaf component");
      Point3 actual{0, 0, .5};
      for (const auto& transform : descriptor.transforms_inner_to_outer)
        actual = transformed(actual, transform);
      const auto expected = expected_center(index, moving);
      close(actual.x, expected.x, "nested flexible X pose mismatch");
      close(actual.y, expected.y, "nested flexible Y pose mismatch");
      close(actual.z, expected.z, "nested flexible Z pose mismatch");
    }
    close(physical.data().volume_mm3, 4.0 * count, "assembly analytic volume mismatch");
    require(physical.data().solid_body_count == count, "assembly solid count mismatch");
    const auto* mass = physical.data().complete_mass();
    require(mass && physical.data().volume_centroid_mm,
            "assembly complete mass/centroid unavailable");
    close(mass->mass_kg, 4.0 * count * 4e-6, "assembly analytic mass mismatch");
    close(physical.data().volume_centroid_mm->x, expected_centroid.x,
          "assembly centroid X mismatch");
    close(physical.data().volume_centroid_mm->y, expected_centroid.y,
          "assembly centroid Y mismatch");
    close(physical.data().volume_centroid_mm->z, expected_centroid.z,
          "assembly centroid Z mismatch");
    (void)f::checked(physical.validate_current(project));
    require(f::checked(serialize_project_to_json(project)) == source,
            "resolver or physical evaluation mutated authored Project");
    require(f::checked(serialize_assembly_document_to_json(
                *project.find_assembly_document(DocumentId("profile.module")))) == module_source,
            "flexible occurrence evaluation mutated shared module definition");
    require(f::checked(serialize_assembly_document_to_json(
                *project.find_assembly_document(DocumentId("profile.wrapper")))) == wrapper_source,
            "nested wrapper definition changed");
    require(std::isfinite(resolver_ms) && std::isfinite(physical_ms), "nonfinite assembly timing");
    if (iteration) {
      resolver_warm.push_back(resolver_ms);
      physical_warm.push_back(physical_ms);
    }
    samples.push_back({{"iteration", iteration},
                       {"phase", iteration ? "warm" : "first-cold"},
                       {"resolver_ms", resolver_ms},
                       {"physical_properties_ms", physical_ms},
                       {"computed_part_count", physical.data().computed_part_count},
                       {"reused_part_count", physical.data().reused_part_count},
                       {"resolved_leaf_count", descriptors.size()},
                       {"independent_volume_mm3", 4.0 * count},
                       {"independent_mass_kg", 4.0 * count * 4e-6}});
    previous = physical;
  }
  return {
      nlohmann::json::parse(source),
      {{"scene", "two-level repeated wrapper/module with two shared native "
                 "Part leaves and independent flexible poses"},
       {"leaf_occurrences", count},
       {"nested_flexible_occurrences", count / 2},
       {"shared_part_definitions", 1},
       {"shared_child_assembly_definitions", 2},
       {"cold_definition", "first timed resolver and first physical inspection "
                           "after public-API fixture validation; no prior "
                           "Geometry executor call; no OS cache flush"},
       {"physical_cache_policy", "first inspection has no previous snapshot; each warm inspection "
                                 "receives preceding immutable snapshot"},
       {"timed",
        {"leaf occurrence resolver",
         "physical properties including production geometry/reuse checks"}},
       {"excluded_from_timing",
        {"scene generation and structural validation", "previous snapshot copy",
         "serialization and analytic pose/volume/mass/centroid checks"}},
       {"unmeasured",
        {"occurrence-local solve/motion", "flat/structured STEP export", "native UI presentation"}},
       {"resolver_warm_summary", range(resolver_warm)},
       {"physical_properties_warm_summary", range(physical_warm)},
       {"samples", std::move(samples)}}};
}
} // namespace

int main(int argc, char** argv) {
  nlohmann::json result{{"schema", "blcad.state-assembly-profile.v1"}, {"status", "failed"}};
  try {
    require(argc == 7, "Usage: blcad_variant_assembly_profile output.json states|assembly "
                       "size runtime_manifest.json runtime_manifest_sha256 executable_sha256");
    const std::string mode = argv[2];
    const std::string size_text = argv[3];
    require(!size_text.empty() && std::all_of(size_text.begin(), size_text.end(),
                                              [](char c) { return c >= '0' && c <= '9'; }),
            "size must be a positive integer");
    const auto size = std::stoul(size_text);
    require((mode == "states" && (size == 2 || size == 10 || size == 50)) ||
                (mode == "assembly" && (size == 10 || size == 100 || size == 1000)),
            "unsupported mode/size; supported States 2/10/50 and Assembly "
            "10/100/1000");
    require(digest(argv[5]) && digest(argv[6]), "invalid provenance digest");
    require(sha256(read(argv[4])) == argv[5], "runtime manifest digest mismatch");
    require(sha256(read("/proc/self/exe")) == argv[6], "running executable digest mismatch");
    struct utsname os {};
    require(::uname(&os) == 0, "OS identity unavailable");
    result.update({{"started_utc", utc_now()},
                   {"process_id", ::getpid()},
                   {"mode", mode},
                   {"size", size},
                   {"warm_repeats", warm_repeats},
                   {"runtime_manifest_sha256", argv[5]},
                   {"executable_sha256", argv[6]},
                   {"compiler", __VERSION__},
                   {"cpp_standard", __cplusplus},
                   {"build_type", BLCAD_P01_PROFILE_BUILD_TYPE},
                   {"occt_version", BLCAD_P01_PROFILE_OCCT_VERSION},
                   {"qt_configured_version", BLCAD_P01_PROFILE_QT_VERSION},
                   {"os", os.sysname},
                   {"kernel", os.release},
                   {"machine", os.machine},
                   {"cpu_model", system_line("/proc/cpuinfo", "model name")},
                   {"memory_total", system_line("/proc/meminfo", "MemTotal")},
                   {"hardware_threads", std::thread::hardware_concurrency()},
                   {"budget_policy", "baseline only; no portable latency assertions"},
                   {"scope", "synchronous headless calls; no native UI or interactive "
                             "latency claim"}});
    const auto workload_start = Clock::now();
    const auto workload = mode == "states" ? states(static_cast<unsigned>(size))
                                           : assembly(static_cast<unsigned>(size));
    result["wall_ms_including_setup_and_checks"] = milliseconds(workload_start);
    struct rusage usage {};
    require(::getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss > 0,
            "positive Linux process peak RSS is unavailable");
    result["peak_rss_kib"] = usage.ru_maxrss;
    result["peak_rss_measurement_point"] =
        "after workload generation, all timed batches and independent checks; before source "
        "and result artifact serialization/publication";
    result["peak_rss_scope"] =
        "Linux process-lifetime getrusage(RUSAGE_SELF).ru_maxrss in KiB; includes provenance "
        "reads, setup, caches/snapshots and checks; no baseline subtraction";
    const auto source_path = std::filesystem::path(std::string(argv[1]) + ".source.json");
    write(source_path, workload.source, true);
    result["source_intent_file"] = source_path.filename().string();
    result["source_intent_sha256"] = sha256(read(source_path));
    result["workload"] = workload.result;
    result["status"] = "passed";
    result["finished_utc"] = utc_now();
    write(argv[1], result);
    std::cout << mode << ' ' << size << ": first sample and " << warm_repeats
              << " warm samples passed independent checks.\n";
    return 0;
  } catch (const std::exception& error) {
    result["error"] = error.what();
    if (argc >= 2) {
      try {
        write(argv[1], result);
      } catch (const std::exception& publish_error) {
        std::cerr << publish_error.what() << '\n';
      }
    }
    std::cerr << error.what() << '\n';
    return 1;
  }
}
