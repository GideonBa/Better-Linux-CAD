#include "blcad/core/error.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name << " <input.blcad.json> <output.step>\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] " << error.object_id() << ": "
            << error.message() << '\n';
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    print_usage(argc > 0 ? argv[0] : "blcad_export_step");
    return 2;
  }

  const auto document = blcad::read_part_document_json_file(argv[1]);
  if (document.has_error()) {
    print_error(document.error());
    return 1;
  }

  auto cache = blcad::geometry::ShapeCache::create(blcad::ShapeCacheId("shape_cache.cli_export"));
  if (cache.has_error()) {
    print_error(cache.error());
    return 1;
  }

  std::error_code path_error;
  const auto input_path = std::filesystem::absolute(argv[1], path_error);
  if (path_error) {
    std::cerr << "Cannot resolve input document asset root: " << path_error.message() << '\n';
    return 1;
  }
  blcad::geometry::StepImportTopologyContext import_context(input_path.parent_path());
  const blcad::geometry::GeometryRecomputeExecutor executor{&import_context};
  const auto recompute_summary = executor.execute_document(document.value(), cache.value());
  if (recompute_summary.has_error()) {
    print_error(recompute_summary.error());
    return 1;
  }

  const blcad::geometry::StepExporter exporter{&import_context};
  const auto written = exporter.write_document_step(document.value(), cache.value(), argv[2]);
  if (written.has_error()) {
    print_error(written.error());
    return 1;
  }

  std::cout << "Exported " << written.value() << " bytes to " << argv[2] << '\n';
  return 0;
}
