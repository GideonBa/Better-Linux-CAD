#include "blcad/core/error.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_interference_analyzer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_usage(std::string_view program_name) {
  std::cerr << "Usage: " << program_name
            << " <input.blcad.project.json> [clearance-threshold-mm]\n";
}

void print_error(const blcad::Error& error) {
  std::cerr << "error[" << blcad::to_string(error.category()) << "] " << error.object_id() << ": "
            << error.message() << '\n';
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    print_usage(argc > 0 ? argv[0] : "blcad_analyze_assembly");
    return 2;
  }

  auto project = blcad::read_project_json_file(argv[1]);
  if (project.has_error()) {
    print_error(project.error());
    return 1;
  }

  blcad::geometry::AssemblyClearanceAnalysisOptions options;
  if (argc == 3) {
    char* parsed_end = nullptr;
    options.clearance_threshold_mm = std::strtod(argv[2], &parsed_end);
    if (parsed_end == nullptr || *parsed_end != '\0') {
      print_usage(argv[0]);
      return 2;
    }
  }

  const blcad::geometry::AssemblyClearanceAnalyzer analyzer;
  const auto analysis = analyzer.analyze(project.value(), options);
  if (analysis.has_error()) {
    print_error(analysis.error());
    return 1;
  }

  std::cout << "Assembly analysis for " << project.value().id().value() << '\n'
            << "  leaves:            " << analysis.value().leaf_count << '\n'
            << "  evaluated pairs:   " << analysis.value().evaluated_pair_count << '\n'
            << "  recomputed parts:  " << analysis.value().recomputed_part_count << '\n'
            << "  clearance limit:   " << options.clearance_threshold_mm << " mm\n";

  std::cout << "Interferences (" << analysis.value().interferences.size() << "):\n";
  for (const auto& record : analysis.value().interferences) {
    std::cout << "  " << record.leaf_a.occurrence_key << " x " << record.leaf_b.occurrence_key
              << " overlap " << record.overlap_volume_mm3 << " mm^3\n";
  }

  std::cout << "Clearance violations (" << analysis.value().clearance_violations.size() << "):\n";
  for (const auto& record : analysis.value().clearance_violations) {
    std::cout << "  " << record.leaf_a.occurrence_key << " x " << record.leaf_b.occurrence_key
              << " distance " << record.minimum_distance_mm << " mm\n";
  }

  return 0;
}
