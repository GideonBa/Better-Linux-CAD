#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"

#include <iostream>
#include <string_view>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name
            << " <input.blcad.project.json> <output.step>\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] "
            << error.object_id() << ": " << error.message() << '\n';
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    print_usage(argc > 0 ? argv[0] : "blcad_export_structured_assembly");
    return 2;
  }

  auto project = blcad::read_project_json_file(argv[1]);
  if (project.has_error()) {
    print_error(project.error());
    return 1;
  }

  const blcad::geometry::AssemblyStructuredStepExporter exporter;
  const auto written = exporter.write_step(project.value(), argv[2]);
  if (written.has_error()) {
    print_error(written.error());
    return 1;
  }

  std::cout << "Recomputed " << written.value().recomputed_part_count
            << " part definition(s), exported "
            << written.value().exported_assembly_occurrence_count
            << " assembly occurrence(s) and "
            << written.value().exported_component_occurrence_count
            << " component occurrence(s) through "
            << written.value().part_product_definition_count
            << " shared part product definition(s) to " << argv[2] << " ("
            << written.value().written_bytes << " bytes)\n";
  return 0;
}
