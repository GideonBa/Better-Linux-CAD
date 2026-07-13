#include "blcad/core/error.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/shape_cache.hpp"
#include "blcad/geometry/step_exporter.hpp"

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

  const blcad::geometry::GeometryRecomputeExecutor executor;
  const auto recompute_summary = executor.execute_document(document.value(), cache.value());
  if (recompute_summary.has_error()) {
    print_error(recompute_summary.error());
    return 1;
  }

  const auto final_shape = blcad::geometry::BodyResultInspector{}.resolve_compatible_final_shape(
      document.value(), cache.value());
  if (final_shape.has_error()) {
    print_error(final_shape.error());
    return 1;
  }

  const blcad::geometry::StepExporter exporter;
  const auto written = exporter.write_step(final_shape.value(), argv[2]);
  if (written.has_error()) {
    print_error(written.error());
    return 1;
  }

  std::cout << "Exported " << written.value() << " bytes to " << argv[2] << '\n';
  return 0;
}
